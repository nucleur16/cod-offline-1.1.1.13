using System.IO.Compression;
using System.Text;
using System.Text.Json;
using Codol.Assets.T5;
using Codol.FastFiles;
using Codol.FastFiles.Crypto;
using Codol.FastFiles.Emulation;
using Codol.FastFiles.FastFile;
using Codol.FastFiles.Iwd;
using Codol.FastFiles.Porting;
using Codol.FastFiles.T5.Porting;

namespace Codol.SoundDumper;

/// zm-wallbuy: the chalk outlines of the zombies wall buys the CoDOL ports of T5's maps lack, for 1.1.1.13.
///
/// The wall buys themselves (the usable entity and the weapon model) are spawned by the server's GSC
/// (codol_rawfiles maps/_zombiemode_weapons.gsc); what only the client can draw is here. CoDOL marks its own wall buys
/// (zm_prototype_sh, zm_asylum_sh) with a BSP decal in wc/glow_weapon_&lt;name&gt; (the neon outline images
/// glow_weapon_&lt;name&gt;_c, streamed from main\*.iwd) and a createfx oneshot of misc/glow_weapon_fx. A BSP decal cannot be
/// added to a built map, so the outline is an effect: one looping "sprite oriented" element (a quad in the effect's
/// left/up plane, facing its forward axis, half extents = the visual state's size) in an additive copy of the
/// glow_weapon_fx flare material whose colour map is the outline image, played by C1's own createfx reader
/// (maps/createfx/&lt;map&gt;_fx.gsc, with the fxid mapped in maps/mp/&lt;map&gt;_fx.gsc), with the CoDOL flare beside it.
/// The grenade walls have no CoDOL outline: Black Ops' chalk (grenade_bag_chalk_c, chalk_weapon_explosives_c) is ported
/// into images/*.iwi of an IWD (IWI 13 -> 9) and drawn the same way.
///
/// Two outputs:
///   --input &lt;map.ff&gt;  the map zone itself, edited in place (the ported, unsigned zm_theater_sh / zm_factory_sh /
///                      zm_sumpf_sh): the effects, materials and images, and the two rawfiles.
///   --new &lt;name&gt;      a zone of its own for Kernel33's client_shipRetail\patch folder (loaded ahead of every stock zone,
///                      so its rawfiles win): for the stock zm_asylum_sh / zm_prototype_sh, whose zones C1's pure check
///                      pins by size. It carries the assets and full copies of those maps' two fx rawfiles plus the walls.
/// --rawfile &lt;name&gt;=&lt;file&gt; replaces a rawfile's text before the walls are added (another worker's createfx edit).
/// Idempotent: entries and mappings whose fxid starts "wallbuy_" are removed before they are written again.
internal static class WallbuyCommands
{
    private const int TypeXModel = 4, TypeMaterial = 5, TypeTechset = 9, TypeImage = 10, TypeMapEnts = 19, TypeFx = 30, TypeRawFile = 36;
    /// CoDOL's weapon case on every one of its own wall buys (zm_prototype_sh / zm_asylum_sh static models, MEASURED:
    /// tool_src/terminals.py). A map zone the walls are added to gets the model (from zm_asylum_sh, top-level, so the
    /// server and C1 find it by name); the server's GSC places one per new wall (spec "terminal").
    private const string TerminalModel = "codo_weapon_box";
    private const uint HashColorMap = 0xA0AB1041;
    private const string FlareFx = "misc/glow_weapon_fx";
    private const string FlareMaterial = "gfx_spotlight_lensflare_rainbow_z5_noeye";
    private const string GlowFxId = "weapon_glow";
    private const uint ElemSize = 260;

    public static int Run(string command, CommandLine cli) => command switch
    {
        "zm-wallbuy" => Build(cli),
        _ => throw new ArgumentException($"unknown command '{command}'"),
    };

    private sealed record Chalk(string Image, float HalfW, float HalfH, bool Atlas, float[] Origin, float[] Angles);
    private sealed record Buy(string Id, string Weapon, Chalk Chalk, float[] GlowOrigin, float[] GlowAngles);

    private sealed class EffectDef
    {
        public required string Key, Image, Name, FxId, Material;
        public required float HalfW, HalfH;
        public required bool Atlas;
    }

    private static string? NameOf(ZoneImage image, uint header, int type)
    {
        uint p = image.Pointer(header + (type == TypeImage ? 0x20u : 0u));
        return p == 0 ? null : image.Memory.CString(p);
    }

