/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** @file
 *  Developer self-test for stored designs. It does nothing unless the environment variable
 *  WZ_DESIGNVAULT_SELFTEST is set to "build" or "check", and WZ_DESIGNVAULT_SELFTEST_OUT names a file for the report.
 *  It runs once, a moment after a game has started, writes a report and quits the game with exit code 0 (all good) or 1.
 *  design-vault/tests/run_selftest.ps1 starts the game twice: "build" creates a line of designs, "check" runs in a fresh process and
 *  checks that they came back from disk.
 */

#include "lib/framework/frame.h"
#include "lib/framework/file.h"
#include "lib/framework/wzapp.h"
#include "lib/gamelib/gtime.h"
#include "lib/ivis_opengl/screen.h"
#include "lib/widget/widget.h"

#include "design.h"
#include "game.h"
#include "hci.h"
#include "main.h"
#include "objmem.h"
#include "stats.h"
#include "structure.h"
#include "template.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{

std::ofstream report;
int failures = 0;
int checks = 0;

void say(const std::string &line)
{
	report << line << std::endl;
}

void expect(bool condition, const std::string &what)
{
	++checks;
	if (!condition)
	{
		++failures;
	}
	say(std::string(condition ? "ok    " : "FAIL  ") + what);
}

STRUCTURE *findFactory()
{
	for (STRUCTURE *psStruct = apsStructLists[selectedPlayer]; psStruct != nullptr; psStruct = psStruct->psNext)
	{
		if (psStruct->status == SS_BUILT && psStruct->pStructureType->type == REF_FACTORY)
		{
			return psStruct;
		}
	}
	return nullptr;
}

DROID_TEMPLATE *findLocal(const std::string &name)
{
	for (DROID_TEMPLATE &t : localTemplates)
	{
		if (t.name.toUtf8() == name)
		{
			return &t;
		}
	}
	return nullptr;
}

/// Names of the designs of this test that the factory would offer, in the order it offers them.
std::string offeredByFactory(STRUCTURE *psFactory)
{
	std::string result;
	for (DROID_TEMPLATE *t : fillTemplateList(psFactory))
	{
		if (t->name.toUtf8().rfind("VT ", 0) == 0)
		{
			result += (result.empty() ? "" : ",") + t->name.toUtf8();
		}
	}
	return result;
}

/// Same for the design screen.
std::string offeredByDesignScreen()
{
	desSetupDesignTemplates();
	std::string result;
	for (DROID_TEMPLATE *t : apsTemplateList)
	{
		if (t->name.toUtf8().rfind("VT ", 0) == 0)
		{
			result += (result.empty() ? "" : ",") + t->name.toUtf8();
		}
	}
	return result;
}

int body(const char *id)
{
	return getCompFromName(COMP_BODY, WzString(id));
}

/// Makes a component available or unavailable to the player for as long as it exists, standing in for research.
struct Availability
{
	COMPONENT_TYPE type;
	int index;
	UBYTE previous;
	Availability(int index_, bool available, COMPONENT_TYPE type_ = COMP_BODY) : type(type_), index(index_), previous(apCompLists[selectedPlayer][type_][index_])
	{
		apCompLists[selectedPlayer][type][index] = available ? AVAILABLE : UNAVAILABLE;
	}
	~Availability()
	{
		apCompLists[selectedPlayer][type][index] = previous;
	}
	Availability(const Availability &) = delete;
	Availability &operator=(const Availability &) = delete;
};

DROID_TEMPLATE *makeStored(const std::string &name, int bodyIndex)
{
	DROID_TEMPLATE t;
	t.name = WzString::fromUtf8(name);
	t.droidType = DROID_WEAPON;
	t.asParts[COMP_BODY] = static_cast<uint8_t>(bodyIndex);
	t.asParts[COMP_PROPULSION] = static_cast<uint8_t>(getCompFromName(COMP_PROPULSION, WzString("wheeled01")));
	t.numWeaps = 1;
	t.asWeaps[0] = static_cast<uint32_t>(getCompFromName(COMP_WEAPON, WzString("MG1Mk1")));
	t.multiPlayerID = generateNewObjectId();
	t.enabled = true;
	t.ref = STAT_TEMPLATE;
	localTemplates.push_back(t);
	DROID_TEMPLATE *entry = &localTemplates.back();
	copyTemplate(selectedPlayer, entry);
	setDesignStored(*entry, true);
	copyTemplate(selectedPlayer, entry);
	return entry;
}

