#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake --preset macos-release -S "${ROOT_DIR}"
cmake --build --preset macos-release
"${ROOT_DIR}/tools/package_macos.sh"

echo "MyQuant updated. Launch only: ${HOME}/Applications/MyQuant.app"
