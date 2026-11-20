// WallBox measure: every static model of a given xmodel in a zone's GfxWorld: origin, axis, scale, model bounds.
// Usage: WallBox measure --game <1.1.1.13> --zone <name> --model <xmodel>
using Codol.Assets.Game;
using Codol.FastFiles.FastFile;
using Codol.FastFiles.Porting;

internal static class WallBox
{
    private static int Main(string[] args)
    {
        var o = new Dictionary<string, string>();
        for (int i = 1; i + 1 < args.Length; i += 2) o[args[i].TrimStart('-')] = args[i + 1];
        string game = o.GetValueOrDefault("game", @"D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13");
        using var inst = GameInstallation.Open(game);
        string path = o.TryGetValue("input", out var p) ? p : Path.Combine(game, "client_shipRetail", "chinese", o["zone"] + ".ff");
        var contents = new FastFileReader(inst.RsaKey).Read(path);
        var image = ZoneCapture.Capture(contents.Build, contents.Name, contents.Zone).Image;
        var m = image.Memory;
        if (args[0] == "fog") return Fog(image, o.GetValueOrDefault("list", "0") == "1");
        string want = o["model"];
        foreach (var (world, type) in image.AssetHeaders)
        {
            if (type != 21) continue;
            uint dpvs = world + 0x1C8;
            int count = (int)m.U32(dpvs);
            uint draws = image.Pointer(dpvs + 0x5C);
            for (int i = 0; i < count; i++)
            {
                uint d = draws + (uint)(76 * i);
                uint model = image.Pointer(d + 0x34);
                if (model == 0) continue;
                string name = m.CString(image.Pointer(model), 128) ?? "?";
                if (name != want) continue;
                float F(uint off) => m.F32(d + off);
                Console.WriteLine($"smodel {i} {name} origin ({F(0):F2} {F(4):F2} {F(8):F2}) axis0 ({F(12):F3} {F(16):F3} {F(20):F3}) axis1 ({F(24):F3} {F(28):F3} {F(32):F3}) axis2 ({F(36):F3} {F(40):F3} {F(44):F3}) scale {F(48):F3}" +
                    $" | model mid ({m.F32(model + 320):F2} {m.F32(model + 324):F2} {m.F32(model + 328):F2}) half ({m.F32(model + 332):F2} {m.F32(model + 336):F2} {m.F32(model + 340):F2}) flags 0x{m.U8(d + 62):x} cull {m.U16(d + 56)}");
            }
        }
        foreach (var (h, t) in image.AssetHeaders)
            if (t == 4) { uint np = image.Pointer(h); string? n = np == 0 ? null : m.CString(np); if (n != null && n.TrimStart(',') == want) Console.WriteLine($"xmodel asset {n} at 0x{h:X8} top-level {image.Assets.Any(a => a.Header == h)}"); }
        return 0;
    }

    /// fog: every techset of the zone; per techset the plain fog lighting slots (odd 9..35, as NativeTechsetConverter)
    /// that still hold a sun-fog ("dfog") technique while the even slot also does. Prints a summary line and, with
    /// --list 1, one line per techset: name, the dfog plain slots, the technique names of slots 9 and 10.
    private static int Fog(ZoneImage image, bool list)
    {
        var m = image.Memory;
        string? Tech(uint t) { if (t == 0) return null; uint n = image.Pointer(t); return n == 0 ? null : m.CString(n, 128); }
        int techsets = 0, lit = 0, dfogPlain = 0;
        var names = new SortedDictionary<string, string>(StringComparer.Ordinal);
        foreach (var (h, t) in image.AssetHeaders)
        {
            if (t != 9) continue;
            uint np = image.Pointer(h);
            string? name = np == 0 ? null : m.CString(np, 128);
            if (name is null || name.StartsWith(',')) continue;
            techsets++;
            var bad = new List<uint>();
            bool any = false;
            for (uint slot = 9; slot <= 35; slot += 2)
            {
                string? odd = Tech(image.Pointer(h + 0x0C + 4 * slot));
                if (odd is null) continue;
                any = true;
                if (odd.Contains("dfog", StringComparison.Ordinal)) bad.Add(slot);
            }
            if (any) lit++;
            if (bad.Count > 0) dfogPlain++;
            names[name] = $"{(bad.Count > 0 ? "DFOG-IN-PLAIN " + string.Join(",", bad) : "ok")} | 9 {Tech(image.Pointer(h + 0x0C + 36))} | 10 {Tech(image.Pointer(h + 0x0C + 40))}";
        }
        Console.WriteLine($"{techsets} techsets, {lit} with lit fog slots, {dfogPlain} with a sun-fog technique in a plain fog slot");
        if (list) foreach (var (n, v) in names) Console.WriteLine($"  {n}: {v}");
        return 0;
    }
}
