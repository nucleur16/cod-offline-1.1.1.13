using System.Text;

namespace VlobbyMerge;

/// stockcmp <stock ui_viewer_mp.ff> <merged.ff> <exe>: what the stock lobby world draws, in the stock zone and in the
/// merged one. Stock content must come through the merge unchanged: every static model of the stock world (its draw
/// instance bytes, its model and that model's surfaces' materials and technique sets), the stock surfaces per material,
/// the stock entities, and every asset of the stock zone by name (non-pointer header bytes, technique set and images of
/// materials).
public static class Compare
{
    const uint W_Dpvs = 0x1C8, P_Smodels = 0, P_Surfaces = 84, P_SurfBounds = 88, P_DrawInsts = 92;
    const int DrawInstSize = 76, SurfSize = 24;
    // the merged scenes lie at x < -12000; the stock lobby world does not
    const float DonorX = -12000;

    public static void Stock(string stockFf, string mergedFf, string exe)
    {
        var a = LoadedZone.Read(stockFf, exe, anyBuild: true);
        var b = LoadedZone.Read(mergedFf, exe, anyBuild: true);
        int issues = 0;

        // static models: by origin
        var sa = Smodels(a, false); var sb = Smodels(b, true);
        int same = 0, diff = 0, missing = 0;
        foreach (var (k, v) in sa)
        {
            if (!sb.TryGetValue(k, out var w)) { if (missing++ < 20) Console.WriteLine($"smodel missing at {k}: {v[0]}"); continue; }
            var x = v.OrderBy(s => s).ToList(); var y = w.OrderBy(s => s).ToList();
            if (x.SequenceEqual(y)) { same++; continue; }
            if (diff++ < 25) Console.WriteLine($"smodel differs at {k}:\n   stock  {string.Join("\n          ", x)}\n   merged {string.Join("\n          ", y)}");
        }
        int extra = sb.Keys.Count(k => !sa.ContainsKey(k));
        Console.WriteLine($"stock static models: {sa.Values.Sum(l => l.Count)} in the stock zone, {sb.Values.Sum(l => l.Count)} in the merged zone's stock area; {same} places alike, {diff} differ, {missing} missing, {extra} extra");
        issues += diff + missing + extra;

        // surfaces: triangles per material (and technique set)
        var ma = Surfaces(a, false); var mb = Surfaces(b, true);
        int md = 0;
        foreach (var k in ma.Keys.Union(mb.Keys).OrderBy(s => s))
        {
            int p = ma.GetValueOrDefault(k), q = mb.GetValueOrDefault(k);
            if (p != q && md++ < 25) Console.WriteLine($"surface triangles of {k}: stock {p}, merged {q}");
        }
        Console.WriteLine($"stock surface materials: {ma.Count} in the stock zone, {mb.Count} in the merged zone's stock area; {md} with other triangle counts");
        issues += md;

        // assets by name: header bytes that are not pointers, and the names pointers lead to
        int ad = 0, am = 0;
        var nb = new Dictionary<(int, string), uint>();
        foreach (var (h, t) in b.Image.AssetHeaders) if (Name(b, h, t) is { } n) nb.TryAdd((t, n.ToLowerInvariant()), h);
        foreach (var (h, t) in a.Image.AssetHeaders)
        {
            if (t is not (AssetTypes.XModel or AssetTypes.Material or AssetTypes.Techset or AssetTypes.Image)) continue;
            if (Name(a, h, t) is not { } n || n.StartsWith(',')) continue;
            if (!nb.TryGetValue((t, n.ToLowerInvariant()), out uint hb)) { if (am++ < 15) Console.WriteLine($"asset missing: type {t} {n}"); continue; }
            string fa = Fingerprint(a, h, t), fb = Fingerprint(b, hb, t);
            if (fa != fb && ad++ < 25) Console.WriteLine($"asset differs: type {t} {n}\n   stock  {fa}\n   merged {fb}");
        }
        Console.WriteLine($"stock assets by name: {ad} differ, {am} missing");
        issues += ad + am;

        // entities
        var ea = Entities(a); var eb = Entities(b);
        int ed = 0, em = 0;
        foreach (var (k, v) in ea)
        {
            if (!eb.TryGetValue(k, out string? w)) { if (em++ < 10) Console.WriteLine($"entity missing: {k}"); continue; }
            if (v != w && ed++ < 10) Console.WriteLine($"entity differs: {k}\n   stock  {v}\n   merged {w}");
        }
        Console.WriteLine($"stock entities: {ea.Count}; {ed} differ, {em} missing");
        issues += ed + em;
        Console.WriteLine(issues == 0 ? "the stock lobby world comes through the merge unchanged" : $"{issues} differences");
    }

    static string? Name(LoadedZone z, uint h, int t)
    {
        uint np = z.Image.Pointer(h + (uint)(t == AssetTypes.Image ? (z.Build == Codol.FastFiles.GameBuild.V0_0_2_2 ? 0x1C : 0x20) : 0));
        return np == 0 ? null : z.M.CString(np, 256);
    }

    static string PtrName(LoadedZone z, uint p)
    {
        if (p == 0) return "null";
        if (z.Image.AssetHeaders.TryGetValue(p, out int t)) return Name(z, p, t) ?? $"#{t}";
        return "data";
    }