void setBody(DROID_TEMPLATE *entry, int bodyIndex, const char *newName)
{
	entry->asParts[COMP_BODY] = static_cast<uint8_t>(bodyIndex);
	if (newName != nullptr)
	{
		entry->name = WzString::fromUtf8(newName);
	}
	copyTemplate(selectedPlayer, entry);
}

std::string readVaultFile()
{
	std::vector<char> data;
	const std::string path = "userdata/" + std::string(rulesettag) + "/templates.json";
	if (!loadFileToBufferVector(path.c_str(), data, false, true))
	{
		return std::string();
	}
	return std::string(data.data());
}

void runBuild(STRUCTURE *psFactory)
{
	const int viper = body("Body1REC"), cobra = body("Body5REC"), python = body("Body11ABT");
	expect(viper >= 0 && cobra >= 0 && python >= 0, "body stats exist");
	Availability haveViper(viper, true), haveCobra(cobra, true), havePython(python, true);
	Availability haveWheels(getCompFromName(COMP_PROPULSION, WzString("wheeled01")), true, COMP_PROPULSION);
	Availability haveMG(getCompFromName(COMP_WEAPON, WzString("MG1Mk1")), true, COMP_WEAPON);
	const UBYTE savedCapacity = psFactory->capacity;
	psFactory->capacity = SIZE_HEAVY;

	expect(findLocal("VT Mk") == nullptr, "no leftovers from an earlier run");
	DROID_TEMPLATE *a = makeStored("VT Mk", viper);
	expect(a->stored && !a->storedId.isEmpty(), "storing a design gives it an identity");
	storeTemplates();

	DROID_TEMPLATE *b = createUpgradedDesign(*a);
	expect(b != nullptr && b->name.toUtf8() == "VT Mk 2", "upgrade is named 'VT Mk 2'");
	expect(b->supersedes == a->storedId && b->stored, "upgrade replaces the design it came from");
	expect(offeredByFactory(psFactory) == "VT Mk 2", "factory offers only the upgrade of an identical design: " + offeredByFactory(psFactory));

	setBody(b, cobra, nullptr);
	DROID_TEMPLATE *c = createUpgradedDesign(*b);
	expect(c != nullptr && c->name.toUtf8() == "VT Mk 3", "second upgrade is named 'VT Mk 3'");
	setBody(c, python, nullptr);
	storeTemplates();

	{
		expect(offeredByFactory(psFactory) == "VT Mk 3", "everything researched: only the newest is offered: " + offeredByFactory(psFactory));
		expect(offeredByDesignScreen() == "VT Mk 3", "design screen agrees: " + offeredByDesignScreen());
		{
			Availability noPython(python, false);
			expect(offeredByFactory(psFactory) == "VT Mk 2", "no Python yet: the Cobra version is offered: " + offeredByFactory(psFactory));
			expect(offeredByDesignScreen() == "VT Mk 2", "design screen agrees: " + offeredByDesignScreen());
			Availability noCobra(cobra, false);
			expect(offeredByFactory(psFactory) == "VT Mk", "no Cobra either: the Viper version is offered: " + offeredByFactory(psFactory));
			expect(offeredByDesignScreen() == "VT Mk", "design screen agrees: " + offeredByDesignScreen());
		}
		expect(offeredByFactory(psFactory) == "VT Mk 3", "research restored: newest again: " + offeredByFactory(psFactory));
	}
	{
		// A factory that cannot take the heavy body keeps offering the design it can build. (Cobra is medium, Viper light.)
		psFactory->capacity = SIZE_MEDIUM;
		expect(offeredByFactory(psFactory) == "VT Mk 2", "medium factory: Cobra version: " + offeredByFactory(psFactory));
		psFactory->capacity = SIZE_LIGHT;
		expect(offeredByFactory(psFactory) == "VT Mk", "light factory: Viper version: " + offeredByFactory(psFactory));
		psFactory->capacity = SIZE_HEAVY;
	}
	{
		const bool oldValue = includeRedundantDesigns;
		includeRedundantDesigns = true;
		const std::string all = offeredByFactory(psFactory);
		expect(all.find("VT Mk,") != std::string::npos || all.rfind("VT Mk", 0) == 0, "obsolete toggle shows the whole line: " + all);
		expect(offeredByDesignScreen().find("VT Mk 2") != std::string::npos, "design screen shows the whole line too: " + offeredByDesignScreen());
		includeRedundantDesigns = oldValue;
	}

	// Delete the middle of the line. The line must stay connected.
	const WzString aId = a->storedId;
	forgetStoredDesign(*b);
	for (auto it = localTemplates.begin(); it != localTemplates.end(); ++it)
	{
		if (&*it == b)
		{
			localTemplates.erase(it);
			break;
		}
	}
	c = findLocal("VT Mk 3");
	expect(c != nullptr && c->supersedes == aId, "after deleting the middle design, the newest replaces the oldest");
	expect(offeredByFactory(psFactory) == "VT Mk 3", "line still resolves: " + offeredByFactory(psFactory));
	{
		Availability noPython(python, false);
		expect(offeredByFactory(psFactory) == "VT Mk", "no Python: the oldest is offered: " + offeredByFactory(psFactory));
	}

	// A design that is stored and then deleted must not come back.
	DROID_TEMPLATE *gone = makeStored("VT Gone", viper);
	storeTemplates();
	forgetStoredDesign(*gone);
	for (auto it = localTemplates.begin(); it != localTemplates.end(); ++it)
	{
		if (&*it == gone)
		{
			localTemplates.erase(it);
			break;
		}
	}
	// ... and an unstored design must not be written.
	DROID_TEMPLATE *plain = makeStored("VT Plain", viper);
	setDesignStored(*plain, false);
	copyTemplate(selectedPlayer, plain);
	storeTemplates();

	const std::string file = readVaultFile();
	say("---- vault file after build ----");
	say(file);
	say("--------------------------------");
	expect(file.find("VT Mk 3") != std::string::npos && file.find("\"VT Mk\"") != std::string::npos, "file has the newest and the oldest design");
	expect(file.find("VT Mk 2") == std::string::npos, "file does not have the deleted middle design");
	expect(file.find("VT Gone") == std::string::npos, "file does not have the design deleted right after storing it");
	expect(file.find("VT Plain") == std::string::npos, "file does not have a design that is not stored");
	expect(file.find("supersedes") != std::string::npos, "file has the link between the designs");

	psFactory->capacity = savedCapacity;
}

