#!/usr/bin/env bash
# Pulse launch script
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [ -f "${ROOT_DIR}/build/ui/pulse" ]; then
    BINARY="${ROOT_DIR}/build/ui/pulse"
elif [ -f "${ROOT_DIR}/build/bin/pulse" ]; then
    BINARY="${ROOT_DIR}/build/bin/pulse"
elif command -v pulse &>/dev/null; then
    BINARY="pulse"
else
    echo "Error: pulse binary not found. Please build the project first:"
    echo "  cmake -S . -B build -G Ninja && cmake --build build"
    exit 1
fi

echo "Launching Pulse from: ${BINARY}"
exec "${BINARY}" "$@"
