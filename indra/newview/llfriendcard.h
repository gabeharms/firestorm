/** 
 * @file llfriendcard.h
 * @brief Definition of classes to process Friends Cards
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLFRIENDCARD_H
#define LL_LLFRIENDCARD_H


#include "llcallingcard.h"
#include "llinventorymodel.h" // for LLInventoryModel::item_array_t

class LLViewerInventoryItem;

class LLFriendCardsManager
	: public LLSingleton<LLFriendCardsManager>
	, public LLFriendObserver
{
	LOG_CLASS(LLFriendCardsManager);

	friend class LLSingleton<LLFriendCardsManager>;
	friend class CreateFriendCardCallback;

public:
	typedef std::map<LLUUID, std::vector<LLUUID> > folderid_buddies_map_t;

	// LLFriendObserver implementation
	void changed(U32 mask)
	{
		onFriendListUpdate(mask);
	}

	/**
	 *	Ensures that all necessary folders are created in Inventory.
	 * 
	 *	For now it processes Calling Card, Calling Card/Friends & Calling Card/Friends/All folders
	 */
	void ensureFriendFoldersExist();


	/**
	 *	Determines if specified Inventory Calling Card exists in any of lists 
	 *	in the Calling Card/Friends/ folder (Default, or Custom)
	 */
	bool isItemInAnyFriendsList(const LLViewerInventoryItem* item);

	/**
	 *	Checks if specified category is contained in the Calling Card/Friends folder and 
	 *	determines if specified Inventory Object exists in that category.
	 */
	bool isObjDirectDescendentOfCategory(const LLInventoryObject* obj, const LLViewerInventoryCategory* cat) const;

	/**
	 *	Checks is the specified category is in the Calling Card/Friends folder
	 */
	bool isCategoryInFriendFolder(const LLViewerInventoryCategory* cat) const;

	/**
	 *	Checks is the specified category is a Friend folder or any its subfolder
	 */
	bool isAnyFriendCategory(const LLUUID& catID) const;

	/**
	 *	Synchronizes content of the Calling Card/Friends/All Global Inventory folder with Agent's Friend List
	 *
	 *	@return true - if folder is already synchronized, false otherwise.
	 */
	bool syncFriendsFolder();

	/*!
	 * \brief
	 * Collects folders' IDs with the buddies' IDs in the Inventory Calling Card/Friends folder.
	 * 
	 * \param folderBuddiesMap
	 * map into collected data will be put. It will be cleared before adding new data.
	 * 
	 * Each item in the out map is a pair where first is an LLViewerInventoryCategory UUID,
	 * second is a vector with UUID of Avatars from this folder.
	 * 
	 */
	void collectFriendsLists(folderid_buddies_map_t& folderBuddiesMap) const;

private:
	LLFriendCardsManager();
	~LLFriendCardsManager();


	/**
	 *	Stores buddy id to avoid sent create_inventory_callingcard several time for the same Avatar
	 */
	void putAvatarData(const LLUUID& avatarID);

	/**
	 *	Extracts buddy id of Created Friend Card
	 */
	const LLUUID extractAvatarID(const LLUUID& avatarID);

	bool isAvatarDataStored(const LLUUID& avatarID) const
	{
		return (mBuddyIDSet.end() != mBuddyIDSet.find(avatarID));
	}

	const LLUUID& findChildFolderUUID(const LLUUID& parentFolderUUID, const std::string& folderLabel) const;
	const LLUUID& findFriendFolderUUIDImpl() const;
	const LLUUID& findFriendAllSubfolderUUIDImpl() const;
	const LLUUID& findFriendCardInventoryUUIDImpl(const LLUUID& avatarID);
	void findMatchedFriendCards(const LLUUID& avatarID, LLInventoryModel::item_array_t& items) const;

	/**
	 *	Adds avatar specified by its UUID into the Calling Card/Friends/All Global Inventory folder
	 */
	bool addFriendCardToInventory(const LLUUID& avatarID);

	/**
	 *	Removes an avatar specified by its UUID from the Calling Card/Friends/All Global Inventory folder
	 *		and from the all Custom Folders
	 */
	void removeFriendCardFromInventory(const LLUUID& avatarID);

	void onFriendListUpdate(U32 changed_mask);

	/**
	 * Force fetching of the Inventory folder specified by passed folder's LLUUID.
	 *
	 * It only sends request to server, server reply should be processed in other place.
	 * Because request can be sent via UDP we need to periodically check if request was completed with success.
	 */
	void forceFriendListIsLoaded(const LLUUID& folder_id) const;


private:
	typedef std::set<LLUUID> avatar_uuid_set_t;

	avatar_uuid_set_t mBuddyIDSet;
	bool mFriendsAllFolderCompleted;
};

#endif // LL_LLFRIENDCARD_H