void runCheck(STRUCTURE *psFactory)
{
	const int viper = body("Body1REC"), python = body("Body11ABT"), cobra = body("Body5REC");
	Availability haveViper(viper, true), haveCobra(cobra, true), havePython(python, true);
	Availability haveWheels(getCompFromName(COMP_PROPULSION, WzString("wheeled01")), true, COMP_PROPULSION);
	Availability haveMG(getCompFromName(COMP_WEAPON, WzString("MG1Mk1")), true, COMP_WEAPON);
	const UBYTE savedCapacity = psFactory->capacity;
	psFactory->capacity = SIZE_HEAVY;

	DROID_TEMPLATE *a = findLocal("VT Mk");
	DROID_TEMPLATE *c = findLocal("VT Mk 3");
	expect(a != nullptr && a->stored && !a->storedId.isEmpty(), "oldest design came back from disk, stored");
	expect(c != nullptr && c->stored && !c->storedId.isEmpty(), "newest design came back from disk, stored");
	expect(a != nullptr && c != nullptr && c->supersedes == a->storedId, "the link between them survived");
	expect(findLocal("VT Mk 2") == nullptr && findLocal("VT Gone") == nullptr && findLocal("VT Plain") == nullptr, "deleted and unstored designs did not come back");
	// "VT Legacy" is a design from a file without identities. It is in no line, so it is always offered.
	expect(offeredByFactory(psFactory) == "VT Mk 3,VT Legacy", "fresh game: newest is offered: " + offeredByFactory(psFactory));
	{
		Availability noPython(python, false);
		expect(offeredByFactory(psFactory) == "VT Mk,VT Legacy", "fresh game without Python: oldest is offered: " + offeredByFactory(psFactory));
		expect(offeredByDesignScreen() == "VT Mk,VT Legacy", "design screen agrees: " + offeredByDesignScreen());
	}
	{
		Availability noCobra(cobra, false);
		Availability noPython(python, false);
		expect(offeredByFactory(psFactory) == "VT Mk,VT Legacy", "without Cobra and Python: oldest: " + offeredByFactory(psFactory));
	}

	// Designs from an older version of the file got an identity, and designs that cannot be loaded are kept.
	DROID_TEMPLATE *legacy = findLocal("VT Legacy");
	expect(legacy != nullptr && legacy->stored && !legacy->storedId.isEmpty(), "a stored design from a file without identities was loaded and given one");

	// Change something and write: the entry this game cannot load must still be in the file afterwards.
	storeTemplates();
	const std::string file = readVaultFile();
	say("---- vault file after check ----");
	say(file);
	say("--------------------------------");
	expect(file.find("VT Ghost") != std::string::npos, "a design with an unknown component was kept in the file");
	expect(file.find("VT Legacy") != std::string::npos && file.find("vaultId") != std::string::npos, "legacy design is written back with an identity");
	expect(findLocal("VT Ghost") == nullptr, "the design with an unknown component is not in the game");

	psFactory->capacity = savedCapacity;
}

