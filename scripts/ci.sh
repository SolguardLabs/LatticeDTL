#!/usr/bin/env bash
set -euo pipefail

if command -v node >/dev/null 2>&1; then
  NODE_BIN=node
elif command -v node.exe >/dev/null 2>&1 && node.exe --version >/dev/null 2>&1; then
  NODE_BIN=node.exe
else
  echo "node is required" >&2
  exit 127
fi

"$NODE_BIN" scripts/build.mjs
"$NODE_BIN" --test "tests/node/*.test.js"
