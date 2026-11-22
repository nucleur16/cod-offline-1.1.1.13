using System.Text;
using Codol.FastFiles.Porting;

namespace VlobbyMerge;

/// a 1.1.1.13 StringTable (16 bytes: name, columnCount, rowCount, values) with its cells {const char *string; int hash}
/// and C1's cell hash (StringTable_HashString: lower-cased, h = c + 31 * h), as zonekit/codecs.py writes them
public static class StringTables
{
    public const int Type = 37;

    public static uint Add(PortContext ctx, string name, string csvText)
    {
        var rows = ParseCsv(csvText);
        int cols = rows[0].Count;
        if (rows.Any(r => r.Count != cols)) throw new InvalidDataException($"{name}: rows of different lengths");
        var T = ctx.T; var img = ctx.Target;
        uint h = ctx.NewObject(16);
        img.SetPointer(h, ctx.NewString(name));
        T.WU32(h + 4, (uint)cols);
        T.WU32(h + 8, (uint)rows.Count);
        uint cells = ctx.NewObject((uint)(8 * cols * rows.Count));
        int i = 0;
        foreach (var row in rows)
            foreach (var cell in row)
            {
                uint c = cells + (uint)(8 * i++);
                img.SetPointer(c, ctx.NewString(cell));
                T.WU32(c + 4, Hash(cell));
            }
        img.SetPointer(h + 12, cells);
        img.AssetHeaders[h] = Type;
        return h;
    }

    static uint Hash(string s)
    {
        uint h = 0;
        foreach (byte b in Encoding.Latin1.GetBytes(s))
        {
            int c = b is >= 0x41 and <= 0x5A ? b + 0x20 : b;
            h = unchecked((uint)c + 31 * h);
        }
        return h;
    }

    static List<List<string>> ParseCsv(string text)
    {
        var rows = new List<List<string>>();
        foreach (var line in text.Replace("\r", "").Split('\n'))
        {
            if (line.Length == 0) continue;
            var row = new List<string>();
            var cur = new StringBuilder();
            bool q = false;
            for (int i = 0; i < line.Length; i++)
            {
                char ch = line[i];
                if (q) { if (ch == '"' && i + 1 < line.Length && line[i + 1] == '"') { cur.Append('"'); i++; } else if (ch == '"') q = false; else cur.Append(ch); }
                else if (ch == '"') q = true;
                else if (ch == ',') { row.Add(cur.ToString()); cur.Clear(); }
                else cur.Append(ch);
            }
            row.Add(cur.ToString());
            rows.Add(row);
        }
        return rows;
    }
}