/// Writes down every design the game knows about, to find out where the designs in the lists come from.
void dumpDesigns(const char *when)
{
	say(std::string("==== designs at ") + when + " (game time " + std::to_string(gameTime / 1000) + " s) ====");
	say("localTemplates (what the design screen and the factory lists are made from): " + std::to_string(localTemplates.size()));
	for (const DROID_TEMPLATE &t : localTemplates)
	{
		say("  [local] \"" + t.name.toUtf8() + "\" id=" + t.id.toUtf8() + " mpid=" + std::to_string(t.multiPlayerID)
		    + (t.prefab ? " prefab" : "") + (t.stored ? " stored" : "") + (t.enabled ? " enabled" : " disabled")
		    + (researchedTemplate(&t, selectedPlayer) ? " researched" : " not-researched"));
	}
	for (int player = 0; player < MAX_PLAYERS; ++player)
	{
		size_t count = 0, designed = 0;
		std::string names;
		enumerateTemplates(player, [&](DROID_TEMPLATE *t) {
			++count;
			if (!t->prefab)
			{
				++designed;
				names += " \"" + t->name.toUtf8() + "\"";
			}
			return true;
		});
		if (count > 0)
		{
			say("player " + std::to_string(player) + (player == selectedPlayer ? " (me)" : "") + ": " + std::to_string(count) + " synchronised templates, " + std::to_string(designed) + " not prefab:" + names);
		}
	}
	if (STRUCTURE *psFactory = findFactory())
	{
		std::string offered;
		for (DROID_TEMPLATE *t : fillTemplateList(psFactory))
		{
			offered += " \"" + t->name.toUtf8() + "\"";
		}
		say("factory offers:" + offered);
	}
	desSetupDesignTemplates();
	std::string screen;
	for (DROID_TEMPLATE *t : apsTemplateList)
	{
		screen += " \"" + t->name.toUtf8() + "\"";
	}
	say("design screen offers:" + screen);
}


/// "save": stores a line of designs and a design that is deleted afterwards, and saves the game in between.
void runSave(STRUCTURE *psFactory)
{
	const int viper = body("Body1REC"), cobra = body("Body5REC");
	Availability haveViper(viper, true), haveCobra(cobra, true);
	Availability haveWheels(getCompFromName(COMP_PROPULSION, WzString("wheeled01")), true, COMP_PROPULSION);
	Availability haveMG(getCompFromName(COMP_WEAPON, WzString("MG1Mk1")), true, COMP_WEAPON);
	const UBYTE savedCapacity = psFactory->capacity;
	psFactory->capacity = SIZE_HEAVY;
	DROID_TEMPLATE *keep = makeStored("VT Keep", viper);
	DROID_TEMPLATE *keep2 = createUpgradedDesign(*keep);
	expect(keep2 != nullptr && keep2->supersedes == keep->storedId, "a line of two designs is stored");
	DROID_TEMPLATE *dropped = makeStored("VT Dropped", cobra);
	storeTemplates();
	expect(offeredByFactory(psFactory) == "VT Keep 2,VT Dropped", "before saving, the factory offers: " + offeredByFactory(psFactory));
	expect(saveGame("savegames/skirmish/vttest.gam", GTYPE_SAVE_MIDMISSION), "the game was saved");

	// ...and the design is deleted after the save was made, so the saved game remembers a design that the file does not have anymore.
	forgetStoredDesign(*dropped);
	for (auto it = localTemplates.begin(); it != localTemplates.end(); ++it)
	{
		if (&*it == dropped)
		{
			localTemplates.erase(it);
			break;
		}
	}
	const std::string file = readVaultFile();
	expect(file.find("VT Keep 2") != std::string::npos && file.find("VT Dropped") == std::string::npos, "the file no longer has the deleted design");
	psFactory->capacity = savedCapacity;
}