    private static Dictionary<(int, string), uint> Named(ZoneImage image)
    {
        var names = new Dictionary<(int, string), uint>();
        foreach (var (header, type) in image.AssetHeaders)
        {
            string? name = NameOf(image, header, type);
            if (name is not null && !name.StartsWith(','))
            {
                names.TryAdd((type, name), header);
            }
        }
        return names;
    }

    private static ZoneCaptureResult Capture(string input, string exe)
    {
        using var rsa = Program.LoadKeyFor(input, exe, null);
        var contents = new FastFileReader(rsa).Read(input);
        return ZoneCapture.Capture(contents.Build, contents.Name, contents.Zone);
    }

    private static List<(string Map, List<Buy> Buys)> ReadSpec(string path, IReadOnlyCollection<string> maps)
    {
        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        float[] F(JsonElement e) => e.EnumerateArray().Select(v => (float)v.GetDouble()).ToArray();
        var result = new List<(string, List<Buy>)>();
        foreach (var m in doc.RootElement.GetProperty("maps").EnumerateArray())
        {
            string map = m.GetProperty("map").GetString()!;
            if (!maps.Contains(map, StringComparer.OrdinalIgnoreCase))
            {
                continue;
            }
            var buys = new List<Buy>();
            foreach (var b in m.GetProperty("buys").EnumerateArray())
            {
                var c = b.GetProperty("chalk");
                var half = F(c.GetProperty("half"));
                var g = b.GetProperty("glow");
                buys.Add(new Buy(b.GetProperty("id").GetString()!, b.GetProperty("weapon").GetString()!,
                    new Chalk(c.GetProperty("image").GetString()!, half[0], half[1], c.GetProperty("atlas").GetBoolean(),
                        F(c.GetProperty("origin")), F(c.GetProperty("angles"))),
                    F(g.GetProperty("origin")), F(g.GetProperty("angles"))));
            }
            result.Add((map, buys));
        }
        foreach (string map in maps.Where(n => !result.Any(r => r.Item1.Equals(n, StringComparison.OrdinalIgnoreCase))))
        {
            throw new InvalidDataException($"{map} is not in {path}");
        }
        return result;
    }

    /// "misc/ol_wallbuy_&lt;key&gt;", padded so that the name with its terminator is a multiple of 4 bytes (FxEffectDef
    /// totalSize = 32 + name + elements + their samples, MEASURED on misc/glow_weapon_fx: 0x47C)
    private static string EffectName(string key)
    {
        string name = "misc/ol_wallbuy_" + key;
        while ((name.Length + 1) % 4 != 0)
        {
            name += "_";
        }
        return name;
    }

