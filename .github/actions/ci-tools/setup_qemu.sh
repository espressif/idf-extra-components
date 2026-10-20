#!/bin/sh
set -e

# QEMU version and date string for easy maintenance
QEMU_VERSION="9.2.2"
QEMU_DATE="20250817"
QEMU_RELEASE="esp-develop-${QEMU_VERSION}-${QEMU_DATE}"

# Each QEMU test job runs one target; download only the matching emulator.
# Keep the supported targets next to the emulator setup, not in test discovery.
case "${IDF_TARGET_NAME:?Missing QEMU target}" in
    esp32s3)
        QEMU_FLAVOR="xtensa"
        ;;
    esp32c3)
        QEMU_FLAVOR="riscv32"
        ;;
    *)
        echo "The QEMU worker does not support target $IDF_TARGET_NAME" >&2
        exit 1
        ;;
esac

# 1. Detect host architecture
ARCH=$(uname -m)
case "$ARCH" in
    x86_64 | aarch64 | arm64) ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac
[ "$ARCH" = "x86_64" ] || ARCH="aarch64"
# Release archives are named by flavor, e.g. qemu-riscv32-softmmu-...; the
# binary inside the tarball is qemu-system-<flavor>.
QEMU_SYSTEM="qemu-system-$QEMU_FLAVOR"
QEMU_FILE="qemu-${QEMU_FLAVOR}-softmmu-esp_develop_${QEMU_VERSION}_${QEMU_DATE}-${ARCH}-linux-gnu.tar.xz"

# Runtime dependencies of the QEMU binaries; cheap and idempotent.
apt-get update
apt-get install -y libgcrypt20 libglib2.0-0 libpixman-1-0 libsdl2-2.0-0 libslirp0

# Download only when the cache did not provide the emulator already.
if [ -x "qemu/bin/$QEMU_SYSTEM" ]; then
    echo "Using cached QEMU $QEMU_SYSTEM."
else
    # 2. Download the matching binary
    # -f: HTTP errors (e.g. a renamed release asset) must fail here, not in tar.
    curl -fLO "https://github.com/espressif/qemu/releases/download/${QEMU_RELEASE}/$QEMU_FILE"
    # 3. Extract the compressed file
    tar -xJf "$QEMU_FILE"
    rm "$QEMU_FILE"

    if [ ! -x "qemu/bin/$QEMU_SYSTEM" ]; then
        echo "QEMU installation failed: qemu/bin/$QEMU_SYSTEM missing from $QEMU_FILE" >&2
        exit 1
    fi
    echo "QEMU $QEMU_SYSTEM installation successful."
fi

# 4. Add QEMU to PATH
export PATH="$PATH:$(pwd)/qemu/bin"
# 5. Verify QEMU installation
if command -v "$QEMU_SYSTEM" >/dev/null 2>&1; then
    echo "$QEMU_SYSTEM is available in PATH."
else
    echo "$QEMU_SYSTEM is not available in PATH." >&2
    exit 1
fi
