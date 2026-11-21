using System.Text;
using Codol.FastFiles;
using Codol.FastFiles.Crypto;
using Codol.FastFiles.Emulation;
using Codol.FastFiles.FastFile;
using Codol.FastFiles.Porting;

namespace VlobbyMerge;

/// One lobby scene to add: a ported 1.1.1.13 zone holding its own maps/mp/mp_ui_viewer.d3dbsp world, the offset it
/// moves by (whole light grid cells: x, y multiples of 32, z of 64) and the prefix its entities' names take.
public sealed record Donor(LoadedZone Zone, float Dx, float Dy, float Dz, string Prefix, string Tag)
{
    /// a box in the donor's own coordinates (mins, maxs): surfaces whose bounds leave it draw no triangles and entities
    /// outside it are left out (a donor world's sky shell around the whole map); static models are all kept
    public float[]? Keep { get; init; }

    /// the scene's light grid is the stock one of the same place (LightGridMerge)
    public bool StockGrid { get; init; }
}

/// Builds ui_viewer_vlobby_mp: every asset of 1.1.1.13's ui_viewer_mp, its mp_ui_viewer GfxWorld and MapEnts with the
/// donors' scenes added, the donors' assets those lead to, and the merged camera table. Kernel33 (vlobby_env.c) opens
/// it for ui_viewer_mp.
///
/// The 1.1.1.13 GfxWorld is IW4's (0x274 bytes; Codol.Assets MapLayouts). The stock lobby world has one cell, so every
/// donor surface and static model joins that cell: its AABB tree gets a new root over the stock tree and the donors'
/// cell trees. Surfaces stay grouped by the dpvs ranges (lit opaque, lit translucent, shadow caster, emissive), stock
/// first in each; static models, vertices, indices, lightmaps, reflection probes, material memory and the light grid
/// rows are appended. The donors' primary lights are left out (their light is in their lightmaps and grid colours):
/// their surfaces and models take no primary light.
public static class Merge
{
    const uint W_SurfaceCount = 0x10, W_SkyCount = 0x14, W_Skies = 0x18, W_PrimaryLightCount = 0x20, W_CellCount = 0x34;
    const uint W_TreeCounts = 0x44, W_Trees = 0x48, W_Cells = 0x4C, W_Draw = 0x50, W_Grid = 0x98, W_ModelCount = 0xD0, W_Models = 0xD4;
    const uint W_Bounds = 0xD8, W_MaterialMemoryCount = 0xF4, W_MaterialMemory = 0xF8, W_ShadowGeom = 0x1C0, W_Dpvs = 0x1C8;
    const uint D_ProbeCount = 0, D_Probes = 4, D_ProbeOrigins = 8, D_ProbeTextures = 0xC, D_LightmapCount = 0x10, D_Lightmaps = 0x14,
        D_LmPrimary = 0x18, D_LmSecondary = 0x1C, D_VertexCount = 0x28, D_Vertices = 0x2C, D_VldSize = 0x34, D_Vld = 0x38, D_IndexCount = 0x40, D_Indices = 0x44;
    const uint P_SmodelCount = 0, P_StaticSurfaceCount = 4, P_NoDecal = 8, P_Ranges = 12, P_SmodelVisCount = 44, P_SurfVisCount = 48,
        P_SmodelVis = 52, P_SurfVis = 64, P_Sorted = 76, P_SmodelInsts = 80, P_Surfaces = 84, P_SurfBounds = 88, P_DrawInsts = 92,
        P_SurfMaterials = 96, P_CastsSun = 100;
    const int XModelSurfsType = 3;
    const int MaxSurfaces = 2048;
    const int SurfSize = 24, BoundsSize = 24, DrawInstSize = 76, InstSize = 36, VertexSize = 44, NodeSize = 44, CellSize = 40, SkySize = 16;

    sealed class World
    {
        public required uint H;              // header in the target image
        public required string Tag;
        public float Dx, Dy, Dz;
        public bool IsDonor;
        public int Surfaces, Smodels, Vertices, Indices, Vld, Lightmaps, Probes, Cells;
        public int[] Begin = new int[4], End = new int[4];
        public int[] NewIndex = [];          // old surface index -> merged
        public float[]? Keep;
        public bool StockGrid;
        public bool NoProbes, NoLightmaps;
        public bool OldLayoutModels;
        public int VertexBase, IndexBase, VldBase, LightmapBase, ProbeBase, SmodelBase;
        public int[] SmodelMap = [];      // static model -> merged index, -1 when left out
        public void SetBases(int v, int i, int vld, int lm, int probe)
        {
            VertexBase = v; IndexBase = i; VldBase = vld; LightmapBase = lm; ProbeBase = probe;
        }
    }

