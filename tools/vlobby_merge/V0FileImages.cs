using System.Buffers.Binary;
using System.Text;
using Codol.Assets.Game;
using Codol.FastFiles;
using Codol.FastFiles.Emulation;
using Codol.FastFiles.Iwd;
using Codol.FastFiles.Porting;

namespace VlobbyMerge;

/// The 0.0.2.2 images whose pixels come from an iwd (GfxImageLoadDef.resourceSize 0) and that no zone loaded with the
/// lobby defines. Merge.V0Images has given them the 1.1.1.13 header layout; this makes them load and draw as 1.1.1.13's
/// own file images do.
///
/// Header (GfxImage, 1.1.1.13 0x24 bytes): every 0.0.2.2 file image the stock zones also hold under its name and with
/// the same pixels (12025 + 3262 pairs over the 200 stock zones) is byte for byte the same in +0x04..+0x1B (map type,
/// semantic, category 3, picmip, card memory, width, height, depth, +0x1B), +0x1C is 0, and +0x1A is the zone's: 1 in
/// the map zones, 0 in ui_viewer_mp, common_* and ui_mp (0 in 0.0.2.2's ui_viewer_mp too). So the header stays as it is.
/// The load definition is what differs: 0.0.2.2 writes it as an insert (-2) with flags 0, 1.1.1.13 inline (-1) with the
/// flags of the .iwi file (the IWI header's flags word: 0x340 colour and specular, 0x40000 / 0x40008 normal maps,
/// 0x360 / 0x370 / 0x140 ...; equal for all 6144 file images of four stock zones, v0img flags). Word 0, the D3D format
/// and resourceSize (0) are the same.
///
/// Pixels (.iwi): 0.0.2.2 and 1.1.1.13 both read IWI version 9, formats 1..13, smallest mip first, fileSizeForPicmip[0]
/// the file size, and the two executables' wavelet tables match; the files are copied as they are (checked; a wavelet
/// file goes through MapImagePorter, which would store it uncompressed if the tables differed). Where 1.1.1.13's iwds
/// hold images/&lt;name&gt;.iwi with the same bytes (164 of the 181) the image streams that file and nothing ships; where
/// they hold other pixels under the name, the 0.0.2.2 image becomes v0_&lt;name&gt; and its file ships as
/// images/v0_&lt;name&gt;.iwi (materials lead to the image header, not to its name; none so far); files 1.1.1.13 does not
/// have at all ship under their own name. They go into main_shared\iw_image_18.iwd with the entries the later builds'
/// ports put there, written after the zone (WriteIwd) so that only images the written zone holds ship.
public static class V0FileImages
{
    const uint G_Name = 0x20, G_Width = 0x14, G_Height = 0x16;
    const uint L_Flags = 4, L_Format = 8, L_Size = 12;

    /// set by Program from iwd=&lt;iw_image_18.iwd&gt;: the later builds' ported images, kept in the written iwd
    public static string? IwdIn;

    /// the D3D format 1.1.1.13 creates an IWI format with (Image_LoadFromData 0x00AB6D40)
    static uint D3dFormat(byte iwiFormat) => iwiFormat switch
    {
        1 => 0x15,          // A8R8G8B8
        2 => 0x16,          // X8R8G8B8 (RGB24 expanded)
        3 => 0x33,          // A8L8
        4 => 0x32,          // L8
        5 => 0x1C,          // A8
        6 => 0x15, 7 => 0x16, 8 => 0x33, 9 => 0x32, 10 => 0x1C,   // wavelet RGBA, RGB, LA, L, A: decoded to those
        11 => 0x31545844,   // DXT1
        12 => 0x33545844,   // DXT3
        13 => 0x35545844,   // DXT5
        _ => 0,
    };

    sealed record Iwi(byte Version, uint Flags, byte Format, int Width, int Height, int Depth, int[] Picmip);

    static Iwi? ParseIwi(byte[] d)
    {
        if (d.Length < 32 || d[0] != 'I' || d[1] != 'W' || d[2] != 'i') return null;
        var pm = new int[4];
        for (int i = 0; i < 4; i++) pm[i] = BinaryPrimitives.ReadInt32LittleEndian(d.AsSpan(16 + 4 * i));
        return new Iwi(d[3], BinaryPrimitives.ReadUInt32LittleEndian(d.AsSpan(4)), d[8], BinaryPrimitives.ReadInt16LittleEndian(d.AsSpan(10)),
            BinaryPrimitives.ReadInt16LittleEndian(d.AsSpan(12)), BinaryPrimitives.ReadInt16LittleEndian(d.AsSpan(14)), pm);
    }

