namespace VlobbyMerge;

public static class Probe
{
    public static void World(LoadedZone z)
    {
        var img = z.Image;
        var m = z.M;
        foreach (var g in img.Assets.GroupBy(a => a.Type)) Console.WriteLine($"type {g.Key}: {g.Count()}");
        uint w = z.Find(AssetTypes.GfxWorld);
        Console.WriteLine($"GfxWorld {z.Str(w)} at 0x{w:X8}");
        for (uint o = 0; o < 0x274; o += 4)
        {
            bool ptr = img.Pointers.Get(w + o);
            uint v = m.U32(w + o);
            string extra = "";
            if (ptr && v != 0)
            {
                extra = img.TryFindObject(v, out uint s, out uint len) ? $" obj 0x{s:X8}+{v - s} len {len}" : " (no object)";
                if (img.AssetHeaders.TryGetValue(v, out int t)) extra += $" asset type {t}";
            }
            Console.WriteLine($"  +0x{o:X3} {(ptr ? "P" : " ")} 0x{v:X8} {(int)v,12} {BitConverter.Int32BitsToSingle((int)v),14:G6}{extra}");
        }
        uint grid = w + 0x98;
        Console.WriteLine($"lightgrid rowAxis {m.U32(grid + 20)} colAxis {m.U32(grid + 24)} mins {m.U16(grid + 8)},{m.U16(grid + 10)},{m.U16(grid + 12)} maxs {m.U16(grid + 14)},{m.U16(grid + 16)},{m.U16(grid + 18)} raw {m.U32(grid + 32)} entries {m.U32(grid + 40)} colors {m.U32(grid + 48)}");
        uint dp = w + 0x1C8;
        string[] names = ["smodelCount", "staticSurfaceCount", "staticSurfaceCountNoDecal", "litOpaqueBegin", "litOpaqueEnd", "litTransBegin", "litTransEnd",
            "shadowCasterBegin", "shadowCasterEnd", "emissiveBegin", "emissiveEnd", "smodelVisDataCount", "surfaceVisDataCount"];
        for (int i = 0; i < names.Length; i++) Console.WriteLine($"  dpvs.{names[i]} = {m.U32(dp + (uint)(4 * i))}");
        uint cells = img.Pointer(w + 0x4C);
        int cellCount = (int)m.U32(w + 0x34);
        uint treeCounts = img.Pointer(w + 0x44);
        uint trees = img.Pointer(w + 0x48);
        for (int c = 0; c < cellCount; c++)
        {
            uint cell = cells + (uint)(40 * c);
            uint tree = img.Pointer(trees + (uint)(4 * c));
            Console.WriteLine($"  cell {c}: mins {m.F32(cell)},{m.F32(cell + 4)},{m.F32(cell + 8)} maxs {m.F32(cell + 12)},{m.F32(cell + 16)},{m.F32(cell + 20)} portals {m.U32(cell + 24)} probes {m.U8(cell + 32)} treeNodes {m.U32(treeCounts + (uint)(4 * c))} root surf {m.U16(tree + 26)}+{m.U16(tree + 26 + 0)} ");
            if (tree != 0)
            {
                Console.Write("    root:");
                for (uint o = 0; o < 44; o += 2) Console.Write($" {m.U16(tree + o)}");
                Console.WriteLine();
            }
        }
        uint draw = w + 0x50;
        Console.WriteLine($"  draw probes {m.U32(draw)} lightmaps {m.U32(draw + 16)} vertices {m.U32(draw + 40)} vld {m.U32(draw + 52)} indices {m.U32(draw + 64)}");
        uint com = z.Find(AssetTypes.ComWorld);
        Console.WriteLine($"ComWorld isInUse {m.U32(com + 4)} primaryLightCount {m.U32(com + 8)}");
        uint clip = z.Find(AssetTypes.ClipMapMp);
        Console.WriteLine($"clipMap at 0x{clip:X8}");
        for (uint o = 0; o < 0x100; o += 4)
        {
            uint v = m.U32(clip + o);
            if (img.Pointers.Get(clip + o) && v != 0 && img.AssetHeaders.TryGetValue(v, out int t)) Console.WriteLine($"  clip+0x{o:X} -> asset type {t} {m.CString(img.Pointer(v), 128)}");
        }
    }
}