    public static void Run(LoadedZone stock, IReadOnlyList<Donor> donors, string exe, string outFf, string? tableCsv, IReadOnlyList<LoadedZone> earlier, Action<string> log)
    {
        // XModel and XModelSurfs (3, 4) are never matched by name to the zone's own (MapPorter does the same): a donor
        // model's surfaces of the same name as a stock model's are another model's geometry (different surface and
        // material counts: a static model drawn with them reads past its material array); colliding surface names
        // are renamed instead (PortContext.RenameCollidingSurfaces)
        var ctx = new PortContext(GameBuild.V1_1_1_13, stock.Image, GameBuild.V1_1_1_13) { InlineSharedDataTypes = [3, 4] };
        var T = ctx.T;
        var img = ctx.Target;

        uint stockWorld = stock.Find(AssetTypes.GfxWorld);
        uint stockEnts = stock.Image.Pointer(stock.Find(AssetTypes.ClipMapMp) + 0x9C);
        // every stock asset, unchanged (the zone stands for ui_viewer_mp); the world and MapEnts are then changed
        ctx.ConvertTopLevel();
        _stockAssets = new HashSet<uint>(img.AssetHeaders.Keys);
        _copiesFrom = ctx.NewObject(4);
        uint aw = ctx.ConvertAsset(stockWorld);
        uint ae = ctx.ConvertAsset(stockEnts);
        var a = Describe(T, img, aw, "stock", false, 0, 0, 0);
        var worlds = new List<World> { a };
        var entText = new StringBuilder(T.CString(img.Pointer(ae + 4), 1 << 24));
        if (entText.Length > 0 && entText[^1] != '\n') entText.Append('\n');
        int lmBase = a.Lightmaps, probeBase = a.Probes;
        // the donors' entities the camera table names (stands, cameras); the others (team lineups, the weapon and item
        // stands of pages 1.1.1.13 does not have) are left out: with all of them the lobby's local server stops at
        // spawn time ("ERROR: 341 targetname")
        HashSet<string>? wanted = tableCsv is null ? null : File.ReadAllLines(tableCsv, Encoding.Latin1)
            .Select(l => l.Split(',')).Where(c => c.Length > 3).SelectMany(c => new[] { c[2], c[3] })
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        foreach (var dn in donors)
        {
            if (dn.Zone.Build == GameBuild.V0_0_2_2) V0Images(dn.Zone, log);
            RenameSceneImages(dn.Zone, lmBase, probeBase, log);
            if (dn.Zone.Build != GameBuild.V0_0_2_2 && Environment.GetEnvironmentVariable("VLOBBY_RENAME_RENDER") == "1")
                RenameRenderAssets(dn.Zone, "_" + dn.Prefix.TrimEnd('_'), log);
            var known = new HashSet<uint>(img.AssetHeaders.Keys);
            uint dw = ctx.CopyTargetAsset(dn.Zone.Image, dn.Zone.Find(AssetTypes.GfxWorld));
            var newAssets = img.AssetHeaders.Where(kv => !known.Contains(kv.Key)).GroupBy(kv => kv.Value).OrderBy(g => g.Key)
                .Select(g => $"{g.Count()} of type {g.Key}");
            log($"{dn.Tag}: copied assets the zone did not hold: {string.Join(", ", newAssets)}");
            if (Environment.GetEnvironmentVariable("VLOBBY_LIST_ADDED") == "1")
                foreach (var kv in img.AssetHeaders.Where(kv => !known.Contains(kv.Key)))
                    log($"  added {kv.Value} {ZoneImageDiff.AssetName(img, kv.Key)}");
            if (Environment.GetEnvironmentVariable("VLOBBY_NO_COMPACT") != "1") Compact.World(ctx, dw, log, dn.Tag, dn.Keep);
            var w = Describe(T, img, dw, dn.Tag, true, dn.Dx, dn.Dy, dn.Dz);
            w.Keep = dn.Keep;
            if (Environment.GetEnvironmentVariable("VLOBBY_TEST_NO_DONOR_PROBES") == "1") { w.Probes = 0; w.NoProbes = true; }
            if (Environment.GetEnvironmentVariable("VLOBBY_TEST_NO_DONOR_LIGHTMAPS") == "1") { w.Lightmaps = 0; w.NoLightmaps = true; }
            w.StockGrid = dn.StockGrid;
            w.OldLayoutModels = dn.Zone.Build == GameBuild.V0_0_2_2;
            worlds.Add(w);
            lmBase += w.Lightmaps;
            probeBase += w.Probes;
            uint dents = dn.Zone.Image.Pointer(dn.Zone.Find(AssetTypes.ClipMapMp) + 0x9C);
            string ents = dn.Zone.M.CString(dn.Zone.Image.Pointer(dents + 4), 1 << 24)!;
            int added = AppendEntities(entText, ents, dn, wanted);
            log($"{dn.Tag}: {w.Surfaces} surfaces, {w.Smodels} static models, {w.Vertices} vertices, {w.Lightmaps} lightmaps, {w.Probes} probes, {w.Cells} cells, {added} entities (moved by {dn.Dx},{dn.Dy},{dn.Dz}, names {dn.Prefix}*)");
        }

        ReferenceEarlier(ctx, earlier, log);
        DedupeByName(ctx, log);
        if (donors.FirstOrDefault(d => d.Zone.Build == GameBuild.V0_0_2_2) is { } v0)
            V0FileImages.Run(ctx, _stockAssets, v0.Zone.Path, exe, outFf, log);
        if (donors.Count > 0 || Environment.GetEnvironmentVariable("VLOBBY_REBUILD_STOCK") == "1")
        {
            uint merged = BuildWorld(ctx, worlds, log);
            img.AssetHeaders[merged] = AssetTypes.GfxWorld;
            ctx.ReplaceAsset(aw, merged);
            _mergedWorld = merged;
        }

        Framing.Apply(entText, log);
        // MapEnts: the stock copy with the longer entity string
        byte[] text = Encoding.Latin1.GetBytes(entText.ToString());
        uint str = ctx.NewObject((uint)text.Length + 1, 1);
        T.Write(str, text);
        T.WU8(str + (uint)text.Length, 0);
        img.SetPointer(ae + 4, str);
        T.WU32(ae + 8, (uint)text.Length + 1);

        if (tableCsv is not null)
        {
            uint table = StringTables.Add(ctx, "mp/camerapostable_vlobby.csv", File.ReadAllText(tableCsv, Encoding.Latin1));
            img.Assets.Insert(0, new ZoneImageAsset(StringTables.Type, table));
        }
        foreach (var wn in ctx.Warnings.Distinct()) log("note: " + wn);

        var refs = img.NameReferences.Values.Distinct().OrderBy(r => r.Type).ThenBy(r => r.Name).ToList();
        log($"{refs.Count} assets named from other zones (they must be loaded before this one):");
        File.WriteAllLines(outFf + ".refs.txt", refs.Select(r => $"{r.Type}	{r.Name}"));
        {
            // A pointer the donor zone wrote as an offset to data another pointer loads (techniques that two technique
            // sets share, wc_unlit_blend_lin and wc_unlit_blend_lin_ua) waits for that other pointer; when what held it
            // is not in this zone (an asset the stock zone stands for), the offset was never filled in, and C1 read every
            // technique of the set from the stream block's base (0x1D9BC000: pass 0 pixel shader 0x476). Every such
            // pointer of the copies is left for the writer to place at its first use and alias at the others.
            int freed = 0;
            foreach (uint f in img.Pointers.Enumerate(_copiesFrom, uint.MaxValue - _copiesFrom).ToList())
            {
                if (img.Encoding(f) != PointerEncoding.Reference || img.AssetHeaders.ContainsKey(T.U32(f))) continue;
                img.SetEncoding(f, PointerEncoding.Unknown);
                img.RawReferences.Remove(f);
                freed++;
            }
            log($"{freed} offset pointers of the copies left to the writer");
        }
        {
            // The donors' models, materials and images become top-level assets ahead of the stock ones. Placed first
            // through a pointer of the new world arrays (written inline into the zone's persistent memory), a material
            // left every other pointer to it aliasing that zone copy instead of the header C1 keeps in its asset pool;
            // C1 numbers materials by their pool slot ((pointer - 0x01DEA3D8) / 96, 0x004CE700), and the material sort at
            // load (0x00A87400) then read a technique set from a slot past the pool (pixel shader 0x476 at 0x00A8753F).
            // A top-level asset is read into temporary memory and every pointer to it aliases its insert slot, which
            // holds the pool header (ZoneWriter: "an asset placed from temp memory always gets an insert slot").
            var listed = new HashSet<uint>(img.Assets.Select(a => a.Header));
            // only what the merged world leads to (a 0.0.2.2 model left out is still in the image, unreached)
            var reached = new HashSet<uint>();
            var todo = new Stack<uint>();
            todo.Push(_mergedWorld);
            var seenObj = new HashSet<uint>();
            while (todo.Count > 0)
            {
                uint at0 = todo.Pop();
                if (!img.TryFindObject(at0, out uint st, out uint len) || !seenObj.Add(st)) continue;
                if (img.AssetHeaders.ContainsKey(st)) reached.Add(st);
                foreach (uint f in img.Pointers.Enumerate(st, len)) todo.Push(T.U32(f));
            }
            var add = img.AssetHeaders.Where(kv => reached.Contains(kv.Key) && !_stockAssets.Contains(kv.Key) && !listed.Contains(kv.Key)
                    && kv.Value is AssetTypes.Image or AssetTypes.Material or AssetTypes.XModel && !img.NameReferences.ContainsKey(kv.Key))
                .OrderBy(kv => kv.Value == AssetTypes.Image ? 0 : kv.Value == AssetTypes.Material ? 1 : 2).ThenBy(kv => kv.Key).ToList();
            img.Assets.InsertRange(0, add.Select(kv => new ZoneImageAsset(kv.Value, kv.Key)));
            // a pointer that wrote one of them inline would write it a second time
            var top = new HashSet<uint>(add.Select(kv => kv.Key));
            foreach (uint f in img.Pointers.Enumerate(0, uint.MaxValue).ToList())
                if (top.Contains(T.U32(f)) && img.Encoding(f) is PointerEncoding.Inline or PointerEncoding.Insert)
                    img.SetEncoding(f, PointerEncoding.Unknown);
            log($"{add.Count} donor assets listed as top-level assets ({string.Join(", ", add.GroupBy(kv => kv.Value).Select(g => $"{g.Count()} of type {g.Key}"))})");
        }
        if (Environment.GetEnvironmentVariable("VLOBBY_INSERT_ASSETS") == "1")
        {
            // A pointer to an asset that the writer places with -1 (inline) leaves later pointers aliasing the asset's
            // pool copy; when C1 already holds an asset of that name from a zone loaded earlier (code_post_gfx_mp,
            // ui_mp: shaders, vertex declarations, technique sets the donors share by name), it keeps the earlier one and
            // frees the pool copy, and those aliases read freed memory (the material sort at 0x00A8753F read a technique
            // pass shader of 0x476). -2 (insert) makes the loader store the header it keeps in a slot that the later
            // pointers alias, as the shipped zones do for shared assets.
            int forced = 0;
            foreach (uint f in img.Pointers.Enumerate(0, uint.MaxValue).ToList())
            {
                uint v = T.U32(f);
                if (img.Encoding(f) == PointerEncoding.Unknown && img.AssetHeaders.TryGetValue(v, out int at)
                    && at is 3 or 5 or 6 or 7 or 8 or 9 or 10 or 0 or 1)
                {
                    img.SetEncoding(f, PointerEncoding.Insert);
                    forced++;
                }
            }
            log($"{forced} asset pointers placed with insert slots");
        }
        string zoneName = Path.GetFileNameWithoutExtension(outFf);
        var written = ZoneWriter.Write(GameBuild.V1_1_1_13, img, zoneName);
        foreach (var wn in written.Warnings.Distinct().Where(x => !x.Contains("aliased before it is loaded")).Take(60)) log("writer: " + wn);
        log($"writer: {written.Warnings.Count(x => x.Contains("aliased before it is loaded"))} assets aliased before they were loaded");
        var reload = new ZoneLoadEmulator(GameBuild.V1_1_1_13).Load(zoneName, written.Zone);
        if (reload.Assets.Count != img.Assets.Count)
            throw new InvalidDataException($"the 1.1.1.13 loaders read {reload.Assets.Count} assets back, {img.Assets.Count} were written");
        if (reload.StreamBytesConsumed != written.Zone.LongLength)
            throw new InvalidDataException($"the 1.1.1.13 loaders consumed {reload.StreamBytesConsumed} of {written.Zone.LongLength} zone bytes");
        using var rsa = FastFileKeyProvider.LoadFromExecutable(exe);
        byte[] ff = FastFileWriter.WriteUnsigned(GameBuild.V1_1_1_13, written.Zone, rsa);
        File.WriteAllBytes(outFf, ff);
        log($"wrote {outFf}: {img.Assets.Count} assets, zone {written.Zone.Length} bytes, file {ff.Length} bytes");
        V0FileImages.WriteIwd(written.Zone, zoneName, log);
    }

