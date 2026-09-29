#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_APP="${ROOT_DIR}/build/macos-release/MyQuant.app"
DEST_APP="${1:-${HOME}/Applications/MyQuant.app}"
DEST_DIR="$(dirname "${DEST_APP}")"
DEST_NAME="$(basename "${DEST_APP}")"
STAGING_APP="${DEST_DIR}/.${DEST_NAME}.staging"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

if [[ ! -d "${SOURCE_APP}" ]]; then
    echo "Missing ${SOURCE_APP}. Build first with: cmake --build --preset macos-release" >&2
    exit 1
fi

mkdir -p "${DEST_DIR}"
if [[ "${DEST_NAME}" == "MyQuant.app" ]]; then
    while IFS= read -r old_app; do
        if [[ -x "${LSREGISTER}" ]]; then
            "${LSREGISTER}" -u "${old_app}" >/dev/null 2>&1 || true
        fi
        rm -rf "${old_app}"
    done < <(find "${DEST_DIR}" -maxdepth 1 -name 'MyQuant*.app' ! -path "${DEST_APP}" -print)
fi
rm -rf "${STAGING_APP}"
ditto --norsrc "${SOURCE_APP}" "${STAGING_APP}"
xattr -cr "${STAGING_APP}" 2>/dev/null || true

if [[ -d "${STAGING_APP}/Contents/PlugIns" || -d "${STAGING_APP}/Contents/Resources/qml" ]]; then
    find "${STAGING_APP}/Contents/PlugIns" "${STAGING_APP}/Contents/Resources/qml" \
        -name '*.dylib' -print0 2>/dev/null | while IFS= read -r -d '' dylib; do
        codesign --force --sign - "${dylib}" >/dev/null 2>&1
    done
fi

if [[ -d "${STAGING_APP}/Contents/Frameworks" ]]; then
    find "${STAGING_APP}/Contents/Frameworks" \
        -path '*/Versions/A' -type d -print0 | while IFS= read -r -d '' framework; do
        codesign --force --sign - "${framework}" >/dev/null 2>&1
    done
fi

codesign --force --sign - "${STAGING_APP}/Contents/MacOS/MyQuant" >/dev/null 2>&1
codesign --force --sign - "${STAGING_APP}" >/dev/null 2>&1
codesign --verify --deep --strict --verbose=1 "${STAGING_APP}"

# Only replace the installed app after the staged copy passes validation.
if [[ -x "${LSREGISTER}" && -d "${DEST_APP}" ]]; then
    "${LSREGISTER}" -u "${DEST_APP}" >/dev/null 2>&1 || true
fi
rm -rf "${DEST_APP}"
mv "${STAGING_APP}" "${DEST_APP}"
codesign --verify --deep --strict --verbose=1 "${DEST_APP}"

if [[ -x "${LSREGISTER}" ]]; then
    "${LSREGISTER}" -u "${SOURCE_APP}" >/dev/null 2>&1 || true
    "${LSREGISTER}" -f "${DEST_APP}" >/dev/null 2>&1 || true
fi

# A discoverable .app in the build tree is treated by macOS as a second app.
# The installed copy is now verified, so remove the transient bundle while
# retaining the remaining CMake build products for incremental compilation.
rm -rf "${SOURCE_APP}"

echo "${DEST_APP}"