    private static int Build(CommandLine cli)
    {
        string game = cli.Required("game");
        string t5Main = cli.Required("t5");
        string specPath = cli.Required("spec");
        string? input = cli.Value("input");
        string? newZone = cli.Value("new");
        string output = cli.Required("out");
        string? iwdOutput = cli.Value("iwd");
        string? zoneOutput = cli.Value("zone");
        // --iwd-extra <image> (repeatable): a Black Ops chalk image the IWD carries although this run draws no wall with it,
        // so that one iw_image_NN.iwd holds the images of every map
        var iwdExtra = cli.Values("iwd-extra").ToList();
        var mapList = cli.Values("map").SelectMany(v => v.Split(',', StringSplitOptions.RemoveEmptyEntries)).ToList();
        var rawOverrides = cli.Values("rawfile").Select(v =>
        {
            int eq = v.IndexOf('=');
            return eq <= 0 ? throw new ArgumentException($"--rawfile takes <name>=<file>, not '{v}'") : (Name: v[..eq], File: v[(eq + 1)..]);
        }).ToList();
        cli.EnsureAllConsumed();
        if ((input is null) == (newZone is null))
        {
            throw new ArgumentException("pass exactly one of --input <map.ff> and --new <zone name>");
        }

        var build = GameBuild.V1_1_1_13;
        string zones = Path.Combine(game, "client_shipRetail", "chinese");
        string exe = Path.Combine(game, "C1.exe");
        if (mapList.Count == 0 && input is not null)
        {
            mapList.Add(Path.GetFileNameWithoutExtension(input));
        }
        var spec = ReadSpec(specPath, mapList);

        // donors: zm_prototype_sh (misc/glow_weapon_fx and its flare material), zm_asylum_sh (all nine glow_weapon_*_c
        // images and a 512x256 DXT1 header), both stock CoDOL zones
        var proto = Capture(Path.Combine(zones, "zm_prototype_sh.ff"), exe).Image;
        var asylum = Capture(Path.Combine(zones, "zm_asylum_sh.ff"), exe).Image;
        var protoNames = Named(proto);
        var asylumNames = Named(asylum);
        var donors = new Dictionary<string, (ZoneImage Image, Dictionary<(int, string), uint> Names)>(StringComparer.OrdinalIgnoreCase)
        {
            ["zm_prototype_sh"] = (proto, protoNames),
            ["zm_asylum_sh"] = (asylum, asylumNames),
        };

        ZoneCaptureResult? baseCapture = null;
        ZoneImage target;
        PortContext context;
        string zoneName;
        if (input is not null)
        {
            baseCapture = Capture(input, exe);
            target = baseCapture.Image;
            zoneName = Path.GetFileNameWithoutExtension(input);
            context = PortContext.InPlace(build, target, 0xD0000000, 0xEF000000, [3, 4], new() { [4] = (4, 0x2C) },
                new() { [28] = [(0x3C, 32), (0x2BC, 16), (0x2DC, 16)] });
        }
        else
        {
            context = new PortContext(build, proto, build);
            target = context.Target;
            target.ScriptStrings.Clear();
            zoneName = newZone!;
        }
        var log = new List<string>();
        uint firstNew = context.Allocate(0);
        var targetNames = Named(target);

        // ---- images ----
        var iwdEntries = new List<IwdWriteEntry>();
        var images = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase);
        var newTop = new List<(int Type, uint Header)>();
        uint ImageFor(string name)
        {
            if (images.TryGetValue(name, out uint known))
            {
                return known;
            }
            uint image;
            if (targetNames.TryGetValue((TypeImage, name), out uint own))
            {
                image = own;
                log.Add($"image {name}: the zone's own");
            }
            else if (asylumNames.TryGetValue((TypeImage, name), out uint donor))
            {
                image = context.CopyTargetAsset(asylum, donor);
                newTop.Add((TypeImage, image));
                log.Add($"image {name}: zm_asylum_sh's definition (streamed from main\\*.iwd)");
            }
            else
            {
                // Black Ops' chalk: IWI 13 from the T5 install, as IWI 9, behind a header copied from a stock image of the same
                // size and format (C1 streams images/<name>.iwi)
                byte[] iwi = T5Image(t5Main, $"images/{name}.iwi");
                int w = BitConverter.ToUInt16(iwi, 0xA), h = BitConverter.ToUInt16(iwi, 0xC);
                var likeImage = asylum;
                uint? found = FindStockImage(asylum, w, h, iwi[8]);
                if (found is null)
                {
                    likeImage = proto;
                    found = FindStockImage(proto, w, h, iwi[8]);
                }
                uint like = found ?? throw new InvalidOperationException($"no stock {w}x{h} format {iwi[8]} image header to copy for {name}");
                image = context.CopyFromTarget(likeImage, like);
                target.SetPointer(image + 0x20, context.NewString(name), PointerEncoding.Inline);
                target.AssetHeaders[image] = TypeImage;
                iwi = MatchImage(target, image, iwi);
                iwdEntries.Add(new IwdWriteEntry($"images/{name}.iwi", iwi));
                newTop.Add((TypeImage, image));
                log.Add($"image {name}: Black Ops chalk {w}x{h}, header of {NameOf(likeImage, like, TypeImage)}, file images/{name}.iwi ({iwi.Length} bytes) for the IWD");
            }
            images[name] = image;
            return image;
        }

        // ---- effects (one per outline image and size) ----
        var effects = new Dictionary<string, EffectDef>(StringComparer.OrdinalIgnoreCase);
        var buyEffect = new Dictionary<Buy, EffectDef>();
        foreach (var (_, buys) in spec)
        {
            foreach (var buy in buys)
            {
                string ekey = buy.Chalk.Image.EndsWith("_c", StringComparison.Ordinal) ? buy.Chalk.Image[..^2] : buy.Chalk.Image;
                ekey = ekey.Replace("glow_weapon_", "glow_").Replace("chalk_weapon_", "chalk_");
                var existing = effects.Values.FirstOrDefault(e => e.Image == buy.Chalk.Image && Math.Abs(e.HalfW - buy.Chalk.HalfW) < 0.05f &&
                    Math.Abs(e.HalfH - buy.Chalk.HalfH) < 0.05f && e.Atlas == buy.Chalk.Atlas);
                if (existing is null)
                {
                    string k = ekey;
                    for (int n = 2; effects.ContainsKey(k); n++)
                    {
                        k = $"{ekey}{n}";
                    }
                    existing = new EffectDef
                    {
                        Key = k, Image = buy.Chalk.Image, Name = EffectName(k), FxId = "wallbuy_" + k, Material = "ol_wallbuy_" + k,
                        HalfW = buy.Chalk.HalfW, HalfH = buy.Chalk.HalfH, Atlas = buy.Chalk.Atlas,
                    };
                    effects[k] = existing;
                }
                buyEffect[buy] = existing;
            }
        }

