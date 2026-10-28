using Codol.FastFiles;
using Codol.FastFiles.FastFile;
using Codol.FastFiles.Porting;

namespace Codol.SoundDumper;

/// mp-zone-inspect: what a zone holds for named models and materials (lods, surfaces, encodings, texture tables), to plan
/// the MP weapon zone (mp_codol_weapons_postgfx_mp).
internal static partial class MpWeaponCommands
{
    internal static ZoneCaptureResult CaptureFile(string input, string? exe = null)
    {
        exe ??= Program.FindExecutable(input);
        using var rsa = Program.LoadKeyFor(input, exe, null);
        var contents = new FastFileReader(rsa).Read(input);
        return ZoneCapture.Capture(contents.Build, contents.Name, contents.Zone);
    }

    internal static string? NameOf(ZoneImage image, uint header, int type)
    {
        uint field = header + (type == 10 ? 0x20u : 0u);
        uint p = image.Pointer(field);
        return p == 0 ? null : image.Memory.CString(p);
    }

    private static string Hex(ZoneImage image, uint at, int n) => Convert.ToHexString(image.Memory.ReadBytes(at, n));

    private static int Inspect(CommandLine cli)
    {
        string input = cli.Required("input");
        var names = cli.Values("name").ToHashSet(StringComparer.OrdinalIgnoreCase);
        string? exe = cli.Value("exe");
        string? dumpSurfs = cli.Value("dump-surfs");
        cli.EnsureAllConsumed();
        var dump = new System.Text.StringBuilder("[");
        var cap = CaptureFile(input, exe);
        var img = cap.Image;
        Console.WriteLine($"{img.Assets.Count} top-level assets, {img.AssetHeaders.Count} headers, {img.ScriptStrings.Count} script strings");
        foreach (string spec in names.Where(n => n.StartsWith("*techset-users:")).ToList())
        {
            string want = spec["*techset-users:".Length..];
            foreach (var (h, t) in img.AssetHeaders.OrderBy(x => x.Key))
            {
                if (t != 5) continue;
                uint ts = img.Pointer(h + 0x50);
                string? tn = ts == 0 ? null : NameOf(img, ts, 6)?.TrimStart(',');
                if (tn is not null && tn.Equals(want, StringComparison.OrdinalIgnoreCase))
                {
                    Console.WriteLine($"material '{NameOf(img, h, 5)}' uses {tn}");
                    PrintMaterial(img, h);
                }
            }
        }
        if (names.Contains("*inline-surfs"))
        {
            int shown = 0;
            foreach (var (h, t) in img.AssetHeaders.OrderBy(x => x.Key))
            {
                if (t != 3 || img.Memory.U32(h + 4) == 0 || shown >= 12)
                {
                    continue;
                }
                uint arr = img.Pointer(h + 4);
                int n = img.Memory.U16(h + 8);
                if (n < 2)
                {
                    continue;
                }
                shown++;
                Console.WriteLine($"inline surfs '{NameOf(img, h, 3)}' n {n}");
                for (uint k = 0; k < n; k++)
                {
                    uint xs = arr + 0x54 * k;
                    Console.WriteLine($"   {k}: verts {img.Memory.U16(xs + 4)} tris {img.Memory.U16(xs + 6)} baseTri {img.Memory.U16(xs + 8)} baseVert {img.Memory.U16(xs + 12)} verts0 0x{img.Pointer(xs + 0x20):X8} tri 0x{img.Pointer(xs + 0x10):X8}");
                    PrintVertList(img, xs);
                }
            }
        }
        foreach (var (header, type) in img.AssetHeaders.OrderBy(h => h.Key))
        {
            string? name = NameOf(img, header, type);
            if (name is null || !names.Contains(name.TrimStart(',')))
            {
                continue;
            }
            uint len = img.Objects.TryGetValue(header, out uint l) ? l : 0;
            bool top = img.Assets.Any(a => a.Header == header);
            Console.WriteLine($"type {type} 0x{header:X8} len {len} '{name}' top-level {top}");
            if (type == 4)
            {
                var m = img.Memory;
                Console.WriteLine($"   bones {m.U8(header + 4)} root {m.U8(header + 5)} surfs {m.U8(header + 6)} lods {m.U8(header + 297)} flags {m.U8(header + 349):X2}");
                uint names2 = img.Pointer(header + 44);
                int nb = m.U8(header + 4);
                var bones = new List<string>();
                for (uint i = 0; names2 != 0 && i < nb; i++)
                {
                    ushort s = m.U16(names2 + 2 * i);
                    bones.Add(s < img.ScriptStrings.Count ? img.ScriptStrings[s] ?? "?" : $"#{s}");
                }
                Console.WriteLine($"   bone names: {string.Join(" ", bones)}");
                for (uint f = 44; f <= 68; f += 4)
                {
                    Console.WriteLine($"   +{f}: 0x{m.U32(header + f):X8} {img.Encoding(header + f)} obj {(img.Objects.TryGetValue(img.Pointer(header + f), out uint ol) ? ol : 0)}");
                }
                int ns = m.U8(header + 6);
                uint mats = img.Pointer(header + 0x44);
                for (uint i = 0; mats != 0 && i < ns; i++)
                {
                    uint mt = img.Pointer(mats + 4 * i);
                    Console.WriteLine($"   surf {i}: material 0x{mt:X8} '{(mt != 0 ? NameOf(img, mt, 5) : null)}' enc {img.Encoding(mats + 4 * i)}");
                }
                for (uint lod = 0; lod < 4; lod++)
                {
                    uint li = header + 72 + 56 * lod;
                    uint s2 = img.Pointer(li + 8);
                    if (s2 == 0)
                    {
                        continue;
                    }
                    Console.WriteLine($"   lod {lod}: dist {BitConverter.UInt32BitsToSingle(m.U32(li))} numsurfs {m.U16(li + 4)} surfIndex {m.U16(li + 6)} " +
                        $"surfs rec 0x{s2:X8} enc {img.Encoding(li + 8)} '{NameOf(img, s2, 3)}' rec.surfs 0x{m.U32(s2 + 4):X8} rec.n {m.U16(s2 + 8)} " +
                        $"lod.surfs 0x{m.U32(li + 48):X8} enc {img.Encoding(li + 48)} +12 0x{m.U32(li + 12):X8} {img.Encoding(li + 12)}");
                    Console.WriteLine($"      lodinfo {Hex(img, li, 56)}");
                    Console.WriteLine($"      surfsrec {Hex(img, s2, 0x30)}");
                    uint sarr = m.U32(s2 + 4);
                    for (uint k = 0; sarr != 0 && k < m.U16(s2 + 8); k++)
                    {
                        uint xs = sarr + 0x54 * k;
                        Console.WriteLine($"      xsurf {k}: verts {m.U16(xs + 4)} tris {m.U16(xs + 6)} baseTri {m.U16(xs + 8)} baseVert {m.U16(xs + 12)} | {Hex(img, xs, 0x54)}");
                        PrintVertList(img, xs);
                        if (dumpSurfs is not null && lod == 0)
                        {
                            uint vp = img.Pointer(xs + 0x20), ip = img.Pointer(xs + 0x10);
                            int nv = m.U16(xs + 4), nt = m.U16(xs + 6);
                            var vv = Enumerable.Range(0, nv).SelectMany(q => new[] { BitConverter.UInt32BitsToSingle(m.U32(vp + 32 * (uint)q)), BitConverter.UInt32BitsToSingle(m.U32(vp + 32 * (uint)q + 4)), BitConverter.UInt32BitsToSingle(m.U32(vp + 32 * (uint)q + 8)) });
                            var ii = Enumerable.Range(0, nt * 3).Select(q => (int)m.U16(ip + 2 * (uint)q));
                            dump.Append(dump.Length > 1 ? "," : "").Append($"{{\"model\":\"{name}\",\"k\":{k},\"baseTri\":{m.U16(xs + 8)},\"baseVert\":{m.U16(xs + 12)},\"vaddr\":{vp},\"iaddr\":{ip},\"verts\":[{string.Join(",", vv.Select(f => f.ToString("R", System.Globalization.CultureInfo.InvariantCulture)))}],\"tris\":[{string.Join(",", ii)}]}}");
                        }
                    }
                }
                Console.WriteLine($"   coll +300 0x{m.U32(header + 300):X8} n {m.U32(header + 304)}; boneInfo +312 0x{m.U32(header + 312):X8} {img.Encoding(header + 312)}; phys {m.U32(header + 352):X8} {m.U32(header + 356):X8}");
                Console.WriteLine($"   hdr {Hex(img, header, 0x48)}");
                Console.WriteLine($"   tail {Hex(img, header + 296, 64)}");
            }
            else if (type == 5)
            {
                PrintMaterial(img, header);
            }
            foreach (uint f in img.Pointers.Enumerate(header, len))
            {
                uint t = img.Memory.U32(f);
                string what = img.AssetHeaders.TryGetValue(t, out int at) ? $"asset type {at} '{NameOf(img, t, at)}'" :
                    img.NameReferences.TryGetValue(t, out var nr) ? $"nameref {nr.Type} {nr.Name}" :
                    img.Objects.TryGetValue(t, out uint ol) ? $"object len {ol}" : "?";
                Console.WriteLine($"   +0x{f - header:X3} -> 0x{t:X8} {img.Encoding(f)} {what}");
            }
        }
        if (dumpSurfs is not null)
        {
            File.WriteAllText(dumpSurfs, dump.Append("]").ToString());
        }
        return 0;
    }

