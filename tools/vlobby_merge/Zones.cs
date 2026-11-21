using Codol.FastFiles;
using Codol.FastFiles.Crypto;
using Codol.FastFiles.Emulation;
using Codol.FastFiles.FastFile;
using Codol.FastFiles.Porting;

namespace VlobbyMerge;

/// a 1.1.1.13 zone read into a pointer-tagged image (the AssetExplorer's ZoneCapture)
public sealed class LoadedZone
{
    public required string Path { get; init; }
    public required ZoneImage Image { get; init; }
    public GameBuild Build { get; init; } = GameBuild.V1_1_1_13;
    public VirtualMemory M => Image.Memory;

    public static LoadedZone Read(string ff, string exe, bool anyBuild = false)
    {
        using var rsa = FastFileKeyProvider.LoadFromExecutable(exe);
        var contents = new FastFileReader(FastFileReader.ReadHeader(ff).Build is { EncryptedFastFiles: false } ? null : rsa).Read(ff);
        if (!anyBuild && contents.Build != GameBuild.V1_1_1_13)
        {
            throw new InvalidDataException($"{ff}: build {contents.Build.Version}, not 1.1.1.13");
        }
        var capture = ZoneCapture.Capture(contents.Build, contents.Name, contents.Zone);
        return new LoadedZone { Path = ff, Image = capture.Image, Build = contents.Build };
    }

    /// the top-level asset of `type`, by its name field (the first pointer of the header, except the types whose
    /// header starts otherwise; the world assets all start with the name)
    public uint Find(int type, string? name = null)
    {
        foreach (var a in Image.Assets)
        {
            if (a.Type != type || a.Header == 0) continue;
            if (name is null) return a.Header;
            uint p = Image.Pointer(a.Header);
            if (p != 0 && string.Equals(M.CString(p, 256), name, StringComparison.OrdinalIgnoreCase)) return a.Header;
        }
        throw new KeyNotFoundException($"{Path}: no asset of type {type} named {name}");
    }

    public string? Str(uint field)
    {
        uint p = Image.Pointer(field);
        return p == 0 ? null : M.CString(p, 1 << 20);
    }
}

public static class AssetTypes
{
    public const int Image = 10, Material = 5, Techset = 9, XModel = 4;
    public const int ClipMapMp = 15, ComWorld = 16, GameWorldMp = 18, MapEnts = 19, FxWorld = 20, GfxWorld = 21;
}