        // the extra IWD images get a header here too, so that the file's flags and format agree with a header C1 links
        foreach (string extra in iwdExtra)
        {
            ImageFor(extra);
        }

        // the flare (CoDOL's own wall-buy glow), when the zone lacks it
        bool needFlare = !targetNames.ContainsKey((TypeFx, FlareFx));
        uint protoFlare = protoNames[(TypeFx, FlareFx)];
        uint protoFlareMaterial = protoNames[(TypeMaterial, FlareMaterial)];
        if (needFlare)
        {
            uint flare = context.CopyTargetAsset(proto, protoFlare);
            newTop.Add((TypeFx, flare));
            log.Add($"fx {FlareFx}: zm_prototype_sh's definition");
        }

        // the outline material: a copy of the flare's (additive, ",effect_add_nofog"), its colour map the outline
        uint baseMaterial = context.CopyTargetAsset(proto, protoFlareMaterial);
        uint techset = target.Pointer(baseMaterial + 0x50);
        var t = target.Memory;
        var materials = new Dictionary<string, uint>();
        // a re-run over an already patched zone keeps the outline effects it finds (same names, same definition)
        var existingFx = effects.Values.Where(fx => targetNames.ContainsKey((TypeFx, fx.Name))).Select(fx => fx.Key).ToHashSet();
        foreach (string k in existingFx)
        {
            log.Add($"fx {effects[k].Name}: already in the zone, kept");
        }
        foreach (var fx in effects.Values.Where(fx => !existingFx.Contains(fx.Key)))
        {
            uint image = ImageFor(fx.Image);
            uint m = context.NewObject(0x60);
            t.Write(m, proto.Memory.ReadBytes(protoFlareMaterial, 0x60));
            target.SetPointer(m, context.NewString(fx.Material), PointerEncoding.Inline);
            target.SetPointer(m + 0x50, techset, PointerEncoding.Unknown);
            int textures = proto.Memory.U8(protoFlareMaterial + 0x48), constants = proto.Memory.U8(protoFlareMaterial + 0x49),
                stateBits = proto.Memory.U8(protoFlareMaterial + 0x4A);
            uint protoTable = proto.Pointer(protoFlareMaterial + 0x54);
            if (textures != 1 || proto.Memory.U32(protoTable) != HashColorMap)
            {
                throw new InvalidDataException($"{FlareMaterial}: expected one colorMap texture, has {textures}");
            }
            uint table = context.NewObject(12);
            t.Write(table, proto.Memory.ReadBytes(protoTable, 8));
            target.SetPointer(table + 8, image, PointerEncoding.Unknown);
            target.SetPointer(m + 0x54, table, PointerEncoding.Inline);
            uint consts = context.NewObject((uint)(32 * constants), 16);
            t.Write(consts, proto.Memory.ReadBytes(proto.Pointer(protoFlareMaterial + 0x58), 32 * constants));
            target.SetPointer(m + 0x58, consts, PointerEncoding.Inline);
            uint bits = context.NewObject((uint)(8 * stateBits));
            t.Write(bits, proto.Memory.ReadBytes(proto.Pointer(protoFlareMaterial + 0x5C), 8 * stateBits));
            target.SetPointer(m + 0x5C, bits, PointerEncoding.Inline);
            target.AssetHeaders[m] = TypeMaterial;
            materials[fx.Key] = m;
            newTop.Add((TypeMaterial, m));
        }
        // the flare material copy only lent its techset; it is not an asset of the zone unless the flare effect uses it
        if (!target.Pointers.Enumerate(0, uint.MaxValue).Any(f => t.U32(f) == baseMaterial && f != baseMaterial))
        {
            target.AssetHeaders.Remove(baseMaterial);
        }

