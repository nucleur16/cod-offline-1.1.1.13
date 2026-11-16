using Codol.Assets.Porting;
using Codol.FastFiles;
using Codol.FastFiles.Crypto;
using Codol.FastFiles.Emulation;
using Codol.FastFiles.FastFile;
using Codol.FastFiles.Porting;

namespace Codol.SoundDumper;

/// mp-weapons-zone: builds mp_weapons_postgfx_mp, the art of the MP weapons CoDOL adds (1.1.1.13).
///
/// The M14 EBR (m14ebr_mp): C1's MP tables already compose it from the M14 parts (m14ebrncm iron sight, mag, grip and
/// muzzle, the m14ebr stocks) but ship no base gun (mp/coweapmodels/m14ebr_mp.csv is missing, so the client composes
/// attachments with no body). The zombies M14 (m14ebr_zm) draws codo_weapon_m14ebr_viewmodel / _world: that gun merged
/// into one model -- the M14 receiver (material mc/mtl_wea_m21ebr_m21ebr_blk: the M21 EBR's texture, the M14's shorter
/// mesh) plus the default parts, with every attachment tag of an MP receiver (tag_ironsight included, which the M21
/// receiver lacks).
/// The base guns wea_m14ebr_m14ebr_blk(_vmgun) and wea_m14ebr_m14ebr_01 .. _21(_vmgun), one per C1 camo row, are copies
/// of those two models on the same streamed surfaces (models/codo_weapon_m14ebr_* in iw_model_*.iwd: the zombies M14's
/// own draw path). The receiver surfaces take mc/mtl_wea_m14ebr_m14ebr_[camo], a copy of the M21 EBR's camo material of
/// that row; the merged default parts take mc/mtl_wea_m14ebr_hidden, a copy of stock mc/shadowcaster (techset
/// mc_shadowcaster: no lit or depth technique, so C1 does not draw it in a view), and C1 attaches its own parts at their
/// tags. (The first build cut the receiver surfaces out into inline geometry; C1 drew that wrong in first person.)
/// Everything the models lead to is in the zone (byte-identical to the stock copies of the same names), so it is complete
/// on its own: it loads at startup (Kernel33: *_postgfx_mp, after code_post_gfx) and stays loaded, and the sh1x server
/// mounts it with every MP map (mp_*.ff of the patch folder).
internal static partial class MpWeaponCommands
{
    private const int TypeXModelSurfs = 3, TypeXModel = 4, TypeMaterial = 5;
    private const uint XSurfaceSize = 0x54;
    private const uint ModelNumSurfs = 6, ModelMaterials = 0x44, ModelLodInfo = 0x48, LodStride = 56, ModelSize = 360;
    private const uint LodNumSurfs = 4, LodSurfIndex = 6, LodModelSurfs = 8, LodSurfsName = 12, ModelBoneInfo = 0x138;
    private const uint SurfsName = 0, SurfsSurfs = 4, SurfsNumSurfs = 8;
    private const string ReceiverMaterial = "mc/mtl_wea_m21ebr_m21ebr_blk";
    private const string HiddenMaterial = "mc/mtl_wea_m14ebr_hidden";

    private static Dictionary<(int, string), uint> NamedAssets(ZoneImage image)
    {
        var names = new Dictionary<(int, string), uint>();
        foreach (var (header, type) in image.AssetHeaders)
        {
            string? name = NameOf(image, header, type);
            if (name is null || name.StartsWith(','))
            {
                continue;
            }
            names.TryAdd((type, name), header);
        }
        return names;
    }

    private static string Camo(int camo) => camo == 0 ? "blk" : camo.ToString("00");