public static class Probe2
{
    static string ImgName(LoadedZone z, uint img) => img == 0 ? "-" : z.M.CString(z.Image.Pointer(img + 32), 128) ?? "?";
    public static void Details(LoadedZone z)
    {
        var m = z.M; var I = z.Image;
        uint w = z.Find(AssetTypes.GfxWorld);
        uint draw = w + 0x50;
        int probes = (int)m.U32(draw), lms = (int)m.U32(draw + 0x10);
        for (int i = 0; i < probes; i++) Console.WriteLine($"probe {i}: {ImgName(z, I.Pointer(I.Pointer(draw + 4) + (uint)(4 * i)))} at {m.F32(I.Pointer(draw + 8) + (uint)(12 * i))},{m.F32(I.Pointer(draw + 8) + (uint)(12 * i) + 4)},{m.F32(I.Pointer(draw + 8) + (uint)(12 * i) + 8)}");
        for (int i = 0; i < lms; i++) Console.WriteLine($"lightmap {i}: {ImgName(z, I.Pointer(I.Pointer(draw + 0x14) + (uint)(8 * i)))} / {ImgName(z, I.Pointer(I.Pointer(draw + 0x14) + (uint)(8 * i) + 4))}");
        uint models = I.Pointer(w + 0xD4);
        for (int i = 0; i < (int)m.U32(w + 0xD0); i++)
        {
            uint b = models + (uint)(60 * i);
            Console.Write($"brushmodel {i}:");
            for (uint o = 48; o < 60; o += 2) Console.Write($" {m.U16(b + o)}");
            Console.WriteLine();
        }
        uint skies = I.Pointer(w + 0x18);
        for (int i = 0; i < (int)m.U32(w + 0x14); i++)
        {
            uint s = skies + (uint)(16 * i);
            int cnt = (int)m.U32(s); uint arr = I.Pointer(s + 4);
            Console.WriteLine($"sky {i}: {cnt} surfs, first {(cnt > 0 ? m.U32(arr) : 0)} image {ImgName(z, I.Pointer(s + 8))}");
        }
        uint dp = w + 0x1C8;
        uint surfs = I.Pointer(dp + 84), di = I.Pointer(dp + 92);
        int n = (int)m.U32(dp + 4), sm = (int)m.U32(dp);
        var lh = new SortedDictionary<int, int>(); var pli = new SortedDictionary<int, int>(); var lmi = new SortedDictionary<int, int>(); var rpi = new SortedDictionary<int, int>();
        int badIdx = 0;
        uint idx = I.Pointer(draw + 0x44);
        for (int i = 0; i < n; i++)
        {
            uint s = surfs + (uint)(24 * i);
            lmi[m.U8(s + 20)] = lmi.GetValueOrDefault(m.U8(s + 20)) + 1;
            rpi[m.U8(s + 21)] = rpi.GetValueOrDefault(m.U8(s + 21)) + 1;
            pli[m.U8(s + 22)] = pli.GetValueOrDefault(m.U8(s + 22)) + 1;
            int vc = m.U16(s + 8), tc = m.U16(s + 10); uint bi = m.U32(s + 12);
            for (int k = 0; k < 3 * tc; k++) if (m.U16(idx + 2 * (uint)(bi + k)) >= vc) { badIdx++; break; }
        }
        Console.WriteLine($"surface lightmapIndex {string.Join(",", lmi)} probe {string.Join(",", rpi)} light {string.Join(",", pli)}; surfaces with an index >= vertexCount: {badIdx}");
        var dl = new SortedDictionary<int, int>(); var dpl = new SortedDictionary<int, int>(); var cache = 0;
        for (int i = 0; i < sm; i++)
        {
            uint d = di + (uint)(76 * i);
            lh[m.U16(d + 58)] = lh.GetValueOrDefault(m.U16(d + 58)) + 1;
            dl[m.U8(d + 60)] = dl.GetValueOrDefault(m.U8(d + 60)) + 1;
            dpl[m.U8(d + 61)] = dpl.GetValueOrDefault(m.U8(d + 61)) + 1;
            if (m.U32(d + 68) != 0 || m.U32(d + 72) != 0) cache++;
        }
        Console.WriteLine($"smodel lightingHandle distinct {lh.Count} (min {lh.Keys.FirstOrDefault()} max {lh.Keys.LastOrDefault()}) probe {string.Join(",", dl)} light {string.Join(",", dpl)} cacheId!=0 {cache}");
        uint sg = I.Pointer(w + 0x1C0);
        for (int l = 0; l < (int)m.U32(w + 0x20); l++)
        {
            uint g = sg + (uint)(12 * l);
            int sc = m.U16(g), mc = m.U16(g + 2);
            uint ss = I.Pointer(g + 4);
            int mx = 0; for (int k = 0; k < sc; k++) mx = Math.Max(mx, m.U16(ss + (uint)(2 * k)));
            if (sc > 0 || mc > 0) Console.WriteLine($"shadowGeom {l}: {sc} surfs (max {mx}) {mc} smodels");
        }
        uint cells = I.Pointer(w + 0x4C);
        for (int c = 0; c < (int)m.U32(w + 0x34); c++)
        {
            uint cell = cells + (uint)(40 * c); int pc = m.U8(cell + 32); uint pl = I.Pointer(cell + 36);
            Console.WriteLine($"cell {c} probes: {string.Join(",", Enumerable.Range(0, pc).Select(k => m.U8(pl + (uint)k)))}");
        }
        uint me = I.Pointer(z.Find(AssetTypes.ClipMapMp) + 0x9C);
        Console.WriteLine($"mapents: name {z.Str(me)} chars {m.U32(me + 8)}; words: {string.Join(" ", Enumerable.Range(0, 8).Select(k => $"{m.U32(me + (uint)(4 * k)):X}"))}");
    }

