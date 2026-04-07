#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR="$(mktemp -d)"
PACK_TGZ=""
PASS_COUNT=0
FAIL_COUNT=0

cleanup() {
  if [[ -n "$PACK_TGZ" && -f "$TMP_DIR/$PACK_TGZ" ]]; then
    rm -f "$TMP_DIR/$PACK_TGZ"
  fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

pass() {
  PASS_COUNT=$((PASS_COUNT + 1))
  echo "PASS: $1"
}

fail() {
  FAIL_COUNT=$((FAIL_COUNT + 1))
  echo "FAIL: $1"
}

run_step() {
  local name="$1"
  shift
  echo
  echo "==> $name"
  if "$@"; then
    pass "$name"
  else
    fail "$name"
  fi
}

run_expect_fail() {
  local name="$1"
  shift
  echo
  echo "==> $name (expected failure)"
  if "$@"; then
    fail "$name (unexpected success)"
  else
    pass "$name"
  fi
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1
}

build_binary() {
  (cd "$ROOT_DIR" && make >/dev/null)
}

run_example() {
  local file="$1"
  (cd "$ROOT_DIR" && ./lovelang "examples/$file" >/dev/null)
}

run_mode() {
  local mode="$1"
  (cd "$ROOT_DIR" && ./lovelang examples/01-romantic-hello.love --mode "$mode" >/dev/null)
}

run_error_example() {
  local file="$1"
  (cd "$ROOT_DIR" && ./lovelang "examples/$file" >/dev/null 2>&1)
}

npm_cli_unit_tests() {
  (cd "$ROOT_DIR/npm/lovelang-runtime" && npm test >/dev/null)
}

pack_cli_package() {
  PACK_TGZ="$(cd "$ROOT_DIR/npm/lovelang-runtime" && npm pack)"
  mv "$ROOT_DIR/npm/lovelang-runtime/$PACK_TGZ" "$TMP_DIR/$PACK_TGZ"
}

offline_install_and_run() {
  local offline_dir="$TMP_DIR/offline"
  mkdir -p "$offline_dir"
  (
    cd "$offline_dir"
    npm init -y >/dev/null
    LOVELANG_SKIP_DOWNLOAD=1 npm install "$TMP_DIR/$PACK_TGZ" >/dev/null
    LOVELANG_BIN_PATH="$ROOT_DIR/lovelang" ./node_modules/.bin/lovelang "$ROOT_DIR/examples/01-romantic-hello.love" >/dev/null
  )
}

online_install_and_run() {
  local online_dir="$TMP_DIR/online"
  mkdir -p "$online_dir"
  (
    cd "$online_dir"
    npm init -y >/dev/null
    npm install lovelang-runtime >/dev/null
    ./node_modules/.bin/lovelang "$ROOT_DIR/examples/01-romantic-hello.love" >/dev/null
    node -e "const fs=require('fs');const p='node_modules/lovelang-runtime/vendor/install.json';if(!fs.existsSync(p)){process.exit(0);}const j=JSON.parse(fs.readFileSync(p,'utf8'));if((j.owner&&j.owner!=='clixiya')||(j.repo&&j.repo!=='lovelang')){console.error('Unexpected manifest',j);process.exit(1);}"
  )
}

echo "Lovelang full test runner"
echo "Root: $ROOT_DIR"

run_step "Check command: make" require_cmd make
run_step "Check command: npm" require_cmd npm
run_step "Check command: node" require_cmd node

run_step "Build interpreter" build_binary
run_step "Run example 01-romantic-hello" run_example "01-romantic-hello.love"
run_step "Run example 11-collections-and-strings" run_example "11-collections-and-strings.love"
run_step "Run example 12-modules-and-filesystem" run_example "12-modules-and-filesystem.love"
run_step "Run example 16-all-features-showcase" run_example "16-all-features-showcase.love"

run_step "Run mode romantic" run_mode "romantic"
run_step "Run mode toxic" run_mode "toxic"
run_step "Run mode shayari" run_mode "shayari"

run_expect_fail "Error example 13-empty-list-pop" run_error_example "13-error-empty-list-pop.love"
run_expect_fail "Error example 14-invalid-named-arg" run_error_example "14-error-invalid-named-arg.love"
run_expect_fail "Error example 15-missing-import" run_error_example "15-error-missing-import.love"

run_step "Run npm CLI unit tests" npm_cli_unit_tests
run_step "Pack npm CLI package" pack_cli_package
run_step "Offline install/run (no download, local binary override)" offline_install_and_run
run_step "Online install/run (npm + GitHub release download)" online_install_and_run

echo
echo "Summary: $PASS_COUNT passed, $FAIL_COUNT failed"
if [[ $FAIL_COUNT -gt 0 ]]; then
  exit 1
fi

echo "All tests passed."