    /// the donor's lightmaps and reflection probe images are named like the stock ones (*lightmap0_primary,
    /// *reflection_probe0): renamed after the merged world's numbering, so that neither stands for the other
    static void RenameSceneImages(LoadedZone z, int lmBase, int probeBase, Action<string> log)
    {
        var m = z.M; var I = z.Image;
        uint w = z.Find(AssetTypes.GfxWorld), draw = w + W_Draw;
        void Rename(uint image, string name)
        {
            if (image == 0) return;
            byte[] b = Encoding.Latin1.GetBytes(name + "\0");
            uint at = NewDonorString(z, b);
            I.SetPointer(image + 32, at);
        }
        int lms = (int)m.U32(draw + D_LightmapCount);
        for (int i = 0; i < lms; i++)
        {
            uint e = I.Pointer(draw + D_Lightmaps) + (uint)(8 * i);
            Rename(I.Pointer(e), $"*lightmap{lmBase + i}_primary");
            Rename(I.Pointer(e + 4), $"*lightmap{lmBase + i}_secondary");
        }
        int probes = (int)m.U32(draw + D_ProbeCount);
        for (int i = 0; i < probes; i++) Rename(I.Pointer(I.Pointer(draw + D_Probes) + (uint)(4 * i)), $"*reflection_probe{probeBase + i}");
    }

    /// C1 links a zone's assets by name, case-insensitively; a second asset of one type and name in the same zone makes it
    /// release the first before its texture exists (0x004CC625 -> image release 0x004CAE50 -> 0x00A921B8). The donors
    /// spell some image names in another case than the stock zone (0.0.2.2: Plastic_case_rifle_black_col), which the
    /// copy's own name match (exact) does not join: every later asset of a type and name stands for the first.
    static void DedupeByName(PortContext ctx, Action<string> log)
    {
        var img = ctx.Target;
        var first = new Dictionary<(int, string), uint>();
        var dups = new List<(uint Dup, uint First)>();
        foreach (var (h, t) in img.AssetHeaders.OrderBy(kv => kv.Key).ToList())
        {
            if (t is AssetTypes.GfxWorld or AssetTypes.MapEnts or XModelSurfsType) continue;
            string? name = ZoneImageDiff.AssetName(img, h);
            if (string.IsNullOrEmpty(name)) continue;
            var key = (t, name.ToLowerInvariant().Replace((char)92, '/'));
            if (first.TryGetValue(key, out uint f)) dups.Add((h, f));
            else first[key] = h;
        }
        var types = Environment.GetEnvironmentVariable("VLOBBY_DEDUPE_TYPES");
        if (types is not null) dups = dups.Where(d => types.Split(',').Contains(img.AssetHeaders[d.Dup].ToString())).ToList();
        foreach (var (dup, f) in dups) ctx.ReplaceAsset(dup, f);
        if (dups.Count > 0) log($"{dups.Count} assets named like an earlier one of their type stand for it: " +
            string.Join(", ", dups.GroupBy(d => img.AssetHeaders.GetValueOrDefault(d.First)).Select(g => $"{g.Count()} of type {g.Key}")));
    }

    /// 0.0.2.2's GfxImage is 0x20 bytes: 1.1.1.13's (0x24) has one more u32 at +0x1C, before the name (zone-diff of
    /// ui_viewer_mp: *lightmap0_primary is 0x00020103 ... name at +0x1C in 0.0.2.2, ... 0, name at +0x20 in 1.1.1.13;
    /// its load definition is the same). Every image of the donor gets a 1.1.1.13 header, and what led to the old one
    /// leads to the new one.
    /// A 0.0.2.2 image whose pixels come from an iwd keeps its load definition here (an insert, flags 0): V0FileImages.cs
    /// gives the ones the merged zone keeps 1.1.1.13's (inline, the .iwi's flags) and ships their files.
    static void V0Images(LoadedZone z, Action<string> log)
    {
        var I = z.Image; var m = z.M;
        var moved = new Dictionary<uint, uint>();
        foreach (var (h, t) in I.AssetHeaders.ToList())
        {
            if (t != AssetTypes.Image) continue;
            uint nh = _donorTop;
            _donorTop += 0x40;
            m.Write(nh, m.ReadBytes(h, 0x1C));
            m.WU32(nh + 0x1C, 0);
            // the category (+6): 1.1.1.13 numbers the scene images' categories anew (lightmaps 2 -> 10, reflection probe
            // cubes 1 -> 9; files stay 3, $outdoor 1). Loaded under 0.0.2.2's numbers, a lightmap or probe became a
            // texture of another kind and d3d9.dll faulted the first time the scene was drawn.
            // The flags word of an image whose pixels are in the zone (GfxImageLoadDef +4) is numbered anew too:
            // 1.1.1.13 writes 0x08000003 for a lightmap, 0x08010301 for a reflection probe cube and 3 for $outdoor,
            // 0.0.2.2 2 (2D) and 0x10000 (cube); C1 then made the textures wrongly and d3d9.dll faulted drawing them.
            byte cat = m.U8(nh + 6);
            string? iname = I.Pointer(h + 0x1C) is uint np and not 0 ? m.CString(np, 128) : null;
            bool probe = cat == 1 && iname is not null && iname.StartsWith("*reflection_probe", StringComparison.OrdinalIgnoreCase);
            uint def = I.Pointer(h);   // the source header's: nh's pointer is only set below (read from nh it was 0, and
                                       // the lightmaps kept 0.0.2.2's flags 2: C1 made them black textures)
            uint? flags = cat == 2 ? 0x08000003u : probe ? 0x08010301u : cat == 1 ? 3u : null;
            if (cat == 2) m.WU8(nh + 6, 10);
            else if (probe) m.WU8(nh + 6, 9);
            if (flags is uint fl && def != 0)
            {
                uint was = m.U32(def + 4);
                if (was is 2 or 0x10000) m.WU32(def + 4, fl);
                else log($"0.0.2.2: image {iname}: load flags 0x{was:X8} not known; left");
            }
            I.Objects[nh] = 0x24;
            // the pixels (GfxImageLoadDef) of an image in the zone: 0.0.2.2 streams them as an insert (-2), 1.1.1.13 inline
            // (-1); its loader leaves a -2 there as the texture pointer (d3d9.dll faulted on 0xFFFFFFFE once drawn)
            var texEnc = I.Encoding(h);
            uint ld = I.Pointer(h);
            bool inZone = ld != 0 && m.U32(ld + 12) > 0;   // resourceSize: the pixels follow in the zone
            I.SetPointer(nh, ld, texEnc == PointerEncoding.Insert && inZone ? PointerEncoding.Inline : texEnc);
            I.SetPointer(nh + 0x20, I.Pointer(h + 0x1C), I.Encoding(h + 0x1C));
            moved[h] = nh;
        }
        int repointed = 0;
        foreach (uint f in I.Pointers.Enumerate(0, uint.MaxValue).ToList())
        {
            if (f >= 0xA0000000 && f < _donorTop) continue;
            if (moved.TryGetValue(m.U32(f), out uint nh)) { I.SetPointer(f, nh, I.Encoding(f)); repointed++; }
        }
        foreach (var (h, nh) in moved)
        {
            I.AssetHeaders.Remove(h);
            I.AssetHeaders[nh] = AssetTypes.Image;
        }
        for (int i = 0; i < I.Assets.Count; i++)
            if (moved.TryGetValue(I.Assets[i].Header, out uint nh)) I.Assets[i] = I.Assets[i] with { Header = nh };
        log($"0.0.2.2: {moved.Count} image headers rewritten in the 1.1.1.13 layout, {repointed} pointers to them");
    }

