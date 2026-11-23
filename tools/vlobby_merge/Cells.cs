using System.Globalization;
using System.Text;
using Codol.FastFiles.Porting;

namespace VlobbyMerge;

/// A donor world's cells, read from the donor zone as it was built (before Compact makes it one cell): which surfaces,
/// static models and reflection probes a set of kept cells holds, and the probe the donor build itself gives a point.
///
/// Reflection probes. A surface and a static model name their probe (GfxSurface +21, GfxStaticModelDrawInst +60); a
/// scene entity (the lobby character and its weapon) has none: C1 takes the probe from the cell the entity's origin is
/// in (the dpvs BSP), the cell's probe list (GfxCell +32 count, +36 indices), the one of those whose origin is nearest.
/// Probe 0 is the map compiler's default cube (at 0,0,0, the same pixels in every build) and no cell lists it. The merged
/// world is one cell, so its list is every listed probe of every scene, and a character takes the nearest of all of
/// them: SelectProbes checks each menu scene's stand against the probe its own build gives it.
public sealed class CellInfo
{
    /// the donor's surfaces (its own indices) the kept cells hold; null: every surface
    public HashSet<int>? Surfaces;
    /// the donor's static models the kept cells' trees name; null: every static model
    public HashSet<int>? Smodels;
    /// donor probe -> its index among the donor's kept probes, -1 when left out
    public int[] ProbeMap = [];
    public int KeptProbes;
    /// the donor probes (its own indices) the kept cells list
    public SortedSet<int> CellProbes = [];
    public string Note = "";
}

public static class Cells
{
    const uint W_CellCount = 0x34, W_Planes = 0x38, W_Nodes = 0x3C, W_TreeCounts = 0x44, W_Trees = 0x48, W_Cells = 0x4C, W_Draw = 0x50, W_Dpvs = 0x1C8;
    const uint P_SmodelCount = 0, P_StaticSurfaceCount = 4, P_Sorted = 76, P_Surfaces = 84, P_DrawInsts = 92;
    const int NodeSize = 44, CellSize = 40, SurfSize = 24, DrawInstSize = 76;

    public static CellInfo Analyze(LoadedZone z, int[]? keepCells)
    {
        var m = z.M; var I = z.Image;
        uint w = z.Find(AssetTypes.GfxWorld), dp = w + W_Dpvs;
        int cells = (int)m.U32(w + W_CellCount), surfaces = (int)m.U32(dp + P_StaticSurfaceCount), smodels = (int)m.U32(dp + P_SmodelCount);
        int probes = (int)m.U32(w + W_Draw);
        uint cellArr = I.Pointer(w + W_Cells), sorted = I.Pointer(dp + P_Sorted);
        var info = new CellInfo();
        var keep = keepCells is null ? Enumerable.Range(0, cells).ToHashSet() : keepCells.ToHashSet();
        foreach (int c in keep)
            if (c < 0 || c >= cells) throw new InvalidDataException($"the world has {cells} cells; no cell {c}");
        for (int c = 0; c < cells; c++)
        {
            if (!keep.Contains(c)) continue;
            uint cell = cellArr + (uint)(CellSize * c);
            uint list = I.Pointer(cell + 36);
            for (int k = 0; k < m.U8(cell + 32); k++) info.CellProbes.Add(m.U8(list + (uint)k));
        }
        if (keepCells is null)
        {
            info.ProbeMap = Enumerable.Range(0, probes).ToArray();
            info.KeptProbes = probes;
            return info;
        }
        info.Surfaces = [];
        info.Smodels = [];
        foreach (int c in keep)
        {
            uint tree = I.Pointer(I.Pointer(w + W_Trees) + (uint)(4 * c));
            int count = (int)m.U32(I.Pointer(w + W_TreeCounts) + (uint)(4 * c));
            // the cell tree's root covers every surface of the cell: a range of the sorted surface list
            int start = m.U16(tree + 28), n = m.U16(tree + 26);
            for (int i = 0; i < n; i++) info.Surfaces.Add(m.U16(sorted + (uint)(2 * (start + i))));
            for (int i = 0; i < count; i++)
            {
                uint node = tree + (uint)(NodeSize * i);
                int sn = m.U16(node + 34);
                uint sl = I.Pointer(node + 36);
                for (int k = 0; sl != 0 && k < sn; k++) info.Smodels.Add(m.U16(sl + (uint)(2 * k)));
            }
        }
        // the probes the kept cells list, and the ones the kept surfaces and static models name
        var used = new SortedSet<int>(info.CellProbes);
        uint surfs = I.Pointer(dp + P_Surfaces), di = I.Pointer(dp + P_DrawInsts);
        foreach (int s in info.Surfaces) used.Add(m.U8(surfs + (uint)(SurfSize * s) + 21));
        foreach (int s in info.Smodels) used.Add(m.U8(di + (uint)(DrawInstSize * s) + 60));
        info.ProbeMap = Enumerable.Repeat(-1, probes).ToArray();
        foreach (int p in used) if (p < probes) info.ProbeMap[p] = info.KeptProbes++;
        info.Note = $"cells {string.Join(",", keep.Order())} of {cells}: {info.Surfaces.Count} of {surfaces} surfaces, {info.Smodels.Count} of {smodels} static models, probes {string.Join(",", used)} of {probes}";
        return info;
    }

