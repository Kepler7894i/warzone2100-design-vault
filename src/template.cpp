/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2021  Warzone 2100 Project

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
/**
 * @file template.cpp
 *
 * Droid template functions.
 *
 */
#include "lib/framework/frame.h"
#include "lib/framework/wzconfig.h"
#include "lib/framework/math_ext.h"
#include "lib/framework/strres.h"
#include "lib/netplay/netplay.h"
#include "template.h"
#include "cmddroiddef.h"
#include "mission.h"
#include "objects.h"
#include "droid.h"
#include "design.h"
#include "hci.h"
#include "multiplay.h"
#include "projectile.h"
#include "main.h"
#include "research.h"
#include "designchain.h"
#include "lib/framework/file.h"
#include <physfs.h>

#include <algorithm>
#include <ctime>
#include <random>
#include <unordered_map>
#include <unordered_set>

// Template storage
std::map<UDWORD, std::unique_ptr<DROID_TEMPLATE>> droidTemplates[MAX_PLAYERS];
std::vector<std::unique_ptr<DROID_TEMPLATE>> replacedDroidTemplates[MAX_PLAYERS];

// Entries of the stored templates file that could not become designs in this game (for example because a
// component does not exist in this ruleset). They are written back unchanged, so that playing a game
// that lacks some components never destroys designs that are stored for another one.
static std::vector<nlohmann::json> preservedStoredTemplates;
// The stored templates file is only ever written after it has been read for the ruleset that is running now.
// Otherwise a game that never loaded it would overwrite it with an empty list.
static bool storedTemplatesLoaded = false;
static std::string storedTemplatesLoadedTag;

#define ASSERT_PLAYER_OR_RETURN(retVal, player) \
	ASSERT_OR_RETURN(retVal, player >= 0 && player < MAX_PLAYERS, "Invalid player: %" PRIu32 "", player);

bool allowDesign = true;
bool includeRedundantDesigns = false;
bool playerBuiltHQ = false;


static bool researchedItem(int player, COMPONENT_TYPE partIndex, int part, bool allowZero, bool allowRedundant)
{
	ASSERT_PLAYER_OR_RETURN(false, player);
	if (allowZero && part <= 0)
	{
		return true;
	}
	int availability = apCompLists[player][partIndex][part];
	return availability == AVAILABLE || (allowRedundant && availability == REDUNDANT);
}

static bool researchedPart(const DROID_TEMPLATE *psCurr, int player, COMPONENT_TYPE partIndex, bool allowZero, bool allowRedundant)
{
	return researchedItem(player, partIndex, psCurr->asParts[partIndex], allowZero, allowRedundant);
}

static bool researchedWeap(const DROID_TEMPLATE *psCurr, int player, int weapIndex, bool allowRedundant)
{
	ASSERT_PLAYER_OR_RETURN(false, player);
	int availability = apCompLists[player][COMP_WEAPON][psCurr->asWeaps[weapIndex]];
	return availability == AVAILABLE || (allowRedundant && availability == REDUNDANT);
}

bool researchedTemplate(const DROID_TEMPLATE *psCurr, int player, bool allowRedundant, bool verbose)
{
	ASSERT_OR_RETURN(false, psCurr, "Given a null template");
	ASSERT_PLAYER_OR_RETURN(false, player);
	bool resBody = researchedPart(psCurr, player, COMP_BODY, false, allowRedundant);
	bool resBrain = researchedPart(psCurr, player, COMP_BRAIN, true, allowRedundant);
	bool resProp = researchedPart(psCurr, player, COMP_PROPULSION, false, allowRedundant);
	bool resSensor = researchedPart(psCurr, player, COMP_SENSOR, true, allowRedundant);
	bool resEcm = researchedPart(psCurr, player, COMP_ECM, true, allowRedundant);
	bool resRepair = researchedPart(psCurr, player, COMP_REPAIRUNIT, true, allowRedundant);
	bool resConstruct = researchedPart(psCurr, player, COMP_CONSTRUCT, true, allowRedundant);
	bool researchedEverything = resBody && resBrain && resProp && resSensor && resEcm && resRepair && resConstruct;
	if (verbose && !researchedEverything)
	{
		debug(LOG_ERROR, "%s : not researched : body=%d brai=%d prop=%d sensor=%d ecm=%d rep=%d con=%d", getStatsName(psCurr),
		      (int)resBody, (int)resBrain, (int)resProp, (int)resSensor, (int)resEcm, (int)resRepair, (int)resConstruct);
	}
	for (int weapIndex = 0; weapIndex < psCurr->numWeaps && researchedEverything; ++weapIndex)
	{
		researchedEverything = researchedWeap(psCurr, player, weapIndex, allowRedundant);
		if (!researchedEverything && verbose)
		{
			debug(LOG_ERROR, "%s : not researched weapon %u", getStatsName(psCurr), weapIndex);
		}
	}
	return researchedEverything;
}

bool droidTemplate_LoadPartByName(COMPONENT_TYPE compType, const WzString &name, DROID_TEMPLATE &outputTemplate)
{
	int index = getCompFromName(compType, name);
	if (index < 0)
	{
		debug(LOG_ERROR, "Stored template contains an unknown (type: %d) component: %s.", compType, name.toUtf8().c_str());
		return false;
	}
	if (index > UINT8_MAX)
	{
		// returned index exceeds uint8_t max - consider changing type of asParts in DROID_TEMPLATE?
		debug(LOG_ERROR, "Stored template contains a (type: %d) component (%s) index that exceeds UINT8_MAX: %d", compType, name.toUtf8().c_str(), index);
		return false;
	}
	outputTemplate.asParts[compType] = static_cast<uint8_t>(index);
	return true;
}

bool droidTemplate_LoadWeapByName(size_t destIndex, const WzString &name, DROID_TEMPLATE &outputTemplate)
{
	int index = getCompFromName(COMP_WEAPON, name);
	if (index < 0)
	{
		debug(LOG_ERROR, "Stored template contains an unknown (type: %d) component: %s.", COMP_WEAPON, name.toUtf8().c_str());
		return false;
	}
#if INT_MAX > UINT32_MAX
	if (index > (int)UINT32_MAX)
	{
		// returned index exceeds uint32_t max - consider changing type of asWeaps in DROID_TEMPLATE?
		debug(LOG_ERROR, "Stored template contains a (type: %d) component (%s) index that exceeds UINT32_MAX: %d", COMP_WEAPON, name.toUtf8().c_str(), index);
		return false;
	}
#endif
	outputTemplate.asWeaps[destIndex] = static_cast<uint32_t>(index);
	return true;
}

bool loadTemplateCommon(WzConfig &ini, DROID_TEMPLATE &outputTemplate)
{
	return loadTemplateCommon(ini.currentJsonValue(), outputTemplate);
}