        // the effects: misc/glow_weapon_fx's first element, looping, alone
        uint protoElems = proto.Pointer(protoFlare + 28);
        if (proto.Memory.U8(protoElems + 176) != 1 || proto.Memory.U8(protoElems + 177) != 1 || proto.Memory.U8(protoElems + 178) != 1 ||
            proto.Memory.U8(protoElems + 179) != 1)
        {
            throw new InvalidDataException($"{FlareFx}: element 0 is not the one-visual oriented sprite this command expects");
        }
        var elemPointers = proto.Pointers.Enumerate(protoElems, ElemSize).Select(f => f - protoElems).ToList();
        if (!elemPointers.SequenceEqual(new uint[] { 180, 184, 188 }))
        {
            throw new InvalidDataException($"{FlareFx}: element 0 holds pointers at {string.Join(",", elemPointers)}, expected 180, 184, 188");
        }
        foreach (var fx in effects.Values.Where(fx => !existingFx.Contains(fx.Key)))
        {
            uint def = context.NewObject(32);
            t.Write(def, proto.Memory.ReadBytes(protoFlare, 32));
            target.SetPointer(def, context.NewString(fx.Name), PointerEncoding.Inline);
            uint elem = context.NewObject(ElemSize);
            t.Write(elem, proto.Memory.ReadBytes(protoElems, (int)ElemSize));
            uint vel = context.NewObject(192);
            t.Write(vel, new byte[192]);
            uint vis = context.NewObject(96);
            byte[] state = new byte[96];
            // two samples x {base, amplitude} x FxElemVisualState { color[4] RGBA, rotationDelta, rotationTotal, size[2], scale }
            byte level = (byte)(fx.Image.StartsWith("glow_weapon_", StringComparison.Ordinal) ? 255 : 190);
            for (int s = 0; s < 2; s++)
            {
                int b = s * 48;
                state[b] = state[b + 1] = state[b + 2] = level; state[b + 3] = 255;
                BitConverter.TryWriteBytes(state.AsSpan(b + 12), fx.HalfW);
                BitConverter.TryWriteBytes(state.AsSpan(b + 16), fx.HalfH);
                state[b + 24] = state[b + 25] = state[b + 26] = level; state[b + 27] = 255;  // the second colour
            }
            t.Write(vis, state);
            target.SetPointer(elem + 180, vel, PointerEncoding.Inline);
            target.SetPointer(elem + 184, vis, PointerEncoding.Inline);
            target.SetPointer(elem + 188, materials[fx.Key], PointerEncoding.Unknown);
            // spawn within 3000 units, fading from 2500 (the flare: 900 / 700); frustum-cull radius covers the quad
            t.WF32(elem + 12, 0); t.WF32(elem + 16, 3000);
            t.WF32(elem + 20, 2500); t.WF32(elem + 24, 500);
            t.WF32(elem + 36, Math.Max(fx.HalfW, fx.HalfH) + 8);
            if (fx.Atlas)
            {
                // 2 x 1 atlas, the right half, fixed (Black Ops' quad uses u 0.47..0.99 of chalk_weapon_explosives_c)
                t.Write(elem + 168, new byte[] { 0, 1, 0, 1, 1, 0, 2, 0 });
            }
            t.WU32(def + 12, 0x7FFFFFFF);
            t.WU32(def + 16, 1); t.WU32(def + 20, 0); t.WU32(def + 24, 0);
            target.SetPointer(def + 28, elem, PointerEncoding.Inline);
            t.WU32(def + 8, (uint)(32 + fx.Name.Length + 1 + ElemSize + 192 + 96));
            target.AssetHeaders[def] = TypeFx;
            newTop.Add((TypeFx, def));
            log.Add($"fx {fx.Name}: outline {fx.Image} {fx.HalfW * 2:0.##} x {fx.HalfH * 2:0.##}{(fx.Atlas ? ", right half" : string.Empty)}, material {fx.Material}");
        }