/// "load": runs in a game that was loaded from that save.
void runLoad(STRUCTURE *psFactory)
{
	const int viper = body("Body1REC"), cobra = body("Body5REC");
	Availability haveViper(viper, true), haveCobra(cobra, true);
	Availability haveWheels(getCompFromName(COMP_PROPULSION, WzString("wheeled01")), true, COMP_PROPULSION);
	Availability haveMG(getCompFromName(COMP_WEAPON, WzString("MG1Mk1")), true, COMP_WEAPON);
	const UBYTE savedCapacity = psFactory->capacity;
	psFactory->capacity = SIZE_HEAVY;
	size_t keeps = 0;
	for (const DROID_TEMPLATE &t : localTemplates)
	{
		keeps += (t.name.toUtf8() == "VT Keep") ? 1 : 0;
	}
	expect(keeps == 1, "the design is in the loaded game exactly once, not once from the save and once from the file: " + std::to_string(keeps));
	DROID_TEMPLATE *keep = findLocal("VT Keep");
	DROID_TEMPLATE *keep2 = findLocal("VT Keep 2");
	expect(keep != nullptr && keep->stored && !keep->storedId.isEmpty(), "'VT Keep' is stored in the loaded game");
	expect(keep2 != nullptr && keep2->stored && keep != nullptr && keep2->supersedes == keep->storedId, "'VT Keep 2' is stored and still replaces it");
	DROID_TEMPLATE *dropped = findLocal("VT Dropped");
	expect(dropped != nullptr, "the design that the saved game has is in the loaded game");
	expect(dropped != nullptr && !dropped->stored, "...but it is not stored, since it was deleted from the stored designs after the save was made");
	expect(offeredByFactory(psFactory) == "VT Keep 2,VT Dropped", "the factory offers: " + offeredByFactory(psFactory));
	storeTemplates();
	const std::string file = readVaultFile();
	expect(file.find("VT Keep 2") != std::string::npos && file.find("VT Dropped") == std::string::npos, "loading the game did not bring the deleted design back into the file");
	size_t occurrences = 0;
	for (size_t at = file.find("\"VT Keep\""); at != std::string::npos; at = file.find("\"VT Keep\"", at + 1))
	{
		++occurrences;
	}
	expect(occurrences == 1, "the file has 'VT Keep' once: " + std::to_string(occurrences));
	psFactory->capacity = savedCapacity;
}

/// Campaign: one run stores a design, the next finds it.
void runCampaign(bool create)
{
	expect(std::string(rulesettag) == "campaign", std::string("ruleset is the campaign: ") + rulesettag);
	const int viper = body("Body1REC");
	Availability haveViper(viper, true);
	Availability haveWheels(getCompFromName(COMP_PROPULSION, WzString("wheeled01")), true, COMP_PROPULSION);
	Availability haveMG(getCompFromName(COMP_WEAPON, WzString("MG1Mk1")), true, COMP_WEAPON);
	if (create)
	{
		DROID_TEMPLATE *camp = makeStored("VT Camp", viper);
		storeTemplates();
		expect(camp->stored && readVaultFile().find("VT Camp") != std::string::npos, "a design stored in the campaign is written to the campaign file");
	}
	else
	{
		DROID_TEMPLATE *camp = findLocal("VT Camp");
		expect(camp != nullptr && camp->stored && !camp->storedId.isEmpty(), "the design stored in the last campaign game is in this one");
		expect(offeredByDesignScreen().find("VT Camp") != std::string::npos, "the campaign design screen offers it: " + offeredByDesignScreen());
	}
	apsTemplateList.clear();
}

