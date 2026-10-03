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
 * @file designvault_icons.cpp
 * Loading and drawing of the small images the design vault adds to the design screen. See designvault_icons.h.
 */

#include "lib/framework/frame.h"
#include "lib/ivis_opengl/ivisdef.h"
#include "lib/ivis_opengl/pietypes.h"
#include "lib/ivis_opengl/pieblitfunc.h"
#include "lib/ivis_opengl/png_util.h"
#include "lib/ivis_opengl/tex.h"
#include "lib/ivis_opengl/gfx_api.h"
#include "lib/widget/widget.h"
#include "lib/widget/button.h"

#include <vector>

#include "designvault_icons.h"

namespace
{
struct IconAtlas
{
	bool tried = false;
	bool ok = false;
	size_t page = 0;
};

IconAtlas &atlas()
{
	static IconAtlas a;
	return a;
}

// Uploads the atlas on first use. A failure is remembered, so that it is reported once and the screen just goes without icons.
bool ensureAtlas()
{
	IconAtlas &a = atlas();
	if (a.tried)
	{
		return a.ok;
	}
	a.tried = true;

	const std::vector<unsigned char> png(DesignVaultIcons::atlasPng, DesignVaultIcons::atlasPng + DesignVaultIcons::atlasPngSize);
	iV_Image image;
	IMGSaveError err = iV_loadImage_PNG(png, &image);
	if (!err.noError())
	{
		debug(LOG_ERROR, "Design vault: the built-in icons did not decode: %s", err.text.c_str());
		return false;
	}
	if (image.width() != static_cast<unsigned>(DesignVaultIcons::atlasSize) || image.height() != static_cast<unsigned>(DesignVaultIcons::atlasSize))
	{
		debug(LOG_ERROR, "Design vault: the built-in icons have an unexpected size (%u x %u)", image.width(), image.height());
		return false;
	}
	image.convert_to_rgba();
	const char *name = "designvault-icons";
	gfx_api::texture *texture = gfx_api::context::get().loadTextureFromUncompressedImage(std::move(image), gfx_api::texture_type::user_interface, name);
	if (texture == nullptr)
	{
		debug(LOG_ERROR, "Design vault: the built-in icons could not be uploaded");
		return false;
	}
	a.page = pie_AddTexPage(texture, name, gfx_api::texture_type::user_interface);
	a.ok = true;
	return true;
}

void drawIcon(const DesignVaultIcons::Rect &r, int x, int y)
{
	if (!ensureAtlas())
	{
		return;
	}
	AtlasImageDef def = {};
	def.TPageID = 0;
	def.Tu = r.x;
	def.Tv = r.y;
	def.Width = r.w;
	def.Height = r.h;
	def.XOffset = 0;
	def.YOffset = 0;
	def.textureId = atlas().page;
	def.invTextureSize = 1.f / DesignVaultIcons::atlasSize;
	iV_DrawImage2(&def, x, y, r.w, r.h);
}
}

void designVaultDrawStoredMarker(int x, int y)
{
	drawIcon(DesignVaultIcons::markerRect, x, y);
}

void intDisplayUpgradeButton(WIDGET *psWidget, unsigned xOffset, unsigned yOffset)
{
	const int x = xOffset + psWidget->x();
	const int y = yOffset + psWidget->y();
	const unsigned state = psWidget->getState();
	const bool lit = (state & (WBUT_HIGHLIGHT | WBUT_DOWN | WBUT_LOCK | WBUT_CLICKLOCK)) != 0;
	drawIcon(lit ? DesignVaultIcons::upgradeHighlightRect : DesignVaultIcons::upgradeRect, x, y);
}

int designVaultUpgradeButtonWidth()
{
	return DesignVaultIcons::upgradeRect.w;
}

int designVaultUpgradeButtonHeight()
{
	return DesignVaultIcons::upgradeRect.h;
}
