#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_APP="${ROOT_DIR}/build/macos-release/MyQuant.app"
DEST_APP="${1:-${HOME}/Applications/MyQuant.app}"
DEST_DIR="$(dirname "${DEST_APP}")"
DEST_NAME="$(basename "${DEST_APP}")"
STAGING_APP="${DEST_DIR}/.${DEST_NAME}.staging"

if [[ ! -d "${SOURCE_APP}" ]]; then
    echo "Missing ${SOURCE_APP}. Build first with: cmake --build --preset macos-release" >&2
    exit 1
fi

mkdir -p "${DEST_DIR}"
if [[ "${DEST_NAME}" == "MyQuant.app" ]]; then
    find "${DEST_DIR}" -maxdepth 1 -name 'MyQuant*.app' ! -name "${DEST_NAME}" -exec rm -rf {} +
fi
rm -rf "${STAGING_APP}"
ditto --norsrc "${SOURCE_APP}" "${STAGING_APP}"
rm -rf "${DEST_APP}"
mv "${STAGING_APP}" "${DEST_APP}"
xattr -cr "${DEST_APP}" 2>/dev/null || true

if [[ -d "${DEST_APP}/Contents/PlugIns" || -d "${DEST_APP}/Contents/Resources/qml" ]]; then
    find "${DEST_APP}/Contents/PlugIns" "${DEST_APP}/Contents/Resources/qml" \
        -name '*.dylib' -print0 2>/dev/null | while IFS= read -r -d '' dylib; do
        codesign --force --sign - "${dylib}" >/dev/null 2>&1
    done
fi

if [[ -d "${DEST_APP}/Contents/Frameworks" ]]; then
    find "${DEST_APP}/Contents/Frameworks" \
        -path '*/Versions/A' -type d -print0 | while IFS= read -r -d '' framework; do
        codesign --force --sign - "${framework}" >/dev/null 2>&1
    done
fi

codesign --force --sign - "${DEST_APP}/Contents/MacOS/MyQuant" >/dev/null 2>&1
codesign --force --sign - "${DEST_APP}" >/dev/null 2>&1
codesign --verify --deep --strict --verbose=1 "${DEST_APP}"

LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
if [[ -x "${LSREGISTER}" ]]; then
    "${LSREGISTER}" -u "${SOURCE_APP}" >/dev/null 2>&1 || true
    "${LSREGISTER}" -u "${DEST_DIR}/MyQuant.app" >/dev/null 2>&1 || true
    "${LSREGISTER}" -f "${DEST_APP}" >/dev/null 2>&1 || true
fi

echo "${DEST_APP}"
