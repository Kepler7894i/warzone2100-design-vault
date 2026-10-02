/*
	This file is part of Warzone 2100.
	Copyright (C) 2011-2021  Warzone 2100 Project

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

#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "lib/framework/wzconfig.h"
#include "droiddef.h"
#include <memory>
#include <functional>
#include <string>
#include <unordered_set>

extern bool allowDesign;
extern bool includeRedundantDesigns;
extern bool playerBuiltHQ;

bool designableTemplate(DROID_TEMPLATE *psTempl, int player);
bool initTemplates();

/// Take ownership of template given by pointer.
/// Returns a new usable DROID_TEMPLATE *
DROID_TEMPLATE* addTemplate(int player, std::unique_ptr<DROID_TEMPLATE> psTemplate);

/// Make a duplicate of template given by pointer and store it. Then return pointer to copy.
DROID_TEMPLATE *copyTemplate(int player, DROID_TEMPLATE *psTemplate);

void enumerateTemplates(int player, const std::function<bool (DROID_TEMPLATE* psTemplate)>& func);
DROID_TEMPLATE* findPlayerTemplateById(int player, UDWORD templateId);
size_t templateCount(int player);

void clearTemplates(int player);
bool shutdownTemplates();
bool storeTemplates();

bool loadDroidTemplates(const char *filename);

/// return whether a template is for an IDF droid
bool templateIsIDF(DROID_TEMPLATE *psTemplate);

/// Fills the list with Templates that can be manufactured in the Factory - based on size
std::vector<DROID_TEMPLATE *> fillTemplateList(STRUCTURE *psFactory);

/* gets a template from its name - relies on the name being unique */
const DROID_TEMPLATE *getTemplateFromTranslatedNameNoPlayer(char const *pName);

/*getTemplateFromMultiPlayerID gets template for unique ID  searching all lists */
DROID_TEMPLATE *getTemplateFromMultiPlayerID(UDWORD multiPlayerID);

/// Have we researched the components of this template?
bool researchedTemplate(const DROID_TEMPLATE *psCurr, int player, bool allowRedundant = false, bool verbose = false);

void listTemplates();

// Stored designs. A stored design is kept in userdata/<ruleset>/templates.json and comes back in every game as soon as its components are researched.
// A stored design can replace an older stored design (see designchain.h), which then is not offered anymore while the newer one can be used.

/// A new identity for a stored design.
WzString newStoredDesignId();
/// Stores or unstores the given design. Gives it an identity, or takes it out of the line of designs it belongs to.
/// This changes `design` itself and any other design that was linked to it. It does not write anything to disk, call storeTemplates() for that.
void setDesignStored(DROID_TEMPLATE &design, bool stored);
/// Removes a design from the stored designs and writes the file. For a design that is about to be deleted. `localEntry` is an element of localTemplates.
void forgetStoredDesign(DROID_TEMPLATE &localEntry);
/// Makes a stored copy of `basis` (an element of localTemplates) that replaces it, and returns the copy (a new element of localTemplates).
DROID_TEMPLATE *createUpgradedDesign(DROID_TEMPLATE &basis);
/// The identities of the stored designs that are replaced by a stored design for which `isUsable` is true.
std::unordered_set<std::string> findSupersededDesigns(const std::function<bool (const DROID_TEMPLATE &)> &isUsable);
bool isDesignSuperseded(const DROID_TEMPLATE &design, const std::unordered_set<std::string> &superseded);
/// Developer self-test of the stored designs, see designvault_selftest.cpp. Does nothing unless WZ_DESIGNVAULT_SELFTEST is set.
void designVaultSelfTestTick();

nlohmann::json saveTemplateCommon(const DROID_TEMPLATE *psCurr);
bool loadTemplateCommon(WzConfig &ini, DROID_TEMPLATE &outputTemplate);

void checkPlayerBuiltHQ(const STRUCTURE *psStruct);

#endif // TEMPLATE_H
