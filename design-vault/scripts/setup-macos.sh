#!/bin/bash
# One-time setup for building on macOS: tools through Homebrew (Xcode 11+ must already be installed), then the submodules.
# The libraries themselves are built by the game's own configure_mac.cmake (through vcpkg) when you run `make config`.
#
#   DATA=full  (default) fetches the game's art and music too (large), so the build is a complete game
#   DATA=none            only the code submodules
set -e
cd "$(dirname "$0")/../.."
DATA="${DATA:-full}"

command -v brew >/dev/null || { echo "Homebrew (https://brew.sh) is needed for cmake, gettext and asciidoctor."; exit 1; }
brew install cmake gettext asciidoctor ninja

echo "== submodules (DATA=${DATA})"
if [ "${DATA}" = "full" ]; then
  git submodule update --init --recursive --depth 1
else
  git submodule update --init --depth 1 -- 3rdparty lib/sound/3rdparty/opusfile
fi
echo "Done. Next: make config build"
