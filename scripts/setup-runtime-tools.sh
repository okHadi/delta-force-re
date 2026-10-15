#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
if [ "$(uname -s)" != Darwin ] || [ "$(uname -m)" != x86_64 ]; then
    echo "Use this setup on an Intel Mac." >&2
    exit 1
fi
mkdir -p .local/tools
fetch() {
    name=$1 url=$2 digest=$3
    if ! printf '%s  %s\n' "$digest" ".local/tools/$name" | shasum -a 256 -c - >/dev/null 2>&1; then
        curl -fLsS --retry 2 "$url" -o ".local/tools/$name"
    fi
    printf '%s  %s\n' "$digest" ".local/tools/$name" | shasum -a 256 -c -
}
fetch zig.tar.xz https://ziglang.org/download/0.17.0/zig-x86_64-macos-0.17.0.tar.xz \
    4f9a1c5269aa17ebda5e6d3c2b89d6cbf36f7d2b22a0306e9ab98f25f95529c6
if [ ! -x .local/tools/zig-x86_64-macos-0.17.0/zig ]; then
    tar -xf .local/tools/zig.tar.xz -C .local/tools
fi
fetch wine.tar.xz https://github.com/Gcenx/macOS_Wine_builds/releases/download/11.0_1/wine-stable-11.0_1-osx64.tar.xz \
    b50dc50ec7f41d58b115a6b685d4d1315ba3c797bd3aa0f49213f2703cb82388
if [ ! -x '.local/tools/Wine Stable.app/Contents/Resources/wine/bin/wine' ]; then
    tar -xf .local/tools/wine.tar.xz -C .local/tools
fi
.local/tools/zig-x86_64-macos-0.17.0/zig version
'.local/tools/Wine Stable.app/Contents/Resources/wine/bin/wine' --version
