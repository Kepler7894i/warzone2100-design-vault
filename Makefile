# Design Vault mod for the unmodified Steam copy of Warzone 2100 (Windows, PowerShell scripts). Nothing is compiled.
# The full version, a build of the game with the change in its C++ code, is on the design-vault branch.
#
#   make play-mod    build + install the mod, start the Steam game with it, rebuild the mod when you quit
#   make help        all targets

PS := powershell -NoProfile -ExecutionPolicy Bypass -File

.DEFAULT_GOAL := help
.PHONY: help mod mod-install mod-uninstall play-mod remove-design test-mod

help:
	@cmake -E cat design-vault/make-help.txt

mod:
	$(PS) mod/build-mod.ps1

mod-install:
	$(PS) mod/install.ps1

mod-uninstall:
	$(PS) mod/uninstall.ps1

play-mod:
	$(PS) mod/play-steam.ps1

# make remove-design NAME="Light Cannon Cobra Hover"
remove-design:
	$(PS) mod/remove-design.ps1 -Name "$(NAME)"

# make test-mod GAMEDIR="C:/path/to/install"   (default: the Steam copy)
test-mod:
	$(PS) mod/tests/run_stock_exe.ps1 $(if $(GAMEDIR),-GameDir "$(GAMEDIR)")