    /// C1 loads the UI zones before ui_viewer_mp (code_pre_gfx_mp, code_post_gfx_mp, the patch folder's zones, ui_mp,
    /// localized_ui_mp ...). A material of this zone named like one of theirs is a second definition: C1 keeps the
    /// first and returns the new one's pool slot to the free list, but the slot stays in the renderer's sorted material
    /// list, and sorting it (0x00A87400 -> 0x00A87330, slot index -> 0x01DEA3D8 + 96 * index) reads a free slot's
    /// garbage as a technique set (the pass 0 pixel shader 0x476 at 0x00A8753F). So every donor asset of a render type
    /// whose name an earlier zone defines is written as a reference by name to that one, as the shipped zones do.
    static void ReferenceEarlier(PortContext ctx, IReadOnlyList<LoadedZone> earlier, Action<string> log)
    {
        if (earlier.Count == 0) return;
        var img = ctx.Target;
        int[] types = [AssetTypes.Material, AssetTypes.Image, AssetTypes.Techset, 6, 7, 8];
        var names = new HashSet<(int, string)>();
        foreach (var z in earlier)
            foreach (var (h, t) in z.Image.AssetHeaders)
                if (types.Contains(t) && ZoneImageDiff.AssetName(z.Image, h) is { } n && !n.StartsWith(','))
                    names.Add((t, n.ToLowerInvariant()));
        var counts = new SortedDictionary<int, int>();
        foreach (var (h, t) in img.AssetHeaders.ToList())
        {
            if (_stockAssets.Contains(h) || !types.Contains(t)) continue;
            if (ZoneImageDiff.AssetName(img, h) is not { } n || n.StartsWith(',') || !names.Contains((t, n.ToLowerInvariant()))) continue;
            ctx.ReplaceAsset(h, ctx.NameReference(t, n));
            counts[t] = counts.GetValueOrDefault(t) + 1;
        }
        log($"donor assets an earlier UI zone defines, referenced by name: {string.Join(", ", counts.Select(kv => $"{kv.Value} of type {kv.Key}"))}");
    }

    static HashSet<uint> _stockAssets = [];
    static uint _copiesFrom;
    static uint _mergedWorld;
    static string _leftOut = "";

    /// A ported donor's technique sets, shaders and vertex declarations get names of their own (name + _v3): the ones
    /// the stock zone has under the same name are other builds' (a vertex declaration "pp" of another stream layout, a
    /// shader compiled for other constants), and matching them by name drew the donor geometry with the stock ones (a
    /// crash in d3d9.dll once a donor scene was in view). 0.0.2.2 keeps its names: its render assets are 1.1.1.13's.
    static void RenameRenderAssets(LoadedZone z, string suffix, Action<string> log)
    {
        var I = z.Image; var m = z.M;
        int n = 0;
        foreach (var (h, t) in I.AssetHeaders.ToList())
        {
            if (t is not (6 or 7 or 8 or 9)) continue;
            uint np = I.Pointer(h);
            string? name = np == 0 ? null : m.CString(np, 256);
            if (string.IsNullOrEmpty(name) || name.StartsWith(',')) continue;
            I.SetPointer(h, NewDonorString(z, Encoding.Latin1.GetBytes(name + suffix + "\0")), I.Encoding(h));
            n++;
        }
        log($"{suffix.TrimStart('_')}: {n} technique sets, shaders and vertex declarations renamed *{suffix}");
    }

    static uint _donorTop = 0xA0000000;
    static uint NewDonorString(LoadedZone z, byte[] bytes)
    {
        uint at = _donorTop;
        _donorTop += (uint)((bytes.Length + 15) & ~15);
        z.M.Write(at, bytes);
        z.Image.Objects[at] = (uint)bytes.Length;
        return at;
    }

    static World Describe(VirtualMemory T, ZoneImage img, uint h, string tag, bool donor, float dx, float dy, float dz)
    {
        uint dp = h + W_Dpvs, draw = h + W_Draw;
        var w = new World
        {
            H = h, Tag = tag, IsDonor = donor, Dx = dx, Dy = dy, Dz = dz,
            Surfaces = (int)T.U32(dp + P_StaticSurfaceCount), Smodels = (int)T.U32(dp + P_SmodelCount),
            Vertices = (int)T.U32(draw + D_VertexCount), Indices = (int)T.U32(draw + D_IndexCount), Vld = (int)T.U32(draw + D_VldSize),
            Lightmaps = (int)T.U32(draw + D_LightmapCount), Probes = (int)T.U32(draw + D_ProbeCount), Cells = (int)T.U32(h + W_CellCount),
        };
        if (T.U32(dp + P_NoDecal) != (uint)w.Surfaces || T.U32(h + W_SurfaceCount) != (uint)w.Surfaces)
            throw new NotSupportedException($"{tag}: decal surfaces (staticSurfaceCountNoDecal != staticSurfaceCount) are not handled");
        int at = 0;
        for (int k = 0; k < 4; k++)
        {
            w.Begin[k] = (int)T.U32(dp + P_Ranges + (uint)(8 * k));
            w.End[k] = (int)T.U32(dp + P_Ranges + (uint)(8 * k) + 4);
            if (w.Begin[k] != at || w.End[k] < w.Begin[k]) throw new NotSupportedException($"{tag}: the dpvs surface ranges are not contiguous");
            at = w.End[k];
        }
        if (at != w.Surfaces) throw new NotSupportedException($"{tag}: the dpvs ranges do not cover the surfaces");
        return w;
    }

    static uint VisCount(int n) => (uint)(((n + 127) >> 7) << 2);

