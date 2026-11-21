using System.Globalization;
using VlobbyMerge;

// vlobby_merge probe <zone.ff> <exe>
// vlobby_merge merge <stock ui_viewer_mp.ff> <exe> <out patch_ui_viewer_mp.ff> <donor.ff>=<dx>,<dy>,<dz>,<prefix>,<tag> ... [table=<camera table csv>]
if (args.Length >= 2 && args[0] == "v0img") return V0FileImages.Cli(args);
if (args.Length >= 3 && args[0] == "probe")
{
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    Probe.World(z);
    Probe2.Details(z);
    return 0;
}
if (args.Length >= 7 && args[0] == "dump")
{
    // dump <zone.ff> <exe> <type> <name> <name offset> <size>
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    Dump.Asset(z, int.Parse(args[3]), args[4], int.Parse(args[5]), int.Parse(args[6]));
    return 0;
}
if (args.Length >= 8 && args[0] == "deref")
{
    // deref <zone.ff> <exe> <type> <name> <name offset> <pointer offset> <size>: the object a header field points to
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    int type = int.Parse(args[3]); uint no = uint.Parse(args[5]), po = uint.Parse(args[6]); int size = int.Parse(args[7]);
    foreach (var (h, t) in z.Image.AssetHeaders)
    {
        if (t != type || !string.Equals(z.M.CString(z.Image.Pointer(h + no), 256), args[4], StringComparison.OrdinalIgnoreCase)) continue;
        uint p = z.Image.Pointer(h + po);
        Console.WriteLine($"0x{p:X8} encoding {z.Image.Encoding(h + po)} raw 0x{z.M.U32(h + po):X8} (object {(z.Image.TryFindObject(p, out uint st, out uint len) ? $"0x{st:X8}+{len}" : "?")})");
        var b = z.M.ReadBytes(p, size);
        for (int o = 0; o < size; o += 4) Console.Write($"{o:X3}:{BitConverter.ToUInt32(b, o):X8} " + (o % 32 == 28 ? Environment.NewLine : ""));
        Console.WriteLine();
        return 0;
    }
    Console.WriteLine("not found");
    return 0;
}
if (args.Length >= 4 && args[0] == "matimages")
{
    // matimages <zone.ff> <exe> <material name filter>: the images each matching material's texture table names
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    var I = z.Image; var m = z.M;
    foreach (var (h, t) in I.AssetHeaders)
    {
        if (t != AssetTypes.Material) continue;
        string name = m.CString(I.Pointer(h), 128) ?? "";
        if (!name.Contains(args[3], StringComparison.OrdinalIgnoreCase)) continue;
        // MaterialInfo 0x18 bytes, then the texture count at +0x4C? find the texture table: the first pointer whose
        // entries are 12 bytes {nameHash, nameStart, nameEnd, samplerState, semantic, image*}
        var imgs = new List<string>();
        for (uint o = 0x18; o < 0x60; o += 4)
        {
            uint tt = I.Pointer(h + o);
            if (tt == 0) continue;
            for (uint k = 0; k < 16; k++)
            {
                uint ip = I.Pointer(tt + 12 * k + 8);
                if (ip != 0 && I.AssetHeaders.TryGetValue(ip, out int it) && it == AssetTypes.Image)
                {
                    uint np = I.Pointer(ip + 0x20); if (np == 0) np = I.Pointer(ip + 0x1C);
                    imgs.Add(m.CString(np, 128) ?? "?");
                }
                else if (k > 0) break;
            }
            if (imgs.Count > 0) break;
        }
        Console.WriteLine($"{name}: {string.Join(", ", imgs)}");
    }
    return 0;
}
if (args.Length >= 3 && args[0] == "vstats")
{
    // vstats <zone.ff> <exe>: the world vertices' colour, texture and lightmap coordinates (GfxWorldVertex: xyz, binormal
    // sign, colour +16, texCoord +20, lmapCoord +28, normal +36, tangent +40), and per surface lightmap index
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    var I = z.Image; var m = z.M;
    uint w = z.Find(AssetTypes.GfxWorld), draw = w + 0x50;
    int nv = (int)m.U32(draw + 0x28);
    uint v = I.Pointer(draw + 0x2C);
    float[] mn = Enumerable.Repeat(float.MaxValue, 4).ToArray(), mx = Enumerable.Repeat(float.MinValue, 4).ToArray();
    var cols = new Dictionary<uint, int>();
    int bad = 0;
    for (int i = 0; i < nv; i++)
    {
        uint b = v + (uint)(44 * i);
        float[] f = { m.F32(b + 20), m.F32(b + 24), m.F32(b + 28), m.F32(b + 32) };
        for (int k = 0; k < 4; k++) { if (float.IsNaN(f[k])) { bad++; continue; } mn[k] = Math.Min(mn[k], f[k]); mx[k] = Math.Max(mx[k], f[k]); }
        uint c = m.U32(b + 16); cols[c] = cols.GetValueOrDefault(c) + 1;
    }
    Console.WriteLine($"{nv} vertices; tex u {mn[0]:0.###}..{mx[0]:0.###} v {mn[1]:0.###}..{mx[1]:0.###}; lmap u {mn[2]:0.###}..{mx[2]:0.###} v {mn[3]:0.###}..{mx[3]:0.###}; NaN {bad}");
    Console.WriteLine("colours: " + string.Join(", ", cols.OrderByDescending(kv => kv.Value).Take(6).Select(kv => $"{kv.Key:X8} x{kv.Value}")));
    uint dp = w + 0x1C8; int n = (int)m.U32(dp + 4); uint surfs = I.Pointer(dp + 84);
    var lms = new Dictionary<int, int>();
    for (int i = 0; i < n; i++) { int l = m.U8(surfs + (uint)(24 * i) + 20); lms[l] = lms.GetValueOrDefault(l) + 1; }
    Console.WriteLine("surface lightmaps: " + string.Join(", ", lms.OrderBy(kv => kv.Key).Select(kv => $"{kv.Key} x{kv.Value}")) + $"; lightmaps {m.U32(draw + 0x10)}");
    return 0;
}
if (args.Length >= 7 && args[0] == "near")
{
    // near <zone.ff> <exe> <x> <y> <z> <radius>: world surfaces whose bounds come within radius of the point
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    var I = z.Image; var m = z.M;
    float px = float.Parse(args[3], CultureInfo.InvariantCulture), py = float.Parse(args[4], CultureInfo.InvariantCulture), pz = float.Parse(args[5], CultureInfo.InvariantCulture), r = float.Parse(args[6], CultureInfo.InvariantCulture);
    uint w = z.Find(AssetTypes.GfxWorld), dp = w + 0x1C8;
    int n = (int)m.U32(dp + 4);
    uint surfs = I.Pointer(dp + 84), bnds = I.Pointer(dp + 88);
    for (int i = 0; i < n; i++)
    {
        uint b = bnds + (uint)(24 * i);
        float[] c = { m.F32(b), m.F32(b + 4), m.F32(b + 8) }, e = { m.F32(b + 12), m.F32(b + 16), m.F32(b + 20) };
        float dx = Math.Max(0, Math.Abs(c[0] - px) - e[0]), dy = Math.Max(0, Math.Abs(c[1] - py) - e[1]), dz = Math.Max(0, Math.Abs(c[2] - pz) - e[2]);
        if (dx * dx + dy * dy + dz * dz > r * r) continue;
        uint s = surfs + (uint)(24 * i), mat = I.Pointer(s + 16);
        Console.WriteLine($"{i,5} {m.CString(I.Pointer(mat), 128),-48} tris {m.U16(s + 10),5} lm {m.U8(s + 20)} centre {c[0]:0},{c[1]:0},{c[2]:0} half {e[0]:0},{e[1]:0},{e[2]:0}");
    }
    return 0;
}
if (args.Length >= 4 && args[0] == "imgflags")
{
    // imgflags <1.1.1.13 zone> <0.0.2.2 zone> <exe>: the load definition words of the images both have, side by side
    var a1 = LoadedZone.Read(args[1], args[3], anyBuild: true);
    var a0 = LoadedZone.Read(args[2], args[3], anyBuild: true);
    var old = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase);
    foreach (var (h, t) in a0.Image.AssetHeaders)
        if (t == AssetTypes.Image && a0.M.CString(a0.Image.Pointer(h + 28), 128) is { } n) old[n] = h;
    var hist = new Dictionary<string, int>();
    foreach (var (h, t) in a1.Image.AssetHeaders)
    {
        if (t != AssetTypes.Image || a1.M.CString(a1.Image.Pointer(h + 32), 128) is not { } n || !old.TryGetValue(n, out uint h0)) continue;
        uint d1 = a1.Image.Pointer(h), d0 = a0.Image.Pointer(h0);
        if (d1 == 0 || d0 == 0) continue;
        string k = $"0022 {a0.M.U32(d0):X8} {a0.M.U32(d0 + 4):X8} -> 1113 {a1.M.U32(d1):X8} {a1.M.U32(d1 + 4):X8} fmt {a1.M.U32(d1 + 8):X8} size {a1.M.U32(d1 + 12)} cat {a1.M.U8(h + 6)} sem {a1.M.U8(h + 5)} {a1.M.U16(h + 0x14)}x{a1.M.U16(h + 0x16)}";
        hist[k] = hist.GetValueOrDefault(k) + 1;
        if (hist[k] == 1) Console.WriteLine($"{n}: {k}");
    }
    return 0;
}
if (args.Length >= 4 && args[0] == "imgcat")
{
    // imgcat <zone.ff> <exe> <name offset>: images by map type / semantic / category (header bytes 4..6), with examples
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    uint no = uint.Parse(args[3]);
    foreach (var g in z.Image.AssetHeaders.Where(kv => kv.Value == AssetTypes.Image)
                 .Select(kv => (Key: $"type {z.M.U8(kv.Key + 4)} semantic {z.M.U8(kv.Key + 5)} category {z.M.U8(kv.Key + 6)}", Name: z.M.CString(z.Image.Pointer(kv.Key + no), 128)))
                 .GroupBy(x => x.Key).OrderBy(g => g.Key))
        Console.WriteLine($"{g.Key}: {g.Count()} ({string.Join(", ", g.Take(3).Select(x => x.Name))})");
    return 0;
}
if (args.Length >= 3 && args[0] == "bsp")
{
    // bsp <zone.ff> <exe>: the world's dpvs BSP (planes, nodes) and the cell of every stand and camera of the MapEnts
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    var I = z.Image; var m = z.M;
    uint w = z.Find(AssetTypes.GfxWorld);
    int planeCount = (int)m.U32(w + 8), nodeCount = (int)m.U32(w + 0xC), cellCount = (int)m.U32(w + 0x34);
    uint planes = I.Pointer(w + 0x38), nodes = I.Pointer(w + 0x3C);
    Console.WriteLine($"planes {planeCount}, nodes {nodeCount}, cells {cellCount}");
    Console.WriteLine("nodes: " + string.Join(" ", Enumerable.Range(0, Math.Min(nodeCount, 60)).Select(i => m.U16(nodes + (uint)(2 * i)).ToString())));
    for (int i = 0; i < Math.Min(planeCount, 12); i++)
    {
        uint pl = planes + (uint)(20 * i);
        Console.WriteLine($"plane {i}: n=({m.F32(pl):0.##},{m.F32(pl + 4):0.##},{m.F32(pl + 8):0.##}) d={m.F32(pl + 12):0.##} type {m.U8(pl + 16)}");
    }
    int Walk(float[] o, bool frontIsNext)
    {
        uint node = nodes;
        for (int guard = 0; guard < 10000; guard++)
        {
            int v = m.U16(node);
            if (v < cellCount + 1) return v - 1;
            uint pl = planes + (uint)(20 * (v - cellCount - 1));
            float d = o[0] * m.F32(pl) + o[1] * m.F32(pl + 4) + o[2] * m.F32(pl + 8) - m.F32(pl + 12);
            bool front = d > 0;
            node = (front == frontIsNext) ? node + 4 : node + 2u * m.U16(node + 2);
        }
        return -99;
    }
    uint me = I.Pointer(z.Find(AssetTypes.ClipMapMp) + 0x9C);
    string ents = m.CString(I.Pointer(me + 4), 1 << 22);
    foreach (System.Text.RegularExpressions.Match e in System.Text.RegularExpressions.Regex.Matches(ents, "[{][^}]*[}]"))
    {
        var tn = System.Text.RegularExpressions.Regex.Match(e.Value, "\"targetname\" \"([^\"]*)\"");
        var og = System.Text.RegularExpressions.Regex.Match(e.Value, "\"origin\" \"([^\"]*)\"");
        if (!tn.Success || !og.Success) continue;
        string name = tn.Groups[1].Value;
        if (!(name.Contains("pos") || name.Contains("camera") || name.Contains("place1"))) continue;
        var o = og.Groups[1].Value.Split(' ').Select(x => float.Parse(x, CultureInfo.InvariantCulture)).ToArray();
        Console.WriteLine($"{name,-40} {og.Groups[1].Value,-28} cell A {Walk(o, true)} B {Walk(o, false)}");
    }
    return 0;
}
if (args.Length >= 3 && args[0] == "check")
{
    // every surface's material and every static model's model must be an asset of that type
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    var I = z.Image; var m = z.M;
    uint w = z.Find(AssetTypes.GfxWorld), dp = w + 0x1C8;
    int n = (int)m.U32(dp + 4), sm = (int)m.U32(dp);
    uint surfs = I.Pointer(dp + 84), di = I.Pointer(dp + 92);
    int bad = 0;
    for (int i = 0; i < n; i++)
    {
        uint f = surfs + (uint)(24 * i) + 16, v = m.U32(f);
        if (!I.AssetHeaders.TryGetValue(v, out int t) || t != AssetTypes.Material) { if (bad++ < 20) Console.WriteLine($"surface {i}: material 0x{v:X8} tagged {I.Pointers.Get(f)}"); }
    }
    for (int i = 0; i < sm; i++)
    {
        uint f = di + (uint)(76 * i) + 52, v = m.U32(f);
        if (!I.AssetHeaders.TryGetValue(v, out int t) || t != AssetTypes.XModel) { if (bad++ < 40) Console.WriteLine($"smodel {i}: model 0x{v:X8} tagged {I.Pointers.Get(f)}"); }
    }
    Console.WriteLine($"{n} surfaces, {sm} static models, {bad} bad");
    return 0;
}
if (args.Length >= 4 && args[0] == "matfield")
{
    // materials whose word at +<offset> is not a pointer (value histogram), with names
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    uint off = Convert.ToUInt32(args[3], 16);
    int n = 0;
    foreach (var (h, t) in z.Image.AssetHeaders)
    {
        if (t != AssetTypes.Material) continue;
        uint v = z.M.U32(h + off);
        if (!z.Image.Pointers.Get(h + off) && v != 0 && n++ < 15) Console.WriteLine($"{z.M.CString(z.Image.Pointer(h), 128)}: +0x{off:X} = 0x{v:X8}");
    }
    Console.WriteLine($"{n} materials with a non-pointer value there");
    return 0;
}
if (args.Length >= 3 && args[0] == "techcheck")
{
    // every technique pass's vertex declaration, vertex shader and pixel shader must be assets of those types
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    var I = z.Image; var m = z.M; int bad = 0, passes = 0;
    foreach (var (h, t) in I.AssetHeaders)
    {
        if (t != 9) continue;
        for (uint k = 0; k < 48; k++)
        {
            uint tech = I.Pointer(h + 0xC + 4 * k);
            if (tech == 0) continue;
            int pc = m.U16(tech + 6);
            for (int p = 0; p < pc && p < 8; p++)
            {
                uint pass = tech + 8 + (uint)(20 * p);
                passes++;
                for (uint f = 0; f < 12; f += 4)
                {
                    uint v = m.U32(pass + f);
                    int want = f == 0 ? 8 : f == 4 ? 7 : 6;
                    if (v != 0 && (!I.AssetHeaders.TryGetValue(v, out int tt) || tt != want) && bad++ < 15)
                        Console.WriteLine($"{m.CString(I.Pointer(h), 128)} tech {k} pass {p} +{f}: 0x{v:X8} tagged {I.Pointers.Get(pass + f)} type {(I.AssetHeaders.TryGetValue(v, out int t2) ? t2 : -1)}");
                }
            }
        }
    }
    Console.WriteLine($"{passes} passes, {bad} bad");
    return 0;
}
if (args.Length >= 3 && args[0] == "count")
{
    // every asset of the zone (top-level or inline) by type
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    Console.WriteLine(string.Join(" ", z.Image.AssetHeaders.GroupBy(kv => kv.Value).OrderBy(g => g.Key).Select(g => $"{g.Key}:{g.Count()}")));
    if (args.Length > 3)
        foreach (var g in z.Image.AssetHeaders.Where(kv => kv.Value == int.Parse(args[3])).GroupBy(kv => Codol.FastFiles.Porting.ZoneImageDiff.AssetName(z.Image, kv.Key) ?? "?").Where(g => g.Count() > 1).Take(15))
            Console.WriteLine($"  {g.Key} x{g.Count()}");
    return 0;
}
if (args.Length >= 4 && args[0] == "names")
{
    // names of every asset of a type (top-level or inline)
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    foreach (var (h, t) in z.Image.AssetHeaders)
        if (t == int.Parse(args[3])) Console.WriteLine(Codol.FastFiles.Porting.ZoneImageDiff.AssetName(z.Image, h));
    return 0;
}
if (args.Length >= 3 && args[0] == "images")
{
    // every image of the zone: name and the size of the pixels it carries (0: streamed from an IWD)
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    foreach (var (h, t) in z.Image.AssetHeaders)
    {
        if (t != AssetTypes.Image) continue;
        uint ld = z.Image.Pointer(h);
        Console.WriteLine($"{z.M.CString(z.Image.Pointer(h + 32), 256)}	{(ld == 0 ? -1 : (int)z.M.U32(ld + 12))}");
    }
    return 0;
}
if (args.Length >= 3 && args[0] == "scenes")
{
    // static models and world surfaces around each scene's stand (any build whose GfxWorld is 1.1.1.13's layout)
    var z = LoadedZone.Read(args[1], args[2], anyBuild: true);
    Probe2.Scenes(z, args.Length > 3 ? float.Parse(args[3], CultureInfo.InvariantCulture) : 1200);
    return 0;
}
if (args.Length >= 4 && args[0] == "merge")
{
    var stock = LoadedZone.Read(args[1], args[2]);
    var donors = new List<Donor>();
    string? table = null;
    var earlier = new List<LoadedZone>();
    foreach (string spec in args.Skip(4))
    {
        if (spec.StartsWith("table=")) { table = spec[6..]; continue; }
        // frame=<file>: cameras at the stock menu's offset from donor stands (Framing.cs)
        if (spec.StartsWith("frame=")) { Framing.File = spec[6..]; continue; }
        // slots=<prefix>: that donor's room character slots are appended too (Merge.AppendEntities)
        if (spec.StartsWith("slots=")) { Merge.SlotPrefixes.Add(spec[6..]); continue; }
        // iwd=<iw_image_18.iwd>: the later builds' ported images, written again with the 0.0.2.2 files next to <out.ff>
        if (spec.StartsWith("iwd=")) { V0FileImages.IwdIn = spec[4..]; continue; }
        // earlier=<zone.ff>: a zone C1 loads before ui_viewer_mp (repeatable)
        if (spec.StartsWith("earlier=")) { earlier.Add(LoadedZone.Read(spec[8..], args[2])); continue; }
        int eq = spec.LastIndexOf('=');
        string[] p = spec[(eq + 1)..].Split(',');
        float F(string s) => float.Parse(s, CultureInfo.InvariantCulture);
        // optional sixth field: the keep box x0:y0:z0:x1:y1:z1 (donor coordinates); seventh: stockgrid
        float[]? keep = p.Length > 5 ? p[5].Split(':').Select(F).ToArray() : null;
        donors.Add(new Donor(LoadedZone.Read(spec[..eq], args[2], anyBuild: true), F(p[0]), F(p[1]), F(p[2]), p[3], p[4]) { Keep = keep, StockGrid = p.Length > 6 && p[6] == "stockgrid" });
    }
    Merge.Run(stock, donors, args[2], args[3], table, earlier, Console.WriteLine);
    return 0;
}
Console.Error.WriteLine("usage: vlobby_merge probe <zone.ff> <exe> | merge <stock.ff> <exe> <out.ff> <donor.ff>=dx,dy,dz,prefix,tag ...");
return 2;