    static bool TryRead(GameInstallation g, string path, out byte[] data, out string origin)
    {
        if (g.ImageArchives?.TryRead(path, out data, out origin) == true) return true;
        if (g.StrippedImageArchives?.TryRead(path, out data, out origin) == true) return true;
        data = []; origin = "";
        return false;
    }

    /// what 1.1.1.13 reads for images/&lt;name&gt;.iwi in an install with this zone: its own iwds (and, through
    /// VLOBBY_EXTRA_IWDS, folders of iwds installed next to them, such as a map pack's main), then the ported entries
    sealed class Target : IDisposable
    {
        public required GameInstallation Stock;
        public List<IwdArchiveSet> Extra = [];
        public Dictionary<string, byte[]> Ported = new(StringComparer.OrdinalIgnoreCase);

        public bool TryRead(string path, out byte[] data, out string origin)
        {
            if (V0FileImages.TryRead(Stock, path, out data, out origin)) return true;
            foreach (var s in Extra) if (s.TryRead(path, out data, out origin)) return true;
            if (Ported.TryGetValue(IwdArchiveSet.Normalize(path), out var p)) { data = p; origin = "iw_image_18.iwd"; return true; }
            data = []; origin = "";
            return false;
        }

        public void Dispose() { foreach (var s in Extra) s.Dispose(); Stock.Dispose(); }
    }

