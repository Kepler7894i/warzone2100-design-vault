# Warzone 2100 – Design Vault

A fork of [Warzone 2100](https://github.com/Warzone2100/warzone2100) **4.4.2** (the version of the Steam release) that keeps the
vehicle designs you like across games, and lets a newer design take over from an older one.

* **Stored designs stay.** A design you store with the disk button is offered in every game, skirmish, multiplayer and campaign,
  as soon as all of its parts are researched there. The game's own store was lossy; this one keeps what it cannot read, merges
  duplicates and keeps a backup.
* **Upgrade lines.** An **Upgrade** button makes a stored copy of a design that replaces it as soon as it can be built, per
  factory. The older design stops showing; *show obsolete* brings the whole line back.
* **Markers.** Stored designs show the floppy disk of the game's own store icon in the corner of their picture.

It is a change to the game's C++ code, not a data mod: the design screen and the factory lists are written in C++, and the
scripting API cannot reach them. (A data-mod variant for the unmodified Steam game exists on the
[`design-vault-mod`](https://github.com/Kepler7894i/warzone2100-design-vault/tree/design-vault-mod) branch, with fewer features.)

## Branches

| Branch | What it is |
| --- | --- |
| `design-vault` (default) | Warzone 2100 4.4.2 + the Design Vault change + build setup for Windows, Linux and macOS. |
| `design-vault-mod` | Warzone 2100 4.4.2 **unchanged** + a mod for the Steam game (`mod/`), no C++ change. |
| `master` and the rest | Mirror of the upstream project. Not touched. |

## What is original and what is changed

The history makes it explicit: `design-vault` is the upstream tag `4.4.2` plus three commits on top, and nothing else.

```sh
git log --oneline 4.4.2..design-vault          # the three commits
git diff --stat 4.4.2 design-vault -- src      # the changes to the game's source
git diff --stat 4.4.2 design-vault             # everything, including the build setup
```

| Commit | What | Files |
| --- | --- | --- |
| 1. Design Vault: stored designs and upgrade lines | **The game change.** | edits to `src/template.cpp`, `src/template.h`, `src/droiddef.h`, `src/design.cpp`, `src/design.h`, `src/init.cpp`, `src/intdisplay.cpp`, `src/loop.cpp`; new `src/designchain.h`, `src/designvault_icons.{h,cpp}`, `src/designvault_icondata.cpp`, `src/designvault_selftest.cpp` |
| 2. Build: libsodium detection | Build fix only, so that a current vcpkg builds 4.4.2 | `cmake/FindSodium.cmake` (4.6.3's version) |
| 3. Build setup, tests, docs, license | Everything around the game, nothing in the game's code | `Makefile`, `design-vault/`, a banner in `README.md`, a few lines in `.gitignore` and `.gitattributes` |

Every file that is not listed above is the upstream project's, byte for byte.

What the new source files do:

* `src/designchain.h` – the line logic (which design hides which), standard library only, unit-tested on its own.
* `src/template.cpp` – storing, loading, identities, lines in the factory and design lists. `src/design.cpp` – the buttons.
* `src/designvault_icons.*`, `src/designvault_icondata.cpp` – the two small pictures (the marker and the Upgrade button), made from
  the game's own `image_des_save(h).png` by `design-vault/scripts/gen-icons.py` and compiled in, so there are no extra data files.
* `src/designvault_selftest.cpp` – a developer self-test inside the game. It does nothing unless `WZ_DESIGNVAULT_SELFTEST` is set.

## How it works

### Stored designs that actually stay

The design screen already had a small disk button (*Store Design*), but it only existed in skirmish/multiplayer, and the stored
list was not safe: a design the game could not load (a component that does not exist in that ruleset) was dropped from the file
the next time it was written, a deleted design came back in the next game (upstream fixed that in a later version), and the same
design could pile up several times. Now:

* The disk button works in the campaign too (`userdata/campaign/templates.json`; multiplayer: `userdata/mp/templates.json`).
* A stored design is offered in every game as soon as all of its components are researched, in the design screen and in the
  factory lists.
* The bin removes a design from the stored designs for good.
* Entries that a game cannot load are kept in the file untouched. Exact copies are merged when the file is read.
* The file is written every time a stored design changes; the first write of each session keeps the previous file as
  `templates.json.bak`.
* A saved game that remembers a design as stored, but whose design was deleted afterwards, loads with the design but does not
  store it again.

It uses the game's **own** file and format (`templates.json`, the same store button). A design gets two extra keys, `vaultId` and
`supersedes`, that the unmodified game ignores when reading.

### Design lines (a newer design replaces an older one)

Select a design in the design screen and press **Upgrade** (the arrow button above the disk button). It makes a stored copy named
"… 2" (then "… 3") that replaces the design you started from, and selects it so that you can change its parts.

In any game where the copy can be built (all its components researched, and the factory can take its body size), the old design
is not offered any more, in the factory lists and in the design screen. Where the new body is not researched yet, the old design
is offered as before; a factory too small for the new body keeps offering the old one. A line can be as long as you like, an
unresearched link in the middle does not break it, and a design can be deleted from the middle of a line.

### Allies' designs: no toggle

An early request was a toggle that hides allies' designs. It is not built because in 4.4.2 they never get into your lists: the
design screen and the factory lists are built from `localTemplates`, and nothing puts another player's design there (checked in the
source, in a game with a Nexus ally, and in a real autosave with five AI players holding 157 to 557 designs each, none of them in
the player's list). What shows up unasked is your own stored designs from earlier games and the built-in cyborg, truck and
transporter designs; the marker and the bin are for those.

## Playing

You need a copy of the game's data (Windows: the Steam install is used) or a full build (Linux/macOS, see below). The build
refers to itself as "4.4.2 (modified locally)". It uses your normal settings, saves and stored designs, the same folder as
the stock game.

```sh
make play            # build if anything changed, then start the game
make run             # start without building
make run ARGS="--window --resolution=1600x900"
```

**Switching back to the unmodified game:** it does not know the two extra keys and writes the file without them. Designs
survive, the upgrade lines are lost. Use this build for all play, or accept that.

## Building

One command per step, the same on every platform: `make setup` (once), `make config`, `make build`, `make play`. `make help`
lists everything. Needs GNU make and CMake (on Windows: PowerShell, cmd and Git Bash all work).

| | Windows 10/11 x64 | Linux | macOS |
| --- | --- | --- | --- |
| Needs | Visual Studio 2022 with *Desktop development with C++*, CMake, git, GNU make, a Steam copy of Warzone 2100 | a compiler, CMake, git, make; `make setup` installs the rest with `sudo` | Xcode 11+, [Homebrew](https://brew.sh), git, make |
| `make setup` | builds the dependencies with vcpkg into `build/vcpkg_installed` (about half an hour the first time) | runs the game's `get-dependencies_linux.sh` for Ubuntu, Fedora, Alpine, Arch or openSUSE Tumbleweed | installs cmake, gettext, asciidoctor and ninja with Homebrew |
| `make config` | Visual Studio 17 2022 project in `build/wz` | Ninja, `Release`, in `build/wz` | the game's own `configure_mac.cmake` (an Xcode project) in `build/wz` |
| Game data | `DATA=steam` (default): linked from the Steam install into `dist/data` | `DATA=full` (default): built from the data submodules, a complete game | the same |
| Tested here | **yes, end to end** | executable and unit tests only (Ubuntu 24.04, WSL) | **no** – follows the game's documented procedure (`macosx/README.md`) |

`DATA` says where the art, music and scripts come from: `steam` (Windows only, needs a Steam install), `full` (built from the
game's data submodules, several hundred MB, a complete game with no other install) or `none` (the executable only; run it with
`--datadir=<folder with base.wz>`). Example: `make setup config build DATA=none`.

The Windows route differs from the game's own `get-dependencies_win.ps1` on purpose: the vcpkg that 4.4.2 pins (2023) does not
know the toolset of a current Visual Studio, and a current vcpkg does not work with the 2023 package set. `design-vault/deps/` pins
a 2025 set (the one release 4.6.3 uses, with SDL2) with the overlay ports of that release, and commit 2 carries 4.6.3's
`FindSodium.cmake` so that the new libsodium port is found.

You can also skip the Makefile and use CMake directly, exactly as the upstream project describes (this fork builds with its
unchanged build files, apart from `cmake/FindSodium.cmake`).

## Tests

```sh
make test-unit      # line logic, any platform, a C++17 compiler only
make test           # Windows: also the scripted game scenarios below
```

The scenarios run the real game in scripted skirmish and campaign games with their own configuration folder under `run/`, never
yours (about 3 minutes, windows appear minimised):

| Check | Script | Result when last run |
| --- | --- | --- |
| Line logic (chains, gaps, branches, loops, removal) | `design-vault/tests/run_tests.ps1` | 52 checks pass |
| Factory and design screen offer the right design as research and factory size change; file contents; fresh process; saved game; campaign | `design-vault/tests/run_selftest.ps1` | 59 pass |
| Upgrade, disk, bin and show-obsolete buttons through their handlers, with screenshots | `design-vault/tests/run_ui.ps1` | 10 pass |
| Lines on top of the game's own file: this build loads a file the unmodified game wrote, upgrades a design; the unmodified Steam exe then reads the file | `design-vault/tests/run_stock_compat.ps1` | 10 pass |
| Your own stored designs file / a save of yours, as copies | `design-vault/tests/run_real_data.ps1` | loads |

Not done: clicking the buttons with a mouse (the tests call the handlers a click calls, and the screenshots show the buttons and
markers), multiplayer (nothing sent over the network changed), and macOS.

## Contributing back to the upstream project

See [UPSTREAMING.md](UPSTREAMING.md).

## License

GPL-2.0-or-later, like the game: [LICENSE](LICENSE), and `COPYING` in the root of the repository.
