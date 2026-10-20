#!/usr/bin/env bash
# Sends a payload to the console's ELF loader (elfldr, port 9021). With the name of a known payload, downloads
# its pinned release first and checks it.
#
# Usage: scripts/send-payload.sh klogsrv          kernel log server on port 3232, for scripts/klog.sh
#        scripts/send-payload.sh path/to/payload.elf
set -euo pipefail

. "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
[[ -n $PS5_HOST ]] || { echo "Set PS5_HOST in config.local.sh" >&2; exit 2; }

payload=${1:?usage: scripts/send-payload.sh klogsrv|<file.elf>}
if [[ $payload == klogsrv ]]; then
    payload=$MM_PS5_DEPS/downloads/payloads/klogsrv-ps5.elf
    if [[ ! -f $payload ]]; then
        mkdir -p "$(dirname "$payload")"
        curl -fsSL -o "$payload" https://github.com/ps5-payload-dev/klogsrv/releases/download/v0.9/klogsrv-ps5.elf
    fi
    echo "e828ec144231f81547cb58bc7d2c396fa984be0c2295f31364b58017816dcceb  $payload" | sha256sum --check --strict
fi

python3 - "$PS5_HOST" "$ELFLDR_PORT" "$payload" <<'PY'
import socket, sys
host, port, path = sys.argv[1], int(sys.argv[2]), sys.argv[3]
data = open(path, "rb").read()
with socket.create_connection((host, port), timeout=10) as connection:
    connection.sendall(data)
print(f"Sent {path} ({len(data)} bytes) to {host}:{port}")
PY
