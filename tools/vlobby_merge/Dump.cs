namespace VlobbyMerge;

public static class Dump
{
    /// the header bytes of the assets of `type` named `name` (case-insensitive) in a zone of any build
    public static void Asset(LoadedZone z, int type, string name, int nameOffset, int size)
    {
        foreach (var (h, t) in z.Image.AssetHeaders)
        {
            if (t != type) continue;
            string? n = z.M.CString(z.Image.Pointer(h + (uint)nameOffset), 256);
            if (!string.Equals(n, name, StringComparison.OrdinalIgnoreCase)) continue;
            var b = z.M.ReadBytes(h, size);
            for (int o = 0; o < size; o += 4)
                Console.Write($"{o:X3}:{BitConverter.ToUInt32(b, o):X8}{(z.Image.Pointers.Get(h + (uint)o) ? "P" : " ")} " + (o % 32 == 28 ? "\n" : ""));
            Console.WriteLine();
            return;
        }
        Console.WriteLine("not found");
    }
}
