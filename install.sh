#!/bin/bash
# Build and install the Dell C1760nw HBPLv1 driver on macOS.
# Usage: ./install.sh [printer-ip]
#   With an IP address, also creates a printer queue named Dell_C1760nw.
set -euo pipefail
cd "$(dirname "$0")"

FILTER_DIR=/Library/Printers/DellHBPL1/Filter
PPD_NAME="Dell C1760nw HBPLv1.ppd"
PPD_DIR=/Library/Printers/PPDs/Contents/Resources

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools are needed. Run: xcode-select --install"
    exit 1
fi

echo "Building filter..."
mkdir -p build
clang -O2 -Wall -Wno-deprecated-declarations -arch arm64 \
    -o build/rastertohbpl1 src/rastertohbpl1.c -lcups

echo "Installing (you may be asked for your password)..."
sudo install -d -o root -g wheel -m 755 "$FILTER_DIR"
sudo install -o root -g wheel -m 755 build/rastertohbpl1 "$FILTER_DIR/"
sudo install -o root -g wheel -m 644 "ppd/$PPD_NAME" "$PPD_DIR/"

if [ -n "${1:-}" ]; then
    echo "Adding printer Dell_C1760nw at $1..."
    sudo lpadmin -p Dell_C1760nw -D "Dell C1760nw" -L "Network" -E \
        -v "socket://$1:9100" -P "$PPD_DIR/$PPD_NAME" \
        -o printer-is-shared=false
    echo "Done. Print a test page with:  lp -d Dell_C1760nw <some.pdf>"
else
    echo "Done. Add the printer in System Settings > Printers & Scanners"
    echo "(IP tab, protocol 'HP Jetdirect - Socket', then choose"
    echo "'Dell C1760nw HBPLv1' under Use > Select Software)."
fi