WzString jsonGetAsWzString(const nlohmann::json& obj, const WzString &key, const WzString &defaultValue = WzString())
{
	auto it = obj.find(key.toUtf8());
	if (it == obj.end())
	{
		return defaultValue;
	}
	else
	{
		// Use json_variant to support conversion from other value types to string
		bool ok = false;
		WzString stringValue = json_variant(it.value()).toWzString(&ok);
		if (!ok)
		{
			ASSERT(false, "Failed to convert value of key \"%s\" to string", key.toUtf8().c_str());
			return defaultValue;
		}
		return stringValue;
	}
}

bool loadTemplateCommon(const nlohmann::json& obj, DROID_TEMPLATE &outputTemplate)
{
	ASSERT_OR_RETURN(false, obj.is_object(), "Not an object?");
	DROID_TEMPLATE &design = outputTemplate;
	design.name = jsonGetAsWzString(obj, "name");
	WzString droidType = jsonGetAsWzString(obj, "type");

	if (droidType == "ECM")
	{
		design.droidType = DROID_ECM;
	}
	else if (droidType == "SENSOR")
	{
		design.droidType = DROID_SENSOR;
	}
	else if (droidType == "CONSTRUCT")
	{
		design.droidType = DROID_CONSTRUCT;
	}
	else if (droidType == "WEAPON")
	{
		design.droidType = DROID_WEAPON;
	}
	else if (droidType == "PERSON")
	{
		design.droidType = DROID_PERSON;
	}
	else if (droidType == "CYBORG")
	{
		design.droidType = DROID_CYBORG;
	}
	else if (droidType == "CYBORG_SUPER")
	{
		design.droidType = DROID_CYBORG_SUPER;
	}
	else if (droidType == "CYBORG_CONSTRUCT")
	{
		design.droidType = DROID_CYBORG_CONSTRUCT;
	}
	else if (droidType == "CYBORG_REPAIR")
	{
		design.droidType = DROID_CYBORG_REPAIR;
	}
	else if (droidType == "TRANSPORTER")
	{
		design.droidType = DROID_TRANSPORTER;
	}
	else if (droidType == "SUPERTRANSPORTER")
	{
		design.droidType = DROID_SUPERTRANSPORTER;
	}
	else if (droidType == "DROID")
	{
		design.droidType = DROID_DEFAULT;
	}
	else if (droidType == "DROID_COMMAND")
	{
		design.droidType = DROID_COMMAND;
	}
	else if (droidType == "REPAIR")
	{
		design.droidType = DROID_REPAIR;
	}
	else
	{
		ASSERT(false, "No such droid type \"%s\" for %s", droidType.toUtf8().c_str(), getID(&design));
	}

	if (!droidTemplate_LoadPartByName(COMP_BODY, jsonGetAsWzString(obj, "body"), design)) return false;
	if (!droidTemplate_LoadPartByName(COMP_BRAIN, jsonGetAsWzString(obj, "brain", WzString("ZNULLBRAIN")), design)) return false;
	if (!droidTemplate_LoadPartByName(COMP_PROPULSION, jsonGetAsWzString(obj, "propulsion", WzString("ZNULLPROP")), design)) return false;
	if (!droidTemplate_LoadPartByName(COMP_REPAIRUNIT, jsonGetAsWzString(obj, "repair", WzString("ZNULLREPAIR")), design)) return false;
	if (!droidTemplate_LoadPartByName(COMP_ECM, jsonGetAsWzString(obj, "ecm", WzString("ZNULLECM")), design)) return false;
	if (!droidTemplate_LoadPartByName(COMP_SENSOR, jsonGetAsWzString(obj, "sensor", WzString("ZNULLSENSOR")), design)) return false;
	if (!droidTemplate_LoadPartByName(COMP_CONSTRUCT, jsonGetAsWzString(obj, "construct", WzString("ZNULLCONSTRUCT")), design)) return false;

	std::vector<WzString> weapons;
	auto it = obj.find("weapons");
	if (it != obj.end())
	{
		weapons = json_variant(it.value()).toWzStringList();
	}
	ASSERT(weapons.size() <= MAX_WEAPONS, "Number of weapons (%zu) exceeds MAX_WEAPONS (%d)", weapons.size(), MAX_WEAPONS);
	design.numWeaps = (weapons.size() <= MAX_WEAPONS) ? (int8_t)weapons.size() : MAX_WEAPONS;
	if (!droidTemplate_LoadWeapByName(0, (weapons.size() >= 1) ? weapons[0] : "ZNULLWEAPON", design)) return false;
	if (!droidTemplate_LoadWeapByName(1, (weapons.size() >= 2) ? weapons[1] : "ZNULLWEAPON", design)) return false;
	if (!droidTemplate_LoadWeapByName(2, (weapons.size() >= 3) ? weapons[2] : "ZNULLWEAPON", design)) return false;

	// Only stored templates have these.
	design.storedId = jsonGetAsWzString(obj, "vaultId");
	design.supersedes = jsonGetAsWzString(obj, "supersedes");

	return true;
}

