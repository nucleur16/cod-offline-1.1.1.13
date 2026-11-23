using Codol.FastFiles.Porting;

namespace VlobbyMerge;

/// Before a donor world is merged its surfaces are joined: in each dpvs range, every surface with the same material,
/// lightmap and flags becomes one surface (only the vertices its triangles use copied, the indices re-based), split
/// where a surface would pass VLOBBY_COMPACT_MAXV vertices or VLOBBY_COMPACT_MAXT triangles (65535 each by default).
/// A surface with vertex layer data keeps its vertices in their order (the layer data follows them). The donor's cell
/// trees go with it: the compacted donor is one cell whose tree is a single leaf holding all its surfaces and static
/// models (a lobby scene is small; culling it as one box costs nothing that matters). This keeps the merged world near
/// the stock lobby's surface count (2006 surfaces with three donors against 3351 joined as they are) and its draw
/// calls few. (An earlier reading that C1 draws no more than 2048 world surfaces came from views that never reached the
/// donor scenes: C1 refused their stand and camera x, see src/vlobby_env.c VlWidenDomains; the check stays as a
/// conservative bound.)
public static class Compact
{
    const uint W_CellCount = 0x34, W_TreeCounts = 0x44, W_Trees = 0x48, W_Cells = 0x4C, W_Draw = 0x50, W_SurfaceCount = 0x10, W_Dpvs = 0x1C8;
    const uint D_VertexCount = 0x28, D_Vertices = 0x2C, D_IndexCount = 0x40, D_Indices = 0x44, D_ProbeCount = 0;
    const uint P_SmodelCount = 0, P_StaticSurfaceCount = 4, P_NoDecal = 8, P_Ranges = 12, P_Sorted = 76, P_Surfaces = 84, P_SurfBounds = 88;
    const int SurfSize = 24, VertexSize = 44, NodeSize = 44, CellSize = 40;

    static readonly int MaxVerts = int.TryParse(Environment.GetEnvironmentVariable("VLOBBY_COMPACT_MAXV"), out int mv) ? mv : 0xFFFF;
    static readonly int MaxTris = int.TryParse(Environment.GetEnvironmentVariable("VLOBBY_COMPACT_MAXT"), out int mt) ? mt : 0xFFFF;
    static readonly bool KeepLayered = Environment.GetEnvironmentVariable("VLOBBY_COMPACT_JOIN_LAYERED") != "1";

