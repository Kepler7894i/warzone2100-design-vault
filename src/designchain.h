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
 *  Bookkeeping for chains ("lines") of stored designs that supersede one another.
 *
 *  A stored design may name an older stored design that it replaces. Whenever a newer design in
 *  such a line can be used (all of its components are researched, the factory can build it, ...),
 *  every older design in the same line is hidden. If the newer design is not available yet, e.g.
 *  in a game where its body has not been researched, the older design is shown as usual.
 *
 *  This header deliberately depends on nothing but the standard library so that it can be unit tested on its own.
 */

#ifndef __INCLUDED_DESIGNCHAIN_H__
#define __INCLUDED_DESIGNCHAIN_H__

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace DesignChain
{

struct Node
{
	std::string id;          ///< Stable identity of a stored design. Nodes with an empty id are ignored.
	std::string supersedes;  ///< Id of the design this one replaces. Empty if it replaces nothing.
	bool usable = false;     ///< Can this design be used right now?
};

namespace detail
{
inline std::unordered_map<std::string, const Node *> indexById(const std::vector<Node> &nodes)
{
	std::unordered_map<std::string, const Node *> byId;
	for (const Node &node : nodes)
	{
		if (!node.id.empty())
		{
			byId.emplace(node.id, &node);  // first one wins, if there are duplicate ids
		}
	}
	return byId;
}
}

/// Ids of the designs that are hidden because a newer design in their line, possibly several steps
/// down the line, is usable. The chain is followed transitively, so an unusable middle link does not
/// break it: with A <- B <- C, a usable C hides A even if B itself is unusable.
inline std::unordered_set<std::string> supersededIds(const std::vector<Node> &nodes)
{
	const auto byId = detail::indexById(nodes);
	std::unordered_set<std::string> hidden;
	for (const Node &newest : nodes)
	{
		if (!newest.usable || newest.id.empty())
		{
			continue;
		}
		std::unordered_set<std::string> visited{newest.id};
		const Node *current = &newest;
		while (!current->supersedes.empty())
		{
			auto older = byId.find(current->supersedes);
			if (older == byId.end() || !visited.insert(older->first).second)
			{
				break;  // link to a design that no longer exists, or a loop
			}
			hidden.insert(older->first);
			current = older->second;
		}
	}
	return hidden;
}

/// Would making `childId` replace `parentId` close a loop?
inline bool wouldCreateCycle(const std::vector<Node> &nodes, const std::string &childId, const std::string &parentId)
{
	if (childId.empty() || parentId.empty())
	{
		return false;
	}
	if (childId == parentId)
	{
		return true;
	}
	const auto byId = detail::indexById(nodes);
	std::string current = parentId;
	for (size_t hops = 0; !current.empty() && hops <= nodes.size(); ++hops)
	{
		if (current == childId)
		{
			return true;
		}
		auto it = byId.find(current);
		if (it == byId.end())
		{
			return false;
		}
		current = it->second->supersedes;
	}
	return !current.empty();  // ran out of hops, so there is a loop somewhere above us
}

/// Repairs a lineage that was damaged on disk: clears links to designs that do not exist, links
/// to oneself and links that close a loop. Returns the ids of the nodes whose link was cleared.
inline std::vector<std::string> sanitize(std::vector<Node> &nodes)
{
	std::vector<std::string> cleared;
	std::unordered_set<std::string> ids;
	for (const Node &node : nodes)
	{
		if (!node.id.empty())
		{
			ids.insert(node.id);
		}
	}
	for (Node &node : nodes)
	{
		if (node.id.empty() || node.supersedes.empty())
		{
			continue;
		}
		if (ids.count(node.supersedes) == 0)
		{
			node.supersedes.clear();
			cleared.push_back(node.id);
			continue;
		}
		// Temporarily detach, then see whether the link would close a loop.
		const std::string link = node.supersedes;
		node.supersedes.clear();
		if (wouldCreateCycle(nodes, node.id, link))
		{
			cleared.push_back(node.id);
		}
		else
		{
			node.supersedes = link;
		}
	}
	return cleared;
}

/// Takes a design out of the lineage: designs that replaced it now replace its predecessor instead,
/// so that the rest of the line stays connected. Returns the ids of the nodes that were re-linked.
inline std::vector<std::string> spliceOut(std::vector<Node> &nodes, const std::string &removedId)
{
	std::vector<std::string> relinked;
	if (removedId.empty())
	{
		return relinked;
	}
	std::string predecessor;
	for (const Node &node : nodes)
	{
		if (node.id == removedId)
		{
			predecessor = node.supersedes;
			break;
		}
	}
	for (Node &node : nodes)
	{
		if (node.id == removedId)
		{
			node.supersedes.clear();
		}
		else if (!node.id.empty() && node.supersedes == removedId)
		{
			node.supersedes = predecessor;
			relinked.push_back(node.id);
		}
	}
	return relinked;
}

/// Name for an upgraded copy of a design: "Tiger HPV" becomes "Tiger HPV 2", and "Tiger HPV 2" becomes "Tiger HPV 3".
inline std::string nextVersionName(const std::string &name)
{
	size_t digitsStart = name.size();
	while (digitsStart > 0 && name[digitsStart - 1] >= '0' && name[digitsStart - 1] <= '9')
	{
		--digitsStart;
	}
	const size_t digits = name.size() - digitsStart;
	// Only a number set off by a space counts as a version ("Tiger HPV 2"), not part of the name ("HPV75").
	const bool hasVersion = digits >= 1 && digits <= 6 && digitsStart >= 2 && name[digitsStart - 1] == ' ';
	if (hasVersion)
	{
		return name.substr(0, digitsStart) + std::to_string(std::stoul(name.substr(digitsStart)) + 1);
	}
	return name.empty() ? std::string("2") : name + " 2";
}

} // namespace DesignChain

#endif // __INCLUDED_DESIGNCHAIN_H__