// A way to check if a design is something someone could legitimately have in multiplayer
bool designableTemplate(const DROID_TEMPLATE *psTempl, int player)
{
	if (!bMultiPlayer || !isHumanPlayer(player))
	{
		return true; // Don't care about AIs
	}

	char const *failPart = nullptr;
	WzString failPartName;
	auto designablePart = [&](COMPONENT_STATS const &component, char const *part) {
		if (!component.designable)
		{
			failPart = part;
			failPartName = component.name;
		}
		return component.designable;
	};

	bool designable = false;

	if (psTempl->droidType == DROID_CYBORG ||
		psTempl->droidType == DROID_CYBORG_SUPER ||
		psTempl->droidType == DROID_CYBORG_CONSTRUCT ||
		psTempl->droidType == DROID_CYBORG_REPAIR)
	{
		bool repair = ((psTempl->asParts[COMP_REPAIRUNIT] == 0) ||
						(!designablePart(*psTempl->getRepairStats(), "Repair unit") && psTempl->getRepairStats()->usageClass == UsageClass::Cyborg) ||
						(psTempl->asParts[COMP_REPAIRUNIT] != 0 && selfRepairEnabled(player)));
		bool isSuperCyborg = psTempl->getBodyStats()->usageClass == UsageClass::SuperCyborg;

		designable =
			   !designablePart(*psTempl->getBodyStats(), "Body")
			&& (strcmp(psTempl->getBodyStats()->bodyClass.toStdString().c_str(), "Cyborgs") == 0)
			&& !designablePart(*psTempl->getPropulsionStats(), "Propulsion")
			&& psTempl->getPropulsionStats()->propulsionType == PROPULSION_TYPE_LEGGED
			&& psTempl->getPropulsionStats()->usageClass == UsageClass::Cyborg
			&& repair
			&& ((psTempl->asParts[COMP_BRAIN] == 0) || (!designablePart(*psTempl->getBrainStats(), "Brain") && psTempl->getBrainStats()->usageClass == UsageClass::Cyborg))
			&& ((psTempl->asParts[COMP_ECM] == 0) || (!designablePart(*psTempl->getECMStats(), "ECM") && psTempl->getECMStats()->usageClass == UsageClass::Cyborg))
			&& ((psTempl->asParts[COMP_SENSOR] == 0) || (!designablePart(*psTempl->getSensorStats(), "Sensor") && psTempl->getSensorStats()->usageClass == UsageClass::Cyborg))
			&& ((psTempl->asParts[COMP_CONSTRUCT] == 0) || (!designablePart(*psTempl->getConstructStats(), "Construction part") && psTempl->getConstructStats()->usageClass == UsageClass::Cyborg))
			&& ((psTempl->numWeaps <= 0) || (psTempl->getBrainStats()->psWeaponStat == psTempl->getWeaponStats(0))
				|| (!designablePart(*psTempl->getWeaponStats(0), "Weapon 0") && ((!isSuperCyborg && psTempl->getWeaponStats(0)->usageClass == UsageClass::Cyborg) || (isSuperCyborg && psTempl->getWeaponStats(0)->usageClass == UsageClass::SuperCyborg))))
			&& ((psTempl->numWeaps <= 1) || (!designablePart(*psTempl->getWeaponStats(1), "Weapon 1") && ((!isSuperCyborg && psTempl->getWeaponStats(1)->usageClass == UsageClass::Cyborg) || (isSuperCyborg && psTempl->getWeaponStats(1)->usageClass == UsageClass::SuperCyborg))))
			&& ((psTempl->numWeaps <= 2) || (!designablePart(*psTempl->getWeaponStats(2), "Weapon 2") && ((!isSuperCyborg && psTempl->getWeaponStats(2)->usageClass == UsageClass::Cyborg) || (isSuperCyborg && psTempl->getWeaponStats(2)->usageClass == UsageClass::SuperCyborg))));
	}
	else
	{
		bool repair = ((psTempl->asParts[COMP_REPAIRUNIT] == 0) ||
						designablePart(*psTempl->getRepairStats(), "Repair unit") ||
						(psTempl->asParts[COMP_REPAIRUNIT] != 0 && selfRepairEnabled(player)));
		bool transporter = (psTempl->droidType == DROID_TRANSPORTER) || (psTempl->droidType == DROID_SUPERTRANSPORTER);

		designable =
			   (transporter || (!transporter && designablePart(*psTempl->getBodyStats(), "Body")))
			&& designablePart(*psTempl->getPropulsionStats(), "Propulsion")
			&& (psTempl->asParts[COMP_BRAIN]      == 0 || designablePart(*psTempl->getBrainStats(), "Brain"))
			&& repair
			&& (psTempl->asParts[COMP_ECM]        == 0 || designablePart(*psTempl->getECMStats(), "ECM"))
			&& (psTempl->asParts[COMP_SENSOR]     == 0 || designablePart(*psTempl->getSensorStats(), "Sensor"))
			&& (psTempl->asParts[COMP_CONSTRUCT]  == 0 || designablePart(*psTempl->getConstructStats(), "Construction part"))
			&& (psTempl->numWeaps <= 0 || psTempl->getBrainStats()->psWeaponStat == psTempl->getWeaponStats(0)
									 || designablePart(*psTempl->getWeaponStats(0), "Weapon 0"))
			&& (psTempl->numWeaps <= 1 || designablePart(*psTempl->getWeaponStats(1), "Weapon 1"))
			&& (psTempl->numWeaps <= 2 || designablePart(*psTempl->getWeaponStats(2), "Weapon 2"));

		if (transporter && designable)
		{
			designable = (psTempl->getPropulsionStats()->propulsionType == PROPULSION_TYPE_LIFT) &&
						!designablePart(*psTempl->getBodyStats(), "Body");
			if (designable)
			{
				designable = strcmp(psTempl->getBodyStats()->bodyClass.toStdString().c_str(), "Transports") == 0;
			}
		}
	}

	if (!designable)
	{
		debug(LOG_ERROR, "%s \"%s\" for \"%s\" cannot be designed", failPart, failPartName.toUtf8().c_str(), psTempl->name.toUtf8().c_str());
	}

	return designable;
}

static WzString storedTemplatesPath()
{
	return "userdata/" + WzString(rulesettag) + "/templates.json";
}

static const char *const kVaultIdKey = "vaultId";
static const char *const kSupersedesKey = "supersedes";

static std::string jsonStringOr(const nlohmann::json &obj, const char *key)
{
	auto it = obj.find(key);
	return (it != obj.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

static std::unordered_set<std::string> knownStoredIds()
{
	std::unordered_set<std::string> ids;
	for (const DROID_TEMPLATE &t : localTemplates)
	{
		if (!t.storedId.isEmpty())
		{
			ids.insert(t.storedId.toUtf8());
		}
	}
	if (selectedPlayer < MAX_PLAYERS)
	{
		for (auto &keyvaluepair : droidTemplates[selectedPlayer])
		{
			if (!keyvaluepair.second->storedId.isEmpty())
			{
				ids.insert(keyvaluepair.second->storedId.toUtf8());
			}
		}
	}
	for (const nlohmann::json &raw : preservedStoredTemplates)
	{
		std::string id = jsonStringOr(raw, kVaultIdKey);
		if (!id.empty())
		{
			ids.insert(std::move(id));
		}
	}
	return ids;
}

WzString newStoredDesignId()
{
	// Not the game's random number generator: this identity never takes part in the simulation and never leaves this machine.
	static std::mt19937_64 rng = []() {
		std::random_device device;
		std::seed_seq seed{device(), device(), static_cast<unsigned>(time(nullptr))};
		return std::mt19937_64(seed);
	}();
	const std::unordered_set<std::string> known = knownStoredIds();
	for (;;)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(rng()));
		if (known.count(buffer) == 0)
		{
			return WzString::fromUtf8(buffer);
		}
	}
}

/// The copy of a UI template in the synchronised list. Edits that are not about the design itself are applied to both.
static DROID_TEMPLATE *synchronisedTwin(const DROID_TEMPLATE &localEntry)
{
	return selectedPlayer < MAX_PLAYERS ? findPlayerTemplateById(selectedPlayer, localEntry.multiPlayerID) : nullptr;
}

