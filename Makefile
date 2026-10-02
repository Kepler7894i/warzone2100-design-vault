# Design Vault fork of Warzone 2100 4.4.2: set up, configure, build, run and test on Windows, Linux and macOS.
# (This file and the design-vault/ folder are additions of the fork; the game itself builds with plain CMake, see README.md.)
#
#   make setup      install what the build needs (once)          make config   configure CMake
#   make build      compile                                      make run      start the game
#   make play       build if anything changed, then start        make test     run the tests
#   make help       all targets
#
# Works with GNU make from PowerShell, cmd, Git Bash, a Linux shell or a macOS terminal. Details: design-vault/README.md.

# ---- platform ----------------------------------------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
PLATFORM := windows
else ifeq ($(shell uname -s),Darwin)
PLATFORM := macos
else
PLATFORM := linux
endif

# DATA says where the game's art, music and scripts come from:
#   steam  (Windows default) not built; taken from an installed Steam copy of the game, linked into dist/
#   full   (Linux/macOS default) built from the game's data submodules: a complete game, no other install needed
#   none   not built and not provided: only the executable (run it with --datadir=<folder with base.wz>)
ifeq ($(PLATFORM),windows)
DATA ?= steam
else
DATA ?= full
endif
CONFIGURATION ?= Release
BUILD := build/wz
EXE   := dist/bin/warzone2100$(if $(filter windows,$(PLATFORM)),.exe)
ROOT  := $(CURDIR)

# every source file of the game: a build is only started when one of them is newer than the packaged exe
rwildcard = $(foreach d,$(wildcard $(1)/*),$(call rwildcard,$(d),$(2)) $(filter $(subst *,%,$(2)),$(d)))
SOURCES   := $(call rwildcard,src,*.cpp) $(call rwildcard,src,*.h) $(call rwildcard,lib,*.cpp) $(call rwildcard,lib,*.h) $(call rwildcard,lib,*.c)
NEED_TREE  = $(if $(wildcard $(BUILD)/CMakeCache.txt),,$(error No build tree yet: run 'make config' first))

PS := powershell -NoProfile -ExecutionPolicy Bypass -File

.DEFAULT_GOAL := help
.PHONY: help setup config reconfigure build package rebuild play run clean \
        test test-unit test-game test-ui test-compat test-real icons

help:
	@cmake -E cat design-vault/make-help.txt

# ---- setup -----------------------------------------------------------------------------------------------------------------

setup:
ifeq ($(PLATFORM),windows)
	$(PS) design-vault/scripts/setup-windows.ps1
else ifeq ($(PLATFORM),macos)
	DATA=$(DATA) bash design-vault/scripts/setup-macos.sh
else
	DATA=$(DATA) bash design-vault/scripts/setup-linux.sh
endif

# ---- configure -------------------------------------------------------------------------------------------------------------

# the parts of the configuration that depend on DATA
ifeq ($(DATA),full)
DATA_FLAGS :=
else
DATA_FLAGS := -DENABLE_DOCS=OFF -DENABLE_NLS=OFF -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="$(ROOT)/design-vault/scripts/skip-data.cmake"
endif

config:
ifeq ($(PLATFORM),windows)
	cmake -S . -B $(BUILD) -G "Visual Studio 17 2022" -A x64 \
		-DCMAKE_TOOLCHAIN_FILE="$(ROOT)/build/vcpkg/scripts/buildsystems/vcpkg.cmake" \
		-DVCPKG_MANIFEST_MODE=OFF -DVCPKG_INSTALLED_DIR="$(ROOT)/build/vcpkg_installed" -DVCPKG_TARGET_TRIPLET=x64-windows \
		-DENABLE_DISCORD=OFF -DWZ_ENABLE_BACKEND_VULKAN=OFF \
		-DZIP_EXECUTABLE="$(firstword $(wildcard build/vcpkg/downloads/tools/7zip-*-windows/7z.exe))" \
		$(DATA_FLAGS)
else ifeq ($(PLATFORM),macos)
	cmake -E make_directory $(BUILD)
	cd $(BUILD) && cmake -P "$(ROOT)/configure_mac.cmake"
else
	cmake -S . -B $(BUILD) -G Ninja -DCMAKE_BUILD_TYPE=$(CONFIGURATION) $(DATA_FLAGS)
endif

reconfigure:
	@$(NEED_TREE)
	cmake $(BUILD)

# ---- build -------------------------------------------------------------------------------------------------------------------

# Without a data build only the executable is a target; with one, the whole tree (game data included).
TARGET_FLAGS := $(if $(filter full,$(DATA)),,--target warzone2100)

build:
	@$(NEED_TREE)
ifeq ($(PLATFORM),windows)
	cmake --build $(BUILD) --config $(CONFIGURATION) $(TARGET_FLAGS) -- -m -nologo -v:m
else
	cmake --build $(BUILD) --config $(CONFIGURATION) $(TARGET_FLAGS) --parallel
endif

# dist/ is a runnable copy: the exe and its DLLs, and the game data (Windows with DATA=steam: linked from the Steam install)
package:
ifeq ($(PLATFORM),windows)
	$(PS) design-vault/scripts/package.ps1
else
	cmake --install $(BUILD) --config $(CONFIGURATION) --prefix dist
endif

rebuild: reconfigure build package

# The exe in dist/ is the stamp: when a source file is newer than it, everything is rebuilt and packaged again.
$(EXE): $(SOURCES)
	$(MAKE) rebuild

clean:
	cmake --build $(BUILD) --config $(CONFIGURATION) --target clean

# ---- run ---------------------------------------------------------------------------------------------------------------------

play: $(EXE) run

# ARGS are passed on to the game, e.g.  make run ARGS="--window --resolution=1600x900"
run:
	@$(if $(wildcard $(EXE)),,$(error Nothing to run yet: use make play, which builds first))
ifeq ($(PLATFORM),windows)
	$(PS) design-vault/scripts/play.ps1 $(ARGS)
else ifeq ($(DATA),full)
	./$(EXE) --datadir="$(ROOT)/dist/share/warzone2100" $(ARGS)
else
	./$(EXE) $(ARGS)
endif

# ---- tests -------------------------------------------------------------------------------------------------------------------
# test-unit works everywhere (it needs a C++17 compiler only); the scripted game scenarios are PowerShell and Windows only.

test: test-unit
ifeq ($(PLATFORM),windows)
test: test-game test-ui test-compat
endif

test-unit:
ifeq ($(PLATFORM),windows)
	$(PS) design-vault/tests/run_tests.ps1
else
	cmake -E make_directory design-vault/tests/out
	$(or $(CXX),c++) -std=c++17 -Wall -Wextra -Werror -Isrc design-vault/tests/designchain_test.cpp -o design-vault/tests/out/designchain_test
	design-vault/tests/out/designchain_test
endif

test-game: $(EXE)
	$(PS) design-vault/tests/run_selftest.ps1

test-ui: $(EXE)
	$(PS) design-vault/tests/run_ui.ps1

test-compat: $(EXE)
	$(PS) design-vault/tests/run_stock_compat.ps1

test-real: $(EXE)
	$(PS) design-vault/tests/run_real_data.ps1

# ---- artwork -----------------------------------------------------------------------------------------------------------------

icons:
	python design-vault/scripts/gen-icons.py
