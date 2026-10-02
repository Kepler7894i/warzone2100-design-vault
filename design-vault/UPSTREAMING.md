# Pushing the changes into the original repository

The original project is [Warzone2100/warzone2100](https://github.com/Warzone2100/warzone2100). Changes go in as a **pull request from
a branch of your fork against its `master` branch**. You cannot push to their repository directly, and you should not try
to; maintainers merge pull requests.

## Read this first

* **This fork is based on 4.7.0 (April 2026); upstream `master` is ahead of it.** A pull request has to be against `master`, so the
  change has to be ported forward. Expect real conflicts in `src/template.cpp`, `src/template.h` and `src/design.cpp`:
  upstream pull request
  [#5058](https://github.com/Warzone2100/warzone2100/pull/5058) reworked how stored designs are saved and loaded (the same code
  this change replaces). Plan on re-applying the *ideas* on top of their new code rather than resolving textual conflicts.
* **Talk to them before you write the port.** Open an issue (or ask in their Discord / forum) that describes the feature:
  stored designs offered in every game, upgrade lines, per-factory hiding. They may want it shaped differently, or already
  have part of it. The feature request template is `.github/ISSUE_TEMPLATE/feature_request.md`.
* **Read their current contribution rules** (`README.md`, `doc/CodingStyle.md`, the project wiki, `.github/`) the day you start;
  they change. In particular, look for a policy on AI-assisted contributions: this change was written with an AI coding assistant,
  and if the project asks for disclosure, say so in the pull request.
* The licence is not an obstacle: the change is GPL-2.0-or-later, the same as the project (see `LICENSE`).

## What goes in, what stays out

Commit 1 of `design-vault` is the game change. Commit 2 (and the mod branches) are this fork's own.

| Take | Leave in the fork |
| --- | --- |
| `src/designchain.h` – the line logic | `Makefile`, `design-vault/` (build setup for this fork, docs, licence note) |
| the edits to `src/template.*`, `src/droiddef.h`, `src/design.*`, `src/init.cpp`, `src/intdisplay.cpp` | the banner in `README.md`, the lines in `.gitignore` and `.gitattributes` |
| `src/designvault_icons.*`, `src/designvault_icondata.cpp` and `design-vault/scripts/gen-icons.py` (or, better, add the two images to the game's `intfac` image set, which a pull request can do and this fork cannot) | a unit test of `designchain.h`, ported to the project's test setup in `tests/` (`design-vault/tests/designchain_test.cpp` is standard C++) | `src/designvault_selftest.cpp` and the one-line hook in `src/loop.cpp`: a developer harness driven by an environment variable; the project may not want it. Ask. |

## Steps

Everything below is run in your clone of the fork.

### 1. Get upstream's `master`

```sh
git remote add upstream https://github.com/Warzone2100/warzone2100.git     # once
git fetch upstream
```

### 2. Start a branch from upstream, not from this fork's history

```sh
git switch -c design-vault-upstream upstream/master
```

### 3. Bring the game change over

Find the commit and try to apply it. It will conflict; that is expected (see above).

```sh
git log --oneline 4.7.0..design-vault                  # the two commits; the first one is the game change
git cherry-pick --no-commit <first-commit>             # applies it to the working tree and stops
git status                                             # lists the conflicts
```

Resolve them against upstream's new store code. What has to survive, whatever the code around it looks like:

1. Every stored design has a stable id (`vaultId`) and an optional link to the design it replaces (`supersedes`), written to and
   read from the stored-designs file; designs without an id get one on load.
2. `fillTemplateList()` (factory lists) and `desSetupDesignTemplates()` (design screen) hide a design when a design that replaces
   it, directly or through a line, can be built, decided per factory, unless *show obsolete* is on.
3. The **Upgrade** button, the marker on stored designs, and the bin deleting from the stored designs for good.
4. The safety of the stored file (unreadable entries kept, duplicates merged, one-time backup) if their rework does not already
   provide it. Check before adding: some of it may be redundant by now.

Commit 1 contains only game files (no build setup), so nothing else needs to be dropped, except what you decide not to offer, for
example the self-test harness (`git rm --cached src/designvault_selftest.cpp`, and revert the one-line hook in `src/loop.cpp`).

### 4. Build and test against `master`

Use the build instructions of `master`, not of this fork (it needs newer dependencies, SDL3 among them). Run the project's own
checks: its CI workflows in `.github/workflows` show what is run, and `doc/CodingStyle.md` says how code must look (tabs, brace
style). New user-visible strings must go through `_()`. Test the feature by hand in a skirmish, a campaign mission and a saved game
(the scenarios of `design-vault/tests/` list what to look at).

### 5. Commit in the project's style and push to your fork

Small, logical commits, imperative subject lines, as in `git log upstream/master`.

```sh
git add src
git commit
git push -u origin design-vault-upstream
```

### 6. Open the pull request

```sh
gh pr create --repo Warzone2100/warzone2100 --base master --head Kepler7894i:design-vault-upstream \
  --title "Stored designs in every game, and upgrade lines" --body-file pr-description.md
```

Or open it on GitHub: your fork's page shows a "Compare & pull request" button for the pushed branch. In the description, say what
the feature does and why (the problem: the same designs recreated every game), what you tested and what you did not (multiplayer;
list the platforms you built on), link the issue from the step above, and mention that the allies'-designs toggle that was
originally asked for was not built because other players' designs never reach the lists in 4.4.2 (check that is still true
in `master`).

Then answer review comments with new commits on the same branch; the pull request updates by itself.

## If upstream does not want all of it

The change splits cleanly in two, so you can offer them separately:

1. **Safer stored designs** (stored in every game, unreadable entries kept, duplicates merged, bin deletes for good, marker).
2. **Upgrade lines** (`designchain.h`, the `supersedes` key, the Upgrade button, hiding in the lists).

Part 2 depends on the ids from part 1.