        // ---- rawfiles ----
        var overrides = rawOverrides.ToDictionary(o => o.Name, o => File.ReadAllText(o.File), StringComparer.OrdinalIgnoreCase);
        foreach (var (map, buys) in spec)
        {
            string fxName = $"maps/mp/{map}_fx.gsc", createName = $"maps/createfx/{map}_fx.gsc";
            uint fxRaw = RawHeader(context, target, targetNames, donors, map, fxName, newTop);
            uint createRaw = RawHeader(context, target, targetNames, donors, map, createName, newTop);
            string fxText = overrides.TryGetValue(fxName, out string? o1) ? o1 : RawText(target, fxRaw);
            string createText = overrides.TryGetValue(createName, out string? o2) ? o2 : RawText(target, createRaw);

            // the fxid map: drop earlier wallbuy_ lines, add one per effect this map uses (and weapon_glow if absent)
            var lines = fxText.Replace("\r\n", "\n").Split('\n').Where(l => !l.Contains("level._effect[\"wallbuy_")).ToList();
            var add = new List<string>();
            if (!fxText.Contains($"level._effect[\"{GlowFxId}\"]", StringComparison.OrdinalIgnoreCase))
            {
                add.Add($"\tlevel._effect[\"{GlowFxId}\"] = loadfx(\"{FlareFx}\");");
            }
            foreach (var fx in buys.Select(b => buyEffect[b]).Distinct())
            {
                add.Add($"\tlevel._effect[\"{fx.FxId}\"] = loadfx(\"{fx.Name}\");");
            }
            int at = lines.FindIndex(l => l.TrimStart().StartsWith("/#", StringComparison.Ordinal));
            if (at < 0)
            {
                at = lines.FindLastIndex(l => l.Trim() == "}");
            }
            if (at < 0)
            {
                throw new InvalidDataException($"{fxName}: no main() end to add the fx map before");
            }
            lines.InsertRange(at, add);
            fxText = string.Join("\n", lines);

            // the placements: drop earlier wallbuy_ entries (each a block up to the blank line), add the chalk and the flare
            var kept = new List<string>();
            var cl = createText.Replace("\r\n", "\n").Split('\n');
            for (int i = 0; i < cl.Length; i++)
            {
                if (cl[i].TrimStart().StartsWith("ent = createOneshotEffect( \"wallbuy_", StringComparison.Ordinal) ||
                    (cl[i].TrimStart().StartsWith($"ent = createOneshotEffect( \"{GlowFxId}\" );", StringComparison.Ordinal) && WallbuyFlareBlock(cl, i)))
                {
                    while (i < cl.Length && cl[i].Trim().Length > 0)
                    {
                        i++;
                    }
                    continue;
                }
                kept.Add(cl[i]);
            }
            int end = kept.FindLastIndex(l => l.Trim() == "}");
            if (end < 0)
            {
                throw new InvalidDataException($"{createName}: no closing brace");
            }
            var block = new List<string>();
            foreach (var buy in buys)
            {
                var fx = buyEffect[buy];
                block.AddRange(OneShot(fx.FxId, buy.Chalk.Origin, buy.Chalk.Angles));
                // the flare entries carry a marker delay (-15.25; CoDOL's are -15) so that a re-run finds its own
                block.AddRange(OneShot(GlowFxId, buy.GlowOrigin, buy.GlowAngles, "-15.25"));
            }
            kept.InsertRange(end, block);
            createText = string.Join("\n", kept);

            SetRawText(context, target, fxRaw, fxText);
            SetRawText(context, target, createRaw, createText);
            log.Add($"{map}: {buys.Count} wall buys -> {fxName} (+{add.Count} fx ids), {createName} (+{2 * buys.Count} oneshots)");
        }

        // ---- the wall-buy terminal model ----
        if (input is not null && spec.Any(m => m.Buys.Count > 0))
        {
            if (targetNames.TryGetValue((TypeXModel, TerminalModel), out _))
            {
                log.Add($"xmodel {TerminalModel}: the zone's own");
            }
            else
            {
                uint terminal = context.CopyTargetAsset(asylum, asylumNames[(TypeXModel, TerminalModel)]);
                newTop.Add((TypeXModel, terminal));
                log.Add($"xmodel {TerminalModel}: zm_asylum_sh's definition, top-level (the wall-buy terminal)");
            }
        }

        // ---- asset list, encodings ----
        for (int i = newTop.Count - 1; i >= 0; i--)
        {
            if (!target.Assets.Any(a => a.Header == newTop[i].Header))
            {
                target.Assets.Insert(0, new ZoneImageAsset(newTop[i].Type, newTop[i].Header));
            }
        }
        uint end2 = context.Allocate(0);
        foreach (uint field in target.Pointers.Enumerate(firstNew, end2 - firstNew).ToList())
        {
            // as zm-pap-patch: the donors' encodings describe where the donor placed the data, not where this zone does
            target.SetEncoding(field, PointerEncoding.Unknown);
            target.RawReferences.Remove(field);
        }

        foreach (string line in log)
        {
            Console.WriteLine("  " + line);
        }
        foreach (var group in context.Warnings.GroupBy(w => w).Take(10))
        {
            Console.WriteLine($"  warning: {group.Key}{(group.Count() > 1 ? $" (x{group.Count()})" : string.Empty)}");
        }

