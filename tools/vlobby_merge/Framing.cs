using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace VlobbyMerge;

/// Cameras that frame a donor scene's character the way 1.1.1.13's own menu frames it. A camera table row's view
/// stands at its camera entity, pulled back along the entity's facing by the row's distance (column 4: 397 for
/// character_loadout_center, 195 for the urban scene); the stock camera entity sits a few units from the stand, turned
/// to face it (player_pos_center 0 180 0, player_camera_center 10 0 0 at +5.8 +1.1 +37.5). The later builds' rows
/// carry their own cameras, placed for their own UI (the character off to one side) with distance 0. A line of the
/// framing file
///
///     <new camera> <donor stand> <stock stand> <stock camera>
///
/// adds a script_origin <new camera> at the donor stand plus the stock camera's offset from the stock stand, turned by
/// the difference of the two stands' yaws, with the stock camera's angles turned the same way. The row that names it
/// takes the stock row's distance and field of view (camera_rows.py), so the character stands where it stands in the
/// stock menu.
///
/// A line "place <name> <x> <y> <z> <pitch> <yaw> <roll>" adds a script_origin <name> there, the numbers written as the
/// line gives them: the cameras and stands the lobby camera editor saved (main\vlobby_cameras.txt), which
/// fold_cameras.py writes in.
public static class Framing
{
    public static string? File { get; set; }

    public static void Apply(StringBuilder entText, Action<string> log)
    {
        if (File is null) return;
        var ents = Parse(entText.ToString());
        int n = 0, placed = 0;
        foreach (string raw in System.IO.File.ReadAllLines(File))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith('#')) continue;
            string[] p = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (p[0] == "place")
            {
                if (p.Length != 8 || p.Skip(2).Any(v => !float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out _)))
                    throw new InvalidDataException($"{File}: \"{line}\": place <name> <x> <y> <z> <pitch> <yaw> <roll>");
                if (ents.ContainsKey(p[1])) throw new InvalidDataException($"{File}: \"{line}\": {p[1]} is already in the MapEnts");
                entText.Append("{\n\"targetname\" \"").Append(p[1]).Append("\"\n\"classname\" \"script_origin\"\n")
                    .Append("\"angles\" \"").Append(p[5]).Append(' ').Append(p[6]).Append(' ').Append(p[7]).Append("\"\n")
                    .Append("\"origin\" \"").Append(p[2]).Append(' ').Append(p[3]).Append(' ').Append(p[4]).Append("\"\n}\n");
                ents[p[1]] = new Dictionary<string, string> { ["targetname"] = p[1], ["origin"] = $"{p[2]} {p[3]} {p[4]}", ["angles"] = $"{p[5]} {p[6]} {p[7]}" };
                placed++;
                continue;
            }
            if (p.Length < 4) throw new InvalidDataException($"{File}: \"{line}\": <new camera> <stand> <stock stand> <stock camera>");
            var stand = Get(ents, p[1]); var sStand = Get(ents, p[2]); var sCam = Get(ents, p[3]);
            float yaw = Vec(stand, "angles")[1], sYaw = Vec(sStand, "angles")[1];
            double d = (yaw - sYaw) * Math.PI / 180;
            float[] so = Vec(sStand, "origin"), co = Vec(sCam, "origin"), o = Vec(stand, "origin"), ca = Vec(sCam, "angles");
            float dx = co[0] - so[0], dy = co[1] - so[1], dz = co[2] - so[2];
            float x = o[0] + (float)(dx * Math.Cos(d) - dy * Math.Sin(d)), y = o[1] + (float)(dx * Math.Sin(d) + dy * Math.Cos(d)), z = o[2] + dz;
            float cy = ca[1] + (yaw - sYaw);
            cy = ((cy % 360) + 360) % 360;
            entText.Append("{\n\"targetname\" \"").Append(p[0]).Append("\"\n\"classname\" \"script_origin\"\n")
                .Append("\"angles\" \"").Append(F(ca[0])).Append(' ').Append(F(cy)).Append(' ').Append(F(ca[2])).Append("\"\n")
                .Append("\"origin\" \"").Append(F(x)).Append(' ').Append(F(y)).Append(' ').Append(F(z)).Append("\"\n}\n");
            n++;
        }
        log($"framing: {n} cameras placed at the stock menu's offset from their stands, {placed} entities placed as saved ({Path.GetFileName(File)})");
    }

    /// the donor stands the framing lines place cameras from (they must be in the MapEnts)
    public static IEnumerable<string> DonorStands()
    {
        if (File is null) yield break;
        foreach (string raw in System.IO.File.ReadAllLines(File))
        {
            string[] p = raw.Trim().Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (p.Length >= 4 && !p[0].StartsWith('#') && p[0] != "place") yield return p[1];
        }
    }

    static string F(float v) => v.ToString("0.###", CultureInfo.InvariantCulture);

    public static Dictionary<string, Dictionary<string, string>> Parse(string text)
    {
        var all = new Dictionary<string, Dictionary<string, string>>(StringComparer.OrdinalIgnoreCase);
        foreach (Match m in Regex.Matches(text, "[{]([^{}]*)[}]"))
        {
            var kv = new Dictionary<string, string>();
            foreach (Match k in Regex.Matches(m.Groups[1].Value, "\"([^\"]+)\" \"([^\"]*)\"")) kv[k.Groups[1].Value] = k.Groups[2].Value;
            if (kv.TryGetValue("targetname", out string? tn)) all.TryAdd(tn, kv);
        }
        return all;
    }

    static Dictionary<string, string> Get(Dictionary<string, Dictionary<string, string>> ents, string name) =>
        ents.TryGetValue(name, out var e) ? e : throw new KeyNotFoundException($"framing: no entity {name} in the merged MapEnts");

    static float[] Vec(Dictionary<string, string> e, string key) =>
        (e.TryGetValue(key, out string? v) ? v : "0 0 0").Split(' ', StringSplitOptions.RemoveEmptyEntries)
        .Select(s => float.Parse(s, CultureInfo.InvariantCulture)).ToArray();
}
