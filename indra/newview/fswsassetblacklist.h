/**
 * @file fswsassetblacklist.h
 * @brief Asset Blacklist and Derender
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Wolfspirit Magic
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_WSASSETBLACKLIST_H
#define FS_WSASSETBLACKLIST_H

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "fscommon.h"
#include "llsingleton.h"
#include "llfloater.h"
#include "llassettype.h"

typedef boost::unordered_set<LLUUID, FSUUIDEntryHasher> t_blacklisted_uuid_container;
typedef std::map<LLAssetType::EType, t_blacklisted_uuid_container> t_blacklist_type_map;
typedef boost::unordered_map<LLUUID, LLSD, FSUUIDEntryHasher> t_blacklist_data;

class FSWSAssetBlacklist : public LLSingleton<FSWSAssetBlacklist>
{
public:
	void init();
	bool isBlacklisted(const LLUUID& id, LLAssetType::EType type);
	void addNewItemToBlacklist(const LLUUID& id, const std::string& name, const std::string& region, LLAssetType::EType type, bool save = true);
	void addNewItemToBlacklistData(const LLUUID& id, const LLSD& data, bool save = true);
	void removeItemFromBlacklist(const LLUUID& id);

	t_blacklist_data getBlacklistData() const { return mBlacklistData; };

private:
	void loadBlacklist();
	void saveBlacklist();
	bool addEntryToBlacklistMap(const LLUUID& id, LLAssetType::EType type);
	
	std::string				mBlacklistFileName;
	t_blacklist_type_map	mBlacklistTypeContainer;
	t_blacklist_data		mBlacklistData;
};

#endif // FS_WSASSETBLACKLIST_H