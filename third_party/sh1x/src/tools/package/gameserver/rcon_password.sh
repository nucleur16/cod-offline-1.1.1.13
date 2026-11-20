#!/bin/sh
# Prepare the lobby-shared RCON credential without overwriting an existing one.
set -eu

if [ "$#" -ne 1 ]; then
    echo 'rcon credential: expected one file path' >&2
    exit 1
fi

credential_file=$1
credential_dir=$(dirname "$credential_file")
if [ ! -d "$credential_dir" ]; then
    echo 'rcon credential: lobby state directory is missing' >&2
    exit 1
fi

if [ ! -e "$credential_file" ]; then
    generated=$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')
    if [ "${#generated}" -ne 32 ]; then
        echo 'rcon credential: random source did not return 16 bytes' >&2
        exit 1
    fi
    # mktemp creates a private file. Link only after the full value is closed:
    # link(2) refuses an existing destination and publishes the same complete
    # inode to concurrent launchers and the lobby.
    temporary_file=$(umask 077; mktemp "${credential_file}.tmp.XXXXXXXX")
    trap 'rm -f -- "$temporary_file"' EXIT HUP INT TERM
    printf '%s' "$generated" > "$temporary_file"
    if ! ln "$temporary_file" "$credential_file" 2>/dev/null &&
       [ ! -e "$credential_file" ]; then
        echo 'rcon credential: could not publish the credential' >&2
        exit 1
    fi
    rm -f -- "$temporary_file"
    trap - EXIT HUP INT TERM
fi

# Accept the Windows helper's alphabet and optional one-line terminator. Check
# raw bytes, so command substitution cannot hide a NUL or extra newlines.
attempt=0
while [ "$attempt" -lt 20 ]; do
    if [ -f "$credential_file" ] && [ -r "$credential_file" ]; then
        size=$(wc -c < "$credential_file")
        if [ "$size" -le 130 ]; then
            count=0
            suffix=0
            valid=1
            for byte in $(od -An -tu1 "$credential_file"); do
                if [ "$suffix" -eq 0 ]; then
                    if { [ "$byte" -ge 48 ] && [ "$byte" -le 57 ]; } ||
                       { [ "$byte" -ge 65 ] && [ "$byte" -le 90 ]; } ||
                       { [ "$byte" -ge 97 ] && [ "$byte" -le 122 ]; } ||
                       [ "$byte" -eq 45 ] || [ "$byte" -eq 95 ]; then
                        count=$((count + 1))
                    elif [ "$byte" -eq 13 ] && [ "$count" -gt 0 ]; then
                        suffix=1
                    elif [ "$byte" -eq 10 ] && [ "$count" -gt 0 ]; then
                        suffix=2
                    else
                        valid=0
                        break
                    fi
                elif [ "$suffix" -eq 1 ] && [ "$byte" -eq 10 ]; then
                    suffix=2
                else
                    valid=0
                    break
                fi
            done
            if [ "$valid" -eq 1 ] && [ "$count" -ge 1 ] &&
               [ "$count" -le 128 ] && [ "$suffix" -ne 1 ]; then
                exit 0
            fi
        fi
    fi
    attempt=$((attempt + 1))
    sleep 0.05
done

echo 'rcon credential: file is empty, unreadable, or contains unsafe bytes' >&2
exit 1