    public static void Scenes(LoadedZone z, float radius)
    {
        var m = z.M; var I = z.Image;
        uint w = z.Find(AssetTypes.GfxWorld), dp = w + 0x1C8;
        uint me = I.Pointer(z.Find(AssetTypes.ClipMapMp) + 0x9C);
        string ents = m.CString(I.Pointer(me + 4), 1 << 24)!;
        var stands = new List<(string, float[])>();
        foreach (System.Text.RegularExpressions.Match e in System.Text.RegularExpressions.Regex.Matches(ents, @"\{[^}]*\}"))
        {
            var t = System.Text.RegularExpressions.Regex.Match(e.Value, "\"targetname\" \"(player_pos_[a-z_]+)\"");
            var o = System.Text.RegularExpressions.Regex.Match(e.Value, "\"origin\" \"([^\"]+)\"");
            if (t.Success && o.Success) stands.Add((t.Groups[1].Value, o.Groups[1].Value.Split(' ').Select(x => float.Parse(x, System.Globalization.CultureInfo.InvariantCulture)).ToArray()));
        }
        uint di = I.Pointer(dp + 92), sb = I.Pointer(dp + 88);
        int sm = (int)m.U32(dp), n = (int)m.U32(dp + 4);
        foreach (var (name, p) in stands.OrderBy(s => s.Item1))
        {
            var models = new SortedDictionary<string, int>();
            for (int i = 0; i < sm; i++)
            {
                uint d = di + (uint)(76 * i);
                float dx = m.F32(d) - p[0], dy = m.F32(d + 4) - p[1], dz = m.F32(d + 8) - p[2];
                if (dx * dx + dy * dy + dz * dz > radius * radius) continue;
                string mn = m.CString(I.Pointer(I.Pointer(d + 52)), 128) ?? "?";
                models[mn] = models.GetValueOrDefault(mn) + 1;
            }
            int surfs = 0; double area = 0;
            for (int i = 0; i < n; i++)
            {
                uint b = sb + (uint)(24 * i);
                float dx = m.F32(b) - p[0], dy = m.F32(b + 4) - p[1], dz = m.F32(b + 8) - p[2];
                if (dx * dx + dy * dy + dz * dz <= radius * radius) { surfs++; area += m.F32(b + 12) * m.F32(b + 16); }
            }
            Console.WriteLine($"{name} at {p[0]},{p[1]},{p[2]}: {surfs} surfaces (sum of half-extent areas {area:F0}), {models.Values.Sum()} static models, {models.Count} distinct");
            Console.WriteLine("   " + string.Join(" ", models.Select(kv => $"{kv.Key}x{kv.Value}")));
        }
    }
}
