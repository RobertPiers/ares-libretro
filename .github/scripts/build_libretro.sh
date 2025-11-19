#!/usr/bin/env bash
set -euo pipefail

: "${TARGET_PRESET:?TARGET_PRESET must be set}"
TARGETS="${LIBRETRO_TARGETS:-ares_gb_libretro}"

cmake --preset "${TARGET_PRESET}"

read -r -a target_array <<< "${TARGETS}"
build_args=()
for target in "${target_array[@]}"; do
  build_args+=("--target" "$target")
done

cmake --build build "${build_args[@]}"
