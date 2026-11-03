#!/usr/bin/env bash
# Configure, build and test. Usage: scripts/build_and_test.sh [build-dir]
# Needs VCPKG_ROOT (manifest mode) or system-installed Crow, nlohmann-json, sqlite3, gtest.
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${1:-build}"
ARGS=()
if [[ -n "${VCPKG_ROOT:-}" ]]; then
  ARGS+=("-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
fi
cmake -S . -B "$BUILD_DIR" "${ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