    /// the copied (compacted) donor world takes only the kept probes: the probe arrays, every surface's and static model's
    /// probe index, and the cell list (the kept cells' lists, which never name probe 0)
    public static void ApplyProbes(PortContext ctx, uint h, CellInfo info)
    {
        var T = ctx.T; var img = ctx.Target;
        uint draw = h + W_Draw, dp = h + W_Dpvs;
        int probes = (int)T.U32(draw);
        if (probes != info.ProbeMap.Length) throw new InvalidDataException($"the copied world has {probes} probes, the donor {info.ProbeMap.Length}");
        if (info.KeptProbes != probes)
        {
            uint arr = img.Pointer(draw + 4), org = img.Pointer(draw + 8);
            uint na = ctx.NewObject((uint)(4 * Math.Max(info.KeptProbes, 1))), no = ctx.NewObject((uint)(12 * Math.Max(info.KeptProbes, 1)));
            for (int i = 0; i < probes; i++)
            {
                int j = info.ProbeMap[i];
                if (j < 0) continue;
                img.SetPointer(na + (uint)(4 * j), img.Pointer(arr + (uint)(4 * i)));
                T.Write(no + (uint)(12 * j), T.ReadBytes(org + (uint)(12 * i), 12));
            }
            img.SetPointer(draw + 4, na);
            img.SetPointer(draw + 8, no);
            img.SetPointer(draw + 0xC, ctx.NewObject((uint)(4 * Math.Max(info.KeptProbes, 1))));
            T.WU32(draw, (uint)info.KeptProbes);
            int n = (int)T.U32(dp + P_StaticSurfaceCount), sm = (int)T.U32(dp + P_SmodelCount);
            uint surfs = img.Pointer(dp + P_Surfaces), di = img.Pointer(dp + P_DrawInsts);
            for (int i = 0; i < n; i++)
            {
                uint s = surfs + (uint)(SurfSize * i) + 21;
                int j = info.ProbeMap[T.U8(s)];
                if (j < 0) throw new InvalidDataException($"a kept surface names probe {T.U8(s)}, which is left out");
                T.WU8(s, (byte)j);
            }
            for (int i = 0; i < sm; i++)
            {
                uint s = di + (uint)(DrawInstSize * i) + 60;
                int j = info.ProbeMap[T.U8(s)];
                // a static model of a left-out cell names a probe that is left out; it is dropped from the merge
                T.WU8(s, (byte)Math.Max(j, 0));
            }
        }
        int cells = (int)T.U32(h + W_CellCount);
        uint cellArr = img.Pointer(h + W_Cells);
        if (cells != 1) throw new NotSupportedException("the donor world is expected to be compacted into one cell");
        var list = info.CellProbes.Select(p => info.ProbeMap[p]).Where(p => p >= 0).Order().ToList();
        uint pl = ctx.NewObject((uint)Math.Max(list.Count, 1), 1);
        for (int k = 0; k < list.Count; k++) T.WU8(pl + (uint)k, (byte)list[k]);
        T.WU8(cellArr + 32, (byte)list.Count);
        img.SetPointer(cellArr + 36, list.Count > 0 ? pl : 0);
    }

    /// the dpvs cell of a point (C1's walk: the front side is the next node, the back side the node the offset leads to),
    /// -1 outside every cell
    public static int CellOf(LoadedZone z, float[] p)
    {
        var m = z.M; var I = z.Image;
        uint w = z.Find(AssetTypes.GfxWorld);
        int cellCount = (int)m.U32(w + W_CellCount);
        uint planes = I.Pointer(w + W_Planes), node = I.Pointer(w + W_Nodes);
        for (int guard = 0; guard < 100000; guard++)
        {
            int v = m.U16(node);
            if (v < cellCount + 1) return v - 1;
            uint pl = planes + (uint)(20 * (v - cellCount - 1));
            float d = p[0] * m.F32(pl) + p[1] * m.F32(pl + 4) + p[2] * m.F32(pl + 8) - m.F32(pl + 12);
            node = d > 0 ? node + 4 : node + 2u * m.U16(node + 2);
        }
        return -1;
    }

    /// the probe a scene entity at p gets in the zone's own world: the nearest of its cell's list (0 outside every cell)
    public static (int Probe, int Cell) SourceProbe(LoadedZone z, float[] p)
    {
        var m = z.M; var I = z.Image;
        uint w = z.Find(AssetTypes.GfxWorld);
        int c = CellOf(z, p);
        if (c < 0) return (0, c);
        uint cell = I.Pointer(w + W_Cells) + (uint)(CellSize * c), list = I.Pointer(cell + 36);
        var probes = Enumerable.Range(0, m.U8(cell + 32)).Select(k => (int)m.U8(list + (uint)k)).ToList();
        uint org = I.Pointer(w + W_Draw + 8);
        return (Nearest(probes, i => Origin(m, org, i), p), c);
    }