/// All the stored designs we know about as nodes of the lineage graph. Designs that did not load are included (as unusable), so that a line can pass through them.
static std::vector<DesignChain::Node> collectChainNodes(const std::function<bool (const DROID_TEMPLATE &)> &isUsable)
{
	std::vector<DesignChain::Node> nodes;
	for (const DROID_TEMPLATE &t : localTemplates)
	{
		if (t.stored && !t.storedId.isEmpty())
		{
			nodes.push_back({t.storedId.toUtf8(), t.supersedes.toUtf8(), isUsable ? isUsable(t) : false});
		}
	}
	for (const nlohmann::json &raw : preservedStoredTemplates)
	{
		std::string id = jsonStringOr(raw, kVaultIdKey);
		if (!id.empty())
		{
			nodes.push_back({std::move(id), jsonStringOr(raw, kSupersedesKey), false});
		}
	}
	return nodes;
}

/// Makes `storedId` replace `supersedes` (which may be empty) wherever that stored design lives in memory.
static void applySupersedes(const std::string &storedId, const std::string &supersedes)
{
	const WzString link = WzString::fromUtf8(supersedes);
	for (DROID_TEMPLATE &t : localTemplates)
	{
		if (t.storedId.toUtf8() == storedId)
		{
			t.supersedes = link;
			if (DROID_TEMPLATE *twin = synchronisedTwin(t))
			{
				twin->supersedes = link;
			}
		}
	}
	for (nlohmann::json &raw : preservedStoredTemplates)
	{
		if (jsonStringOr(raw, kVaultIdKey) == storedId)
		{
			if (supersedes.empty())
			{
				raw.erase(kSupersedesKey);
			}
			else
			{
				raw[kSupersedesKey] = supersedes;
			}
		}
	}
}

/// A stored design leaves the lineage: the designs that replaced it replace its predecessor instead.
static void spliceStoredDesignOut(const WzString &storedId)
{
	if (storedId.isEmpty())
	{
		return;
	}
	std::vector<DesignChain::Node> nodes = collectChainNodes(nullptr);
	for (const std::string &relinked : DesignChain::spliceOut(nodes, storedId.toUtf8()))
	{
		for (const DesignChain::Node &node : nodes)
		{
			if (node.id == relinked)
			{
				applySupersedes(relinked, node.supersedes);
			}
		}
	}
}

void setDesignStored(DROID_TEMPLATE &design, bool stored)
{
	if (stored)
	{
		if (design.storedId.isEmpty())
		{
			design.storedId = newStoredDesignId();
		}
		design.stored = true;
		return;
	}
	spliceStoredDesignOut(design.storedId);
	design.stored = false;
	design.storedId.clear();
	design.supersedes.clear();
}

void forgetStoredDesign(DROID_TEMPLATE &localEntry)
{
	if (!localEntry.stored)
	{
		return;
	}
	// The synchronised copy has to stay, since a factory may still be building it, but it must not be written to the file again.
	if (DROID_TEMPLATE *twin = synchronisedTwin(localEntry))
	{
		twin->stored = false;
		twin->storedId.clear();
		twin->supersedes.clear();
	}
	setDesignStored(localEntry, false);
	storeTemplates();
}

DROID_TEMPLATE *createUpgradedDesign(DROID_TEMPLATE &basis)
{
	if (selectedPlayer >= MAX_PLAYERS)
	{
		return nullptr;
	}
	// The line starts with the design we are upgrading, so that has to be stored as well.
	const bool basisWasStored = basis.stored;
	setDesignStored(basis, true);
	if (!basisWasStored)
	{
		if (DROID_TEMPLATE *twin = synchronisedTwin(basis))
		{
			twin->stored = true;
			twin->storedId = basis.storedId;
		}
	}

	DROID_TEMPLATE upgraded = basis;
	upgraded.multiPlayerID = generateNewObjectId();
	upgraded.prefab = false;
	upgraded.id.clear();
	upgraded.name = WzString::fromUtf8(DesignChain::nextVersionName(basis.name.toUtf8()));
	upgraded.storedId.clear();
	setDesignStored(upgraded, true);
	upgraded.supersedes = basis.storedId;

	copyTemplate(selectedPlayer, &upgraded);
	localTemplates.push_back(upgraded);
	storeTemplates();
	return &localTemplates.back();
}

std::unordered_set<std::string> findSupersededDesigns(const std::function<bool (const DROID_TEMPLATE &)> &isUsable)
{
	return DesignChain::supersededIds(collectChainNodes(isUsable));
}

bool isDesignSuperseded(const DROID_TEMPLATE &design, const std::unordered_set<std::string> &superseded)
{
	return design.stored && !design.storedId.isEmpty() && superseded.count(design.storedId.toUtf8()) != 0;
}

/// Are these the same design: same name, same components?
static bool sameDesign(const DROID_TEMPLATE &a, const DROID_TEMPLATE &b)
{
	return a.droidType == b.droidType
	       && a.name.compare(b.name) == 0
	       && a.numWeaps == b.numWeaps
	       && a.asWeaps[0] == b.asWeaps[0]
	       && a.asWeaps[1] == b.asWeaps[1]
	       && a.asWeaps[2] == b.asWeaps[2]
	       && a.asParts[COMP_BODY] == b.asParts[COMP_BODY]
	       && a.asParts[COMP_PROPULSION] == b.asParts[COMP_PROPULSION]
	       && a.asParts[COMP_REPAIRUNIT] == b.asParts[COMP_REPAIRUNIT]
	       && a.asParts[COMP_ECM] == b.asParts[COMP_ECM]
	       && a.asParts[COMP_SENSOR] == b.asParts[COMP_SENSOR]
	       && a.asParts[COMP_CONSTRUCT] == b.asParts[COMP_CONSTRUCT]
	       && a.asParts[COMP_BRAIN] == b.asParts[COMP_BRAIN];
}

