namespace Codol.SoundDumper;

/// MP weapon zones for 1.1.1.13 (mp_weapons / mp_weapons_postgfx_mp): the inspection helper and the builder.
internal static partial class MpWeaponCommands
{
    public static int Run(string command, CommandLine cli) => command switch
    {
        "mp-zone-inspect" => Inspect(cli),
        "mp-weapons-zone" => Build(cli),
        _ => throw new ArgumentException($"unknown command '{command}'"),
    };
}
