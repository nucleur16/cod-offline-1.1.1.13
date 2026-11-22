using Codol.FastFiles.Porting;

namespace VlobbyMerge;

/// GfxLightGrid (IW4, 0x38 bytes at GfxWorld + 0x98): hasLightRegions u8 +0, lastSunPrimaryLightIndex +4, mins u16[3] +8,
/// maxs u16[3] +14 (cells: x, y in 32 units, z in 64, from -131072), rowAxis +20, colAxis +24, rowDataStart u16* +28
/// (one per row from mins[rowAxis], in 4-byte units of rawRowData, 0xFFFF = empty row), rawRowDataSize +32,
/// rawRowData +36 (per row: u16 colStart, colCount, zStart, zCount, u32 firstEntry, then runs), entryCount +40,
/// entries +44 (4 bytes: u16 colorsIndex, u8 primaryLightIndex, u8 needsTrace), colorCount +48, colors +52 (168 bytes).
/// (Codol.Assets LightGrid reads the same.) A donor's rows go after the stock ones: the donor moves by whole cells
/// along the row axis far enough that no row holds both, so every row record is kept as it is, re-based.
public static class LightGridMerge
{
    const int ColorSize = 168;

    /// a world whose StockGrid is set brings no grid of its own: its rows (moved like it) reuse the stock world's row
    /// records of the same place (0.0.2.2: its grid holds 65536 distinct colours, more than a merged grid can index,
    /// and its scenes are earlier versions of the stock ones at the same places)
    public static void Merge(PortContext ctx, uint grid, List<(uint Grid, bool Donor, float Dx, float Dy, float Dz, bool StockGrid)> worlds, Action<string> log)
    {
        var T = ctx.T; var img = ctx.Target;
        var a = worlds[0];
        int ra = (int)T.U32(a.Grid + 20), ca = (int)T.U32(a.Grid + 24);
        var mins = new int[3]; var maxs = new int[3];
        var shifts = new List<int[]>();
        for (int k = 0; k < 3; k++) { mins[k] = int.MaxValue; maxs[k] = int.MinValue; }
        foreach (var w0 in worlds)
        {
            var w = w0.StockGrid ? (a.Grid, w0.Donor, w0.Dx, w0.Dy, w0.Dz, true) : w0;
            if ((int)T.U32(w.Grid + 20) != ra || (int)T.U32(w.Grid + 24) != ca)
                throw new NotSupportedException("light grids with different row / column axes");
            var sh = new[] { Cells(w.Dx, 32), Cells(w.Dy, 32), Cells(w.Dz, 64) };
            shifts.Add(sh);
            for (int k = 0; k < 3; k++)
            {
                mins[k] = Math.Min(mins[k], T.U16(w.Grid + 8 + (uint)(2 * k)) + sh[k]);
                maxs[k] = Math.Max(maxs[k], T.U16(w.Grid + 14 + (uint)(2 * k)) + sh[k]);
            }
        }
        int rows = maxs[ra] - mins[ra] + 1;
        var rowStart = Enumerable.Repeat((ushort)0xFFFF, rows).ToArray();
        var raw = new List<byte>();
        var entries = new List<byte>();
        var colors = new List<byte>();
        var colorIndex = new Dictionary<string, int>();   // identical colour sets are kept once (0.0.2.2 stores 65536)
        int lastSun = (int)T.U32(a.Grid + 4);
        int stockRawBase = 0;
        for (int wi = 0; wi < worlds.Count; wi++)
        {
            var w = worlds[wi]; var sh = shifts[wi];
            if (w.StockGrid)
            {
                if (sh[ca] != 0 || sh[2] != 0) throw new NotSupportedException("a stock-grid scene may move along the row axis only");
                int sMin = T.U16(a.Grid + 8 + (uint)(2 * ra)), sRows = T.U16(a.Grid + 14 + (uint)(2 * ra)) - sMin + 1;
                uint sStarts = img.Pointer(a.Grid + 28);
                for (int r = 0; r < sRows; r++)
                {
                    ushort s0 = T.U16(sStarts + (uint)(2 * r));
                    if (s0 == 0xFFFF) continue;
                    int slot = r + sMin + sh[ra] - mins[ra];
                    if (rowStart[slot] != 0xFFFF) throw new NotSupportedException($"light grid row {slot + mins[ra]} is held by two scenes");
                    rowStart[slot] = (ushort)(stockRawBase + s0);
                }
                continue;
            }
            int wMin = T.U16(w.Grid + 8 + (uint)(2 * ra)), wRows = T.U16(w.Grid + 14 + (uint)(2 * ra)) - wMin + 1;
            int rawSize = (int)T.U32(w.Grid + 32), entryCount = (int)T.U32(w.Grid + 40), colorCount = (int)T.U32(w.Grid + 48);
            byte[] wr = T.ReadBytes(img.Pointer(w.Grid + 36), rawSize);
            uint starts = img.Pointer(w.Grid + 28);
            if (raw.Count % 4 != 0 || rawSize % 4 != 0) throw new NotSupportedException("light grid row data not in 4-byte units");
            int rawBase = raw.Count / 4, entryBase = entries.Count / 4;
            byte[] wc = T.ReadBytes(img.Pointer(w.Grid + 52), ColorSize * colorCount);
            var remap = new int[colorCount];
            for (int c = 0; c < colorCount; c++)
            {
                string key = Convert.ToBase64String(wc, ColorSize * c, ColorSize);
                if (!colorIndex.TryGetValue(key, out int at))
                {
                    at = colorIndex[key] = colors.Count / ColorSize;
                    colors.AddRange(new ArraySegment<byte>(wc, ColorSize * c, ColorSize));
                }
                remap[c] = at;
            }
            var patched = new HashSet<int>();
            for (int r = 0; r < wRows; r++)
            {
                ushort s = T.U16(starts + (uint)(2 * r));
                if (s == 0xFFFF) continue;
                int slot = r + wMin + sh[ra] - mins[ra];
                if (rowStart[slot] != 0xFFFF) throw new NotSupportedException($"light grid row {slot + mins[ra]} is held by two scenes");
                if (rawBase + s > 0xFFFE) throw new NotSupportedException("merged light grid row data exceeds 256 KB");
                rowStart[slot] = (ushort)(rawBase + s);
                if (!patched.Add(s) || wi == 0) continue;
                int o = 4 * s;
                W16(wr, o, R16(wr, o) + sh[ca]);
                W16(wr, o + 4, R16(wr, o + 4) + sh[2]);
                BitConverter.TryWriteBytes(wr.AsSpan(o + 8), BitConverter.ToUInt32(wr, o + 8) + (uint)entryBase);
            }
            raw.AddRange(wr);
            byte[] we = T.ReadBytes(img.Pointer(w.Grid + 44), 4 * entryCount);
            for (int e = 0; e < entryCount; e++)
            {
                int ci = R16(we, 4 * e);
                W16(we, 4 * e, ci < colorCount ? remap[ci] : ci);
                if (wi > 0)
                {
                    int l = we[4 * e + 2];
                    // the donor's own lights are not in the merged world: none, except the sun encodings
                    if (l != 0 && l != lastSun && l < 256 - lastSun) we[4 * e + 2] = 0;
                }
            }
            entries.AddRange(we);
        }
        if (colors.Count / ColorSize > 0xFFFF) throw new NotSupportedException("more than 65535 light grid colours");
        for (int k = 0; k < 3; k++)
        {
            T.WU16(grid + 8 + (uint)(2 * k), (ushort)mins[k]);
            T.WU16(grid + 14 + (uint)(2 * k), (ushort)maxs[k]);
        }
        uint rs = ctx.NewObject((uint)(2 * rows), 2);
        for (int r = 0; r < rows; r++) T.WU16(rs + (uint)(2 * r), rowStart[r]);
        img.SetPointer(grid + 28, rs);
        img.SetPointer(grid + 36, NewData(ctx, raw.ToArray(), 1));
        T.WU32(grid + 32, (uint)raw.Count);
        img.SetPointer(grid + 44, NewData(ctx, entries.ToArray(), 4));
        T.WU32(grid + 40, (uint)(entries.Count / 4));
        img.SetPointer(grid + 52, NewData(ctx, colors.ToArray(), 4));
        T.WU32(grid + 48, (uint)(colors.Count / ColorSize));
        log($"light grid: rows {mins[ra]}..{maxs[ra]} (axis {ra}), {raw.Count} bytes of rows, {entries.Count / 4} entries, {colors.Count / ColorSize} colours");
    }

    static uint NewData(PortContext ctx, byte[] data, uint align)
    {
        uint at = ctx.NewObject((uint)Math.Max(data.Length, 1), align);
        ctx.T.Write(at, data);
        return at;
    }

    static int Cells(float d, int size)
    {
        if (d % size != 0) throw new ArgumentException($"offset {d} is not a whole number of {size}-unit light grid cells");
        return (int)(d / size);
    }

    static int R16(byte[] b, int o) => b[o] | (b[o + 1] << 8);

    static void W16(byte[] b, int o, int v)
    {
        if (v < 0 || v > 0xFFFF) throw new OverflowException("light grid cell coordinate out of range");
        b[o] = (byte)v; b[o + 1] = (byte)(v >> 8);
    }
}