        var written = ZoneWriter.Write(build, target, zoneName);
        Console.WriteLine($"wrote a {written.Zone.Length}-byte zone ({target.Assets.Count} assets{(baseCapture is not null ? $", stock {baseCapture.Load.Assets.Count}" : string.Empty)}); {written.Warnings.Count} writer warnings");
        foreach (string w in written.Warnings.Where(w => !w.Contains("is aliased before it is loaded")).Distinct().Take(10))
        {
            Console.WriteLine($"  writer warning: {w}");
        }
        if (zoneOutput is not null)
        {
            File.WriteAllBytes(zoneOutput, written.Zone);
        }
        var reload = new ZoneLoadEmulator(build).Load(zoneName, written.Zone);
        Console.WriteLine($"the 1.1.1.13 loaders read it back: {reload.Assets.Count} assets, {reload.Warnings.Count} warnings");
        foreach (string w in reload.Warnings.Take(10))
        {
            Console.WriteLine($"  reload warning: {w}");
        }
        if (reload.Warnings.Count > 0 || reload.Assets.Count != target.Assets.Count)
        {
            throw new InvalidDataException("the written zone does not read back cleanly");
        }
        // what C1 will see: the rawfiles and effects of the written zone
        var back = ZoneCapture.Capture(build, zoneName, written.Zone).Image;
        var backNames = Named(back);
        foreach (var fx in effects.Values)
        {
            if (!backNames.ContainsKey((TypeFx, fx.Name)) || !backNames.ContainsKey((TypeMaterial, fx.Material)) || !backNames.ContainsKey((TypeImage, fx.Image)))
            {
                throw new InvalidDataException($"{fx.Name}: missing from the written zone");
            }
        }
        if (input is not null && spec.Any(m => m.Buys.Count > 0) && !backNames.ContainsKey((TypeXModel, TerminalModel)))
        {
            throw new InvalidDataException($"{TerminalModel}: missing from the written zone");
        }
        foreach (var (map, buys) in spec)
        {
            string text = RawText(back, backNames[(TypeRawFile, $"maps/createfx/{map}_fx.gsc")]);
            int count = text.Split('\n').Count(l => l.Contains("createOneshotEffect( \"wallbuy_"));
            Console.WriteLine($"  {map}: the written createfx holds {count} wall-buy outlines");
            if (count != buys.Count)
            {
                throw new InvalidDataException($"{map}: createfx holds {count} outlines, expected {buys.Count}");
            }
        }

        using var key = FastFileKeyProvider.LoadFromExecutable(exe);
        byte[] file = FastFileWriter.WriteUnsigned(build, written.Zone, key);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        File.WriteAllBytes(output, file);
        Console.WriteLine($"wrote {output} ({file.Length} bytes)");