/// "real": no checks. Lists what a game made of your own files ends up with, and writes the stored designs back, so that the result can be compared with the file that was read.
void runReal()
{
	size_t stored = 0;
	for (const DROID_TEMPLATE &t : localTemplates)
	{
		if (t.stored)
		{
			++stored;
			say("  stored: \"" + t.name.toUtf8() + "\" id=" + t.storedId.toUtf8() + " mpid=" + std::to_string(t.multiPlayerID) + (t.supersedes.isEmpty() ? "" : " supersedes=" + t.supersedes.toUtf8())
			    + (researchedTemplate(&t, selectedPlayer) ? " researched" : " not-researched"));
		}
	}
	say(std::to_string(localTemplates.size()) + " designs in the game, " + std::to_string(stored) + " of them stored");
	for (const DROID_TEMPLATE &t : localTemplates)
	{
		if (t.name.toUtf8().find("Repair Turret") != std::string::npos)
		{
			say("  all designs named like that: \"" + t.name.toUtf8() + "\" mpid=" + std::to_string(t.multiPlayerID) + (t.stored ? " stored" : " not-stored") + " id=" + t.storedId.toUtf8());
		}
	}
	storeTemplates();
	say("---- stored designs file after this game ----");
	say(readVaultFile());
}

/// "stockfile": the stored designs file is one the unmodified game wrote (no ids, no lines). The designs have to load as they are,
/// an upgrade line has to be possible on top of them, and the file written afterwards has to be one the unmodified game still
/// reads (extra keys only, same file, same format).
void runStockFile()
{
	size_t stored = 0, withId = 0, withLink = 0;
	DROID_TEMPLATE *first = nullptr;
	for (DROID_TEMPLATE &t : localTemplates)
	{
		if (t.stored)
		{
			++stored;
			withId += t.storedId.isEmpty() ? 0 : 1;
			withLink += t.supersedes.isEmpty() ? 0 : 1;
			if (first == nullptr || (!researchedTemplate(first, selectedPlayer) && researchedTemplate(&t, selectedPlayer)))
			{
				first = &t; // the first one the player can build, to see the line at work
			}
		}
	}
	say("designs from the game's own file: " + std::to_string(stored));
	expect(stored > 0 && first != nullptr, "the designs of the game's own file are loaded as stored designs");
	if (first == nullptr)
	{
		return;
	}
	expect(withId == stored, "each of them got an id of its own, without the file having one");
	expect(withLink == 0, "none of them is in a line yet");
	const std::string basisName = first->name.toUtf8();
	const std::string basisId = first->storedId.toUtf8();
	// stand-in for research: the player can build the design from here on
	apCompLists[selectedPlayer][COMP_BODY][first->asParts[COMP_BODY]] = AVAILABLE;
	apCompLists[selectedPlayer][COMP_PROPULSION][first->asParts[COMP_PROPULSION]] = AVAILABLE;
	for (unsigned i = 0; i < first->numWeaps; ++i)
	{
		apCompLists[selectedPlayer][COMP_WEAPON][first->asWeaps[i]] = AVAILABLE;
	}
	expect(researchedTemplate(first, selectedPlayer), "'" + basisName + "' can be built after its components are researched");
	DROID_TEMPLATE *upgraded = createUpgradedDesign(*first);
	expect(upgraded != nullptr, "an upgrade of '" + basisName + "' can be made");
	if (upgraded == nullptr)
	{
		return;
	}
	first = findLocal(basisName);
	expect(first != nullptr && upgraded->supersedes.toUtf8() == basisId, "the upgrade replaces the design it was made from");
	const std::string file = readVaultFile();
	say("---- stored designs file after the upgrade ----");
	say(file);
	expect(file.find("\"supersedes\"") != std::string::npos, "the line is in the game's stored designs file itself (no separate file)");
	expect(findLocal(upgraded->name.toUtf8()) != nullptr, "'" + upgraded->name.toUtf8() + "' is in the list");
	desSetupDesignTemplates();
	bool basisOffered = false, upgradeOffered = false;
	for (DROID_TEMPLATE *t : apsTemplateList)
	{
		basisOffered = basisOffered || t->storedId.toUtf8() == basisId;
		upgradeOffered = upgradeOffered || t == upgraded;
	}
	say(std::string("design screen: original ") + (basisOffered ? "offered" : "hidden") + ", upgrade " + (upgradeOffered ? "offered" : "hidden"));
	expect(!(basisOffered && upgradeOffered), "the design screen never shows both of a pair");
	if (researchedTemplate(findLocal(basisName), selectedPlayer))
	{
		expect(upgradeOffered && !basisOffered, "'" + basisName + "' can be built, so the design screen offers the upgrade and not the original");
	}
	apsTemplateList.clear();
}