    private static void PrintVertList(ZoneImage img, uint xs)
    {
        var m = img.Memory;
        int n = m.U8(xs + 0x28);
        uint vl = img.Pointer(xs + 0x2C);
        var parts = new List<string>();
        for (uint i = 0; vl != 0 && i < n && i < 40; i++)
        {
            uint e = vl + 12 * i;
            parts.Add($"[bone {m.U16(e)} v {m.U16(e + 2)} triOff {m.U16(e + 4)} tris {m.U16(e + 6)}]");
        }
        Console.WriteLine($"         vertlist {n}: {string.Join(" ", parts)}");
    }

    internal static void PrintMaterial(ZoneImage img, uint m)
    {
        Console.WriteLine($"      hdr {Hex(img, m, 0x60)}");
        uint ts = img.Pointer(m + 0x50);
        Console.WriteLine($"      techset '{(ts != 0 ? NameOf(img, ts, 6) : null)}'");
        uint t = img.Pointer(m + 0x54);
        int count = img.Memory.U8(m + 0x48);
        for (uint i = 0; t != 0 && i < count; i++)
        {
            uint e = t + 12 * i;
            uint ip = img.Pointer(e + 8);
            Console.WriteLine($"      tex {i}: hash {img.Memory.U32(e):X8} sampler {img.Memory.U8(e + 6):X2} semantic {img.Memory.U8(e + 7)} -> " +
                (ip != 0 && img.AssetHeaders.TryGetValue(ip, out int it) ? $"'{NameOf(img, ip, it)}' {Hex(img, ip + 4, 0x1C)}" : $"0x{ip:X8}"));
        }
    }
}
