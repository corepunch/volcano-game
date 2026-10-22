#!/bin/sh
# Native SDL2, statically linked so macOS bundles never inherit SDL3 via compat.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
version=2.32.10
archive="$root/build/deps/SDL2-$version.tar.gz"
expected=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
mkdir -p "$root/build/deps" "$root/build/sdl2-objects"
if [ ! -f "$archive" ]; then
    curl -fL "https://www.libsdl.org/release/SDL2-$version.tar.gz" -o "$archive"
fi
actual=$(shasum -a 256 "$archive" | cut -d ' ' -f 1)
if [ "$actual" != "$expected" ]; then
    echo "SDL2 archive checksum mismatch: $archive" >&2
    exit 1
fi
tar -xzf "$archive" -C "$root/build/deps"
cd "$root/build/sdl2-objects"
"$root/build/deps/SDL2-$version/configure" --prefix="$root/build/sdl2-native" --disable-shared --enable-static
make -j "${JOBS:-4}"
make install
echo "Native SDL2 installed. Run make to rebuild the game against it."