    static uint BuildWorld(PortContext ctx, List<World> worlds, Action<string> log)
    {
        var T = ctx.T; var img = ctx.Target;
        var a = worlds[0];
        uint h = ctx.NewObject(0x274);
        Copy(ctx, h, a.H, 0x274);

        // ---- surfaces: category by category, stock first
        int[] catStart = new int[4];
        int total = 0;
        for (int k = 0; k < 4; k++)
        {
            catStart[k] = total;
            foreach (var w in worlds) total += w.End[k] - w.Begin[k];
        }
        bool donorLast = Environment.GetEnvironmentVariable("VLOBBY_TEST_DONOR_LAST") == "1";
        if (donorLast)
        {
            // test: stock surfaces keep their indices, every donor surface goes after them in the last range
            var a1 = worlds[0];
            for (int k = 0; k < 4; k++) catStart[k] = a1.Begin[k];
            int at = a1.Surfaces;
            foreach (var w in worlds)
            {
                w.NewIndex = new int[w.Surfaces];
                for (int i = 0; i < w.Surfaces; i++) w.NewIndex[i] = w.IsDonor ? at++ : i;
            }
        }
        else
        {
            int[] fill = (int[])catStart.Clone();
            foreach (var w in worlds)
            {
                w.NewIndex = new int[w.Surfaces];
                for (int k = 0; k < 4; k++)
                    for (int i = w.Begin[k]; i < w.End[k]; i++) w.NewIndex[i] = fill[k]++;
            }
        }
        // A donor static model whose model is one of ui_viewer_mp's own that no stock static model places (the later
        // lobbies' weapon and attachment props: sto_default_ak104_01, wea_m4_m4_01, sentry_minigun ...) is left out:
        // C1 makes those models load on demand for the character and weapon views ("Static model set as load on demand"),
        // and a static model drawn with one reads vertex buffers that do not exist (crash in the model pass).
        {
            var stockPlaced = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var a0 = worlds[0];
            uint sd0 = img.Pointer(a0.H + W_Dpvs + P_DrawInsts);
            for (int i = 0; i < a0.Smodels; i++) stockPlaced.Add(ZoneImageDiff.AssetName(img, img.Pointer(sd0 + (uint)(DrawInstSize * i) + 52)) ?? "");
            var stockModels = new HashSet<string>(_stockAssets.Where(x => img.AssetHeaders.TryGetValue(x, out int t) && t == AssetTypes.XModel)
                .Select(x => ZoneImageDiff.AssetName(img, x) ?? ""), StringComparer.OrdinalIgnoreCase);
            // and these donor models, which C1 reported loading on demand ("Static model set as load on demand: <model>",
            // console_mp.log; 0x00AA1E30: the model's flag byte +0x15D has no bit 8): 3.19.26.22's character and
            // weapon props
            foreach (string m in new[] { "heg_baseball_tf_s_00", "mag_default_m4a1techncm_blk", "mil_weapon_m4_clip_prop", "sentry_minigun" })
                stockModels.Add(m);
            // C1 also loads on demand every model the lobby's item tables name (mp/cocharitems*, coweapitems*,
            // coweapmodels/*: heg_baseball_tf_s_00, mag_default_m4a1techncm_blk, sentry_minigun ...)
            foreach (uint st in _stockAssets.Where(x => img.AssetHeaders.TryGetValue(x, out int t) && t == StringTables.Type))
            {
                int cols = (int)T.U32(st + 4), rows = (int)T.U32(st + 8);
                uint cells = img.Pointer(st + 12);
                for (int c = 0; cells != 0 && c < cols * rows; c++)
                    if (img.Pointer(cells + (uint)(8 * c)) is uint sp && sp != 0 && T.CString(sp, 256) is { Length: > 0 } cell) stockModels.Add(cell);
            }
            int at = 0, left = 0;
            var dropNames = new SortedSet<string>(StringComparer.OrdinalIgnoreCase);
            int outsideKeep = 0;
            var v0Missing = new SortedSet<string>(StringComparer.OrdinalIgnoreCase);
            var stockModelHeaders = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase);
            foreach (uint x in _stockAssets)
                if (img.AssetHeaders.TryGetValue(x, out int tx) && tx == AssetTypes.XModel && ZoneImageDiff.AssetName(img, x) is { } xn)
                    stockModelHeaders.TryAdd(xn, x);
            foreach (var w in worlds)
            {
                w.SmodelBase = at;
                w.SmodelMap = new int[w.Smodels];
                uint sd = img.Pointer(w.H + W_Dpvs + P_DrawInsts);
                for (int i = 0; i < w.Smodels; i++)
                {
                    uint modelHeader = img.Pointer(sd + (uint)(DrawInstSize * i) + 52);
                    string model = ZoneImageDiff.AssetName(img, modelHeader) ?? T.CString(img.Pointer(modelHeader), 256) ?? "";
                    if (Environment.GetEnvironmentVariable("VLOBBY_LIST_MODELS") == "1" && w.IsDonor) log($"  {w.Tag} smodel {i}: {model} (0x{modelHeader:X8})");
                    bool drop = w.IsDonor && stockModels.Contains(model) && !stockPlaced.Contains(model);
                    if (w.IsDonor && Environment.GetEnvironmentVariable("VLOBBY_TEST_NO_DONOR_SMODELS") == "1") drop = true;
                    if (w.OldLayoutModels && Environment.GetEnvironmentVariable("VLOBBY_TEST_NO_V0_SMODELS") == "1") drop = true;
                    if (w.IsDonor && w.Keep is { } kb)
                    {
                        // a static model outside the keep box (0.0.2.2's sky shell props) stays out: moved with the
                        // scene it would stretch the merged cell past the stock world's bounds
                        uint o = sd + (uint)(DrawInstSize * i);
                        float ox = T.F32(o), oy = T.F32(o + 4), oz = T.F32(o + 8);
                        if (ox < kb[0] || oy < kb[1] || oz < kb[2] || ox > kb[3] || oy > kb[4] || oz > kb[5]) { drop = true; outsideKeep++; }
                    }
                    if (w.IsDonor && model.StartsWith(','))
                    {
                        // a stub (",name": the model is another zone's of the donor build; 2.6.9.7's character and
                        // magazine props): the stock model of that name when a stock static model places it, else left out
                        string named = model[1..];
                        if (stockModelHeaders.TryGetValue(named, out uint sm) && stockPlaced.Contains(named))
                            img.SetPointer(sd + (uint)(DrawInstSize * i) + 52, sm);
                        else drop = true;
                        model = named;
                    }
                    if (w.IsDonor && w.OldLayoutModels)
                    {
                        // 0.0.2.2's XModel (0x16C) and XModelSurfs are not 1.1.1.13's: its static models take the stock model
                        // of the same name (the scenes are earlier versions of the stock ones), and the others are left out
                        if (stockModelHeaders.TryGetValue(model, out uint stockModel) && stockPlaced.Contains(model))
                            img.SetPointer(sd + (uint)(DrawInstSize * i) + 52, stockModel);
                        else { drop = true; v0Missing.Add(model); }
                    }
                    w.SmodelMap[i] = drop ? -1 : at++;
                    if (drop) { left++; if (!v0Missing.Contains(model)) dropNames.Add(model); }
                }
            }
            _leftOut = (left == 0 ? "" : $"{left} donor static models left out (models the stock lobby loads on demand): {string.Join(", ", dropNames)}")
                + (v0Missing.Count == 0 ? "" : $"; 0.0.2.2 models no stock static model places (left out): {string.Join(", ", v0Missing)}")
                + (outsideKeep == 0 ? "" : $"; {outsideKeep} outside their keep box");
        }
        int N = total, M = worlds.Sum(w => w.SmodelMap.Count(x => x >= 0));
        if (_leftOut.Length > 0) log(_leftOut);
        if (N > 0xFFFF || M > 0xFFFF) throw new NotSupportedException("more than 65535 surfaces or static models");
        // a conservative bound (the stock lobby has 1748 surfaces; Compact keeps the merge near that)
        if (N > MaxSurfaces) throw new NotSupportedException($"{N} world surfaces: the lobby draws {MaxSurfaces} at most");
        uint dp = h + W_Dpvs, draw = h + W_Draw;
        uint a0surfaces = worlds[0].H + W_Dpvs + P_Surfaces;
        uint surfaces = ctx.NewObject((uint)(SurfSize * N)), bounds = ctx.NewObject((uint)(BoundsSize * N));
        int vBase = 0, iBase = 0, vldBase = 0, lmBase = 0, probeBase = 0, dropped = 0;
        foreach (var w in worlds)
        {
            uint src = img.Pointer(w.H + W_Dpvs + P_Surfaces), srcB = img.Pointer(w.H + W_Dpvs + P_SurfBounds);
            for (int i = 0; i < w.Surfaces; i++)
            {
                uint d = surfaces + (uint)(SurfSize * w.NewIndex[i]);
                Copy(ctx, d, src + (uint)(SurfSize * i), SurfSize);
                Copy(ctx, bounds + (uint)(BoundsSize * w.NewIndex[i]), srcB + (uint)(BoundsSize * i), BoundsSize);
                if (!w.IsDonor) continue;
                if (Environment.GetEnvironmentVariable("VLOBBY_TEST_STOCK_MATERIAL") == "1")
                    img.SetPointer(d + 16, img.Pointer(img.Pointer(a0surfaces) + 16));
                if (w.Keep is { } kb && !Inside(T, bounds + (uint)(BoundsSize * w.NewIndex[i]), kb)) { T.WU16(d + 10, 0); dropped++; }
                if (Environment.GetEnvironmentVariable("VLOBBY_TEST_VLD0") == "1") T.WU32(d + 0, 0);
                else T.WU32(d + 0, T.U32(d + 0) + (uint)vldBase);
                T.WU32(d + 4, T.U32(d + 4) + (uint)vBase);
                T.WU32(d + 12, T.U32(d + 12) + (uint)iBase);
                byte lm = T.U8(d + 20);
                if (lm != 31) T.WU8(d + 20, w.NoLightmaps ? (byte)31 : (byte)(lm + lmBase));
                T.WU8(d + 21, w.NoProbes ? (byte)0 : (byte)(T.U8(d + 21) + probeBase));
                T.WU8(d + 22, 0);
                MoveBounds(T, bounds + (uint)(BoundsSize * w.NewIndex[i]), w);
            }
            w.SetBases(vBase, iBase, vldBase, lmBase, probeBase);
            vBase += w.Vertices; iBase += w.Indices; vldBase += w.Vld; lmBase += w.Lightmaps; probeBase += w.Probes;
        }
        if (dropped > 0) log($"{dropped} donor surfaces outside their keep box draw nothing");
        if (probeBase > 255 || lmBase > 30) throw new NotSupportedException("too many reflection probes or lightmaps");
        img.SetPointer(dp + P_Surfaces, surfaces);
        img.SetPointer(dp + P_SurfBounds, bounds);
        T.WU32(h + W_SurfaceCount, (uint)N);
        T.WU32(dp + P_StaticSurfaceCount, (uint)N);
        T.WU32(dp + P_NoDecal, (uint)N);
        for (int k = 0; k < 4; k++)
        {
            int len = worlds.Sum(w => w.End[k] - w.Begin[k]);
            T.WU32(dp + P_Ranges + (uint)(8 * k), (uint)catStart[k]);
            T.WU32(dp + P_Ranges + (uint)(8 * k) + 4, (uint)(catStart[k] + len));
            if (donorLast) T.WU32(dp + P_Ranges + (uint)(8 * k) + 4, k == 3 ? (uint)N : (uint)worlds[0].End[k]);
        }

        // sorted surface lists: [stock, donors...] for the first half, the same for the no-decal half
        uint sorted = ctx.NewObject((uint)(4 * N));
        {
            int at = 0, at2 = N;
            foreach (var w in worlds)
            {
                uint src = img.Pointer(w.H + W_Dpvs + P_Sorted);
                for (int i = 0; i < w.Surfaces; i++) T.WU16(sorted + (uint)(2 * at++), (ushort)w.NewIndex[T.U16(src + (uint)(2 * i))]);
                for (int i = 0; i < w.Surfaces; i++) T.WU16(sorted + (uint)(2 * at2++), (ushort)w.NewIndex[T.U16(src + (uint)(2 * (w.Surfaces + i)))]);
            }
        }
        img.SetPointer(dp + P_Sorted, sorted);