bool initTemplates()
{
	if (selectedPlayer >= MAX_PLAYERS) { return false; }

	storedTemplatesLoaded = false;
	storedTemplatesLoadedTag = rulesettag;
	preservedStoredTemplates.clear();

	WzConfig ini(storedTemplatesPath(), WzConfig::ReadOnly);
	if (!ini.status())
	{
		debug(LOG_WZ, "Could not open %s", ini.fileName().toUtf8().c_str());
		storedTemplatesLoaded = true; // nothing is stored yet, so there is nothing to lose if the file is created later
		return false;
	}
	int version = ini.value("version", 0).toInt();
	if (version == 0)
	{
		return true; // too old version, which is also why we never write to it
	}

	std::unordered_set<std::string> idsInFile;
	std::vector<DROID_TEMPLATE> loadedFromFile;
	for (ini.beginArray("templates"); ini.remainingArrayItems(); ini.nextArrayItem())
	{
		const nlohmann::json raw = ini.currentJsonValue();
		DROID_TEMPLATE design;
		bool loadCommonSuccess = loadTemplateCommon(ini, design);
		design.multiPlayerID = generateNewObjectId();
		design.prefab = false;		// not AI template
		design.stored = true;

		if (!loadCommonSuccess)
		{
			debug(LOG_ERROR, "Stored template \"%s\" contains an unknown component. Keeping it in the file.", design.name.toUtf8().c_str());
			preservedStoredTemplates.push_back(raw);
			continue;
		}
		bool valid = intValidTemplate(&design, ini.value("name").toWzString().toUtf8().c_str(), false, selectedPlayer);
		if (!valid)
		{
			debug(LOG_ERROR, "Invalid template \"%s\" from stored templates. Keeping it in the file.", design.name.toUtf8().c_str());
			preservedStoredTemplates.push_back(raw);
			continue;
		}
		if (design.storedId.isEmpty() || !idsInFile.insert(design.storedId.toUtf8()).second)
		{
			// Written by an older version, or a duplicate: needs a new identity (a link to it from elsewhere is lost, which is the best we can do).
			design.storedId = newStoredDesignId();
			idsInFile.insert(design.storedId.toUtf8());
		}
		loadedFromFile.push_back(std::move(design));
	}
	ini.endArray();
	// The same design stored in several games piles up in the file. Exact copies are merged into one.
	{
		std::unordered_map<std::string, std::string> mergedInto;
		std::vector<DROID_TEMPLATE> unique;
		for (DROID_TEMPLATE &design : loadedFromFile)
		{
			auto same = std::find_if(unique.begin(), unique.end(), [&design](const DROID_TEMPLATE &other) { return sameDesign(other, design); });
			if (same != unique.end())
			{
				mergedInto[design.storedId.toUtf8()] = same->storedId.toUtf8();
				idsInFile.erase(design.storedId.toUtf8());
				continue;
			}
			unique.push_back(std::move(design));
		}
		for (DROID_TEMPLATE &design : unique)
		{
			auto merged = mergedInto.find(design.supersedes.toUtf8());
			if (merged != mergedInto.end())
			{
				design.supersedes = WzString::fromUtf8(merged->second);
			}
		}
		loadedFromFile = std::move(unique);
	}
	for (const nlohmann::json &raw : preservedStoredTemplates)
	{
		std::string id = jsonStringOr(raw, kVaultIdKey);
		if (!id.empty())
		{
			idsInFile.insert(std::move(id));
		}
	}

	// Designs that are already in the game (for example from a saved game) become the ones in the file.
	for (DROID_TEMPLATE &design : loadedFromFile)
	{
		DROID_TEMPLATE *existing = nullptr;
		for (DROID_TEMPLATE &candidate : localTemplates)
		{
			if (!candidate.storedId.isEmpty() && candidate.storedId == design.storedId)
			{
				existing = &candidate;
				break;
			}
		}
		if (existing == nullptr)
		{
			// Same design, stored by an older version or in a saved game made before designs had an identity?
			for (DROID_TEMPLATE &candidate : localTemplates)
			{
				if ((candidate.storedId.isEmpty() || !candidate.stored) && sameDesign(candidate, design))
				{
					existing = &candidate;
					break;
				}
			}
		}
		if (existing != nullptr)
		{
			existing->stored = true; // assimilate it
			existing->storedId = design.storedId;
			existing->supersedes = design.supersedes;
			if (DROID_TEMPLATE *twin = synchronisedTwin(*existing))
			{
				twin->stored = true;
				twin->storedId = design.storedId;
				twin->supersedes = design.supersedes;
			}
			continue; // next!
		}
		design.enabled = allowDesign;
		copyTemplate(selectedPlayer, &design);
		localTemplates.push_back(design);
	}

	// A design that a saved game remembers as stored, but that is no longer in the file, was deleted since. It stays in this game, but is not stored anymore.
	const auto unstoreIfDeleted = [&idsInFile](DROID_TEMPLATE &t) {
		if (t.stored && idsInFile.count(t.storedId.toUtf8()) == 0)
		{
			t.stored = false;
			t.storedId.clear();
			t.supersedes.clear();
		}
	};
	for (DROID_TEMPLATE &t : localTemplates)
	{
		unstoreIfDeleted(t);
	}
	for (auto &keyvaluepair : droidTemplates[selectedPlayer])
	{
		unstoreIfDeleted(*keyvaluepair.second);
	}

	// A hand-edited or damaged file must not be able to create loops or links to nowhere.
	std::vector<DesignChain::Node> nodes = collectChainNodes(nullptr);
	for (const std::string &cleared : DesignChain::sanitize(nodes))
	{
		debug(LOG_ERROR, "Stored template %s replaces a template that does not exist, or would form a loop. Ignoring that link.", cleared.c_str());
		applySupersedes(cleared, std::string());
	}

	storedTemplatesLoaded = true;
	return true;
}

