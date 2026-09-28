#!/bin/sh
# asciicast installer - Termux, Debian/Ubuntu, Arch, Fedora
#
# Usage: ./install.sh [--no-deps] [--user]
#   --no-deps  skip installing dependencies (only build + install)
#   --user     install to ~/.local/bin instead of /usr/local/bin (non-Termux)

set -e

DEPS=1
USER_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        --no-deps) DEPS=0 ;;
        --user) USER_INSTALL=1 ;;
        -h|--help) sed -n '2,7p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

say() { printf '==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

cd "$(dirname "$0")"

# ---- detect environment -----------------------------------------------------
IS_TERMUX=0
if [ -n "$TERMUX_VERSION" ] || [ -d /data/data/com.termux ]; then
    IS_TERMUX=1
fi

SUDO=""
if [ "$IS_TERMUX" -eq 0 ] && [ "$(id -u)" -ne 0 ]; then
    have sudo && SUDO="sudo"
fi

# ---- dependencies -----------------------------------------------------------
install_deps() {
    if [ "$IS_TERMUX" -eq 1 ]; then
        say "Termux detected"
        pkg update -y
        pkg install -y clang make ffmpeg imagemagick libjpeg-turbo pulseaudio
    elif have apt-get; then
        say "Debian/Ubuntu detected"
        $SUDO apt-get update
        $SUDO apt-get install -y gcc make ffmpeg imagemagick libjpeg-dev \
            pulseaudio-utils alsa-utils
    elif have pacman; then
        say "Arch Linux detected"
        $SUDO pacman -S --needed --noconfirm gcc make ffmpeg imagemagick \
            libjpeg-turbo libpulse alsa-utils
    elif have dnf; then
        say "Fedora detected"
        # ffmpeg needs RPM Fusion on stock Fedora; fall back to ffmpeg-free
        $SUDO dnf install -y gcc make ImageMagick libjpeg-turbo-devel \
            pulseaudio-utils alsa-utils
        have ffmpeg || $SUDO dnf install -y ffmpeg \
            || $SUDO dnf install -y ffmpeg-free
    else
        die "unsupported system: install gcc, make, ffmpeg, imagemagick, libjpeg headers and pacat/aplay manually, then rerun with --no-deps"
    fi
}

if [ "$DEPS" -eq 1 ]; then
    install_deps
fi

# ---- build ------------------------------------------------------------------
say "Building"
make clean >/dev/null 2>&1 || true
make

# ---- install ----------------------------------------------------------------
if [ "$IS_TERMUX" -eq 1 ]; then
    BINDIR="${PREFIX:-/data/data/com.termux/files/usr}/bin"
    INSTALL_SUDO=""
elif [ "$USER_INSTALL" -eq 1 ]; then
    BINDIR="$HOME/.local/bin"
    INSTALL_SUDO=""
else
    BINDIR="/usr/local/bin"
    INSTALL_SUDO="$SUDO"
fi

say "Installing to $BINDIR"
$INSTALL_SUDO mkdir -p "$BINDIR"
$INSTALL_SUDO cp asciicast "$BINDIR/asciicast"
$INSTALL_SUDO chmod 755 "$BINDIR/asciicast"

case ":$PATH:" in
    *":$BINDIR:"*) ;;
    *) say "Note: $BINDIR is not in your PATH" ;;
esac

if [ "$IS_TERMUX" -eq 1 ]; then
    say "Termux tips:"
    echo "  - Videos in shared storage: run 'termux-setup-storage' once, then use ~/storage/..."
    echo "  - If audio is silent, run: pulseaudio --start --exit-idle-time=-1"
    echo "  - If video runs ahead of sound: ASCIICAST_AUDIO_DELAY_MS=150 asciicast video.mp4"
fi

say "Done. Usage: asciicast <video-file>   (add -n to disable audio)"
