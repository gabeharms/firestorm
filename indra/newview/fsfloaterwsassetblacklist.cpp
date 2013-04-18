/**
 * @file fsfloaterwsassetblacklist.cpp
 * @brief Floater for Asset Blacklist and Derender
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

#include "llviewerprecompiledheaders.h"

#include "fsfloaterwsassetblacklist.h"

#include "fswsassetblacklist.h"
#include "llscrolllistctrl.h"
#include "llviewercontrol.h"


FSFloaterWSAssetBlacklist::FSFloaterWSAssetBlacklist(const LLSD& key)
 : LLFloater(key)
{
}

FSFloaterWSAssetBlacklist::~FSFloaterWSAssetBlacklist()
{
	if (mResultList)
	{
		gSavedSettings.setString("FSFloaterBlacklistSortColumn", mResultList->getSortColumnName());
		gSavedSettings.setBOOL("FSFloaterBlacklistSortAscending", mResultList->getSortAscending());
	}
}

std::string FSFloaterWSAssetBlacklist::getTypeString(S32 type)
{
	switch (type)
	{
		case 0:
			return getString("asset_texture");
			break;
		case 1:
			return getString("asset_sound");
			break;
		case 6:
			return getString("asset_object");
			break;
		default:
			return getString("asset_unknown");
			break;
	}
}

void FSFloaterWSAssetBlacklist::buildBlacklist()
{
	blacklist_data_t data = FSWSAssetBlacklist::instance().getBlacklistData();
	
	for (blacklist_data_t::const_iterator iterator = data.begin(); iterator != data.end(); ++iterator)
	{
		addElementToList(iterator->first, iterator->second);
	}
}

void FSFloaterWSAssetBlacklist::addElementToList(const LLUUID& id, const LLSD& data)
{
	LLSD element;
	element["id"] = id;
	element["columns"][0]["column"] = "name";
	element["columns"][0]["type"] = "text";
	element["columns"][0]["value"] = !data["asset_name"].asString().empty() ? data["asset_name"].asString() : getString("unknown_object");
	element["columns"][1]["column"] = "region";
	element["columns"][1]["type"] = "text";
	element["columns"][1]["value"] = !data["asset_region"].asString().empty() ? data["asset_region"].asString() : getString("unknown_region");
	element["columns"][2]["column"] = "type";
	element["columns"][2]["type"] = "text";
	element["columns"][2]["value"] = getTypeString(data["asset_type"].asInteger());
	element["columns"][3]["column"] = "date";
	element["columns"][3]["type"] = "text";
	element["columns"][3]["value"] = data["asset_date"].asString();

	mResultList->addElement(element, ADD_BOTTOM);
}

void FSFloaterWSAssetBlacklist::removeElementFromList(const LLUUID& id)
{
	mResultList->deleteSingleItem(mResultList->getItemIndex(id));
}


void FSFloaterWSAssetBlacklist::onOpen(const LLSD& key)
{	
	mResultList->clearRows();
	buildBlacklist();
}

BOOL FSFloaterWSAssetBlacklist::postBuild()
{
	mResultList = getChild<LLScrollListCtrl>("result_list");
    childSetAction("remove_btn", boost::bind(&FSFloaterWSAssetBlacklist::onRemoveBtn, this));
	std::string order_by = gSavedSettings.getString("FSFloaterBlacklistSortColumn");
	BOOL ascending = gSavedSettings.getBOOL("FSFloaterBlacklistSortAscending");

	if (!order_by.empty())
	{
		mResultList->sortByColumn(order_by, ascending);
	}

    return TRUE;
}

void FSFloaterWSAssetBlacklist::onRemoveBtn()
{
	std::vector<LLScrollListItem*> list = mResultList->getAllSelected();

	for (std::vector<LLScrollListItem*>::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		FSWSAssetBlacklist::instance().removeItemFromBlacklist((*it)->getUUID());
	}
	
	mResultList->deleteSelectedItems();
}

void FSFloaterWSAssetBlacklist::onCancelBtn()
{
    closeFloater();
}