nlohmann::json saveTemplateCommon(const DROID_TEMPLATE *psCurr)
{
	nlohmann::json templateObj = nlohmann::json::object();
	templateObj["name"] = psCurr->name;
	if (!psCurr->storedId.isEmpty())
	{
		templateObj["vaultId"] = psCurr->storedId;
		if (!psCurr->supersedes.isEmpty())
		{
			templateObj["supersedes"] = psCurr->supersedes;
		}
	}
	switch (psCurr->droidType)
	{
	case DROID_ECM: templateObj["type"] = "ECM"; break;
	case DROID_SENSOR: templateObj["type"] = "SENSOR"; break;
	case DROID_CONSTRUCT: templateObj["type"] = "CONSTRUCT"; break;
	case DROID_WEAPON: templateObj["type"] = "WEAPON"; break;
	case DROID_PERSON: templateObj["type"] = "PERSON"; break;
	case DROID_CYBORG: templateObj["type"] = "CYBORG"; break;
	case DROID_CYBORG_SUPER: templateObj["type"] = "CYBORG_SUPER"; break;
	case DROID_CYBORG_CONSTRUCT: templateObj["type"] = "CYBORG_CONSTRUCT"; break;
	case DROID_CYBORG_REPAIR: templateObj["type"] = "CYBORG_REPAIR"; break;
	case DROID_TRANSPORTER: templateObj["type"] = "TRANSPORTER"; break;
	case DROID_SUPERTRANSPORTER: templateObj["type"] = "SUPERTRANSPORTER"; break;
	case DROID_COMMAND: templateObj["type"] = "DROID_COMMAND"; break;
	case DROID_REPAIR: templateObj["type"] = "REPAIR"; break;
	case DROID_DEFAULT: templateObj["type"] = "DROID"; break;
	default: ASSERT(false, "No such droid type \"%d\" for %s", psCurr->droidType, psCurr->name.toUtf8().c_str());
	}
	ASSERT(psCurr->asParts[COMP_BODY] < asBodyStats.size(), "asParts[COMP_BODY] (%d) exceeds numBodyStats (%zu)", (int)psCurr->asParts[COMP_BODY], asBodyStats.size());
	templateObj["body"] = psCurr->getBodyStats()->id;
	ASSERT(psCurr->asParts[COMP_PROPULSION] < asPropulsionStats.size(), "asParts[COMP_PROPULSION] (%d) exceeds numPropulsionStats (%zu)", (int)psCurr->asParts[COMP_PROPULSION], asPropulsionStats.size());
	templateObj["propulsion"] = psCurr->getPropulsionStats()->id;
	if (psCurr->asParts[COMP_BRAIN] != 0)
	{
		ASSERT(psCurr->asParts[COMP_BRAIN] < asBrainStats.size(), "asParts[COMP_BRAIN] (%d) exceeds numBrainStats (%zu)", (int)psCurr->asParts[COMP_BRAIN], asBrainStats.size());
		templateObj["brain"] = psCurr->getBrainStats()->id;
	}
	if (psCurr->getRepairStats()->location == LOC_TURRET) // avoid auto-repair...
	{
		ASSERT(psCurr->asParts[COMP_REPAIRUNIT] < asRepairStats.size(), "asParts[COMP_REPAIRUNIT] (%d) exceeds numRepairStats (%zu)", (int)psCurr->asParts[COMP_REPAIRUNIT], asRepairStats.size());
		templateObj["repair"] = psCurr->getRepairStats()->id;
	}
	if (psCurr->getECMStats()->location == LOC_TURRET)
	{
		ASSERT(psCurr->asParts[COMP_ECM] < asECMStats.size(), "asParts[COMP_ECM] (%d) exceeds numECMStats (%zu)", (int)psCurr->asParts[COMP_ECM], asECMStats.size());
		templateObj["ecm"] = psCurr->getECMStats()->id;
	}
	if (psCurr->getSensorStats()->location == LOC_TURRET)
	{
		ASSERT(psCurr->asParts[COMP_SENSOR] < asSensorStats.size(), "asParts[COMP_SENSOR] (%d) exceeds numSensorStats (%zu)", (int)psCurr->asParts[COMP_SENSOR], asSensorStats.size());
		templateObj["sensor"] = psCurr->getSensorStats()->id;
	}
	if (psCurr->asParts[COMP_CONSTRUCT] != 0)
	{
		ASSERT(psCurr->asParts[COMP_CONSTRUCT] < asConstructStats.size(), "asParts[COMP_CONSTRUCT] (%d) exceeds numConstructStats (%zu)", (int)psCurr->asParts[COMP_CONSTRUCT], asConstructStats.size());
		templateObj["construct"] = psCurr->getConstructStats()->id;
	}
	nlohmann::json weapons = nlohmann::json::array();
	for (int j = 0; j < psCurr->numWeaps; j++)
	{
		ASSERT(psCurr->asWeaps[j] < asWeaponStats.size(), "psCurr->asWeaps[%d] (%d) exceeds numWeaponStats (%zu)", j, (int)psCurr->asWeaps[j], asWeaponStats.size());
		weapons.push_back(psCurr->getWeaponStats(j)->id);
	}
	if (!weapons.empty())
	{
		templateObj["weapons"] = std::move(weapons);
	}
	return templateObj;
}

bool storeTemplates()
{
	if (selectedPlayer >= MAX_PLAYERS) { return false; }
	if (!storedTemplatesLoaded || storedTemplatesLoadedTag != rulesettag)
	{
		debug(LOG_WZ, "Not writing stored templates: they have not been read for ruleset \"%s\"", rulesettag);
		return false;
	}

	nlohmann::json templates = nlohmann::json::array();
	// What the player designed is in localTemplates. The synchronised copies are not used when they have a twin there, because
	// research that makes a component obsolete swaps components in those: such a change belongs to one game, not to the stored design.
	std::unordered_set<UDWORD> written;
	for (const DROID_TEMPLATE &t : localTemplates)
	{
		if (t.stored)
		{
			templates.push_back(saveTemplateCommon(&t));
			written.insert(t.multiPlayerID);
		}
	}
	for (auto &keyvaluepair : droidTemplates[selectedPlayer])
	{
		const DROID_TEMPLATE *psCurr = keyvaluepair.second.get();
		if (psCurr->stored && written.count(psCurr->multiPlayerID) == 0)
		{
			templates.push_back(saveTemplateCommon(psCurr));
		}
	}
	for (const nlohmann::json &raw : preservedStoredTemplates)
	{
		templates.push_back(raw);
	}
	nlohmann::json root = nlohmann::json::object();
	root["version"] = 1; // for breaking backwards compatibility in a nice way
	root["templates"] = std::move(templates);

	const std::string path = storedTemplatesPath().toUtf8();
	const std::string directory = path.substr(0, path.rfind('/'));
	PHYSFS_mkdir(directory.c_str()); // custom rulesets have no directory yet

	// Stored designs are the one thing in here that cannot be recreated by playing again, so
	// the first write of each session keeps a copy of what was there before.
	static std::string backedUpPath;
	if (backedUpPath != path)
	{
		backedUpPath = path;
		std::vector<char> previous;
		if (PHYSFS_exists(path.c_str()) && loadFileToBufferVector(path.c_str(), previous, false, false) && !previous.empty())
		{
			const std::string backupPath = path + ".bak";
			saveFile(backupPath.c_str(), previous.data(), static_cast<UDWORD>(previous.size()));
		}
	}

	std::string jsonText;
	try
	{
		jsonText = root.dump(4) + "\n";
	}
	catch (const std::exception &e)
	{
		ASSERT(false, "Failed to serialise stored templates: %s", e.what());
		return false;
	}
	if (!saveFile(path.c_str(), jsonText.c_str(), static_cast<UDWORD>(jsonText.size())))
	{
		debug(LOG_ERROR, "Could not write %s", path.c_str());
		return false;
	}
	return true;
}

bool shutdownTemplates()
{
	return storeTemplates();
}

DROID_TEMPLATE::DROID_TEMPLATE()  // This constructor replaces a memset in scrAssembleWeaponTemplate(), not needed elsewhere.
	: BASE_STATS(STAT_TEMPLATE)
	  //, asParts
	, numWeaps(0)
	  //, asWeaps
	, droidType(DROID_WEAPON)
	, multiPlayerID(0)
	, prefab(false)
	, stored(false)
	, enabled(false)
{
	std::fill_n(asParts, DROID_MAXCOMP, static_cast<uint8_t>(0));
	std::fill_n(asWeaps, MAX_WEAPONS, 0);
}

BODY_STATS* DROID_TEMPLATE::getBodyStats() const
{
	return &asBodyStats[asParts[COMP_BODY]];
}

BRAIN_STATS* DROID_TEMPLATE::getBrainStats() const
{
	return &asBrainStats[asParts[COMP_BRAIN]];
}

