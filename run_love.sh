#!/usr/bin/env bash
set -euo pipefail

FILE="${1:-examples/01-romantic-hello.love}"
shift || true

BIN="./lovelang"
if [[ -x "./lovelang.exe" ]]; then
  BIN="./lovelang.exe"
fi

"$BIN" "$FILE" "$@"