/// Drives the design screen through the same handlers a click on its buttons would call, and takes screenshots of it.
struct UiTest
{
	unsigned frame = 0;
	std::vector<std::unique_ptr<Availability>> available;
	DROID_TEMPLATE *a = nullptr;
	bool opened = false;

	static int indexInList(const DROID_TEMPLATE *t)
	{
		for (size_t i = 0; i < apsTemplateList.size(); ++i)
		{
			if (apsTemplateList[i] == t)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}
	static void click(const DROID_TEMPLATE *t)
	{
		const int index = indexInList(t);
		if (index >= 0)
		{
			intProcessDesign(IDDES_TEMPLSTART + static_cast<UDWORD>(index));
		}
	}
	static void shot(const char *name)
	{
		screenDumpToDisk("screenshots/", name);
	}

	/// Returns true when the test is over.
	bool step()
	{
		++frame;
		const int viper = body("Body1REC"), cobra = body("Body5REC");
		switch (frame)
		{
		case 10:
			available.push_back(std::make_unique<Availability>(viper, true));
			available.push_back(std::make_unique<Availability>(cobra, true));
			available.push_back(std::make_unique<Availability>(getCompFromName(COMP_PROPULSION, WzString("wheeled01")), true, COMP_PROPULSION));
			available.push_back(std::make_unique<Availability>(getCompFromName(COMP_WEAPON, WzString("MG1Mk1")), true, COMP_WEAPON));
			a = makeStored("VT Mk", viper);
			storeTemplates();
			makeStored("VT Other", cobra);
			{
				DROID_TEMPLATE *plain = findLocal("VT Other");
				setDesignStored(*plain, false);
				copyTemplate(selectedPlayer, plain);
				plain->name = WzString::fromUtf8("VT Not stored");
			}
			break;
		case 12:
			intResetScreen(true);
			widgSetButtonState(psWScreen, IDRET_DESIGN, WBUT_CLICKLOCK);
			intShowPowerBar();
			intAddDesign(false);
			intMode = INT_DESIGN;
			opened = true;
			break;
		case 16:
			expect(indexInList(a) > 0, "the stored design is in the design screen's list");
			click(a);
			break;
		case 22:
			shot("1-stored-design-selected");
			break;
		case 26:
			intProcessDesign(IDDES_UPGRADEBUTTON);
			break;
		case 28:
		{
			DROID_TEMPLATE *b = findLocal("VT Mk 2");
			expect(b != nullptr && b->stored && b->supersedes == a->storedId, "the Upgrade button made a stored 'VT Mk 2' that replaces 'VT Mk'");
			expect(indexInList(b) > 0 && indexInList(a) < 0, "the list now shows the upgrade and not the design it replaces: " + offeredByDesignScreenNoRebuild());
			expect(readVaultFile().find("VT Mk 2") != std::string::npos, "the upgrade was written to the file right away");
			break;
		}
		case 34:
			shot("2-after-upgrade");
			break;
		case 38:
			intProcessDesign(IDSTAT_OBSOLETE_BUTTON);
			break;
		case 40:
			expect(indexInList(a) > 0 && indexInList(findLocal("VT Mk 2")) > 0, "with obsolete designs shown, the whole line is listed: " + offeredByDesignScreenNoRebuild());
			break;
		case 44:
			shot("3-obsolete-shown");
			break;
		case 48:
			intProcessDesign(IDSTAT_OBSOLETE_BUTTON); // hide again
			break;
		case 52:
			click(findLocal("VT Mk 2"));
			break;
		case 56:
			intProcessDesign(IDDES_BIN);
			break;
		case 60:
			expect(findLocal("VT Mk 2") == nullptr, "the bin removed the upgrade");
			expect(readVaultFile().find("VT Mk 2") == std::string::npos && readVaultFile().find("\"VT Mk\"") != std::string::npos, "...and from the file, while the original is still there");
			a = findLocal("VT Mk");
			expect(a != nullptr && indexInList(a) > 0, "the original design is back in the list");
			break;
		case 64:
			click(a);
			break;
		case 68:
			intProcessDesign(IDDES_STOREBUTTON); // un-store
			break;
		case 72:
			expect(!findLocal("VT Mk")->stored && readVaultFile().find("\"VT Mk\"") == std::string::npos, "the disk button un-stores a design, and the file follows");
			shot("4-unstored");
			break;
		case 76:
			intProcessDesign(IDDES_STOREBUTTON); // store again
			break;
		case 80:
			expect(findLocal("VT Mk")->stored && readVaultFile().find("\"VT Mk\"") != std::string::npos, "the disk button stores it again");
			break;
		case 84:
			intResetScreen(true);
			apsTemplateList.clear();
			available.clear(); // gives the components back now: after the game has shut down their lists are gone
			return true;
		}
		return false;
	}

	static std::string offeredByDesignScreenNoRebuild()
	{
		std::string result;
		for (DROID_TEMPLATE *t : apsTemplateList)
		{
			if (t->name.toUtf8().rfind("VT ", 0) == 0)
			{
				result += (result.empty() ? "" : ",") + t->name.toUtf8();
			}
		}
		return result;
	}
};

} // namespace