    private static int Build(CommandLine cli)
    {
        string game = cli.Required("game");
        string output = cli.Required("out");
        string? zoneOutput = cli.Value("zone");
        cli.EnsureAllConsumed();

        var build = GameBuild.V1_1_1_13;
        string zones = Path.Combine(game, "client_shipRetail", "chinese");
        string exe = Path.Combine(game, "C1.exe");
        var log = new List<string>();

        var zm = CaptureFile(Path.Combine(zones, "common_gametype_zm.ff"), exe).Image;
        var mp = CaptureFile(Path.Combine(zones, "common_gametype_mp.ff"), exe).Image;
        var namesZm = NamedAssets(zm);
        var namesMp = NamedAssets(mp);
        Console.WriteLine($"common_gametype_zm: {zm.Assets.Count} assets; common_gametype_mp: {mp.Assets.Count} assets");

        // the hidden material: stock mc/shadowcaster (techset mc_shadowcaster: only the shadow-map technique, so C1 draws
        // no lit or depth pass for a surface that has it). zm_prototype_sh and the MP maps define it; renamed here.
        var proto = CaptureFile(Path.Combine(zones, "zm_prototype_sh.ff"), exe).Image;
        var namesProto = NamedAssets(proto);

        var context = new PortContext(build, zm, build)
        {
            InlineSharedDataTypes = [TypeXModelSurfs, TypeXModel],
            ScriptStringArrays = new() { [TypeXModel] = (4, 0x2C) },
        };
        var target = context.Target;
        target.ScriptStrings.Clear();
        target.ScriptStrings.Add(null);

        // the camo materials: the M21 EBR's, one per C1 camo row (mp/coweapmodels/m21ebr_mp.csv rows 0..21), renamed
        var materials = new uint[22];
        for (int camo = 0; camo < 22; camo++)
        {
            string from = $"mc/mtl_wea_m21ebr_m21ebr_{Camo(camo)}";
            if (!namesMp.TryGetValue((TypeMaterial, from), out uint source))
            {
                throw new InvalidOperationException($"common_gametype_mp has no material {from}");
            }
            uint material = context.CopySourceAsset(mp, source);
            string name = $"mc/mtl_wea_m14ebr_m14ebr_{Camo(camo)}";
            target.SetPointer(material, context.NewString(name), PointerEncoding.Inline);
            materials[camo] = material;
        }
        log.Add("22 receiver materials mc/mtl_wea_m14ebr_m14ebr_blk.._21 (copies of mc/mtl_wea_m21ebr_m21ebr_*)");
        if (!namesProto.TryGetValue((TypeMaterial, "mc/shadowcaster"), out uint shadowSource))
        {
            throw new InvalidOperationException("zm_prototype_sh has no material mc/shadowcaster");
        }
        uint hidden = context.CopySourceAsset(proto, shadowSource);
        target.SetPointer(hidden, context.NewString(HiddenMaterial), PointerEncoding.Inline);
        log.Add($"{HiddenMaterial}: a copy of mc/shadowcaster (zm_prototype_sh) for the merged default parts");

        // The base guns are the zombies models themselves, on the same streamed surfaces (models/codo_weapon_m14ebr_* in
        // iw_model_*.iwd, the path the zombies M14 draws through): the receiver surfaces (material mc/mtl_wea_m21ebr_
        // m21ebr_blk) take the camo material, the merged default parts (iron sight, mag, grip, muzzle, stock -- C1 attaches
        // its own at their tags) take the hidden one. An earlier build cut the receiver surfaces out into inline geometry
        // and first person drew it wrong (2026-10-27: front of the gun missing, muzzle floating).
        var models = new List<uint>();
        foreach (var (sourceName, baseName) in new[]
        {
            ("codo_weapon_m14ebr_viewmodel", "wea_m14ebr_m14ebr_{0}_vmgun"),
            ("codo_weapon_m14ebr_world", "wea_m14ebr_m14ebr_{0}"),
        })
        {
            if (!namesZm.TryGetValue((TypeXModel, sourceName), out uint sourceModel))
            {
                throw new InvalidOperationException($"common_gametype_zm has no model {sourceName}");
            }
            int total = zm.Memory.U8(sourceModel + ModelNumSurfs);
            uint sourceMats = zm.Pointer(sourceModel + ModelMaterials);
            var receiver = new bool[total];
            for (int k = 0; k < total; k++)
            {
                uint m = zm.Pointer(sourceMats + 4 * (uint)k);
                receiver[k] = NameOf(zm, m, TypeMaterial)?.TrimStart(',') == ReceiverMaterial;
            }
            if (!receiver.Any(r => r))
            {
                throw new InvalidOperationException($"{sourceName}: no surface draws {ReceiverMaterial}");
            }
            log.Add($"{sourceName}: surfaces {string.Join(",", Enumerable.Range(0, total).Where(k => receiver[k]))} of {total} keep the receiver (camo), the rest are hidden");

            uint model = context.CopySourceAsset(zm, sourceModel);
            var mem = target.Memory;
            for (uint lod = 0; lod < 4; lod++)
            {
                uint record = target.Pointer(model + ModelLodInfo + LodStride * lod + LodModelSurfs);
                if (record != 0 && (mem.U32(record + SurfsSurfs) != 0 || mem.U16(record + SurfsNumSurfs) != 0))
                {
                    throw new InvalidOperationException($"{sourceName} lod {lod}: its surfaces are not streamed");
                }
            }

            for (int camo = 0; camo < 22; camo++)
            {
                uint variant = model;
                if (camo > 0)
                {
                    variant = context.NewObject(ModelSize, 4);
                    mem.Write(variant, mem.ReadBytes(model, (int)ModelSize));
                    foreach (uint field in target.Pointers.Enumerate(model, ModelSize).ToList())
                    {
                        target.SetPointer(variant + (field - model), mem.U32(field), PointerEncoding.Unknown);
                    }
                    target.AssetHeaders[variant] = TypeXModel;
                    // the bone bounds are each model's own, inline, as stock camo models carry them (the sh1x server's
                    // XModel reader, ReadXModelBounds, reads +0x138 inline whenever it is set)
                    uint boneInfo = target.Pointer(model + ModelBoneInfo);
                    if (boneInfo != 0)
                    {
                        uint length = target.Objects.TryGetValue(boneInfo, out uint l) ? l
                            : throw new InvalidOperationException($"{sourceName}: no object at the bone info");
                        uint copy = context.NewObject(length, 4);
                        mem.Write(copy, mem.ReadBytes(boneInfo, (int)length));
                        target.SetPointer(variant + ModelBoneInfo, copy, PointerEncoding.Inline);
                    }
                }
                uint mats = context.NewObject(4 * (uint)total, 4);
                for (int k = 0; k < total; k++)
                {
                    target.SetPointer(mats + 4 * (uint)k, receiver[k] ? materials[camo] : hidden, PointerEncoding.Unknown);
                }
                target.SetPointer(variant + ModelMaterials, mats, PointerEncoding.Inline);
                target.SetPointer(variant, context.NewString(string.Format(baseName, Camo(camo))), PointerEncoding.Inline);
                models.Add(variant);
            }
            log.Add($"{sourceName} -> {string.Format(baseName, "blk")} .. {string.Format(baseName, "21")}");
        }

        // the writer places each shared object at its first pointer and aliases it at the others
        foreach (uint field in target.Pointers.Enumerate(0, uint.MaxValue).ToList())
        {
            if (target.AssetHeaders.TryGetValue(target.Memory.U32(field), out int type) && type != TypeXModelSurfs
                && target.Encoding(field) is PointerEncoding.Insert)
            {
                target.SetEncoding(field, PointerEncoding.Unknown);
            }
        }
        target.Assets.Clear();
        target.Assets.AddRange(models.Select(m => new ZoneImageAsset(TypeXModel, m)));

        foreach (string line in log)
        {
            Console.WriteLine("  " + line);
        }
        foreach (var group in context.Warnings.GroupBy(w => w).Take(20))
        {
            Console.WriteLine($"  warning: {group.Key}{(group.Count() > 1 ? $" (x{group.Count()})" : string.Empty)}");
        }

        const string zoneName = "mp_weapons_postgfx_mp";
        var written = ZoneWriter.Write(build, target, zoneName);
        Console.WriteLine($"wrote a {written.Zone.Length}-byte zone, {written.Warnings.Count} writer warnings");
        foreach (string warning in written.Warnings.Distinct().Take(20))
        {
            Console.WriteLine($"  writer warning: {warning}");
        }
        if (zoneOutput is not null)
        {
            File.WriteAllBytes(zoneOutput, written.Zone);
        }
        var reload = new ZoneLoadEmulator(build).Load(zoneName, written.Zone);
        Console.WriteLine($"the 1.1.1.13 loaders read it back: {reload.Assets.Count} assets, {reload.Warnings.Count} warnings");
        foreach (string warning in reload.Warnings.Take(10))
        {
            Console.WriteLine($"  reload warning: {warning}");
        }
        var back = ZoneCapture.Capture(build, zoneName, written.Zone).Image;
        var byType = back.AssetHeaders.GroupBy(h => h.Value).OrderBy(g => g.Key).Select(g => $"{g.Key}:{g.Count()}");
        Console.WriteLine($"asset headers by type: {string.Join(" ", byType)}");
        var duplicates = back.AssetHeaders.Keys.Select(h => (Type: back.AssetHeaders[h], Name: NameOf(back, h, back.AssetHeaders[h])))
            .Where(x => x.Name is not null).GroupBy(x => x).Where(g => g.Count() > 1).ToList();
        Console.WriteLine($"assets loaded more than once: {duplicates.Count}" +
            (duplicates.Count > 0 ? " (" + string.Join(", ", duplicates.Take(12).Select(g => $"{g.Key.Type} {g.Key.Name} x{g.Count()}")) + ")" : string.Empty));

        using var key = FastFileKeyProvider.LoadFromExecutable(exe);
        byte[] file = FastFileWriter.WriteUnsigned(build, written.Zone, key);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        File.WriteAllBytes(output, file);
        Console.WriteLine($"wrote {output} ({file.Length} bytes)");
        return 0;
    }
}