    /// Gives every 0.0.2.2 file image of the merged zone a 1.1.1.13 load definition, renames the ones whose name 1.1.1.13
    /// uses for another image, and writes iw_image_18.iwd (next to outFf) with the files 1.1.1.13 lacks.
    public static void Run(PortContext ctx, ISet<uint> stockAssets, string v0Ff, string exe, string outFf, Action<string> log)
    {
        var img = ctx.Target; var T = ctx.T;
        using var v0 = GameInstallation.Open(v0Ff);
        using var target = new Target { Stock = GameInstallation.Open(exe) };
        byte[] key = target.Stock.IwdKey ?? throw new InvalidDataException("1.1.1.13 has no iwd key");
        foreach (string dir in (Environment.GetEnvironmentVariable("VLOBBY_EXTRA_IWDS") ?? "").Split(';', StringSplitOptions.RemoveEmptyEntries))
            target.Extra.Add(IwdArchiveSet.Open(dir, key, p => p.StartsWith("images/", StringComparison.Ordinal)));
        var keep = new List<IwdWriteEntry>();
        if (IwdIn is not null)
        {
            using var a = IwdArchive.Open(IwdIn, key);
            foreach (var e in a.Entries.Where(e => !e.Name.EndsWith('/')))
            {
                byte[] d = a.Read(e);
                keep.Add(new IwdWriteEntry(e.Name, d));
                target.Ported[IwdArchiveSet.Normalize(e.Name)] = d;
            }
        }

        var ship = new List<(string Image, IwdWriteEntry Entry)>();
        var counts = new SortedDictionary<string, int>();
        var renamed = new List<string>();
        var problems = new List<string>();
        var loadDefs = new HashSet<uint>();
        foreach (var (h, t) in img.AssetHeaders.OrderBy(kv => kv.Key).ToList())
        {
            if (t != AssetTypes.Image || stockAssets.Contains(h) || img.Encoding(h) != PointerEncoding.Insert) continue;
            uint ld = img.Pointer(h);
            if (ld == 0 || T.U32(ld + L_Size) != 0) continue;
            string name = T.CString(img.Pointer(h + G_Name), 256) ?? "";
            if (name.Length == 0 || name.StartsWith(',')) continue;
            if (!loadDefs.Add(ld)) throw new InvalidDataException($"0.0.2.2 image {name}: its load definition is another image's too");
            string path = "images/" + name + ".iwi";
            if (!TryRead(v0, path, out byte[] data, out string from))
            {
                problems.Add($"{name}: 0.0.2.2 has no {path}");
                continue;
            }
            var iwi = ParseIwi(data);
            string? bad = iwi is null ? "not an IWI file"
                : iwi.Version != 9 ? $"IWI version {iwi.Version}"
                : D3dFormat(iwi.Format) == 0 ? $"IWI format {iwi.Format}"
                : iwi.Picmip[0] != data.Length ? $"fileSizeForPicmip[0] {iwi.Picmip[0]}, file {data.Length} bytes"
                : iwi.Width != T.U16(h + G_Width) || iwi.Height != T.U16(h + G_Height) ? $"IWI {iwi.Width}x{iwi.Height}, image {T.U16(h + G_Width)}x{T.U16(h + G_Height)}"
                : D3dFormat(iwi.Format) != T.U32(ld + L_Format) ? $"IWI format {iwi.Format}, load definition 0x{T.U32(ld + L_Format):X}"
                : null;
            if (bad is not null)
            {
                problems.Add($"{name} ({from}): {bad}");
                continue;
            }
            // a wavelet file (formats 6..10) decodes with tables from the executable: the port of the later builds'
            // images (MapImagePorter) copies it when both builds' tables match and stores it uncompressed otherwise
            byte[] ported = data;
            if (iwi.Format is >= 6 and <= 10)
            {
                var r = new Codol.Assets.Porting.MapImageReference(name, false, h, iwi.Width, iwi.Height, iwi.Depth, T.U8(h + 4),
                    T.U32(ld + L_Format), 0, 0, 0);
                var warnings = new List<string>();
                var got = Codol.Assets.Porting.MapImagePorter.Collect([r], v0, target.Stock, warnings).Single();
                if (got.Entry.Status == Codol.Assets.Porting.MapImageStatus.Ported) ported = got.Data;
                else if (got.Entry.Status != Codol.Assets.Porting.MapImageStatus.InTarget)
                {
                    problems.Add($"{name} ({from}): wavelet file not ported: {got.Entry.Status} {got.Entry.Problem}");
                    continue;
                }
                if (got.Entry.Conversion is { } conv) log($"0.0.2.2: {name}: {conv}");
                foreach (string w in warnings) log($"0.0.2.2: {name}: {w}");
            }
            string how;
            if (target.TryRead(path, out byte[] have, out string at))
            {
                if (have.AsSpan().SequenceEqual(data)) how = "1.1.1.13 has the same file";
                else
                {
                    // another image of this name: this one becomes v0_<name>
                    string to = "v0_" + name;
                    string toPath = "images/" + to + ".iwi";
                    if (target.TryRead(toPath, out _, out string clash)) throw new InvalidDataException($"{toPath} is in {clash} already");
                    byte[] text = Encoding.Latin1.GetBytes(to + "\0");
                    uint str = ctx.NewObject((uint)text.Length, 1);
                    T.Write(str, text);
                    img.SetPointer(h + G_Name, str, img.Encoding(h + G_Name));
                    var hi = ParseIwi(have);
                    renamed.Add($"{name} ({iwi.Width}x{iwi.Height} fmt {iwi.Format}; {at}: {(hi is null ? "?" : $"{hi.Width}x{hi.Height} fmt {hi.Format}")})");
                    // (MapImagePorter leaves a wavelet file 1.1.1.13 has under the name unconverted)
                    if (iwi.Format is >= 6 and <= 10) throw new NotSupportedException($"{name}: a wavelet image to rename");
                    ship.Add((to, new IwdWriteEntry(toPath, ported)));
                    target.Ported[IwdArchiveSet.Normalize(toPath)] = ported;
                    how = "renamed v0_*, file shipped";
                }
            }
            else
            {
                ship.Add((name, new IwdWriteEntry(path, ported)));
                target.Ported[IwdArchiveSet.Normalize(path)] = ported;
                how = "file shipped";
            }
            // 1.1.1.13's load definition: flags = the file's, inline
            T.WU32(ld + L_Flags, iwi.Flags);
            img.SetPointer(h, ld, PointerEncoding.Inline);
            counts[how] = counts.GetValueOrDefault(how) + 1;
            if (Environment.GetEnvironmentVariable("VLOBBY_LIST_V0_FILES") == "1")
                log($"  v0 file image {name}: {how}; semantic {T.U8(h + 5)} {iwi.Width}x{iwi.Height} IWI format {iwi.Format} flags 0x{iwi.Flags:X} ({from})");
        }
        log($"0.0.2.2: {counts.Values.Sum()} file images in the 1.1.1.13 form: {string.Join(", ", counts.Select(kv => $"{kv.Value} {kv.Key}"))}");
        if (renamed.Count > 0) log($"0.0.2.2: {renamed.Count} renamed v0_* (1.1.1.13 has another image of the name): {string.Join(", ", renamed)}");
        if (problems.Count > 0)
            throw new InvalidDataException($"0.0.2.2: {problems.Count} file images cannot be ported: {string.Join("; ", problems)}");

        _pending = (Path.Combine(Path.GetDirectoryName(Path.GetFullPath(outFf))!, "iw_image_18.iwd"), keep, ship, key);
    }