        // static models
        uint insts = ctx.NewObject((uint)(InstSize * M)), drawInsts = ctx.NewObject((uint)(DrawInstSize * M));
        {
            foreach (var w in worlds)
            {
                uint si = img.Pointer(w.H + W_Dpvs + P_SmodelInsts), sd = img.Pointer(w.H + W_Dpvs + P_DrawInsts);
                for (int i = 0; i < w.Smodels; i++)
                {
                    int at = w.SmodelMap[i];
                    if (at < 0) continue;
                    uint di = insts + (uint)(InstSize * at), dd = drawInsts + (uint)(DrawInstSize * at);
                    Copy(ctx, di, si + (uint)(InstSize * i), InstSize);
                    Copy(ctx, dd, sd + (uint)(DrawInstSize * i), DrawInstSize);
                    if (!w.IsDonor) continue;
                    MoveBounds(T, di, w);
                    MovePoint(T, di + 24, w);
                    MovePoint(T, dd, w);
                    T.WU8(dd + 60, w.NoProbes ? (byte)0 : (byte)(T.U8(dd + 60) + w.ProbeBase));
                    T.WU8(dd + 61, 0);
                }
            }
        }
        img.SetPointer(dp + P_SmodelInsts, insts);
        img.SetPointer(dp + P_DrawInsts, drawInsts);
        T.WU32(dp + P_SmodelCount, (uint)M);
        T.WU32(dp + P_SmodelVisCount, VisCount(M));
        T.WU32(dp + P_SurfVisCount, VisCount(N));
        for (uint k = 0; k < 3; k++)
        {
            img.SetPointer(dp + P_SmodelVis + 4 * k, ctx.NewObject((uint)M, 1));
            img.SetPointer(dp + P_SurfVis + 4 * k, ctx.NewObject((uint)N, 1));
        }
        img.SetPointer(dp + P_SurfMaterials, ctx.NewObject((uint)(8 * N)));
        img.SetPointer(dp + P_CastsSun, ctx.NewObject(4 * VisCount(N)));

        // vertices, vertex layer data, indices
        int extraVerts = int.TryParse(Environment.GetEnvironmentVariable("VLOBBY_TEST_EXTRA_VERTS"), out int ev) ? ev : 0;
        vBase += extraVerts;
        iBase += int.TryParse(Environment.GetEnvironmentVariable("VLOBBY_TEST_EXTRA_INDICES"), out int ei) ? ei : 0;
        uint verts = ctx.NewObject((uint)(VertexSize * vBase), 4), vld = ctx.NewObject((uint)Math.Max(vldBase, 1), 4), idx = ctx.NewObject((uint)(2 * iBase), 2);
        foreach (var w in worlds)
        {
            uint wd = w.H + W_Draw;
            T.Write(verts + (uint)(VertexSize * w.VertexBase), T.ReadBytes(img.Pointer(wd + D_Vertices), VertexSize * w.Vertices));
            if (w.IsDonor) for (int i = 0; i < w.Vertices; i++) MovePoint(T, verts + (uint)(VertexSize * (w.VertexBase + i)), w);
            if (w.Vld > 0) T.Write(vld + (uint)w.VldBase, T.ReadBytes(img.Pointer(wd + D_Vld), w.Vld));
            T.Write(idx + (uint)(2 * w.IndexBase), T.ReadBytes(img.Pointer(wd + D_Indices), 2 * w.Indices));
        }
        if (Environment.GetEnvironmentVariable("VLOBBY_TEST_RELOCATE") == "1")
        {
            // test: the 40 stock surfaces nearest the urban stand use copies of their vertices placed past 65535
            var near = Enumerable.Range(0, N).Select(i => (i, d2: Dist2(T, bounds + (uint)(BoundsSize * i), 286, 334))).OrderBy(x => x.d2).Take(40).ToList();
            int extra = near.Sum(x => (int)T.U16(surfaces + (uint)(SurfSize * x.i) + 8));
            uint nv = ctx.NewObject((uint)(VertexSize * (vBase + Math.Max(65536 - vBase, 0) + extra)), 4);
            T.Write(nv, T.ReadBytes(verts, VertexSize * vBase));
            int at = Math.Max(vBase, 65536);
            foreach (var (i, _) in near)
            {
                uint sf = surfaces + (uint)(SurfSize * i);
                int fv = (int)T.U32(sf + 4), vc = T.U16(sf + 8);
                T.Write(nv + (uint)(VertexSize * at), T.ReadBytes(verts + (uint)(VertexSize * fv), VertexSize * vc));
                T.WU32(sf + 4, (uint)at);
                at += vc;
            }
            verts = nv; vBase = at;
            log($"test: 40 surfaces moved to vertices from 65536 ({vBase} vertices)");
        }
        img.SetPointer(draw + D_Vertices, verts);
        img.SetPointer(draw + D_Vld, vld);
        img.SetPointer(draw + D_Indices, idx);
        T.WU32(draw + D_VertexCount, (uint)vBase);
        T.WU32(draw + D_VldSize, (uint)vldBase);
        T.WU32(draw + D_IndexCount, (uint)iBase);

        // lightmaps and reflection probes
        uint lms = ctx.NewObject((uint)(8 * lmBase)), probes = ctx.NewObject((uint)(4 * probeBase)), origins = ctx.NewObject((uint)(12 * probeBase));
        foreach (var w in worlds)
        {
            uint wd = w.H + W_Draw;
            Copy(ctx, lms + (uint)(8 * w.LightmapBase), img.Pointer(wd + D_Lightmaps), (uint)(8 * w.Lightmaps));
            Copy(ctx, probes + (uint)(4 * w.ProbeBase), img.Pointer(wd + D_Probes), (uint)(4 * w.Probes));
            T.Write(origins + (uint)(12 * w.ProbeBase), T.ReadBytes(img.Pointer(wd + D_ProbeOrigins), 12 * w.Probes));
            if (w.IsDonor) for (int i = 0; i < w.Probes; i++) MovePoint(T, origins + (uint)(12 * (w.ProbeBase + i)), w);
        }
        img.SetPointer(draw + D_Lightmaps, lms);
        img.SetPointer(draw + D_LmPrimary, ctx.NewObject((uint)(4 * lmBase)));
        img.SetPointer(draw + D_LmSecondary, ctx.NewObject((uint)(4 * lmBase)));
        T.WU32(draw + D_LightmapCount, (uint)lmBase);
        img.SetPointer(draw + D_Probes, probes);
        img.SetPointer(draw + D_ProbeOrigins, origins);
        img.SetPointer(draw + D_ProbeTextures, ctx.NewObject((uint)(4 * probeBase)));
        T.WU32(draw + D_ProbeCount, (uint)probeBase);

        // material memory
        {
            var mmWorlds = Environment.GetEnvironmentVariable("VLOBBY_TEST_STOCK_MATMEM") == "1" ? worlds.Take(1).ToList() : worlds;
            int count = mmWorlds.Sum(w => (int)T.U32(w.H + W_MaterialMemoryCount));
            uint mm = ctx.NewObject((uint)(8 * count));
            int at = 0;
            foreach (var w in mmWorlds)
            {
                int c = (int)T.U32(w.H + W_MaterialMemoryCount);
                Copy(ctx, mm + (uint)(8 * at), img.Pointer(w.H + W_MaterialMemory), (uint)(8 * c));
                at += c;
            }
            img.SetPointer(h + W_MaterialMemory, mm);
            T.WU32(h + W_MaterialMemoryCount, (uint)count);
        }

        // skies: the stock sky, then each donor's
        {
            // only the stock sky: C1 draws the world's skies as one sky box (a second GfxSky crashed d3d9.dll while the
            // urban scene was drawn); a donor's sky surfaces stay ordinary surfaces of their scene
            var skyWorlds = worlds.Take(1).ToList();
            int count = skyWorlds.Sum(w => (int)T.U32(w.H + W_SkyCount));
            uint skies = ctx.NewObject((uint)(SkySize * count));
            int at = 0;
            foreach (var w in skyWorlds)
            {
                uint src = img.Pointer(w.H + W_Skies);
                for (int s = 0; s < (int)T.U32(w.H + W_SkyCount); s++, at++)
                {
                    uint d = skies + (uint)(SkySize * at), ss = src + (uint)(SkySize * s);
                    Copy(ctx, d, ss, SkySize);
                    int n = (int)T.U32(ss);
                    uint list = ctx.NewObject((uint)(4 * Math.Max(n, 1)));
                    for (int i = 0; i < n; i++) T.WU32(list + (uint)(4 * i), (uint)w.NewIndex[T.U32(img.Pointer(ss + 4) + (uint)(4 * i))]);
                    img.SetPointer(d + 4, n > 0 ? list : 0);
                }
            }
            img.SetPointer(h + W_Skies, skies);
            T.WU32(h + W_SkyCount, (uint)count);
        }