    public static void World(PortContext ctx, uint h, Action<string> log, string tag, float[]? keep, HashSet<int>? keepSurfaces = null)
    {
        var T = ctx.T; var img = ctx.Target;
        uint dp = h + W_Dpvs, draw = h + W_Draw;
        int n = (int)T.U32(dp + P_StaticSurfaceCount);
        uint surfs = img.Pointer(dp + P_Surfaces), bnds = img.Pointer(dp + P_SurfBounds);
        uint verts = img.Pointer(draw + D_Vertices), idx = img.Pointer(draw + D_Indices);
        uint totalVerts = T.U32(draw + D_VertexCount), totalIdx = T.U32(draw + D_IndexCount);
        int bad = 0, layered = 0, maxV = 0, maxT = 0;
        var outSurf = new List<(byte[] Surf, uint Material, int Cat, List<int> Members)>();
        var newVerts = new List<byte>();
        var newIdx = new List<byte>();
        var ranges = new int[4, 2];
        var boxes = new List<(float[] mn, float[] mx)>();
        int at = 0;
        for (int k = 0; k < 4; k++)
        {
            int begin = (int)T.U32(dp + P_Ranges + (uint)(8 * k)), end = (int)T.U32(dp + P_Ranges + (uint)(8 * k) + 4);
            ranges[k, 0] = outSurf.Count;
            // group by material, lightmap, probe, primary light, flags (first appearance order)
            var groups = new List<(string Key, List<int> Members)>();
            var byKey = new Dictionary<string, int>();
            for (int i = begin; i < end; i++)
            {
                uint s = surfs + (uint)(SurfSize * i);
                if (T.U16(s + 10) == 0) continue;
                if (keepSurfaces is not null && !keepSurfaces.Contains(i)) continue;   // a cell left out (Cells.cs)
                if (T.U32(s + 4) + T.U16(s + 8) > totalVerts || T.U32(s + 12) + 3u * T.U16(s + 10) > totalIdx) { bad++; continue; }
                if (keep is not null)
                {
                    // the keep box (the donor's own coordinates): a 0.0.2.2 sky shell around the whole map stays out
                    uint b0 = bnds + (uint)(24 * i);
                    bool inside = true;
                    for (int a = 0; a < 3; a++)
                    {
                        float c = T.F32(b0 + (uint)(4 * a)), e = T.F32(b0 + 12 + (uint)(4 * a));
                        if (c - e < keep[a] || c + e > keep[3 + a]) inside = false;
                    }
                    if (!inside) continue;
                }
                // material, lightmap, reflection probe and flags (the merge clears the primary light). The probe is part
                // of the key: a donor world holds several scenes (3.19.26.22 its lobby, team rooms, depot ..), and joined
                // across them a surface took its first member's probe, another room's or the default cube (probe 0)
                string key = $"{img.Pointer(s + 16):X8}/{T.U8(s + 20)}/{T.U8(s + 21)}/{T.U8(s + 23)}";
                if (T.U32(s) != 0 && KeepLayered) key += $"/vld{i}";
                if (!byKey.TryGetValue(key, out int g)) { byKey[key] = g = groups.Count; groups.Add((key, [])); }
                groups[g].Members.Add(i);
            }
            foreach (var (_, members) in groups)
            {
                int gi = 0;
                while (gi < members.Count)
                {
                    // one merged surface: as many members as fit in 65535 vertices and triangles; only the vertices
                    // the members' triangles use are copied (a surface's vertex range can span its neighbours')
                    var take = new List<int>();
                    var local = new Dictionary<int, int>();
                    var tris = new List<int>();
                    while (gi < members.Count)
                    {
                        uint s = surfs + (uint)(SurfSize * members[gi]);
                        int fv = (int)T.U32(s + 4), t = T.U16(s + 10);
                        uint bi = T.U32(s + 12);
                        var used = new HashSet<int>();
                        for (int j = 0; j < 3 * t; j++) used.Add(fv + T.U16(idx + 2 * (uint)(bi + j)));
                        int fresh = used.Count(u => !local.ContainsKey(u));
                        if (take.Count > 0 && (local.Count + fresh > MaxVerts || tris.Count / 3 + t > MaxTris)) break;
                        if (T.U32(s) != 0 && KeepLayered)
                        {
                            // vertex layer data is addressed by the surface's own vertex order: its vertices go as they are
                            for (int v = 0; v < T.U16(s + 8); v++) local[fv + v] = v;
                            for (int j = 0; j < 3 * t; j++) tris.Add(T.U16(idx + 2 * (uint)(bi + j)));
                            take.Add(members[gi]); gi++; layered++;
                            break;
                        }
                        foreach (int j in Enumerable.Range(0, 3 * t))
                        {
                            int abs = fv + T.U16(idx + 2 * (uint)(bi + j));
                            if (!local.TryGetValue(abs, out int l)) { l = local.Count; local[abs] = l; }
                            tris.Add(l);
                        }
                        take.Add(members[gi]); gi++;
                    }
                    uint first = surfs + (uint)(SurfSize * take[0]);
                    byte[] ns = T.ReadBytes(first, SurfSize);
                    int firstVertex = newVerts.Count / VertexSize, baseIndex = newIdx.Count / 2;
                    foreach (var (abs, _) in local.OrderBy(kv => kv.Value)) newVerts.AddRange(T.ReadBytes(verts + (uint)(VertexSize * abs), VertexSize));
                    foreach (int l in tris) { newIdx.Add((byte)l); newIdx.Add((byte)(l >> 8)); }
                    int vc = local.Count, tc = tris.Count / 3;
                    maxV = Math.Max(maxV, vc); maxT = Math.Max(maxT, tc);
                    float[] mn = { float.MaxValue, float.MaxValue, float.MaxValue }, mx = { float.MinValue, float.MinValue, float.MinValue };
                    foreach (int i in take)
                    {
                        uint b = bnds + (uint)(24 * i);
                        for (int a = 0; a < 3; a++)
                        {
                            float c = T.F32(b + (uint)(4 * a)), e = T.F32(b + 12 + (uint)(4 * a));
                            mn[a] = Math.Min(mn[a], c - e); mx[a] = Math.Max(mx[a], c + e);
                        }
                    }
                    BitConverter.TryWriteBytes(ns.AsSpan(4), firstVertex);
                    BitConverter.TryWriteBytes(ns.AsSpan(8), (ushort)vc);
                    BitConverter.TryWriteBytes(ns.AsSpan(10), (ushort)tc);
                    BitConverter.TryWriteBytes(ns.AsSpan(12), baseIndex);
                    outSurf.Add((ns, img.Pointer(first + 16), k, take));
                    boxes.Add((mn, mx));
                    at++;
                }
            }
            ranges[k, 1] = outSurf.Count;
        }
        int m = outSurf.Count;
        // the new arrays
        uint ns2 = ctx.NewObject((uint)(SurfSize * m)), nb = ctx.NewObject((uint)(24 * m));
        for (int i = 0; i < m; i++)
        {
            T.Write(ns2 + (uint)(SurfSize * i), outSurf[i].Surf);
            img.SetPointer(ns2 + (uint)(SurfSize * i) + 16, outSurf[i].Material);
            var (mn, mx) = boxes[i];
            for (int a = 0; a < 3; a++)
            {
                T.WI32(nb + (uint)(24 * i) + (uint)(4 * a), BitConverter.SingleToInt32Bits((mn[a] + mx[a]) / 2));
                T.WI32(nb + (uint)(24 * i) + 12 + (uint)(4 * a), BitConverter.SingleToInt32Bits((mx[a] - mn[a]) / 2));
            }
        }
        uint nv = ctx.NewObject((uint)Math.Max(newVerts.Count, 4), 4), ni = ctx.NewObject((uint)Math.Max(newIdx.Count, 2), 2);
        T.Write(nv, newVerts.ToArray());
        T.Write(ni, newIdx.ToArray());
        uint sorted = ctx.NewObject((uint)(4 * Math.Max(m, 1)), 2);
        for (int i = 0; i < m; i++) { T.WU16(sorted + (uint)(2 * i), (ushort)i); T.WU16(sorted + (uint)(2 * (m + i)), (ushort)i); }
        img.SetPointer(dp + P_Surfaces, ns2);
        img.SetPointer(dp + P_SurfBounds, nb);
        img.SetPointer(dp + P_Sorted, sorted);
        img.SetPointer(draw + D_Vertices, nv);
        img.SetPointer(draw + D_Indices, ni);
        T.WU32(draw + D_VertexCount, (uint)(newVerts.Count / VertexSize));
        T.WU32(draw + D_IndexCount, (uint)(newIdx.Count / 2));
        T.WU32(h + W_SurfaceCount, (uint)m);
        T.WU32(dp + P_StaticSurfaceCount, (uint)m);
        T.WU32(dp + P_NoDecal, (uint)m);
        for (int k = 0; k < 4; k++)
        {
            T.WU32(dp + P_Ranges + (uint)(8 * k), (uint)ranges[k, 0]);
            T.WU32(dp + P_Ranges + (uint)(8 * k) + 4, (uint)ranges[k, 1]);
        }
        // one cell, one leaf: every surface and static model
        int smodels = (int)T.U32(dp + P_SmodelCount);
        float[] amn = { float.MaxValue, float.MaxValue, float.MaxValue }, amx = { float.MinValue, float.MinValue, float.MinValue };
        foreach (var (mn, mx) in boxes)
            for (int a = 0; a < 3; a++) { amn[a] = Math.Min(amn[a], mn[a]); amx[a] = Math.Max(amx[a], mx[a]); }
        uint node = ctx.NewObject(NodeSize, 128);
        for (int a = 0; a < 3; a++)
        {
            T.WI32(node + (uint)(4 * a), BitConverter.SingleToInt32Bits((amn[a] + amx[a]) / 2));
            T.WI32(node + 12 + (uint)(4 * a), BitConverter.SingleToInt32Bits((amx[a] - amn[a]) / 2 + 1));
        }
        T.WU16(node + 24, 0);
        T.WU16(node + 26, (ushort)m);
        T.WU16(node + 28, 0);
        T.WU16(node + 30, (ushort)m);
        T.WU16(node + 32, (ushort)m);
        T.WU16(node + 34, (ushort)smodels);
        if (smodels > 0)
        {
            uint list = ctx.NewObject((uint)(2 * smodels), 2);
            for (int i = 0; i < smodels; i++) T.WU16(list + (uint)(2 * i), (ushort)i);
            img.SetPointer(node + 36, list);
        }
        T.WI32(node + 40, 0);
        // the world bounds become the kept geometry's (0.0.2.2's cover its whole map, sky shell included: moved, they
        // stretched the merged world's bounds past the stock sky box and every donor scene drew black)
        for (int a = 0; a < 6; a++) T.WI32(h + 0xD8 + (uint)(4 * a), T.I32(node + (uint)(4 * a)));
        uint counts = ctx.NewObject(4); T.WU32(counts, 1);
        uint trees = ctx.NewObject(4, 128); img.SetPointer(trees, node);
        // the cell: the first cell's fields; its probe list is the donor cells' lists (Cells.ApplyProbes)
        uint oldCells = img.Pointer(h + W_Cells);
        uint cell = ctx.NewObject(CellSize);
        T.Write(cell, T.ReadBytes(oldCells, CellSize));
        img.CopyTags(cell, oldCells, CellSize);
        for (int a = 0; a < 6; a++) T.WI32(cell + (uint)(4 * a), T.I32(node + (uint)(4 * a)));
        T.WU32(cell + 24, 0);
        img.SetPointer(cell + 28, 0);
        int probes = (int)T.U32(draw + D_ProbeCount);
        uint plist = ctx.NewObject((uint)Math.Max(probes, 1), 1);
        for (int i = 0; i < probes; i++) T.WU8(plist + (uint)i, (byte)i);
        T.WU8(cell + 32, (byte)probes);
        img.SetPointer(cell + 36, probes > 0 ? plist : 0);
        img.SetPointer(h + W_TreeCounts, counts);
        img.SetPointer(h + W_Trees, trees);
        img.SetPointer(h + W_Cells, cell);
        T.WU32(h + W_CellCount, 1);
        log($"{tag}: {n} surfaces joined into {m} (one per material, lightmap and probe in each dpvs range){(bad > 0 ? $", {bad} with vertex or index ranges past the arrays left out" : "")}; {newVerts.Count / VertexSize} vertices, {newIdx.Count / 2} indices; {layered} with vertex layer data kept whole; largest {maxV} vertices, {maxT} triangles");
    }
}