void designVaultSelfTestTick()
{
	static const char *mode = getenv("WZ_DESIGNVAULT_SELFTEST");
	static const char *outPath = getenv("WZ_DESIGNVAULT_SELFTEST_OUT");
	static bool done = false;
	if (mode == nullptr || outPath == nullptr || done)
	{
		return;
	}
	if (std::string(mode) == "dump")
	{
		// Look at the designs at a few moments of a game with AI players on the same team.
		static unsigned nextDump = 3000;
		static bool sped = false;
		if (!sped)
		{
			sped = true;
			gameTimeSetMod(Rational(20)); // single player only: run the game faster than real time
		}
		if (gameTime < nextDump)
		{
			return;
		}
		if (!report.is_open())
		{
			report.open(outPath, std::ios::trunc);
			say(std::string("design dump, ruleset ") + rulesettag);
		}
		dumpDesigns(std::to_string(nextDump / 1000).c_str());
		report.flush();
		nextDump = nextDump == 3000 ? 60000 : nextDump + 120000;
		if (nextDump > 400000)
		{
			report.close();
			done = true;
			wzQuit(0);
		}
		return;
	}
	if (std::string(mode) == "ui")
	{
		static UiTest ui;
		if (!report.is_open())
		{
			report.open(outPath, std::ios::trunc);
			say(std::string("design vault UI test, ruleset ") + rulesettag);
		}
		if (ui.step())
		{
			say(std::to_string(checks) + " checks, " + std::to_string(failures) + " failures");
			report.close();
			done = true;
			wzQuit(failures == 0 ? 0 : 1);
		}
		return;
	}
	if (gameTime < 3000)
	{
		return;
	}
	done = true;

	report.open(outPath, std::ios::trunc);
	say(std::string("design vault self-test, mode ") + mode + ", ruleset " + rulesettag);
	if (std::string(mode) == "stockfile")
	{
		runStockFile();
		say(std::to_string(checks) + " checks, " + std::to_string(failures) + " failures");
		report.close();
		wzQuit(failures == 0 ? 0 : 1);
		return;
	}
	if (std::string(mode) == "real")
	{
		runReal();
		report.close();
		wzQuit(0);
		return;
	}
	if (std::string(mode).rfind("campaign", 0) == 0)
	{
		runCampaign(std::string(mode) == "campaign-build");
		say(std::to_string(checks) + " checks, " + std::to_string(failures) + " failures");
		report.close();
		wzQuit(failures == 0 ? 0 : 1);
		return;
	}
	STRUCTURE *psFactory = findFactory();
	expect(psFactory != nullptr, "the player has a factory (start the game with bases)");
	if (psFactory != nullptr)
	{
		if (std::string(mode) == "build")
		{
			runBuild(psFactory);
		}
		else if (std::string(mode) == "save")
		{
			runSave(psFactory);
		}
		else if (std::string(mode) == "load")
		{
			runLoad(psFactory);
		}
		else
		{
			runCheck(psFactory);
		}
	}
	apsTemplateList.clear(); // it points into localTemplates, from which this test has erased designs
	say(std::to_string(checks) + " checks, " + std::to_string(failures) + " failures");
	report.close();
	wzQuit(failures == 0 ? 0 : 1);
}