        // the stock lights' shadow surface lists name surfaces by index
        {
            int lights = (int)T.U32(a.H + W_PrimaryLightCount);
            uint sg = ctx.NewObject((uint)(12 * lights));
            Copy(ctx, sg, img.Pointer(a.H + W_ShadowGeom), (uint)(12 * lights));
            for (int l = 0; l < lights; l++)
            {
                uint g = sg + (uint)(12 * l);
                int n = T.U16(g);
                if (n == 0) continue;
                uint list = ctx.NewObject((uint)(2 * n), 2);
                uint src = img.Pointer(g + 4);
                for (int i = 0; i < n; i++) T.WU16(list + (uint)(2 * i), (ushort)a.NewIndex[T.U16(src + (uint)(2 * i))]);
                img.SetPointer(g + 4, list);
            }
            img.SetPointer(h + W_ShadowGeom, sg);
        }

        // brush models: model 0 is the whole world
        {
            int count = (int)T.U32(a.H + W_ModelCount);
            uint models = ctx.NewObject((uint)(60 * count));
            Copy(ctx, models, img.Pointer(a.H + W_Models), (uint)(60 * count));
            // GfxBrushModel (60 bytes): writable bounds, bounds, radius, u16 surfaceCount +52, startSurfIndex +54,
            // surfaceCountNoDecal +56
            T.WU16(models + 52, (ushort)N);
            T.WU16(models + 54, 0);
            T.WU16(models + 56, (ushort)N);
            for (int i = 1; i < count; i++)
            {
                uint b = models + (uint)(60 * i);
                int n = T.U16(b + 52), s = T.U16(b + 54);
                if (n > 0) T.WU16(b + 54, (ushort)a.NewIndex[s]);
            }
            img.SetPointer(h + W_Models, models);
        }

