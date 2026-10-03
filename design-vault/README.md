# Warzone 2100 â€“ Design Vault (mod branch)

> **This is the 4.4.2 branch (`design-vault-mod-4.4.2`)** - the mod on Warzone 2100 4.4.2, the version of the Steam release,
> which is what it was tested with on the Steam copy. The same mod on 4.7.0 is the default mod branch
> [`design-vault-mod`](https://github.com/Kepler7894i/warzone2100-design-vault/tree/design-vault-mod).


This branch is Warzone 2100 **4.4.2 with no change to the game's code**, plus a **mod** (`mod/`) for the unmodified Steam game that
keeps the vehicle designs you like across games and lets a newer design take over from an older one. Nothing has to be compiled.

The version with the full feature set (an **Upgrade** button in the design screen, a marker on stored designs, a safer store,
campaign support, per-factory hiding) is a change to the game's C++ code and lives on the
[`design-vault`](https://github.com/Kepler7894i/warzone2100-design-vault/tree/design-vault) branch. This one is the lighter
alternative for people who want to keep playing the stock executable.

| | **This branch: mod for the Steam game** | **`design-vault` branch: modified game** |
| --- | --- | --- |
| Runs on | the unmodified Steam `warzone2100.exe` | a build of the game with the change in its C++ code |
| Saved designs offered once researched | yes, skirmish and multiplayer | yes, skirmish, multiplayer and campaign |
| Newer design replaces an older one | yes, set in a text file | yes, with an **Upgrade** button in the design screen |
| Marker on stored designs, bin deletes for good, no lost or duplicated designs | no (see below) | yes |
| Replacement decided per factory (a factory too small for the new body keeps the old design) | no, by research only | yes |
| New designs get into it | when the mod is rebuilt (`play-steam.ps1` does it at launch and at exit) | at once |

Why a mod cannot do it all: the design screen and the factory lists are C++, and a mod can only bring data and scripts. What a mod
can do is define your designs as built-in designs and hide an older one with a script when its replacement can be built.

## What is original and what is added

This branch is the upstream tag `4.4.2` plus one commit that only adds files: `mod/`, `Makefile`, `design-vault/`, a banner in
`README.md` and a few lines in `.gitignore`. No file of the game is edited.

```sh
git diff --stat 4.4.2 design-vault-mod
```

## Using it

Windows, the Steam copy of Warzone 2100 (the scripts are PowerShell; the paths of the Steam install and of your profile are
parameters if yours differ). `make help` lists the same things.

```powershell
make play-mod                 # builds + installs the mod, starts the Steam exe with it, rebuilds when you quit the game
make mod-install              # just rebuild and install (after storing designs, or after editing the lines)
make mod-uninstall            # take it out again (your library and lines are kept)
make remove-design NAME="Light Cannon Cobra Hover"   # remove a saved design for good
```

or call the scripts in `mod/` directly. It installs as `%APPDATA%\Warzone 2100 Project\Warzone 2100\mods\4.4.2\multiplay\designvault.wz`.
It is a "multiplay" mod, so it only applies to skirmish and multiplayer games started with `--mod_mp=designvault.wz`, and never to the
campaign. To start it from Steam instead, set the launch option of Warzone 2100 to `--mod_mp=designvault.wz`.

### How it works

1. Store designs the way you always did, with the disk button in the design screen.
2. `mod/build-mod.ps1` (run by the commands above) copies them from the game's `userdata\mp\templates.json` into the mod's own
   library, `userdata\designvault\designs.json`. Nothing is ever dropped from the library, and identical designs are merged, so
   a design the game's own store loses is still there.
3. The mod defines every design of the library as a built-in design (`stats/templates.json`: the game's own file with yours added),
   which the game offers when all its components are researched.
4. Lines are written in `userdata\designvault\lines.txt`, one pair per line:
   `Lancer Cobra Hover replaces Machinegun Viper Wheels` (names as the game shows them, or ids from the library; `mod/lines.txt` is
   the starting point). A script in the mod (`multiplay/script/mods/init.js`, the game's own hook for mods) checks every second and
   takes the older design out of the lists as soon as the newer one can be built, for the rest of that game.

### What it cannot do

* There is no Upgrade button, no marker and no in-game way to make a line: lines are edited in the text file. The bin of the design
  screen removes a design for the current game only; use `remove-design` to remove one for good. Removing a design also removes the
  lines that mention it.
* A saved design gets the name the game generates for its parts ("Light Cannon Cobra Hover"), not the one you typed. A design you
  renamed shows up once under the generated name from the mod and once under your name from the game's own store.
* The older design is hidden when the newer one is researched, whatever the factory: a factory too small for the new body then offers
  neither.
* It does nothing for the campaign, and a game that uses a mod can only be joined by players who have it. Playing online with the mod
  installed was not tried.
* Designs stored in a session reach the mod when it is rebuilt: at the next `play-steam.ps1` launch (or at exit, if you started the
  game with it), or `mod-install`. Starting the game from Steam without the launcher does not rebuild.

## Test

`make test-mod` (or `mod/tests/run_stock_exe.ps1`) runs the **unmodified** Steam executable with the mod in a scripted skirmish with
its own configuration folder under `run/` (your settings are not touched), saves the game right away and reads the game's own list of
designs from the save. When last run: at tech level 2 and 3 the two older designs of a line were gone; at tech level 1, where the
newer ones are not researched, all six stayed. (A longer scenario that grants components step by step, 18 checks, needed the self-test harness of the modified game and is
not included in either branch.)

## License

GPL-2.0-or-later, like the game: [LICENSE](LICENSE), and `COPYING` in the root of the repository.
