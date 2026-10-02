#!/bin/bash
# One-time setup for building on Linux: installs the build dependencies with the game's own get-dependencies_linux.sh
# (Ubuntu, Fedora, Alpine, Arch, openSUSE Tumbleweed; needs root, so it uses sudo) and fetches the submodules.
#
#   DATA=full  (default) fetches everything, including the game's art and music (large), so the build is a complete game
#   DATA=none            only the code submodules; the executable is built, the game data has to come from elsewhere
set -e
cd "$(dirname "$0")/../.."
DATA="${DATA:-full}"

. /etc/os-release
case "${ID}" in
  ubuntu|fedora|alpine|archlinux) DISTRO="${ID}" ;;
  opensuse-tumbleweed) DISTRO="opensuse-tumbleweed" ;;
  debian|linuxmint|pop) DISTRO="ubuntu" ;;   # same package names as Ubuntu; not tested here
  *) echo "Unsupported distribution '${ID}'. See get-dependencies_linux.sh for the package names."; exit 1 ;;
esac
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"

echo "== dependencies for ${DISTRO}"
${SUDO} ./get-dependencies_linux.sh "${DISTRO}" build-all

echo "== submodules (DATA=${DATA})"
if [ "${DATA}" = "full" ]; then
  git submodule update --init --recursive --depth 1
else
  git submodule update --init --depth 1 -- 3rdparty lib/sound/3rdparty/opusfile
fi
echo "Done. Next: make config build"
