#!/usr/bin/env bash
# Pulse AppImage generator script using linuxdeploy
set -e

APP_NAME="Pulse"
BUILD_DIR="build_appimage"
APPDIR="${BUILD_DIR}/AppDir"

echo "=== Packaging Pulse as AppImage ==="

rm -rf "${BUILD_DIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"

# Compile Pulse if not built
if [ ! -f "build/ui/pulse" ]; then
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
fi

cp "build/ui/pulse" "${APPDIR}/usr/bin/pulse"
cp "packaging/pulse.desktop" "${APPDIR}/usr/share/applications/pulse.desktop"
cp "packaging/pulse.desktop" "${APPDIR}/pulse.desktop"

# Download linuxdeploy if not present
if ! command -v linuxdeploy &>/dev/null; then
    if [ ! -f "linuxdeploy-x86_64.AppImage" ]; then
        echo "Downloading linuxdeploy..."
        wget -q -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
        chmod +x linuxdeploy-x86_64.AppImage
    fi
    LINUXDEPLOY="./linuxdeploy-x86_64.AppImage"
else
    LINUXDEPLOY="linuxdeploy"
fi

${LINUXDEPLOY} --appdir "${APPDIR}" --plugin qt --output appimage
echo "=== AppImage generated successfully ==="