    static float[] Origin(Codol.FastFiles.Emulation.VirtualMemory m, uint org, int i) =>
        [m.F32(org + (uint)(12 * i)), m.F32(org + (uint)(12 * i) + 4), m.F32(org + (uint)(12 * i) + 8)];

    /// C1's choice among a cell's probes: the nearest origin, the first of equals
    public static int Nearest(IReadOnlyList<int> probes, Func<int, float[]> origin, float[] p)
    {
        int best = 0; float bd = float.MaxValue;
        foreach (int i in probes)
        {
            float[] o = origin(i);
            float dx = o[0] - p[0], dy = o[1] - p[1], dz = o[2] - p[2], d = dx * dx + dy * dy + dz * dz;
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    }

    /// One merged world, one cell: the character takes the nearest of every scene's listed probes, which in a donor
    /// scene can be another scene's (2.6.9.7's lobby stand is nearer the team room's probe than its own, which hangs
    /// 2200 units above it). For every menu scene of the camera table (character_loadout_*) whose stand is in a donor,
    /// the probe that donor's own world gives the stand is found (its cell, the nearest of the cell's list); when the
    /// merged cell would give another, that probe's origin moves to the stand. A probe's origin only chooses it (the
    /// surfaces and static models name theirs by index), so nothing else changes.
    public static void SelectProbes(PortContext ctx, uint merged, IReadOnlyList<(string Prefix, LoadedZone Zone, float[] D, int ProbeBase, int[] ProbeMap)> worlds,
        string entText, string tableCsv, Action<string> log)
    {
        var T = ctx.T; var img = ctx.Target;
        uint draw = merged + W_Draw;
        int probeCount = (int)T.U32(draw);
        uint org = img.Pointer(draw + 8);
        uint cell = img.Pointer(merged + W_Cells), pl = img.Pointer(cell + 36);
        var list = Enumerable.Range(0, T.U8(cell + 32)).Select(k => (int)T.U8(pl + (uint)k)).ToList();
        float[] O(int i) => [T.F32(org + (uint)(12 * i)), T.F32(org + (uint)(12 * i) + 4), T.F32(org + (uint)(12 * i) + 8)];
        var ents = Framing.Parse(entText);
        var rows = File.ReadAllLines(tableCsv, Encoding.Latin1).Select(l => l.Split(','))
            .Where(r => r.Length > 3 && r[0].StartsWith("character_loadout_", StringComparison.OrdinalIgnoreCase)).ToList();
        var want = new List<(string Row, float[] P, int Probe, string Src)>();
        foreach (var r in rows)
        {
            string env = r[0]["character_loadout_".Length..];
            var wd = worlds.Skip(1).FirstOrDefault(x => env.StartsWith(x.Prefix, StringComparison.OrdinalIgnoreCase));
            if (wd.Zone is null) continue;   // a stock scene: its probes are the stock world's
            if (!ents.TryGetValue(r[2], out var e) || !e.TryGetValue("origin", out string? os)) { log($"probes: {r[0]}: no stand {r[2]}"); continue; }
            float[] p = os.Split(' ', StringSplitOptions.RemoveEmptyEntries).Select(s => float.Parse(s, CultureInfo.InvariantCulture)).ToArray();
            float[] q = [p[0] - wd.D[0], p[1] - wd.D[1], p[2] - wd.D[2]];
            var (src, c) = SourceProbe(wd.Zone, q);
            int mapped = src < wd.ProbeMap.Length ? wd.ProbeMap[src] : -1;
            if (mapped < 0) { log($"probes: {r[0]}: its build gives probe {src}, which the merge left out"); continue; }
            want.Add((r[0], p, wd.ProbeBase + mapped, $"cell {c} probe {src}"));
        }
        var moved = new Dictionary<int, string>();
        foreach (var (row, p, probe, src) in want)
        {
            int got = Nearest(list, O, p);
            if (got == probe) continue;
            if (!list.Contains(probe)) { log($"probes: {row}: probe {probe} is not in the merged cell's list"); continue; }
            if (moved.TryGetValue(probe, out string? by)) { log($"probes: {row}: probe {probe} already moved to {by}'s stand"); continue; }
            float[] was = O(probe);
            for (int a = 0; a < 3; a++) T.WI32(org + (uint)(12 * probe + 4 * a), BitConverter.SingleToInt32Bits(p[a]));
            moved[probe] = row;
            log($"probes: {row}: the merged cell gave probe {got}; probe {probe}'s origin {F(was)} moved to the stand {F(p)}");
        }
        foreach (var (row, p, probe, src) in want)
        {
            int got = Nearest(list, O, p);
            log($"probes: {row} stand {F(p)}: probe {got} ({(got == probe ? "as" : "NOT as")} its build: {src} = merged {probe})");
        }
        if (probeCount < list.DefaultIfEmpty(0).Max()) throw new InvalidDataException("the merged cell lists a probe past the probe array");
    }

    static string F(float[] v) => string.Join(" ", v.Select(x => x.ToString("0.#", CultureInfo.InvariantCulture)));
}
