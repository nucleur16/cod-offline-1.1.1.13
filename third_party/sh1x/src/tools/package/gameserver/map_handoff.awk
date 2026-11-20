# Parse one lobby startgame.txt decision into safe, bounded command-line tokens.
# The Windows reader in tools/ol_map_handoff.ps1 uses the same age and name
# limits. The lobby publishes this file atomically, one record at a time.
{
    if (NR != 1) { invalid = 1; next }
    sub(/\r$/, "", $0)
    if (NF < 4 || $1 !~ /^[0-9]+([.][0-9]+)?$/) { invalid = 1; next }
    age = now - int($1)
    if (age < -60 || age > 600) { invalid = 1; next }
    for (i = 4; i <= NF; i++) {
        split($i, kv, "=")
        if (kv[1] != "") value[kv[1]] = substr($i, length(kv[1]) + 2)
    }
    if (value["session_map"] == "") value["session_map"] = value["map"]
    if (length(value["map"]) > 64 || value["map"] !~ /^[a-z][a-z0-9_]*$/ ||
        length(value["session_map"]) > 64 || value["session_map"] !~ /^[a-z][a-z0-9_]*$/ ||
        value["type"] !~ /^(mp|coop|zm)$/ ||
        length(value["gametype"]) > 32 || value["gametype"] !~ /^[a-z][a-z0-9_]*$/) {
        invalid = 1; next
    }
    valid = 1
}
END {
    if (NR != 1 || invalid || !valid) exit 1
    print value["map"], value["gametype"], value["type"], value["session_map"]
}