PROPULSION_STATS* DROID_TEMPLATE::getPropulsionStats() const
{
	return &asPropulsionStats[asParts[COMP_PROPULSION]];
}

SENSOR_STATS* DROID_TEMPLATE::getSensorStats() const
{
	return &asSensorStats[asParts[COMP_SENSOR]];
}

ECM_STATS* DROID_TEMPLATE::getECMStats() const
{
	return &asECMStats[asParts[COMP_ECM]];
}

REPAIR_STATS* DROID_TEMPLATE::getRepairStats() const
{
	return &asRepairStats[asParts[COMP_REPAIRUNIT]];
}

CONSTRUCT_STATS* DROID_TEMPLATE::getConstructStats() const
{
	return &asConstructStats[asParts[COMP_CONSTRUCT]];
}

WEAPON_STATS* DROID_TEMPLATE::getWeaponStats(int weaponSlot) const
{
	return &asWeaponStats[asWeaps[weaponSlot]];
}

bool loadDroidTemplates(const char *filename)
{
	WzConfig ini(filename, WzConfig::ReadOnlyAndRequired);
	std::vector<WzString> list = ini.childGroups();
	for (int i = 0; i < list.size(); ++i)
	{
		ini.beginGroup(list[i]);
		DROID_TEMPLATE design;
		if (!loadTemplateCommon(ini, design))
		{
			debug(LOG_ERROR, "Stored template \"%s\" contains an unknown component.", ini.string("name").toUtf8().c_str());
			continue;
		}
		design.id = list[i];
		design.name = ini.string("name");
		design.multiPlayerID = generateNewObjectId();
		design.prefab = true;
		design.stored = false;
		design.enabled = true;
		bool available = ini.value("available", false).toBool();
		char const *droidResourceName = getDroidResourceName(list[i].toUtf8().c_str());
		design.name = WzString::fromUtf8(droidResourceName != nullptr ? droidResourceName : GetDefaultTemplateName(&design));
		ini.endGroup();

		for (int playerIdx = 0; playerIdx < MAX_PLAYERS; ++playerIdx)
		{
			// Give those meant for humans to all human players.
			if (NetPlay.players[playerIdx].allocated && available)
			{
				design.prefab = false;
				copyTemplate(playerIdx, &design);

				// This sets up the UI templates for display purposes ONLY--we still only use droidTemplates for making them.
				// FIXME: Why are we doing this here, and not on demand ?
				// Only add unique designs to the UI list (Note, perhaps better to use std::map instead?)
				std::list<DROID_TEMPLATE>::iterator it;
				for (it = localTemplates.begin(); it != localTemplates.end(); ++it)
				{
					DROID_TEMPLATE *psCurr = &*it;
					if (psCurr->multiPlayerID == design.multiPlayerID)
					{
						debug(LOG_WARNING, "Design id:%d (%s) *NOT* added to UI list (duplicate), player= %d", design.multiPlayerID, getStatsName(&design), playerIdx);
						break;
					}
				}
				if (it == localTemplates.end())
				{
					debug(LOG_NEVER, "Design id:%d (%s) added to UI list, player =%d", design.multiPlayerID, getStatsName(&design), playerIdx);
					localTemplates.push_back(design);
				}
			}
			else if (!NetPlay.players[playerIdx].allocated)	// AI template
			{
				design.prefab = true;  // prefabricated templates referenced from VLOs
				copyTemplate(playerIdx, &design);
			}
		}
		debug(LOG_NEVER, "Droid template found, Name: %s, MP ID: %d, ref: %u, ID: %s, prefab: %s, type:%d (loading)",
		      getStatsName(&design), design.multiPlayerID, design.ref, getID(&design), design.prefab ? "yes" : "no", design.droidType);
	}

	return true;
}

DROID_TEMPLATE *copyTemplate(int player, DROID_TEMPLATE *psTemplate)
{
	auto dup = std::make_unique<DROID_TEMPLATE>(*psTemplate);
	return addTemplate(player, std::move(dup));
}

DROID_TEMPLATE* addTemplate(int player, std::unique_ptr<DROID_TEMPLATE> psTemplate)
{
	ASSERT_PLAYER_OR_RETURN(nullptr, player);
	UDWORD multiPlayerID = psTemplate->multiPlayerID;
	auto it = droidTemplates[player].find(multiPlayerID);
	if (it != droidTemplates[player].end())
	{
		// replacing existing template
		it->second.swap(psTemplate);
		replacedDroidTemplates[player].push_back(std::move(psTemplate));
		return it->second.get();
	}
	else
	{
		// new template
		auto result = droidTemplates[player].insert(std::pair<UDWORD, std::unique_ptr<DROID_TEMPLATE>>(multiPlayerID, std::move(psTemplate)));
		return result.first->second.get();
	}
}

void enumerateTemplates(int player, const std::function<bool (DROID_TEMPLATE* psTemplate)>& func)
{
	ASSERT_PLAYER_OR_RETURN(, player);
	for (auto &keyvaluepair : droidTemplates[player])
	{
		if (!func(keyvaluepair.second.get()))
		{
			break;
		}
	}
}

DROID_TEMPLATE* findPlayerTemplateById(int player, UDWORD templateId)
{
	ASSERT_PLAYER_OR_RETURN(nullptr, player);
	auto it = droidTemplates[player].find(templateId);
	if (it != droidTemplates[player].end())
	{
		return it->second.get();
	}
	return nullptr;
}

size_t templateCount(int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return droidTemplates[player].size();
}

void clearTemplates(int player)
{
	ASSERT_PLAYER_OR_RETURN(, player);
	droidTemplates[player].clear();
	replacedDroidTemplates[player].clear();
}

//free the storage for the droid templates
bool droidTemplateShutDown()
{
	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		clearTemplates(player);
	}
	localTemplates.clear();
	return true;
}

/*!
 * Get a static template from its name. This is used from scripts. These templates must
 * never be changed or deleted.
 * \param pName Template name
 * \pre pName has to be the unique, untranslated name!
 */
const DROID_TEMPLATE *getTemplateFromTranslatedNameNoPlayer(char const *pName)
{
	for (auto &droidTemplate : droidTemplates)
	{
		for (auto &keyvaluepair : droidTemplate)
		{
			if (keyvaluepair.second->id.compare(pName) == 0)
			{
				return keyvaluepair.second.get();
			}
		}
	}
	return nullptr;
}

/*getTemplatefFromMultiPlayerID gets template for unique ID  searching all lists */
DROID_TEMPLATE *getTemplateFromMultiPlayerID(UDWORD multiPlayerID)
{
	for (auto &droidTemplate : droidTemplates)
	{
		if (droidTemplate.count(multiPlayerID) > 0)
		{
			return droidTemplate[multiPlayerID].get();
		}
	}
	return nullptr;
}