    /// an asset's header: every byte that is not a pointer, and for pointers the name of the asset they lead to;
    /// a material adds its technique set's name and its texture table's images
    static string Fingerprint(LoadedZone z, uint h, int t)
    {
        uint size = t switch { AssetTypes.XModel => 0x168, AssetTypes.Material => 0x60, AssetTypes.Techset => 0xC, _ => 0x24 };
        var sb = new StringBuilder();
        for (uint o = 0; o < size; o += 4)
        {
            if (z.Image.Pointers.Get(h + o)) sb.Append('[').Append(PtrName(z, z.Image.Pointer(h + o))).Append(']');
            else sb.Append(z.M.U32(h + o).ToString("X8"));
            sb.Append(' ');
        }
        if (t == AssetTypes.Material)
        {
            for (uint o = 0x18; o < 0x60; o += 4)
            {
                uint tt = z.Image.Pointer(h + o);
                if (tt == 0 || z.Image.AssetHeaders.ContainsKey(tt)) continue;
                for (uint k = 0; k < 16; k++)
                {
                    uint ip = z.Image.Pointer(tt + 12 * k + 8);
                    if (ip == 0 || !z.Image.AssetHeaders.TryGetValue(ip, out int it) || it != AssetTypes.Image) break;
                    sb.Append(" img:").Append(Name(z, ip, it));
                }
            }
        }
        if (t == AssetTypes.XModel)
        {
            uint mh = z.Image.Pointer(h + 0x2C);
            for (int k = 0; mh != 0 && k < z.M.U8(h + 6) && k < 64; k++)
            {
                uint mat = z.Image.Pointer(mh + (uint)(4 * k));
                sb.Append(" mat:").Append(PtrName(z, mat));
                uint ts = mat == 0 ? 0 : z.Image.Pointer(mat + 0x50);
                sb.Append('/').Append(PtrName(z, ts));
            }
        }
        return sb.ToString();
    }

    static Dictionary<string, List<string>> Smodels(LoadedZone z, bool stockArea)
    {
        var I = z.Image; var m = z.M;
        uint w = z.Find(AssetTypes.GfxWorld), dp = w + W_Dpvs;
        int n = (int)m.U32(dp + P_Smodels);
        uint di = I.Pointer(dp + P_DrawInsts);
        var r = new Dictionary<string, List<string>>();
        for (int i = 0; i < n; i++)
        {
            uint d = di + (uint)(DrawInstSize * i);
            float x = m.F32(d), y = m.F32(d + 4), zz = m.F32(d + 8);
            if (stockArea && x < DonorX) continue;
            var sb = new StringBuilder();
            for (uint o = 0; o < DrawInstSize; o += 4)
                sb.Append(o == 52 ? $"[{PtrName(z, I.Pointer(d + 52))}]" : m.U32(d + o).ToString("X8")).Append(' ');
            uint model = I.Pointer(d + 52);
            if (model != 0) sb.Append(Fingerprint(z, model, AssetTypes.XModel));
            string key = $"{x:0.00},{y:0.00},{zz:0.00}";
            (r.TryGetValue(key, out var l) ? l : r[key] = []).Add(sb.ToString());
        }
        return r;
    }

    static Dictionary<string, int> Surfaces(LoadedZone z, bool stockArea)
    {
        var I = z.Image; var m = z.M;
        uint w = z.Find(AssetTypes.GfxWorld), dp = w + W_Dpvs;
        int n = (int)m.U32(dp + 4);
        uint surfs = I.Pointer(dp + P_Surfaces), bnds = I.Pointer(dp + P_SurfBounds);
        var r = new Dictionary<string, int>();
        for (int i = 0; i < n; i++)
        {
            if (stockArea && m.F32(bnds + (uint)(24 * i)) < DonorX) continue;
            uint s = surfs + (uint)(SurfSize * i), mat = I.Pointer(s + 16);
            uint ts = mat == 0 ? 0 : I.Pointer(mat + 0x50);
            string k = $"{PtrName(z, mat)}/{PtrName(z, ts)}/lm{m.U8(s + 20)}";
            r[k] = r.GetValueOrDefault(k) + m.U16(s + 10);
        }
        return r;
    }

    static Dictionary<string, string> Entities(LoadedZone z)
    {
        uint me = z.Image.Pointer(z.Find(AssetTypes.ClipMapMp) + 0x9C);
        string text = z.M.CString(z.Image.Pointer(me + 4), 1 << 24) ?? "";
        var r = new Dictionary<string, string>();
        int i = 0, n = 0;
        while ((i = text.IndexOf('{', i)) >= 0)
        {
            int e = text.IndexOf('}', i);
            if (e < 0) break;
            string block = text.Substring(i, e - i + 1).Replace("\r", "");
            i = e + 1;
            var tn = System.Text.RegularExpressions.Regex.Match(block, "\"targetname\" \"([^\"]+)\"");
            string key = tn.Success ? tn.Groups[1].Value : $"#{n}";
            n++;
            if (tn.Success && (key.StartsWith("v3_") || key.StartsWith("v2_") || key.StartsWith("v0_"))) continue;
            r.TryAdd(key, block);
        }
        return r;
    }
}
