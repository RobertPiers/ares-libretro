#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

run_with_clone() {
  local component="$1"
  local dir="$2"
  local clone_script="$3"
  local run_script="$4"

  echo "::group::${component}"
  if [ -n "$clone_script" ] && [ ! -d "${dir}/tests" ]; then
    "$clone_script"
  fi
  "$run_script"
  echo "::endgroup::"
}

cd "$repo_root"

run_with_clone "ARM7TDMI tests" \
  "$repo_root/tests/arm7tdmi" \
  "$repo_root/tests/arm7tdmi/clone-tests.sh" \
  "$repo_root/tests/arm7tdmi/run-tests.sh"

run_with_clone "m68000 tests" \
  "$repo_root/tests/m68000" \
  "$repo_root/tests/m68000/clone-tests.sh" \
  "$repo_root/tests/m68000/run-tests.sh"

echo "::group::i8080 tests"
if [ ! -x "$repo_root/build/i8080/rundir/i8080" ]; then
  echo "i8080 test binary is missing. Did the build step complete?" >&2
  exit 1
fi
"$repo_root/build/i8080/rundir/i8080"
echo "::endgroup::"