    static (string Path, List<IwdWriteEntry> Keep, List<(string Image, IwdWriteEntry Entry)> Ship, byte[] Key)? _pending;

    /// After the zone is written: iw_image_18.iwd next to it, with the entries of iwd= and the 0.0.2.2 files of the images
    /// the written zone holds (the ones only static models the merge leaves out lead to are not shipped)
    public static void WriteIwd(byte[] zone, string zoneName, Action<string> log)
    {
        if (_pending is not { } p) return;
        _pending = null;
        var image = ZoneCapture.Capture(GameBuild.V1_1_1_13, zoneName, zone).Image;
        var held = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var (h, t) in image.AssetHeaders)
            if (t == AssetTypes.Image && image.Pointer(h + G_Name) is uint np and not 0 && image.Memory.CString(np, 256) is { } n) held.Add(n);
        var ship = p.Ship.Where(s => held.Contains(s.Image)).Select(s => s.Entry).ToList();
        IwdWriter.Write(p.Path, p.Keep.Concat(ship), p.Key);
        using (var check = IwdArchive.Open(p.Path, p.Key))
            foreach (var e in check.Entries.Where(e => !e.Name.EndsWith('/'))) check.Read(e);
        log($"wrote {p.Path}: {p.Keep.Count} entries of {IwdIn ?? "(no iwd=)"} and {ship.Count} 0.0.2.2 images " +
            $"({p.Ship.Count - ship.Count} more only left-out static models use: not shipped), {new FileInfo(p.Path).Length} bytes (read back, CRCs match)");
    }

    static string Hex(VirtualMemory m, uint at, int size)
    {
        var sb = new StringBuilder();
        for (int o = 0; o < size; o += 4) sb.Append($"{m.U32(at + (uint)o):X8} ");
        return sb.ToString().TrimEnd();
    }

    /// diagnostics:
    ///   v0img scan &lt;exe&gt; &lt;0.0.2.2 zone&gt; &lt;stock zone&gt; ...: the 0.0.2.2 file images with their header and load
    ///     definition words, the stock images of the same name, the per-byte differences of the pairs, and the stock
    ///     file image header kinds
    ///   v0img flags &lt;exe&gt; &lt;stock zone&gt; ...: stock file images whose load definition flags are not their .iwi's
    public static int Cli(string[] args)
    {
        if (args.Length >= 5 && args[1] == "users")
        {
            // users <exe> <zone> <names file>: the materials that lead to the images named in the file
            var z = LoadedZone.Read(args[3], args[2], anyBuild: true);
            var want = File.ReadAllLines(args[4]).Select(l => l.Trim()).Where(l => l.Length > 0).ToHashSet(StringComparer.OrdinalIgnoreCase);
            var heads = new Dictionary<uint, string>();
            foreach (var (h, t) in z.Image.AssetHeaders)
                if (t == AssetTypes.Image && z.Str(h + G_Name) is { } n && (want.Contains(n) || want.Contains(n.StartsWith("v0_") ? n[3..] : n))) heads[h] = n;
            var mats = new Dictionary<uint, string>();
            foreach (var (h, t) in z.Image.AssetHeaders) if (t == AssetTypes.Material) mats[h] = z.Str(h) ?? "?";
            var byMat = new SortedDictionary<string, List<string>>();
            foreach (uint f in z.Image.Pointers.Enumerate(0, uint.MaxValue))
            {
                if (!heads.TryGetValue(z.M.U32(f), out string? img)) continue;
                // the texture table holding the field: the material whose +0x54 points at or before it
                string owner = "?";
                if (z.Image.TryFindObject(f, out uint st, out _))
                    foreach (var (mh, mn) in mats) if (z.Image.Pointer(mh + 0x54) == st) { owner = mn; break; }
                (byMat.TryGetValue(owner, out var l) ? l : byMat[owner] = []).Add(img);
            }
            foreach (var (mn, l) in byMat) Console.WriteLine($"{mn}: {string.Join(" ", l)}");
            Console.WriteLine($"{heads.Count} of {want.Count} images in the zone, {byMat.Count} materials");
            return 0;
        }
        if (args.Length >= 5 && args[1] == "surfaces")
        {
            // surfaces <exe> <0.0.2.2 zone> <x0:y0:z0:x1:y1:z1>: the world surfaces near the stands that leave the keep box
            var z = LoadedZone.Read(args[3], args[2], anyBuild: true);
            var kb = args[4].Split(':').Select(x => float.Parse(x, System.Globalization.CultureInfo.InvariantCulture)).ToArray();
            var m = z.M; var I = z.Image;
            uint w = z.Find(AssetTypes.GfxWorld), dp = w + 0x1C8;
            int n = (int)m.U32(dp + 4);
            uint surfs = I.Pointer(dp + 84), bnds = I.Pointer(dp + 88);
            uint me = I.Pointer(z.Find(AssetTypes.ClipMapMp) + 0x9C);
            string ents = m.CString(I.Pointer(me + 4), 1 << 24)!;
            foreach (System.Text.RegularExpressions.Match e in System.Text.RegularExpressions.Regex.Matches(ents, @"\{[^}]*\}"))
            {
                var t = System.Text.RegularExpressions.Regex.Match(e.Value, "\"targetname\" \"((?:v[0-9]_)?player_pos_[a-z_]+)\"");
                var o = System.Text.RegularExpressions.Regex.Match(e.Value, "\"origin\" \"([^\"]+)\"");
                if (!t.Success || !o.Success) continue;
                var p = o.Groups[1].Value.Split(' ').Select(x => float.Parse(x, System.Globalization.CultureInfo.InvariantCulture)).ToArray();
                Console.WriteLine($"{t.Groups[1].Value} at {string.Join(",", p)}");
                for (int i = 0; i < n; i++)
                {
                    uint b = bnds + (uint)(24 * i);
                    float[] c = [m.F32(b), m.F32(b + 4), m.F32(b + 8)], ex = [m.F32(b + 12), m.F32(b + 16), m.F32(b + 20)];
                    // under the stand: the box holds the stand's x, y (within 600) and reaches its height
                    bool near = Math.Abs(c[0] - p[0]) < ex[0] + 600 && Math.Abs(c[1] - p[1]) < ex[1] + 600 && Math.Abs(c[2] - p[2]) < ex[2] + 300;
                    if (!near) continue;
                    bool inside = true;
                    for (int a = 0; a < 3; a++) if (c[a] - ex[a] < kb[a] || c[a] + ex[a] > kb[3 + a]) inside = false;
                    uint s = surfs + (uint)(24 * i);
                    string mat = z.M.CString(I.Pointer(I.Pointer(s + 16)), 128) ?? "?";
                    Console.WriteLine($"  {(inside ? "kept   " : "DROPPED")} surface {i} {mat} lm {m.U8(s + 20)} tris {m.U16(s + 10)} center {c[0]:0},{c[1]:0},{c[2]:0} half {ex[0]:0},{ex[1]:0},{ex[2]:0}");
                }
            }
            return 0;
        }
        if (args.Length >= 4 && args[1] == "flags")
        {
            using var g = GameInstallation.Open(args[2]);
            int same = 0, other = 0, missing = 0;
            foreach (string zf in args.Skip(3))
            {
                var z = LoadedZone.Read(zf, args[2]);
                foreach (var (h, t) in z.Image.AssetHeaders)
                {
                    if (t != AssetTypes.Image) continue;
                    uint ld = z.Image.Pointer(h);
                    string? n = z.Str(h + G_Name);
                    if (n is null || n.StartsWith(',') || ld == 0 || z.M.U32(ld + L_Size) != 0) continue;
                    if (!TryRead(g, "images/" + n + ".iwi", out byte[] d, out _) || ParseIwi(d) is not { } iwi) { missing++; continue; }
                    if (iwi.Flags == z.M.U32(ld + L_Flags) && D3dFormat(iwi.Format) == z.M.U32(ld + L_Format)) same++;
                    else if (other++ < 20) Console.WriteLine($"{Path.GetFileName(zf)} {n}: load def flags 0x{z.M.U32(ld + L_Flags):X} format 0x{z.M.U32(ld + L_Format):X}, IWI flags 0x{iwi.Flags:X} format {iwi.Format}");
                }
            }
            Console.WriteLine($"{same} file images whose load definition has the .iwi's flags and format, {other} not, {missing} without a file (runtime images)");
            return 0;
        }
        if (args.Length >= 4 && args[1] == "scan")
        {
            string exe = args[2];
            var v0 = LoadedZone.Read(args[3], exe, anyBuild: true);
            var names = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var v0h = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase);
            foreach (var (h, t) in v0.Image.AssetHeaders)
            {
                if (t != AssetTypes.Image) continue;
                uint ld = v0.Image.Pointer(h);
                string? n = v0.Image.Pointer(h + 0x1C) is uint np and not 0 ? v0.M.CString(np, 128) : null;
                if (n is null || ld == 0 || v0.M.U32(ld + L_Size) > 0) continue;
                v0h[n] = h;
                names[n] = $"v0 {Hex(v0.M, h, 0x20)} | {Hex(v0.M, ld, 16)} enc {v0.Image.Encoding(h)}";
            }
            Console.WriteLine($"# {names.Count} file images in {args[3]}");
            var hits = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
            var stat = new SortedDictionary<string, int>();
            var diff = new Dictionary<string, int>(); var diffEx = new Dictionary<string, string>();
            foreach (string zf in args.Skip(4))
            {
                LoadedZone z;
                try { z = LoadedZone.Read(zf, exe); }
                catch (Exception ex) { Console.WriteLine($"# {zf}: {ex.Message}"); continue; }
                foreach (var (h, t) in z.Image.AssetHeaders)
                {
                    if (t != AssetTypes.Image) continue;
                    uint ld = z.Image.Pointer(h);
                    string? n = z.Str(h + G_Name);
                    if (n is null || n.StartsWith(',') || ld == 0 || z.M.U32(ld + L_Size) > 0) continue;
                    // the fields that are not per-image (width, height, card memory are)
                    string k = $"map {z.M.U8(h + 4)} sem {z.M.U8(h + 5)} cat {z.M.U8(h + 6)} b7 {z.M.U8(h + 7)} pm {z.M.U8(h + 8)},{z.M.U8(h + 9)} np {z.M.U8(h + 10)} tr {z.M.U8(h + 11)} 1A {z.M.U8(h + 0x1A)} 1B {z.M.U8(h + 0x1B)} 1C {z.M.U32(h + 0x1C):X} | ld0 {z.M.U32(ld):X} fl {z.M.U32(ld + 4):X8} fmt {z.M.U32(ld + 8):X8} enc {z.Image.Encoding(h)}";
                    stat[k] = stat.GetValueOrDefault(k) + 1;
                    if (!v0h.TryGetValue(n, out uint oh)) continue;
                    // per-field differences: 0.0.2.2 +0..+0x1B against 1.1.1.13 +0..+0x1B, then the load definitions
                    var d = new List<string>();
                    for (uint o = 4; o < 0x1C; o++) if (v0.M.U8(oh + o) != z.M.U8(h + o)) d.Add($"+{o:X2} {v0.M.U8(oh + o):X2}>{z.M.U8(h + o):X2}");
                    uint old = v0.Image.Pointer(oh);
                    for (uint o = 0; o < 16; o += 4) if (o != L_Flags && v0.M.U32(old + o) != z.M.U32(ld + o)) d.Add($"ld+{o:X} {v0.M.U32(old + o):X}>{z.M.U32(ld + o):X}");
                    if (z.M.U32(h + 0x1C) != 0) d.Add($"+1C {z.M.U32(h + 0x1C):X}");
                    string dk = string.Join(" ", d);
                    diff[dk] = diff.GetValueOrDefault(dk) + 1;
                    diffEx.TryAdd(dk, $"{n} in {Path.GetFileName(zf)}");
                    (hits.TryGetValue(n, out var l) ? l : hits[n] = []).Add($"{Path.GetFileName(zf)}: {Hex(z.M, h, 0x24)} | {Hex(z.M, ld, 16)} enc {z.Image.Encoding(h)}");
                }
                Console.Error.WriteLine($"{Path.GetFileName(zf)} done");
            }
            foreach (var (n, v) in names.OrderBy(k => k.Key))
            {
                Console.WriteLine($"{n}\n  {v}");
                if (hits.TryGetValue(n, out var l)) foreach (var s in l.Take(3)) Console.WriteLine($"  {s}");
            }
            Console.WriteLine("# differences 0.0.2.2 -> 1.1.1.13 of the images of the same name (load definition flags left out)");
            foreach (var (k, c) in diff.OrderByDescending(kv => kv.Value)) Console.WriteLine($"{c,6} [{k}] e.g. {diffEx[k]}");
            Console.WriteLine("# stock file image header kinds");
            foreach (var (k, c) in stat.OrderByDescending(kv => kv.Value)) Console.WriteLine($"{c,6} {k}");
            return 0;
        }
        Console.Error.WriteLine("usage: vlobby_merge v0img scan <exe> <0.0.2.2 zone> <stock zone> ... | v0img flags <exe> <stock zone> ...");
        return 2;
    }
}