        BuildCell(ctx, h, worlds, N, log);
        Bounds(T, h + W_Bounds, worlds);
        if (Environment.GetEnvironmentVariable("VLOBBY_TEST_STOCK_GRID") != "1")
        LightGridMerge.Merge(ctx, h + W_Grid, worlds.Select(w => (w.H + W_Grid, w.IsDonor, w.Dx, w.Dy, w.Dz, w.StockGrid)).ToList(), log);
        log($"merged world: {N} surfaces, {M} static models, {vBase} vertices, {iBase} indices, {lmBase} lightmaps, {probeBase} probes");
        return h;
    }

    static void BuildCell(PortContext ctx, uint h, List<World> worlds, int N, Action<string> log)
    {
        var T = ctx.T; var img = ctx.Target;
        var a = worlds[0];
        if (a.Cells != 1) throw new NotSupportedException("the stock lobby world is expected to have one cell");
        // the new root's children: the stock root, then one node per donor whose children are the donor's cell roots
        var donors = worlds.Skip(1).ToList();
        int stockNodes = (int)T.U32(img.Pointer(a.H + W_TreeCounts));
        int total = 1 + stockNodes + donors.Sum(d => 1 + Enumerable.Range(0, d.Cells).Sum(c => (int)T.U32(img.Pointer(d.H + W_TreeCounts) + (uint)(4 * c))));
        uint nodes = ctx.NewObject((uint)(NodeSize * total), 128);
        // layout: [root][stock root][donor heads...][stock rest][donor 0: cell roots, cell 0 rest, cell 1 rest ...]...
        int pos = 0;
        uint Node(int i) => nodes + (uint)(NodeSize * i);
        int root = pos++;
        int stockRoot = pos++;
        var heads = donors.Select(_ => pos++).ToList();
        int stockRest = pos; pos += stockNodes - 1;
        uint stockTree = img.Pointer(img.Pointer(a.H + W_Trees));
        CopyNodes(ctx, Node(stockRoot), stockTree, 1, a, 0, N);
        CopyNodes(ctx, Node(stockRest), stockTree + NodeSize, stockNodes - 1, a, 0, N);
        FixChildren(T, Node(stockRoot), stockTree, Node(stockRest) - NodeSize);
        int surfFirstHalf = a.Surfaces, surfSecondHalf = N + a.Surfaces;
        var probeSet = new SortedSet<int>();
        uint cellA = img.Pointer(a.H + W_Cells);
        for (int k = 0; k < T.U8(cellA + 32); k++) probeSet.Add(T.U8(img.Pointer(cellA + 36) + (uint)k));
        for (int di = 0; di < donors.Count; di++)
        {
            var d = donors[di];
            int cellRoots = pos; pos += d.Cells;
            uint head = Node(heads[di]);
            // head: covers the donor's whole range of the sorted lists
            T.WU16(head + 24, (ushort)d.Cells);
            T.WU16(head + 26, (ushort)d.Surfaces);
            T.WU16(head + 28, (ushort)surfFirstHalf);
            T.WU16(head + 30, (ushort)d.Surfaces);
            T.WU16(head + 32, (ushort)surfSecondHalf);
            T.WU16(head + 34, 0);
            img.SetPointer(head + 36, 0);
            T.WI32(head + 40, (int)(Node(cellRoots) - head));
            uint dcells = img.Pointer(d.H + W_Cells);
            var boxes = new List<(float[] mn, float[] mx)>();
            for (int c = 0; c < d.Cells; c++)
            {
                uint tree = img.Pointer(img.Pointer(d.H + W_Trees) + (uint)(4 * c));
                int count = (int)T.U32(img.Pointer(d.H + W_TreeCounts) + (uint)(4 * c));
                int rest = pos; pos += count - 1;
                CopyNodes(ctx, Node(cellRoots + c), tree, 1, d, 0, N, surfFirstHalf, surfSecondHalf);
                CopyNodes(ctx, Node(rest), tree + NodeSize, count - 1, d, 0, N, surfFirstHalf, surfSecondHalf);
                FixChildren(T, Node(cellRoots + c), tree, Node(rest) - NodeSize);
                boxes.Add(Box(T, Node(cellRoots + c)));
                uint cell = dcells + (uint)(CellSize * c);
                for (int k = 0; !d.NoProbes && k < T.U8(cell + 32); k++) probeSet.Add(d.ProbeBase + T.U8(img.Pointer(cell + 36) + (uint)k));
            }
            SetBox(T, head, Union(boxes));
            surfFirstHalf += d.Surfaces;
            surfSecondHalf += d.Surfaces;
        }
        if (pos != total) throw new InvalidOperationException($"tree layout {pos} != {total}");
        // root: the stock root and the donor heads
        Copy(ctx, Node(root), Node(stockRoot), NodeSize);
        T.WU16(Node(root) + 24, (ushort)(Environment.GetEnvironmentVariable("VLOBBY_TEST_NO_DONOR_TREE") == "1" ? 1 : 1 + donors.Count));
        T.WU16(Node(root) + 26, (ushort)N);
        T.WU16(Node(root) + 28, 0);
        T.WU16(Node(root) + 30, (ushort)N);
        T.WU16(Node(root) + 32, (ushort)N);
        T.WU16(Node(root) + 34, 0);
        img.SetPointer(Node(root) + 36, 0);
        T.WI32(Node(root) + 40, NodeSize);
        SetBox(T, Node(root), Union(Enumerable.Range(0, 1 + donors.Count).Select(i => Box(T, Node(1 + i))).ToList()));
        // the stock root kept its static models; it stays a child, so nothing moves up

        uint counts = ctx.NewObject(4);
        T.WU32(counts, (uint)total);
        uint trees = ctx.NewObject(4, 128);
        img.SetPointer(trees, nodes);
        img.SetPointer(h + W_TreeCounts, counts);
        img.SetPointer(h + W_Trees, trees);

        uint cells = ctx.NewObject(CellSize);
        Copy(ctx, cells, cellA, CellSize);
        SetBox(T, cells, Union([Box(T, cells), Box(T, Node(root))]));
        uint plist = ctx.NewObject((uint)Math.Max(probeSet.Count, 1), 1);
        int pi = 0;
        foreach (int p in probeSet) T.WU8(plist + (uint)pi++, (byte)p);
        T.WU8(cells + 32, (byte)probeSet.Count);
        img.SetPointer(cells + 36, plist);
        img.SetPointer(h + W_Cells, cells);
        log($"cell tree: {total} nodes, {probeSet.Count} reflection probes in the cell");
    }

    /// copies `count` tree nodes; surface ranges move to the merged sorted lists, static model indices by the world's base
    static void CopyNodes(PortContext ctx, uint dst, uint src, int count, World w, int _, int N, int firstHalf = 0, int secondHalf = -1)
    {
        var T = ctx.T; var img = ctx.Target;
        if (secondHalf < 0) secondHalf = N;
        for (int i = 0; i < count; i++)
        {
            uint d = dst + (uint)(NodeSize * i), s = src + (uint)(NodeSize * i);
            Copy(ctx, d, s, NodeSize);
            int start = T.U16(s + 28), startNd = T.U16(s + 32);
            T.WU16(d + 28, (ushort)(firstHalf + start));
            T.WU16(d + 32, (ushort)(secondHalf + (startNd - w.Surfaces)));
            if (w.IsDonor) MoveBounds(T, d, w);
            int n = T.U16(s + 34);
            uint list = img.Pointer(s + 36);
            if (n > 0 && list != 0)
            {
                var kept = Enumerable.Range(0, n).Select(k => w.SmodelMap[T.U16(list + (uint)(2 * k))]).Where(x => x >= 0).ToList();
                uint nl = ctx.NewObject((uint)(2 * Math.Max(kept.Count, 1)), 2);
                for (int k = 0; k < kept.Count; k++) T.WU16(nl + (uint)(2 * k), (ushort)kept[k]);
                T.WU16(d + 34, (ushort)kept.Count);
                img.SetPointer(d + 36, kept.Count > 0 ? nl : 0);
            }
        }
    }

    /// a node moved away from the rest of its tree: its children offset now leads from its new place to the rest
    static void FixChildren(VirtualMemory T, uint movedRoot, uint oldRoot, uint restBaseMinusOne)
    {
        int off = T.I32(oldRoot + 40);
        if (T.U16(oldRoot + 24) == 0) return;
        uint oldChild = oldRoot + (uint)off;
        uint newChild = restBaseMinusOne + (oldChild - oldRoot);
        T.WI32(movedRoot + 40, (int)(newChild - movedRoot));
    }

    static void Copy(PortContext ctx, uint dst, uint src, uint len)
    {
        if (len == 0) return;
        ctx.T.Write(dst, ctx.T.ReadBytes(src, (int)len));
        ctx.Target.CopyTags(dst, src, len);
        foreach (uint f in ctx.Target.Pointers.Enumerate(dst, len).ToList()) ctx.Target.SetEncoding(f, PointerEncoding.Unknown);
    }

    static float Dist2(VirtualMemory T, uint b, float x, float y)
    {
        float dx = T.F32(b) - x, dy = T.F32(b + 4) - y;
        return dx * dx + dy * dy;
    }

    static bool Inside(VirtualMemory T, uint b, float[] k)
    {
        for (int i = 0; i < 3; i++)
        {
            float c = T.F32(b + (uint)(4 * i)), e = T.F32(b + 12 + (uint)(4 * i));
            if (c - e < k[i] || c + e > k[3 + i]) return false;
        }
        return true;
    }

    static void MovePoint(VirtualMemory T, uint p, World w)
    {
        WF(T, p, T.F32(p) + w.Dx); WF(T, p + 4, T.F32(p + 4) + w.Dy); WF(T, p + 8, T.F32(p + 8) + w.Dz);
    }

    static void MoveBounds(VirtualMemory T, uint b, World w) => MovePoint(T, b, w);   // Bounds { midPoint, halfSize }

    static void WF(VirtualMemory T, uint p, float v) => T.WI32(p, BitConverter.SingleToInt32Bits(v));

    static (float[] mn, float[] mx) Box(VirtualMemory T, uint b)
    {
        var mn = new float[3]; var mx = new float[3];
        for (int i = 0; i < 3; i++)
        {
            float c = T.F32(b + (uint)(4 * i)), e = T.F32(b + 12 + (uint)(4 * i));
            mn[i] = c - e; mx[i] = c + e;
        }
        return (mn, mx);
    }

    static (float[] mn, float[] mx) Union(List<(float[] mn, float[] mx)> boxes)
    {
        var mn = new float[] { float.MaxValue, float.MaxValue, float.MaxValue };
        var mx = new float[] { float.MinValue, float.MinValue, float.MinValue };
        foreach (var (a, b) in boxes)
            for (int i = 0; i < 3; i++) { mn[i] = Math.Min(mn[i], a[i]); mx[i] = Math.Max(mx[i], b[i]); }
        return (mn, mx);
    }

    static void SetBox(VirtualMemory T, uint b, (float[] mn, float[] mx) box)
    {
        for (int i = 0; i < 3; i++)
        {
            WF(T, b + (uint)(4 * i), (box.mn[i] + box.mx[i]) / 2);
            WF(T, b + 12 + (uint)(4 * i), (box.mx[i] - box.mn[i]) / 2);
        }
    }

    static void Bounds(VirtualMemory T, uint b, List<World> worlds)
    {
        var boxes = new List<(float[] mn, float[] mx)>();
        foreach (var w in worlds)
        {
            var bx = Box(T, w.H + W_Bounds);
            if (w.IsDonor)
                for (int i = 0; i < 3; i++) { float d = i == 0 ? w.Dx : i == 1 ? w.Dy : w.Dz; bx.mn[i] += d; bx.mx[i] += d; }
            boxes.Add(bx);
        }
        SetBox(T, b, Union(boxes));
    }

    /// the donor's entities (all but worldspawn), moved, their targetname / target keys prefixed
    /// donor prefixes whose room character slots are appended too (Program: slots=<prefix>)
    public static HashSet<string> SlotPrefixes { get; } = [];

    static int AppendEntities(StringBuilder sb, string ents, Donor d, HashSet<string>? wanted)
    {
        int added = 0;
        int i = 0;
        var seen = new Dictionary<string, int>();
        while ((i = ents.IndexOf('{', i)) >= 0)
        {
            int e = ents.IndexOf('}', i);
            if (e < 0) break;
            string block = ents.Substring(i + 1, e - i - 1);
            i = e + 1;
            var lines = block.Split('\n', StringSplitOptions.RemoveEmptyEntries);
            var kv = new List<(string k, string v)>();
            foreach (var line in lines)
            {
                var parts = line.Trim().Split('"');
                if (parts.Length >= 5) kv.Add((parts[1], parts[3]));
            }
            if (kv.Any(p => p.k == "classname" && p.v == "worldspawn")) continue;
            if (d.Keep is { } kb && kv.FirstOrDefault(p => p.k == "origin").v is { } org)
            {
                var o = org.Split(' ').Select(x => float.Parse(x, System.Globalization.CultureInfo.InvariantCulture)).ToArray();
                if (o.Length == 3 && Enumerable.Range(0, 3).Any(a => o[a] < kb[a] || o[a] > kb[3 + a])) continue;
            }
            var outb = new StringBuilder("{\n");
            foreach (var (k, v0) in kv)
            {
                // a stand's preview model and lineup pose (3.x: model cha_quartermaster, animation
                // menu_lobby_slot1_rifle_idle, script_noteworthy lobby_slot_01): 1.1.1.13's character stands carry none
                // (C1 poses the player from mp/uivieweranimposes.csv), and the later builds' models and lineup poses
                // are not in 1.1.1.13
                if (k is "model" or "animation" or "script_noteworthy") continue;
                string v = v0;
                if (k == "origin")
                {
                    var f = v.Split(' ').Select(x => float.Parse(x, System.Globalization.CultureInfo.InvariantCulture)).ToArray();
                    v = string.Join(" ", new[] { f[0] + d.Dx, f[1] + d.Dy, f[2] + d.Dz }.Select(x => x.ToString("0.###", System.Globalization.CultureInfo.InvariantCulture)));
                }
                else if (k == "targetname")
                {
                    // a name the donor gives twice (3.x: s1_place1 in the team lobby and at the supply depot): the
                    // later ones get _2, _3.. so that each stand can be named
                    int n = seen[v] = seen.GetValueOrDefault(v) + 1;
                    v = d.Prefix + v + (n > 1 ? "_" + n : "");
                }
                else if (k == "target") v = d.Prefix + v;
                outb.Append('"').Append(k).Append("\" \"").Append(v).Append("\"\n");
            }
            outb.Append("}\n");
            var mt = System.Text.RegularExpressions.Regex.Match(outb.ToString(), "\"targetname\" \"([^\"]+)\"");
            // a room's character slots (mp/uiViewerScenes.csv: s<scene>_place<1..9> and their _target): kept for the
            // donors named by slots= (0.0.2.2, whose rooms Kernel33 can show: vlobby_env.c VlFindEntHook)
            bool slot = mt.Success && SlotPrefixes.Contains(d.Prefix)
                && System.Text.RegularExpressions.Regex.IsMatch(mt.Groups[1].Value[d.Prefix.Length..], "^s[0-9]+_place[1-9](_target)?$");
            if (wanted is not null && !slot && (!mt.Success || !wanted.Contains(mt.Groups[1].Value))) continue;
            sb.Append(outb);
            added++;
        }
        return added;
    }
}
