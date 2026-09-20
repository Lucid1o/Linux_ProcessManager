#!/usr/bin/env bash
# Pulse Debian (.deb) package generator script
set -e

VERSION="1.0.0"
PACKAGE_NAME="pulse"
ARCH="amd64"
BUILD_DIR="build_deb"
INSTALL_PREFIX="${BUILD_DIR}/usr"

echo "=== Building Pulse Debian Package v${VERSION} ==="

# Clean previous build dir
rm -rf "${BUILD_DIR}"
mkdir -p "${INSTALL_PREFIX}/bin"
mkdir -p "${INSTALL_PREFIX}/share/applications"
mkdir -p "${INSTALL_PREFIX}/share/pixmaps"
mkdir -p "${BUILD_DIR}/DEBIAN"

# Compile Pulse if not built
if [ ! -f "build/ui/pulse" ]; then
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
fi

# Copy binary & assets
cp "build/ui/pulse" "${INSTALL_PREFIX}/bin/pulse"
cp "packaging/pulse.desktop" "${INSTALL_PREFIX}/share/applications/pulse.desktop"

# Control file
cat <<EOF > "${BUILD_DIR}/DEBIAN/control"
Package: ${PACKAGE_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: Pulse Team <pulse@example.org>
Depends: libc6 (>= 2.34), libstdc++6 (>= 11), libqt6widgets6 (>= 6.2.0), libqt6gui6 (>= 6.2.0), libqt6core6 (>= 6.2.0)
Description: Application-centric process and system manager for Linux
 Pulse groups processes into semantic user-facing applications (such as
 Chrome, VS Code, Spotify) with live custom QPainter sparklines, tree-aware
 process management, and duration-tracked recommendation insights.
EOF

# Build package
dpkg-deb --build "${BUILD_DIR}" "${PACKAGE_NAME}_${VERSION}_${ARCH}.deb"
echo "=== Successfully built ${PACKAGE_NAME}_${VERSION}_${ARCH}.deb ==="