        if (iwdEntries.Count > 0)
        {
            if (iwdOutput is null)
            {
                throw new ArgumentException($"the outlines need {iwdEntries.Count} Black Ops images: pass --iwd <iw_image_NN.iwd>");
            }
            using var install = Codol.Assets.Game.GameInstallation.Open(game);
            IwdWriter.Write(iwdOutput, iwdEntries, install.IwdKey);
            var check = IwdArchive.Open(iwdOutput, install.IwdKey);
            Console.WriteLine($"wrote {iwdOutput}: {string.Join(", ", check.Entries.Select(e => $"{e.Name} ({check.Read(e).Length} bytes)"))}");
        }
        return 0;
    }

    /// a flare oneshot this command wrote: the "weapon_glow" block whose delay is the marker -15.25
    private static bool WallbuyFlareBlock(string[] lines, int start)
    {
        for (int i = start; i < lines.Length && lines[i].Trim().Length > 0; i++)
        {
            if (lines[i].Contains("ent.v[ \"delay\" ] = -15.25;", StringComparison.Ordinal))
            {
                return true;
            }
        }
        return false;
    }

    private static string Num(float v)
    {
        string text = Math.Round(v, 4).ToString("0.####", System.Globalization.CultureInfo.InvariantCulture);
        return text == "-0" ? "0" : text;
    }

    private static IEnumerable<string> OneShot(string fxid, float[] origin, float[] angles, string delay = "-15")
    {
        yield return $"\tent = createOneshotEffect( \"{fxid}\" );";
        yield return $"\tent.v[ \"origin\" ] = ( {Num(origin[0])}, {Num(origin[1])}, {Num(origin[2])} );";
        yield return $"\tent.v[ \"angles\" ] = ( {Num(angles[0])}, {Num(angles[1])}, {Num(angles[2])} );";
        yield return $"\tent.v[ \"fxid\" ] = \"{fxid}\";";
        yield return $"\tent.v[ \"delay\" ] = {delay};";
        yield return string.Empty;
    }

    /// the zone's rawfile of that name, or (for a zone of its own) a copy of the stock map zone's
    private static uint RawHeader(PortContext context, ZoneImage target, Dictionary<(int, string), uint> targetNames,
        Dictionary<string, (ZoneImage Image, Dictionary<(int, string), uint> Names)> donors, string map, string name,
        List<(int Type, uint Header)> newTop)
    {
        if (targetNames.TryGetValue((TypeRawFile, name), out uint own))
        {
            return own;
        }
        if (!donors.TryGetValue(map, out var donor) || !donor.Names.TryGetValue((TypeRawFile, name), out uint stock))
        {
            throw new InvalidDataException($"{name}: neither the zone nor a stock map zone holds it");
        }
        uint copy = context.CopyTargetAsset(donor.Image, stock);
        newTop.Add((TypeRawFile, copy));
        targetNames[(TypeRawFile, name)] = copy;
        return copy;
    }

    /// RawFile: name +0, compressedLen +4, len +8, buffer +12
    private static string RawText(ZoneImage image, uint header)
    {
        var m = image.Memory;
        int compressed = m.I32(header + 4), length = m.I32(header + 8);
        uint buffer = image.Pointer(header + 12);
        byte[] bytes;
        if (compressed == 0)
        {
            bytes = m.ReadBytes(buffer, length);
        }
        else
        {
            using var z = new ZLibStream(new MemoryStream(m.ReadBytes(buffer, compressed)), CompressionMode.Decompress);
            bytes = new byte[length];
            z.ReadExactly(bytes);
        }
        return Encoding.Latin1.GetString(bytes).TrimEnd('\0');
    }

    /// written back uncompressed, as port-map's createfx edits are
    private static void SetRawText(PortContext context, ZoneImage target, uint header, string text)
    {
        byte[] contents = Encoding.Latin1.GetBytes(text);
        uint fresh = context.NewObject((uint)contents.Length + 1, 1);
        target.Memory.Write(fresh, contents);
        target.Memory.WU8(fresh + (uint)contents.Length, 0);
        target.Memory.WU32(header + 4, 0);
        target.Memory.WU32(header + 8, (uint)contents.Length);
        target.SetPointer(header + 12, fresh, PointerEncoding.Inline);
    }

    /// a streamed stock image of the given size and IWI format (11 DXT1, 13 DXT5): GfxImageLoadDef +8 is the D3DFORMAT
    private static uint? FindStockImage(ZoneImage image, int width, int height, byte iwiFormat)
    {
        uint fourcc = iwiFormat switch { 11 => 0x31545844u, 12 => 0x33545844u, 13 => 0x35545844u, _ => 0 };
        foreach (var (header, type) in image.AssetHeaders.OrderBy(h => h.Key))
        {
            if (type != TypeImage || NameOf(image, header, TypeImage) is not { } name || name.StartsWith(',') || name.StartsWith('*') || name.StartsWith('$'))
            {
                continue;
            }
            uint loadDef = image.Pointer(header);
            if (loadDef != 0 && image.Memory.U16(header + 0x14) == width && image.Memory.U16(header + 0x16) == height &&
                image.Memory.U32(loadDef + 8) == fourcc)
            {
                return header;
            }
        }
        return null;
    }

    /// IWI 9: +4 flags, +8 format; the image's load definition (+4 flags, +8 D3DFORMAT) and the file must agree
    private static byte[] MatchImage(ZoneImage image, uint header, byte[] iwi)
    {
        uint loadDef = image.Pointer(header);
        uint flags = image.Memory.U32(loadDef + 4);
        System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(iwi.AsSpan(4), flags);
        uint fourcc = iwi[8] switch
        {
            11 => 0x31545844u,
            12 => 0x33545844u,
            13 => 0x35545844u,
            _ => throw new InvalidDataException($"IWI format {iwi[8]} is not a DXT format"),
        };
        image.Memory.WU32(loadDef + 8, fourcc);
        ushort width = BitConverter.ToUInt16(iwi, 0xA), height = BitConverter.ToUInt16(iwi, 0xC);
        if (image.Memory.U16(header + 0x14) != width || image.Memory.U16(header + 0x16) != height)
        {
            throw new InvalidDataException($"IWI is {width}x{height}, the image header says {image.Memory.U16(header + 0x14)}x{image.Memory.U16(header + 0x16)}");
        }
        return iwi;
    }

    /// a Black Ops image (IWI 13) from the install's IWDs, as an IWI 9 file
    private static byte[] T5Image(string t5Main, string path)
    {
        foreach (string iwd in Directory.EnumerateFiles(t5Main, "*.iwd").Order(StringComparer.OrdinalIgnoreCase).Reverse())
        {
            using var zip = ZipFile.OpenRead(iwd);
            var entry = zip.Entries.FirstOrDefault(e => e.FullName.Equals(path, StringComparison.OrdinalIgnoreCase));
            if (entry is null)
            {
                continue;
            }
            using var stream = entry.Open();
            using var memory = new MemoryStream();
            stream.CopyTo(memory);
            return T5Iwi.ToVersion9(path, memory.ToArray());
        }
        throw new FileNotFoundException($"{path} is in none of the IWDs of {t5Main}");
    }
}
