#!/bin/bash
# One-time setup for building on Linux: installs the build dependencies with the game's own get-dependencies_linux.sh
# (Ubuntu, Debian, Fedora, Alpine, Arch, openSUSE Tumbleweed; needs root, so it uses sudo), builds SDL3 from source where the
# distribution does not package it (Ubuntu 22.04/24.04, Debian 12/13: the game needs SDL3 3.2.12 or newer), and fetches the
# submodules.
#
#   DATA=full  (default) fetches everything, including the game's art and music (large), so the build is a complete game
#   DATA=none            only the code submodules; the executable is built, the game data has to come from elsewhere
set -e
cd "$(dirname "$0")/../.."
DATA="${DATA:-full}"
SDL3_TAG="${SDL3_TAG:-release-3.2.30}"

. /etc/os-release
case "${ID}" in
  ubuntu|debian|fedora|alpine|archlinux) DISTRO="${ID}" ;;
  opensuse-tumbleweed) DISTRO="opensuse-tumbleweed" ;;
  linuxmint|pop) DISTRO="ubuntu" ;;   # same package names as Ubuntu; not tested here
  *) echo "Unsupported distribution '${ID}'. See get-dependencies_linux.sh for the package names."; exit 1 ;;
esac
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"

echo "== dependencies for ${DISTRO}"
${SUDO} ./get-dependencies_linux.sh "${DISTRO}" build-all

if ! pkg-config --atleast-version=3.2.12 sdl3 2>/dev/null; then
  case "${DISTRO}" in
    ubuntu|debian)
      echo "== SDL3 is not packaged on this release: building ${SDL3_TAG} from source into /usr/local"
      ${SUDO} env DEBIAN_FRONTEND=noninteractive apt-get -y install build-essential cmake ninja-build pkg-config \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libxkbcommon-dev \
        libwayland-dev wayland-protocols libdecor-0-dev libdrm-dev libgbm-dev libgl1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev \
        libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev libibus-1.0-dev libusb-1.0-0-dev
      SDL_SRC="$(mktemp -d)"
      git clone --depth 1 --branch "${SDL3_TAG}" https://github.com/libsdl-org/SDL.git "${SDL_SRC}"
      cmake -S "${SDL_SRC}" -B "${SDL_SRC}/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_TESTS=OFF -DSDL_INSTALL_TESTS=OFF
      cmake --build "${SDL_SRC}/build" --parallel
      ${SUDO} cmake --install "${SDL_SRC}/build"
      ${SUDO} ldconfig
      rm -rf "${SDL_SRC}"
      ;;
    *)
      echo "SDL3 3.2.12 or newer was not found and cannot be installed automatically on ${DISTRO}; install it, then run make config."
      ;;
  esac
fi

echo "== submodules (DATA=${DATA})"
if [ "${DATA}" = "full" ]; then
  git submodule update --init --recursive --depth 1
else
  git submodule update --init --recursive --depth 1 -- 3rdparty lib tools
fi
echo "Done. Next: make config build"
