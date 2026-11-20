#!/bin/sh
set -eu

helper=${1:-tools/package/gameserver/rcon_password.sh}
fixture=$(mktemp -d "${TMPDIR:-/tmp}/codol-package-rcon.XXXXXXXX")
trap 'rm -rf -- "$fixture"' EXIT HUP INT TERM
file="$fixture/native_server_rcon.txt"

sh "$helper" "$file"
first=$(cat "$file")
[ "${#first}" -eq 32 ] || exit 1
case "$first" in *[!0-9a-f]*) exit 1 ;; esac
[ "$(stat -c %a "$file")" = 600 ] || exit 1
sh "$helper" "$file"
[ "$(cat "$file")" = "$first" ] || exit 1

printf '%s' 'known-test-credential' > "$file"
sh "$helper" "$file"
[ "$(cat "$file")" = 'known-test-credential' ] || exit 1

printf '%s' 'bad" +set rcon_password stolen' > "$file"
if sh "$helper" "$file" > /dev/null 2>&1; then exit 1; fi
printf '\000' > "$file"
if sh "$helper" "$file" > /dev/null 2>&1; then exit 1; fi
printf 'valid\n\n' > "$file"
if sh "$helper" "$file" > /dev/null 2>&1; then exit 1; fi
printf '%0129d' 0 > "$file"
if sh "$helper" "$file" > /dev/null 2>&1; then exit 1; fi
printf '' > "$file"
if sh "$helper" "$file" > /dev/null 2>&1; then exit 1; fi
if sh "$helper" "$fixture/missing/native_server_rcon.txt" > /dev/null 2>&1; then exit 1; fi

printf 'valid\n' > "$file"
sh "$helper" "$file"
printf 'valid\r\n' > "$file"
sh "$helper" "$file"
[ "$(tr -d '\r\n' < "$file")" = valid ] || exit 1

rm -f -- "$file"
sh "$helper" "$file" &
first_pid=$!
sh "$helper" "$file" &
second_pid=$!
wait "$first_pid"
wait "$second_pid"
sh "$helper" "$file"
[ "$(wc -c < "$file")" -eq 32 ] || exit 1

echo 'CoDOL packaged RCON credential: OK'
