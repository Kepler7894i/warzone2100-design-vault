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
/**
 * @file designvault_icons.h
 * The small images the design vault adds to the design screen: the marker on stored designs and the Upgrade button.
 * Their pixels are in designvault_icondata.cpp (made by scripts/gen-icons.py); they are uploaded as one texture on first use.
 */

#ifndef __INCLUDED_DESIGNVAULT_ICONS_H__
#define __INCLUDED_DESIGNVAULT_ICONS_H__

#include <cstddef>

class WIDGET;

namespace DesignVaultIcons
{
struct Rect
{
	int x, y, w, h;
};

// generated data, see designvault_icondata.cpp
extern const unsigned char atlasPng[];
extern const size_t atlasPngSize;
extern const int atlasSize;
extern const Rect upgradeRect;
extern const Rect upgradeHighlightRect;
extern const Rect markerRect;
}

/// Draws the marker of a stored design (the floppy disk without a frame) with its top left corner at x, y.
void designVaultDrawStoredMarker(int x, int y);

/// Display function of the Upgrade button: the game's button frame with an arrow pointing up, highlighted like the other buttons.
void intDisplayUpgradeButton(WIDGET *psWidget, unsigned xOffset, unsigned yOffset);

/// Size of the Upgrade button, the same as the store button next to it.
int designVaultUpgradeButtonWidth();
int designVaultUpgradeButtonHeight();

#endif // __INCLUDED_DESIGNVAULT_ICONS_H__