/*called when a Template is deleted in the Design screen*/
void deleteTemplateFromProduction(DROID_TEMPLATE *psTemplate, unsigned player, QUEUE_MODE mode)
{
	ASSERT_OR_RETURN(, psTemplate != nullptr, "Null psTemplate");
	ASSERT_OR_RETURN(, player < MAX_PLAYERS, "Invalid player: %u", player);

	//see if any factory is currently using the template
	for (unsigned i = 0; i < 2; ++i)
	{
		StructureList* psList = nullptr;
		switch (i)
		{
		case 0:
			psList = &apsStructLists[player];
			break;
		case 1:
			psList = &mission.apsStructLists[player];
			break;
		}
		if (!psList)
		{
			continue;
		}
		for (STRUCTURE* psStruct : *psList)
		{
			if (psStruct && psStruct->isFactory())
			{
				if (psStruct->pFunctionality == nullptr)
				{
					continue;
				}
				FACTORY *psFactory = &psStruct->pFunctionality->factory;

				if (psFactory->psAssemblyPoint && psFactory->psAssemblyPoint->factoryType < NUM_FACTORY_TYPES
					&& psFactory->psAssemblyPoint->factoryInc < asProductionRun[psFactory->psAssemblyPoint->factoryType].size())
				{
					ProductionRun &productionRun = asProductionRun[psFactory->psAssemblyPoint->factoryType][psFactory->psAssemblyPoint->factoryInc];
					for (unsigned inc = 0; inc < productionRun.size(); ++inc)
					{
						if (productionRun[inc].psTemplate && productionRun[inc].psTemplate->multiPlayerID == psTemplate->multiPlayerID && mode == ModeQueue)
						{
							//just need to erase this production run entry
							productionRun.erase(productionRun.begin() + inc);
							--inc;
						}
					}
				}

				if (psFactory->psSubject == nullptr)
				{
					continue;
				}

				// check not being built in the factory for the template player
				if (psTemplate->multiPlayerID == psFactory->psSubject->multiPlayerID && mode == ModeImmediate)
				{
					syncDebugStructure(psStruct, '<');
					syncDebug("Clearing production");

					// Clear the factory's subject, and returns power.
					cancelProduction(psStruct, ModeImmediate, false);
					// Check to see if anything left to produce. (Also calls cancelProduction again, if nothing left to produce, which is a no-op. But if other things are left to produce, doesn't call cancelProduction, so wouldn't return power without the explicit cancelProduction call above.)
					doNextProduction(psStruct, nullptr, ModeImmediate);

					syncDebugStructure(psStruct, '>');
				}
			}
		}
	}
}

// return whether a template is for an IDF droid
bool templateIsIDF(const DROID_TEMPLATE *psTemplate)
{
	//add Cyborgs
	if (!(psTemplate->droidType == DROID_WEAPON || psTemplate->droidType == DROID_CYBORG || psTemplate->droidType == DROID_CYBORG_SUPER))
	{
		return false;
	}

	if (proj_Direct(psTemplate->getWeaponStats(0)))
	{
		return false;
	}

	return true;
}

void listTemplates()
{
	ASSERT_OR_RETURN(, selectedPlayer < MAX_PLAYERS, "selectedPlayer (%" PRIu32 ") >= MAX_PLAYERS", selectedPlayer);
	for (auto &keyvaluepair : droidTemplates[selectedPlayer])
	{
		DROID_TEMPLATE *t = keyvaluepair.second.get();
		debug(LOG_INFO, "template %s : %ld : %s : %s : %s", getStatsName(t), (long)t->multiPlayerID, t->enabled ? "Enabled" : "Disabled", t->stored ? "Stored" : "Temporal", t->prefab ? "Prefab" : "Designed");
	}
}

/*
fills the list with Templates that can be manufactured
in the Factory - based on size. There is a limit on how many can be manufactured
at any one time.
*/
std::vector<DROID_TEMPLATE *> fillTemplateList(STRUCTURE *psFactory)
{
	std::vector<DROID_TEMPLATE *> pList;
	const int player = psFactory->player;

	BODY_SIZE	iCapacity = (BODY_SIZE)psFactory->capacity;

	const auto bodyFitsFactory = [iCapacity](const DROID_TEMPLATE *psTempl) {
		const BODY_SIZE bodySize = psTempl->getBodyStats()->size;
		// Special case for Super heavy bodyies (Super Transporter)
		return bodySize <= iCapacity || (bMultiPlayer && iCapacity == SIZE_HEAVY && bodySize == SIZE_SUPER_HEAVY);
	};
	const auto buildableHere = [&](const DROID_TEMPLATE &templ) {
		return templ.enabled
			&& !(bMultiPlayer && !playerBuiltHQ && (templ.droidType != DROID_CONSTRUCT && templ.droidType != DROID_CYBORG_CONSTRUCT))
			&& validTemplateForFactory(&templ, psFactory, false)
			&& researchedTemplate(&templ, player, includeRedundantDesigns)
			&& bodyFitsFactory(&templ);
	};
	// Designs that a newer design replaces are not offered, unless obsolete designs are being shown.
	// Whether a newer design counts is decided per factory: if this factory cannot build it, the old design stays.
	std::unordered_set<std::string> superseded;
	if (!includeRedundantDesigns)
	{
		superseded = findSupersededDesigns(buildableHere);
	}

	/* Add the templates to the list*/
	for (DROID_TEMPLATE &i : localTemplates)
	{
		DROID_TEMPLATE *psCurr = &i;
		// Must add droids if currently in production.
		if (!getProduction(psFactory, psCurr).quantity)
		{
			if (!psCurr->enabled
				|| (bMultiPlayer && !playerBuiltHQ && (psCurr->droidType != DROID_CONSTRUCT && psCurr->droidType != DROID_CYBORG_CONSTRUCT))
				|| !validTemplateForFactory(psCurr, psFactory, false)
				|| !researchedTemplate(psCurr, player, includeRedundantDesigns)
				|| isDesignSuperseded(*psCurr, superseded))
			{
				continue;
			}
		}

		//check the factory can cope with this sized body
		if (psCurr->getBodyStats()->size <= iCapacity)
		{
			pList.push_back(psCurr);
		}
		else if (bMultiPlayer && (iCapacity == SIZE_HEAVY))
		{
			// Special case for Super heavy bodyies (Super Transporter)
			if (psCurr->getBodyStats()->size == SIZE_SUPER_HEAVY)
			{
				pList.push_back(psCurr);
			}
		}
	}

	return pList;
}

void checkPlayerBuiltHQ(const STRUCTURE *psStruct)
{
	if (selectedPlayer == psStruct->player && psStruct->pStructureType->type == REF_HQ)
	{
		playerBuiltHQ = true;
	}
}
