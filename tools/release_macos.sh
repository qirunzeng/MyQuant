#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/macos-release"
APP="${BUILD_DIR}/MyQuant.app"
DIST_DIR="${ROOT_DIR}/dist"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

VERSION="$(sed -nE 's/^project\(MyQuant VERSION ([0-9.]+).*/\1/p' "${ROOT_DIR}/CMakeLists.txt")"
if [[ -z "${VERSION}" ]]; then
    echo "Unable to read MyQuant version from CMakeLists.txt" >&2
    exit 1
fi

cmake --preset macos-release -S "${ROOT_DIR}"
# The normal update flow removes the transient build-tree bundle after installing
# the verified copy. Remove any partial remnant so Ninja must recreate the full
# bundle and rerun deployment/signing before a release archive is produced.
rm -rf "${APP}"
cmake --build --preset macos-release
ctest --test-dir "${BUILD_DIR}" --output-on-failure

if [[ ! -d "${APP}" ]]; then
    echo "Missing release bundle: ${APP}" >&2
    exit 1
fi

STAGING_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/myquant-release.XXXXXX")"
trap 'rm -rf "${STAGING_ROOT}"' EXIT
STAGING_APP="${STAGING_ROOT}/MyQuant.app"
ditto --norsrc "${APP}" "${STAGING_APP}"

# Sign the final copied tree in one pass. Qt's source frameworks may carry
# filesystem metadata that prevents a reliable signature in the build tree.
codesign --force --deep --sign - "${STAGING_APP}"
codesign --verify --deep --strict --verbose=1 "${STAGING_APP}"

ARCHS="$(lipo -archs "${STAGING_APP}/Contents/MacOS/MyQuant" 2>/dev/null | tr ' ' '-')"
if [[ -z "${ARCHS}" ]]; then
    ARCHS="$(uname -m)"
fi

mkdir -p "${DIST_DIR}"
ARCHIVE="${DIST_DIR}/MyQuant-${VERSION}-macOS-${ARCHS}.zip"
CHECKSUM="${ARCHIVE}.sha256"
rm -f "${ARCHIVE}" "${CHECKSUM}"

# ditto preserves the macOS bundle layout and extended attributes. Runtime data
# lives outside the app bundle and is therefore never included in this archive.
ditto -c -k --sequesterRsrc --keepParent "${STAGING_APP}" "${ARCHIVE}"
(
    cd "${DIST_DIR}"
    shasum -a 256 "$(basename "${ARCHIVE}")" > "$(basename "${CHECKSUM}")"
)

# A discoverable app bundle in the build tree is indexed by Spotlight and
# appears as a second MyQuant. The verified archive is now the release artifact,
# so unregister and remove only the transient build-tree bundle.
if [[ -x "${LSREGISTER}" ]]; then
    "${LSREGISTER}" -u "${APP}" >/dev/null 2>&1 || true
fi
rm -rf "${APP}"

echo "${ARCHIVE}"
echo "${CHECKSUM}"
