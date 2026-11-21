using System.Globalization;
using VlobbyMerge;

// vlobby_merge probe <zone.ff> <exe>
// vlobby_merge merge <stock ui_viewer_mp.ff> <exe> <out patch_ui_viewer_mp.ff> <donor.ff>=<dx>,<dy>,<dz>,<prefix>,<tag> ... [table=<camera table csv>]
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
    foreach (string spec in args.Skip(4))
    {
        if (spec.StartsWith("table=")) { table = spec[6..]; continue; }
        int eq = spec.LastIndexOf('=');
        string[] p = spec[(eq + 1)..].Split(',');
        float F(string s) => float.Parse(s, CultureInfo.InvariantCulture);
        // optional sixth field: the keep box x0:y0:z0:x1:y1:z1 (donor coordinates); seventh: stockgrid
        float[]? keep = p.Length > 5 ? p[5].Split(':').Select(F).ToArray() : null;
        donors.Add(new Donor(LoadedZone.Read(spec[..eq], args[2], anyBuild: true), F(p[0]), F(p[1]), F(p[2]), p[3], p[4]) { Keep = keep, StockGrid = p.Length > 6 && p[6] == "stockgrid" });
    }
    Merge.Run(stock, donors, args[2], args[3], table, Console.WriteLine);
    return 0;
}
Console.Error.WriteLine("usage: vlobby_merge probe <zone.ff> <exe> | merge <stock.ff> <exe> <out.ff> <donor.ff>=dx,dy,dz,prefix,tag ...");
return 2;
