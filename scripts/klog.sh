#!/usr/bin/env bash
# Streams the console's kernel log from klogsrv (port 3232) into logs/klog-<timestamp>.txt and prints
# the file name. klogsrv only sends new messages, so start this before launching a title. Send klogsrv to the
# console first with scripts/send-payload.sh klogsrv (again after every console restart).
# Usage: scripts/klog.sh [seconds]   (default: run until killed)
set -euo pipefail
. "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
[[ -n $PS5_HOST ]] || { echo "Set PS5_HOST in config.local.sh" >&2; exit 2; }
mkdir -p "$MM_PS5_ROOT/logs"
out="$MM_PS5_ROOT/logs/klog-$(date +%Y%m%d-%H%M%S).txt"
echo "$out"
exec python3 - "$PS5_HOST" "${1:-0}" "$out" <<'PY'
import socket, sys, time
host, seconds, path = sys.argv[1], float(sys.argv[2]), sys.argv[3]
deadline = time.monotonic() + seconds if seconds > 0 else None
with socket.create_connection((host, 3232), timeout=10) as connection, open(path, "ab", buffering=0) as log:
    connection.settimeout(1)
    while deadline is None or time.monotonic() < deadline:
        try:
            data = connection.recv(65536)
        except socket.timeout:
            continue
        if not data:
            break
        log.write(data)
PY
