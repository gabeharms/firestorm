/** 
 * @file llagentwearables.cpp
 * @brief LLAgentWearables class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llagent.h" 
#include "llagentwearables.h"

#include "llfloatercustomize.h"
#include "llfloaterinventory.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"
#include "llnotify.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llwearable.h"
#include "llwearablelist.h"

#include <boost/scoped_ptr.hpp>

// For viewer2.0 internal demo, don't use current outfit folder contents at all during initial startup.  Will reenable
// this once we're sure this works completely.
// #define USE_CURRENT_OUTFIT_FOLDER

LLAgentWearables gAgentWearables;

BOOL LLAgentWearables::mInitialWearablesUpdateReceived = FALSE;

using namespace LLVOAvatarDefines;

void LLAgentWearables::dump()
{
	llinfos << "LLAgentWearablesDump" << llendl;
	for (S32 i = 0; i < WT_COUNT; i++)
	{
		U32 count = getWearableCount((EWearableType)i);
		llinfos << "Type: " << i << " count " << count << llendl;
		for (U32 j=0; j<count; j++)
		{
			LLWearable* wearable = getWearable((EWearableType)i,j);
			if (wearable == NULL)
			{
				llinfos << "    " << j << " NULL wearable" << llendl;
			}
			llinfos << "    " << j << " Name " << wearable->getName()
					<< " description " << wearable->getDescription() << llendl;
			
		}
	}
	llinfos << "Total items awaiting wearable update " << mItemsAwaitingWearableUpdate.size() << llendl;
	for (std::set<LLUUID>::iterator it = mItemsAwaitingWearableUpdate.begin();
		 it != mItemsAwaitingWearableUpdate.end();
		 ++it)
	{
		llinfos << (*it).asString() << llendl;
	}
}

// MULTI-WEARABLE: debugging
struct LLAgentDumper
{
	LLAgentDumper(std::string name):
		mName(name)
	{
		llinfos << llendl;
		llinfos << "LLAgentDumper " << mName << llendl;
		gAgentWearables.dump();
	}

	~LLAgentDumper()
	{
		llinfos << llendl;
		llinfos << "~LLAgentDumper " << mName << llendl;
		gAgentWearables.dump();
	}

	std::string mName;
};

LLAgentWearables::LLAgentWearables() :
	mWearablesLoaded(FALSE),
	mAvatarObject(NULL)
{
	// MULTI-WEARABLE: TODO remove null entries.
	for (U32 i = 0; i < WT_COUNT; i++)
	{
		mWearableDatas[(EWearableType)i].push_back(NULL);
	}
}

LLAgentWearables::~LLAgentWearables()
{
	cleanup();
}

void LLAgentWearables::cleanup()
{
	mAvatarObject = NULL;
}

void LLAgentWearables::setAvatarObject(LLVOAvatarSelf *avatar)
{ 
	mAvatarObject = avatar;
	if (avatar)
	{
		sendAgentWearablesRequest();
	}
}

// wearables
LLAgentWearables::createStandardWearablesAllDoneCallback::~createStandardWearablesAllDoneCallback()
{
	gAgentWearables.createStandardWearablesAllDone();
}

LLAgentWearables::sendAgentWearablesUpdateCallback::~sendAgentWearablesUpdateCallback()
{
	gAgentWearables.sendAgentWearablesUpdate();
}

/**
 * @brief Construct a callback for dealing with the wearables.
 *
 * Would like to pass the agent in here, but we can't safely
 * count on it being around later.  Just use gAgent directly.
 * @param cb callback to execute on completion (??? unused ???)
 * @param type Type for the wearable in the agent
 * @param wearable The wearable data.
 * @param todo Bitmask of actions to take on completion.
 */
LLAgentWearables::addWearableToAgentInventoryCallback::addWearableToAgentInventoryCallback(
	LLPointer<LLRefCount> cb, S32 type, U32 index, LLWearable* wearable, U32 todo) :
	mType(type),
	mIndex(index),	
	mWearable(wearable),
	mTodo(todo),
	mCB(cb)
{
}

void LLAgentWearables::addWearableToAgentInventoryCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
		return;

	gAgentWearables.addWearabletoAgentInventoryDone(mType, mIndex, inv_item, mWearable);

	if (mTodo & CALL_UPDATE)
	{
		gAgentWearables.sendAgentWearablesUpdate();
	}
	if (mTodo & CALL_RECOVERDONE)
	{
		gAgentWearables.recoverMissingWearableDone();
	}
	/*
	 * Do this for every one in the loop
	 */
	if (mTodo & CALL_CREATESTANDARDDONE)
	{
		gAgentWearables.createStandardWearablesDone(mType, mIndex);
	}
	if (mTodo & CALL_MAKENEWOUTFITDONE)
	{
		gAgentWearables.makeNewOutfitDone(mType, mIndex);
	}
}

void LLAgentWearables::addWearabletoAgentInventoryDone(const S32 type,
													   const U32 index,
													   const LLUUID& item_id,
													   LLWearable* wearable)
{
	if (item_id.isNull())
		return;

	LLUUID old_item_id = getWearableItemID((EWearableType)type,index);
	if (wearable)
	{
		wearable->setItemID(item_id);
	}
	setWearable((EWearableType)type,index,wearable);

	if (old_item_id.notNull())
		gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
	gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);
	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if (item && wearable)
	{
		// We're changing the asset id, so we both need to set it
		// locally via setAssetUUID() and via setTransactionID() which
		// will be decoded on the server. JC
		item->setAssetUUID(wearable->getAssetID());
		item->setTransactionID(wearable->getTransactionID());
		gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		item->updateServer(FALSE);
	}
	gInventory.notifyObservers();
}

void LLAgentWearables::sendAgentWearablesUpdate()
{
	// MULTI-WEARABLE: call i "type" or something.
	// First make sure that we have inventory items for each wearable
	for (S32 i=0; i < WT_COUNT; ++i)
	{
		for (U32 j=0; j < getWearableCount((EWearableType)i); j++)
		{
			LLWearable* wearable = getWearable((EWearableType)i,j);
			if (wearable)
			{
				if (wearable->getItemID().isNull())
				{
					LLPointer<LLInventoryCallback> cb =
						new addWearableToAgentInventoryCallback(
							LLPointer<LLRefCount>(NULL),
							i,
							j,
							wearable,
							addWearableToAgentInventoryCallback::CALL_NONE);
					addWearableToAgentInventory(cb, wearable);
				}
				else
				{
					gInventory.addChangedMask(LLInventoryObserver::LABEL,
											  wearable->getItemID());
				}
			}
		}
	}

	// Then make sure the inventory is in sync with the avatar.
	gInventory.notifyObservers();

	// Send the AgentIsNowWearing 
	gMessageSystem->newMessageFast(_PREHASH_AgentIsNowWearing);

	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	lldebugs << "sendAgentWearablesUpdate()" << llendl;
	// MULTI-WEARABLE: update for multi-wearables after server-side support is in.
	for (S32 i=0; i < WT_COUNT; ++i)
	{
		gMessageSystem->nextBlockFast(_PREHASH_WearableData);

		U8 type_u8 = (U8)i;
		gMessageSystem->addU8Fast(_PREHASH_WearableType, type_u8);

		// MULTI-WEARABLE: TODO: hacked index to 0, needs to loop over all once messages support this.
		LLWearable* wearable = getWearable((EWearableType)i, 0);
		if (wearable)
		{
			//llinfos << "Sending wearable " << wearable->getName() << llendl;
			gMessageSystem->addUUIDFast(_PREHASH_ItemID, wearable->getItemID());
		}
		else
		{
			//llinfos << "Not wearing wearable type " << LLWearableDictionary::getInstance()->getWearable((EWearableType)i) << llendl;
			gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID::null);
		}

		lldebugs << "       " << LLWearableDictionary::getTypeLabel((EWearableType)i) << ": " << (wearable ? wearable->getAssetID() : LLUUID::null) << llendl;
	}
	gAgent.sendReliableMessage();
}

// MULTI-WEARABLE: add index.
void LLAgentWearables::saveWearable(const EWearableType type, const U32 index, BOOL send_update)
{
	LLWearable* old_wearable = getWearable(type, index);
	if (old_wearable && (old_wearable->isDirty() || old_wearable->isOldVersion()))
	{
		LLUUID old_item_id = old_wearable->getItemID();
		LLWearable* new_wearable = LLWearableList::instance().createCopyFromAvatar(old_wearable);
		new_wearable->setItemID(old_item_id); // should this be in LLWearable::copyDataFrom()?
		setWearable(type,index,new_wearable);

		LLInventoryItem* item = gInventory.getItem(old_item_id);
		if (item)
		{
			// Update existing inventory item
			LLPointer<LLViewerInventoryItem> template_item =
				new LLViewerInventoryItem(item->getUUID(),
										  item->getParentUUID(),
										  item->getPermissions(),
										  new_wearable->getAssetID(),
										  new_wearable->getAssetType(),
										  item->getInventoryType(),
										  item->getName(),
										  item->getDescription(),
										  item->getSaleInfo(),
										  item->getFlags(),
										  item->getCreationDate());
			template_item->setTransactionID(new_wearable->getTransactionID());
			template_item->updateServer(FALSE);
			gInventory.updateItem(template_item);
		}
		else
		{
			// Add a new inventory item (shouldn't ever happen here)
			U32 todo = addWearableToAgentInventoryCallback::CALL_NONE;
			if (send_update)
			{
				todo |= addWearableToAgentInventoryCallback::CALL_UPDATE;
			}
			LLPointer<LLInventoryCallback> cb =
				new addWearableToAgentInventoryCallback(
					LLPointer<LLRefCount>(NULL),
					(S32)type,
					index,
					new_wearable,
					todo);
			addWearableToAgentInventory(cb, new_wearable);
			return;
		}

		gAgent.getAvatarObject()->wearableUpdated( type );

		if (send_update)
		{
			sendAgentWearablesUpdate();
		}
	}
}

// MULTI-WEARABLE: add index
void LLAgentWearables::saveWearableAs(const EWearableType type,
									  const U32 index,
									  const std::string& new_name,
									  BOOL save_in_lost_and_found)
{
	if (!isWearableCopyable(type, index))
	{
		llwarns << "LLAgent::saveWearableAs() not copyable." << llendl;
		return;
	}
	LLWearable* old_wearable = getWearable(type, index);
	if (!old_wearable)
	{
		llwarns << "LLAgent::saveWearableAs() no old wearable." << llendl;
		return;
	}

	LLInventoryItem* item = gInventory.getItem(getWearableItemID(type,index));
	if (!item)
	{
		llwarns << "LLAgent::saveWearableAs() no inventory item." << llendl;
		return;
	}
	std::string trunc_name(new_name);
	LLStringUtil::truncate(trunc_name, DB_INV_ITEM_NAME_STR_LEN);
	LLWearable* new_wearable = LLWearableList::instance().createCopyFromAvatar(
		old_wearable,
		trunc_name);
	LLPointer<LLInventoryCallback> cb =
		new addWearableToAgentInventoryCallback(
			LLPointer<LLRefCount>(NULL),
			type,
			index,
			new_wearable,
			addWearableToAgentInventoryCallback::CALL_UPDATE);
	LLUUID category_id;
	if (save_in_lost_and_found)
	{
		category_id = gInventory.findCategoryUUIDForType(
			LLAssetType::AT_LOST_AND_FOUND);
	}
	else
	{
		// put in same folder as original
		category_id = item->getParentUUID();
	}

	copy_inventory_item(
		gAgent.getID(),
		item->getPermissions().getOwner(),
		item->getUUID(),
		category_id,
		new_name,
		cb);
}

void LLAgentWearables::revertWearable(const EWearableType type, const U32 index)
{
	LLWearable* wearable = getWearable(type, index);
	if (wearable)
	{
		wearable->writeToAvatar(TRUE);
	}
	gAgent.sendAgentSetAppearance();
}

void LLAgentWearables::saveAllWearables()
{
	//if (!gInventory.isLoaded())
	//{
	//	return;
	//}

	for (S32 i=0; i < WT_COUNT; i++)
	{
		for (U32 j=0; j < getWearableCount((EWearableType)i); j++)
			saveWearable((EWearableType)i, j, FALSE);
	}
	sendAgentWearablesUpdate();
}

// Called when the user changes the name of a wearable inventory item that is currently being worn.
void LLAgentWearables::setWearableName(const LLUUID& item_id, const std::string& new_name)
{
	for (S32 i=0; i < WT_COUNT; i++)
	{
		for (U32 j=0; j < getWearableCount((EWearableType)i); j++)
		{
			LLUUID curr_item_id = getWearableItemID((EWearableType)i,j);
			if (curr_item_id == item_id)
			{
				LLWearable* old_wearable = getWearable((EWearableType)i,j);
				llassert(old_wearable);

				std::string old_name = old_wearable->getName();
				old_wearable->setName(new_name);
				LLWearable* new_wearable = LLWearableList::instance().createCopy(old_wearable);
				new_wearable->setItemID(item_id);
				LLInventoryItem* item = gInventory.getItem(item_id);
				if (item)
				{
					new_wearable->setPermissions(item->getPermissions());
				}
				old_wearable->setName(old_name);

				setWearable((EWearableType)i,j,new_wearable);
				sendAgentWearablesUpdate();
				break;
			}
		}
	}
}


BOOL LLAgentWearables::isWearableModifiable(EWearableType type, U32 index) const
{
	LLUUID item_id = getWearableItemID(type, index);
	if (!item_id.isNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if (item && item->getPermissions().allowModifyBy(gAgent.getID(),
														 gAgent.getGroupID()))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL LLAgentWearables::isWearableCopyable(EWearableType type, U32 index) const
{
	LLUUID item_id = getWearableItemID(type, index);
	if (!item_id.isNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if (item && item->getPermissions().allowCopyBy(gAgent.getID(),
													   gAgent.getGroupID()))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/*
  U32 LLAgentWearables::getWearablePermMask(EWearableType type)
  {
  LLUUID item_id = getWearableItemID(type);
  if (!item_id.isNull())
  {
  LLInventoryItem* item = gInventory.getItem(item_id);
  if (item)
  {
  return item->getPermissions().getMaskOwner();
  }
  }
  return PERM_NONE;
  }
*/

LLInventoryItem* LLAgentWearables::getWearableInventoryItem(EWearableType type, U32 index)
{
	LLUUID item_id = getWearableItemID(type,index);
	LLInventoryItem* item = NULL;
	if (item_id.notNull())
	{
		item = gInventory.getItem(item_id);
	}
	return item;
}

const LLWearable* LLAgentWearables::getWearableFromWearableItem(const LLUUID& item_id) const
{
	for (S32 i=0; i < WT_COUNT; i++)
	{
		for (U32 j=0; j < getWearableCount((EWearableType)i); j++)
		{
			LLUUID curr_item_id = getWearableItemID((EWearableType)i, j);
			if (curr_item_id == item_id)
			{
				return getWearable((EWearableType)i, j);
			}
		}
	}
	return NULL;
}

void LLAgentWearables::sendAgentWearablesRequest()
{
	gMessageSystem->newMessageFast(_PREHASH_AgentWearablesRequest);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gAgent.sendReliableMessage();
}

// MULTI-WEARABLE: update for multiple items per type.
// Used to enable/disable menu items.
// static
BOOL LLAgentWearables::selfHasWearable(EWearableType type)
{
	// MULTI-WEARABLE: TODO could be getWearableCount > 0, once null entries have been eliminated.
	return gAgentWearables.getWearable(type,0) != NULL;
}

LLWearable* LLAgentWearables::getWearable(const EWearableType type, U32 index)
{
	wearableentry_map_t::iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		return NULL;
	}
	wearableentry_vec_t& wearable_vec = wearable_iter->second;
	if (index>=wearable_vec.size())
	{
		return NULL;
	}
	else
	{
		return wearable_vec[index];
	}
}

void LLAgentWearables::setWearable(const EWearableType type, U32 index, LLWearable *wearable)
{
	wearableentry_map_t::iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		llwarns << "invalid type, type " << type << " index " << index << llendl; 
		return;
	}
	wearableentry_vec_t& wearable_vec = wearable_iter->second;
	if (index>=wearable_vec.size())
	{
		llwarns << "invalid index, type " << type << " index " << index << llendl; 
	}
	else
	{
		wearable_vec[index] = wearable;
	}
}

const LLWearable* LLAgentWearables::getWearable(const EWearableType type, U32 index) const
{
	wearableentry_map_t::const_iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		return NULL;
	}
	const wearableentry_vec_t& wearable_vec = wearable_iter->second;
	if (index>=wearable_vec.size())
	{
		return NULL;
	}
	else
	{
		return wearable_vec[index];
	}
}

//MULTI-WEARABLE: this will give wrong values until we get rid of the "always one empty object" scheme.
U32 LLAgentWearables::getWearableCount(const EWearableType type) const
{
	wearableentry_map_t::const_iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		return 0;
	}
	const wearableentry_vec_t& wearable_vec = wearable_iter->second;
	return wearable_vec.size();
}

BOOL LLAgentWearables::itemUpdatePending(const LLUUID& item_id) const
{
	return mItemsAwaitingWearableUpdate.find(item_id) != mItemsAwaitingWearableUpdate.end();
}

U32 LLAgentWearables::itemUpdatePendingCount() const
{
	return mItemsAwaitingWearableUpdate.size();
}

const LLUUID LLAgentWearables::getWearableItemID(EWearableType type, U32 index) const
{
	const LLWearable *wearable = getWearable(type,index);
	if (wearable)
		return wearable->getItemID();
	else
		return LLUUID();
}

// Warning: include_linked_items = TRUE makes this operation expensive.
BOOL LLAgentWearables::isWearingItem(const LLUUID& item_id, BOOL include_linked_items) const
{
	if (getWearableFromWearableItem(item_id) != NULL) return TRUE;
	if (include_linked_items)
	{
		LLInventoryModel::item_array_t item_array;
		gInventory.collectLinkedItems(item_id, item_array);
		for (LLInventoryModel::item_array_t::iterator iter = item_array.begin();
			 iter != item_array.end();
			 iter++)
		{
			LLViewerInventoryItem *linked_item = (*iter);
			const LLUUID &item_id = linked_item->getUUID();
			if (getWearableFromWearableItem(item_id) != NULL) return TRUE;
		}
	}
	return FALSE;
}

// MULTI-WEARABLE: update for multiple
// static
void LLAgentWearables::processAgentInitialWearablesUpdate(LLMessageSystem* mesgsys, void** user_data)
{
	// We should only receive this message a single time.  Ignore subsequent AgentWearablesUpdates
	// that may result from AgentWearablesRequest having been sent more than once.
	if (mInitialWearablesUpdateReceived)
		return;
	mInitialWearablesUpdateReceived = true;

	LLUUID agent_id;
	gMessageSystem->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (avatar && (agent_id == avatar->getID()))
	{
		gMessageSystem->getU32Fast(_PREHASH_AgentData, _PREHASH_SerialNum, gAgentQueryManager.mUpdateSerialNum);
		
		S32 num_wearables = gMessageSystem->getNumberOfBlocksFast(_PREHASH_WearableData);
		if (num_wearables < 4)
		{
			// Transitional state.  Avatars should always have at least their body parts (hair, eyes, shape and skin).
			// The fact that they don't have any here (only a dummy is sent) implies that this account existed
			// before we had wearables, or that the database has gotten messed up.
			return;
		}

		// Get the UUID of the current outfit folder (will be created if it doesn't exist)
		LLUUID current_outfit_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_CURRENT_OUTFIT);
		
		LLOutfitFolderFetch* outfit = new LLOutfitFolderFetch();
		
		//lldebugs << "processAgentInitialWearablesUpdate()" << llendl;
		// Add wearables
		// MULTI-WEARABLE: TODO: update once messages change.  Currently use results to populate the zeroth element.
		gAgentWearables.mItemsAwaitingWearableUpdate.clear();
		for (S32 i=0; i < num_wearables; i++)
		{
			// Parse initial werables data from message system
			U8 type_u8 = 0;
			gMessageSystem->getU8Fast(_PREHASH_WearableData, _PREHASH_WearableType, type_u8, i);
			if (type_u8 >= WT_COUNT)
			{
				continue;
			}
			const EWearableType type = (EWearableType) type_u8;
			
			LLUUID item_id;
			gMessageSystem->getUUIDFast(_PREHASH_WearableData, _PREHASH_ItemID, item_id, i);
			
			LLUUID asset_id;
			gMessageSystem->getUUIDFast(_PREHASH_WearableData, _PREHASH_AssetID, asset_id, i);
			if (asset_id.isNull())
			{
				LLWearable::removeFromAvatar(type, FALSE);
			}
			else
			{
				LLAssetType::EType asset_type = LLWearableDictionary::getAssetType(type);
				if (asset_type == LLAssetType::AT_NONE)
				{
					continue;
				}
				
				// MULTI-WEARABLE: TODO: update once messages change.  Currently use results to populate the zeroth element.
				
				// Store initial wearables data until we know whether we have the current outfit folder or need to use the data.
				InitialWearableData * temp_wearable_data = new InitialWearableData(type, 0, item_id, asset_id); // MULTI-WEARABLE: update
				outfit->mAgentInitialWearables.push_back(temp_wearable_data);
				
			}
			
			lldebugs << "       " << LLWearableDictionary::getTypeLabel(type) << llendl;
		}
		
		// What we do here is get the complete information on the items in
		// the inventory, and set up an observer that will wait for that to
		// happen.
		LLInventoryFetchDescendentsObserver::folder_ref_t folders;
		folders.push_back(current_outfit_id);
		outfit->fetchDescendents(folders);
		if(outfit->isEverythingComplete())
		{
			// everything is already here - call done.
			outfit->done();
		}
		else
		{
			// it's all on it's way - add an observer, and the inventory
			// will call done for us when everything is here.
			gInventory.addObserver(outfit);
		}
	}
}

// static 
void LLAgentWearables::fetchInitialWearables(initial_wearable_data_vec_t & current_outfit_links, initial_wearable_data_vec_t & message_wearables)
{
#ifdef USE_CURRENT_OUTFIT_FOLDER
	if (!current_outfit_links.empty())
	{
		for (U8 i = 0; i < current_outfit_links.size(); ++i)
		{
			// Fetch the wearables in the current outfit folder
			LLWearableList::instance().getAsset(current_outfit_links[i]->mAssetID,
												LLStringUtil::null,
												LLWearableDictionary::getAssetType(current_outfit_links[i]->mType),
												onInitialWearableAssetArrived, (void*)(current_outfit_links[i]));			
		}
	}
	else 
#endif
	if (!message_wearables.empty()) // We have an empty current outfit folder, use the message data instead.
	{
		LLUUID current_outfit_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_CURRENT_OUTFIT);
		for (U8 i = 0; i < message_wearables.size(); ++i)
		{
			// Populate the current outfit folder with links to the wearables passed in the message
#ifdef USE_CURRENT_OUTFIT_FOLDER
			std::string link_name = "WearableLink";
			link_inventory_item(gAgent.getID(), message_wearables[i]->mItemID, current_outfit_id, link_name,
								LLAssetType::AT_LINK, LLPointer<LLInventoryCallback>(NULL));
#endif
			// Fetch the wearables
			LLWearableList::instance().getAsset(message_wearables[i]->mAssetID,
												LLStringUtil::null,
												LLWearableDictionary::getAssetType(message_wearables[i]->mType),
												onInitialWearableAssetArrived, (void*)(message_wearables[i]));
		}
	}
	else
	{
		LL_WARNS("Wearables") << "No current outfit folder iterms found and no initial wearables fallback message received." << LL_ENDL;
	}
}

// A single wearable that the avatar was wearing on start-up has arrived from the database.
// static
void LLAgentWearables::onInitialWearableAssetArrived(LLWearable* wearable, void* userdata)
{
	boost::scoped_ptr<InitialWearableData> wear_data((InitialWearableData*)userdata); 
	const EWearableType type = wear_data->mType;
	const U32 index = wear_data->mIndex;

	LLVOAvatarSelf* avatar = gAgent.getAvatarObject();
	if (!avatar)
	{
		return;
	}

	if (wearable)
	{
		llassert(type == wearable->getType());
		// MULTI-WEARABLE: is this always zeroth element?  Change sometime.
		wearable->setItemID(wear_data->mItemID);
		gAgentWearables.setWearable(type, index, wearable);
		gAgentWearables.mItemsAwaitingWearableUpdate.erase(wear_data->mItemID);

		// disable composites if initial textures are baked
		avatar->setupComposites();

		wearable->writeToAvatar(FALSE);
		avatar->setCompositeUpdatesEnabled(TRUE);
		gInventory.addChangedMask(LLInventoryObserver::LABEL, wearable->getItemID());
	}
	else
	{
		// Somehow the asset doesn't exist in the database.
		// MULTI-WEARABLE: assuming zeroth elt
		gAgentWearables.recoverMissingWearable(type,index);
	}

	gInventory.notifyObservers();

	// Have all the wearables that the avatar was wearing at log-in arrived?
	// MULTI-WEARABLE: update when multiple wearables can arrive per type.

	gAgentWearables.updateWearablesLoaded();
	if (gAgentWearables.areWearablesLoaded())
	{

		// Can't query cache until all wearables have arrived, so calling this earlier is a no-op.
		gAgentWearables.queryWearableCache();

		// Make sure that the server's idea of the avatar's wearables actually match the wearables.
		gAgent.sendAgentSetAppearance();

		// Check to see if there are any baked textures that we hadn't uploaded before we logged off last time.
		// If there are any, schedule them to be uploaded as soon as the layer textures they depend on arrive.
		if (gAgent.cameraCustomizeAvatar())
		{
			avatar->requestLayerSetUploads();
		}
	}
}

// Normally, all wearables referred to "AgentWearablesUpdate" will correspond to actual assets in the
// database.  If for some reason, we can't load one of those assets, we can try to reconstruct it so that
// the user isn't left without a shape, for example.  (We can do that only after the inventory has loaded.)
void LLAgentWearables::recoverMissingWearable(const EWearableType type, U32 index)
{
	// Try to recover by replacing missing wearable with a new one.
	LLNotifications::instance().add("ReplacedMissingWearable");
	lldebugs << "Wearable " << LLWearableDictionary::getTypeLabel(type) << " could not be downloaded.  Replaced inventory item with default wearable." << llendl;
	LLWearable* new_wearable = LLWearableList::instance().createNewWearable(type);

	S32 type_s32 = (S32) type;
	setWearable(type,index,new_wearable);
	new_wearable->writeToAvatar(TRUE);

	// Add a new one in the lost and found folder.
	// (We used to overwrite the "not found" one, but that could potentially
	// destory content.) JC
	LLUUID lost_and_found_id = 
		gInventory.findCategoryUUIDForType(LLAssetType::AT_LOST_AND_FOUND);
	LLPointer<LLInventoryCallback> cb =
		new addWearableToAgentInventoryCallback(
			LLPointer<LLRefCount>(NULL),
			type_s32,
			index,
			new_wearable,
			addWearableToAgentInventoryCallback::CALL_RECOVERDONE);
	addWearableToAgentInventory(cb, new_wearable, lost_and_found_id, TRUE);
}

void LLAgentWearables::recoverMissingWearableDone()
{
	// Have all the wearables that the avatar was wearing at log-in arrived or been fabricated?
	updateWearablesLoaded();
	if (areWearablesLoaded())
	{
		// Make sure that the server's idea of the avatar's wearables actually match the wearables.
		gAgent.sendAgentSetAppearance();
	}
	else
	{
		gInventory.addChangedMask(LLInventoryObserver::LABEL, LLUUID::null);
		gInventory.notifyObservers();
	}
}

void LLAgentWearables::addLocalTextureObject(const EWearableType wearable_type, const LLVOAvatarDefines::ETextureIndex texture_type, U32 wearable_index)
{
	LLWearable* wearable = getWearable((EWearableType)wearable_type, wearable_index);
	if (!wearable)
	{
		llerrs << "Tried to add local texture object to invalid wearable with type " << wearable_type << " and index " << wearable_index << llendl;
	}
	
	wearable->setLocalTextureObject(texture_type, new LLLocalTextureObject());
}

void LLAgentWearables::createStandardWearables(BOOL female)
{
	llwarns << "Creating Standard " << (female ? "female" : "male")
			<< " Wearables" << llendl;

	if (mAvatarObject.isNull())
	{
		return;
	}

	mAvatarObject->setSex(female ? SEX_FEMALE : SEX_MALE);

	const BOOL create[WT_COUNT] = 
		{
			TRUE,  //WT_SHAPE
			TRUE,  //WT_SKIN
			TRUE,  //WT_HAIR
			TRUE,  //WT_EYES
			TRUE,  //WT_SHIRT
			TRUE,  //WT_PANTS
			TRUE,  //WT_SHOES
			TRUE,  //WT_SOCKS
			FALSE, //WT_JACKET
			FALSE, //WT_GLOVES
			TRUE,  //WT_UNDERSHIRT
			TRUE,  //WT_UNDERPANTS
			FALSE  //WT_SKIRT
		};

	for (S32 i=0; i < WT_COUNT; i++)
	{
		bool once = false;
		LLPointer<LLRefCount> donecb = NULL;
		if (create[i])
		{
			if (!once)
			{
				once = true;
				donecb = new createStandardWearablesAllDoneCallback;
			}
			// MULTI_WEARABLE: only elt 0, may be the right thing?
			llassert(getWearable((EWearableType)i,0) == NULL);
			LLWearable* wearable = LLWearableList::instance().createNewWearable((EWearableType)i);
			setWearable((EWearableType)i,0,wearable);
			// no need to update here...
			// MULTI_WEARABLE: hardwired index = 0 here.
			LLPointer<LLInventoryCallback> cb =
				new addWearableToAgentInventoryCallback(
					donecb,
					i,
					0,
					wearable,
					addWearableToAgentInventoryCallback::CALL_CREATESTANDARDDONE);
			addWearableToAgentInventory(cb, wearable, LLUUID::null, FALSE);
		}
	}
}

void LLAgentWearables::createStandardWearablesDone(S32 type, U32 index)
{
	LLWearable* wearable = getWearable((EWearableType)type, index);

	if (wearable)
	{
		wearable->writeToAvatar(TRUE);
	}
}

void LLAgentWearables::createStandardWearablesAllDone()
{
	// ... because sendAgentWearablesUpdate will notify inventory
	// observers.
	mWearablesLoaded = TRUE; 
	checkWearablesLoaded();
	
	updateServer();

	// Treat this as the first texture entry message, if none received yet
	mAvatarObject->onFirstTEMessageReceived();
}

// Note:	wearables_to_include should be a list of EWearableType types
//			attachments_to_include should be a list of attachment points
void LLAgentWearables::makeNewOutfit(const std::string& new_folder_name,
									 const LLDynamicArray<S32>& wearables_to_include,
									 const LLDynamicArray<S32>& attachments_to_include,
									 BOOL rename_clothing)
{
	if (mAvatarObject.isNull())
	{
		return;
	}

	// First, make a folder in the Clothes directory.
	LLUUID folder_id = gInventory.createNewCategory(
		gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING),
		LLAssetType::AT_NONE,
		new_folder_name);

	bool found_first_item = false;

	///////////////////
	// Wearables

	if (wearables_to_include.count())
	{
		// Then, iterate though each of the wearables and save copies of them in the folder.
		S32 i;
		S32 count = wearables_to_include.count();
		LLDynamicArray<LLUUID> delete_items;
		LLPointer<LLRefCount> cbdone = NULL;
		for (i = 0; i < count; ++i)
		{
			const S32 type = wearables_to_include[i];
			for (U32 j=0; j<getWearableCount((EWearableType)i); j++)
			{
				LLWearable* old_wearable = getWearable((EWearableType)type, j);
				if (old_wearable)
				{
					std::string new_name;
					LLWearable* new_wearable;
					new_wearable = LLWearableList::instance().createCopy(old_wearable);
					if (rename_clothing)
					{
						new_name = new_folder_name;
						new_name.append(" ");
						new_name.append(old_wearable->getTypeLabel());
						LLStringUtil::truncate(new_name, DB_INV_ITEM_NAME_STR_LEN);
						new_wearable->setName(new_name);
					}

					LLViewerInventoryItem* item = gInventory.getItem(getWearableItemID((EWearableType)type,j));
					S32 todo = addWearableToAgentInventoryCallback::CALL_NONE;
					if (!found_first_item)
					{
						found_first_item = true;
						/* set the focus to the first item */
						todo |= addWearableToAgentInventoryCallback::CALL_MAKENEWOUTFITDONE;
						/* send the agent wearables update when done */
						cbdone = new sendAgentWearablesUpdateCallback;
					}
					LLPointer<LLInventoryCallback> cb =
						new addWearableToAgentInventoryCallback(
							cbdone,
							type,
							j,
							new_wearable,
							todo);
					if (isWearableCopyable((EWearableType)type, j))
					{
						copy_inventory_item(
							gAgent.getID(),
							item->getPermissions().getOwner(),
							item->getUUID(),
							folder_id,
							new_name,
							cb);
					}
					else
					{
						move_inventory_item(
							gAgent.getID(),
							gAgent.getSessionID(),
							item->getUUID(),
							folder_id,
							new_name,
							cb);
					}
				}
			}
		}
		gInventory.notifyObservers();
	}


	///////////////////
	// Attachments

	if (attachments_to_include.count())
	{
		BOOL msg_started = FALSE;
		LLMessageSystem* msg = gMessageSystem;
		for (S32 i = 0; i < attachments_to_include.count(); i++)
		{
			S32 attachment_pt = attachments_to_include[i];
			LLViewerJointAttachment* attachment = get_if_there(mAvatarObject->mAttachmentPoints, attachment_pt, (LLViewerJointAttachment*)NULL);
			if (!attachment) continue;
			LLViewerObject* attached_object = attachment->getObject();
			if (!attached_object) continue;
			const LLUUID& item_id = attachment->getItemID();
			if (item_id.isNull()) continue;
			LLInventoryItem* item = gInventory.getItem(item_id);
			if (!item) continue;
			if (!msg_started)
			{
				msg_started = TRUE;
				msg->newMessage("CreateNewOutfitAttachments");
				msg->nextBlock("AgentData");
				msg->addUUID("AgentID", gAgent.getID());
				msg->addUUID("SessionID", gAgent.getSessionID());
				msg->nextBlock("HeaderData");
				msg->addUUID("NewFolderID", folder_id);
			}
			msg->nextBlock("ObjectData");
			msg->addUUID("OldItemID", item_id);
			msg->addUUID("OldFolderID", item->getParentUUID());
		}

		if (msg_started)
		{
			gAgent.sendReliableMessage();
		}

	} 
}

// Note:	wearables_to_include should be a list of EWearableType types
//			attachments_to_include should be a list of attachment points
void LLAgentWearables::makeNewOutfitLinks(const std::string& new_folder_name,
										  const LLDynamicArray<S32>& wearables_to_include,
										  const LLDynamicArray<S32>& attachments_to_include,
										  BOOL rename_clothing)
{
	if (mAvatarObject.isNull())
	{
		return;
	}

	// First, make a folder in the Clothes directory.
	LLUUID folder_id = gInventory.createNewCategory(
		gInventory.findCategoryUUIDForType(LLAssetType::AT_MY_OUTFITS),
		LLAssetType::AT_OUTFIT,
		new_folder_name);

//	bool found_first_item = false;

	///////////////////
	// Wearables

	if (wearables_to_include.count())
	{
		// Then, iterate though each of the wearables and save links to them in the folder.
		S32 i;
		S32 count = wearables_to_include.count();
		LLDynamicArray<LLUUID> delete_items;
		LLPointer<LLRefCount> cbdone = NULL;
		for (i = 0; i < count; ++i)
		{
			const S32 type = wearables_to_include[i];
			for (U32 j=0; j<getWearableCount((EWearableType)i); j++)
			{
				LLWearable* old_wearable = getWearable((EWearableType)type,j);
				if (old_wearable)
				{
					std::string new_name;
					if (rename_clothing)
					{
						new_name = new_folder_name;
						new_name.append(" ");
						new_name.append(old_wearable->getTypeLabel());
						LLStringUtil::truncate(new_name, DB_INV_ITEM_NAME_STR_LEN);
					}

					LLViewerInventoryItem* item = gInventory.getItem(getWearableItemID((EWearableType) type, j));
					if (!item) continue;
					LLPointer<LLInventoryCallback> cb = NULL;
					link_inventory_item(gAgent.getID(),
										item->getUUID(),
										folder_id,
										new_name,
										LLAssetType::AT_LINK,
										cb);
				}
			}
		}
		gInventory.notifyObservers();
	}


	///////////////////
	// Attachments

	if (attachments_to_include.count())
	{
		for (S32 i = 0; i < attachments_to_include.count(); i++)
		{
			S32 attachment_pt = attachments_to_include[i];
			LLViewerJointAttachment* attachment = get_if_there(mAvatarObject->mAttachmentPoints, attachment_pt, (LLViewerJointAttachment*)NULL);
			if (!attachment) continue;
			LLViewerObject* attached_object = attachment->getObject();
			if (!attached_object) continue;
			const LLUUID& item_id = attachment->getItemID();
			if (item_id.isNull()) continue;
			LLInventoryItem* item = gInventory.getItem(item_id);
			if (!item) continue;

			LLPointer<LLInventoryCallback> cb = NULL;
			link_inventory_item(gAgent.getID(),
								item->getUUID(),
								folder_id,
								item->getName(),
								LLAssetType::AT_LINK,
								cb);
		}
	} 
}

void LLAgentWearables::makeNewOutfitDone(S32 type, U32 index)
{
	LLUUID first_item_id = getWearableItemID((EWearableType)type, index);
	// Open the inventory and select the first item we added.
	if (first_item_id.notNull())
	{
		LLFloaterInventory* view = LLFloaterInventory::getActiveInventory();
		if (view)
		{
			view->getPanel()->setSelection(first_item_id, TAKE_FOCUS_NO);
		}
	}
}


void LLAgentWearables::addWearableToAgentInventory(LLPointer<LLInventoryCallback> cb,
												   LLWearable* wearable,
												   const LLUUID& category_id,
												   BOOL notify)
{
	create_inventory_item(gAgent.getID(),
						  gAgent.getSessionID(),
						  category_id,
						  wearable->getTransactionID(),
						  wearable->getName(),
						  wearable->getDescription(),
						  wearable->getAssetType(),
						  LLInventoryType::IT_WEARABLE,
						  wearable->getType(),
						  wearable->getPermissions().getMaskNextOwner(),
						  cb);
}

void LLAgentWearables::removeWearable(const EWearableType type, bool do_remove_all, U32 index)
{

	if (do_remove_all)
	{
		removeWearableFinal(type, do_remove_all, index);
	}
	else
	{
// MULTI_WEARABLE: handle vector changes from arbitrary removal.
		LLWearable* old_wearable = getWearable(type,index);
		
		if ((gAgent.isTeen())
			&& (type == WT_UNDERSHIRT || type == WT_UNDERPANTS))
		{
			// Can't take off underclothing in simple UI mode or on PG accounts
			return;
		}
		
		if (old_wearable)
		{
			if (old_wearable->isDirty())
			{
				LLSD payload;
				payload["wearable_type"] = (S32)type;
				// Bring up view-modal dialog: Save changes? Yes, No, Cancel
				LLNotifications::instance().add("WearableSave", LLSD(), payload, &LLAgentWearables::onRemoveWearableDialog);
				return;
			}
			else
			{
				removeWearableFinal(type, do_remove_all, index);
			}
		}
	}
}


// MULTI_WEARABLE: assuming one wearable per type.
// MULTI_WEARABLE: hardwiring 0th elt for now - notification needs to change.
// static 
bool LLAgentWearables::onRemoveWearableDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	EWearableType type = (EWearableType)notification["payload"]["wearable_type"].asInteger();
	switch(option)
	{
		case 0:  // "Save"
			gAgentWearables.saveWearable(type, 0);
			gAgentWearables.removeWearableFinal(type, false, 0);
			break;

		case 1:  // "Don't Save"
			gAgentWearables.removeWearableFinal(type, false, 0);
			break;

		case 2: // "Cancel"
			break;

		default:
			llassert(0);
			break;
	}
	return false;
}

// Called by removeWearable() and onRemoveWearableDialog() to actually do the removal.
void LLAgentWearables::removeWearableFinal(const EWearableType type, bool do_remove_all, U32 index)
{
	//LLAgentDumper dumper("removeWearable");
	if (do_remove_all)
	{
		S32 max_entry = mWearableDatas[type].size()-1;
		for (S32 i=max_entry; i>=0; i--)
		{
			LLWearable* old_wearable = getWearable(type,i);
			gInventory.addChangedMask(LLInventoryObserver::LABEL, getWearableItemID(type,i));
			setWearable(type,i,NULL);

			//queryWearableCache(); // moved below
			// MULTI_WEARABLE: FIXME - currently we keep a null entry, so can't delete the last one.
			if (i>0)
			{
				mWearableDatas[type].pop_back();
			}
			if (old_wearable)
			{
				old_wearable->removeFromAvatar(TRUE);
			}
		}
	}
	else
	{
		LLWearable* old_wearable = getWearable(type, index);

		gInventory.addChangedMask(LLInventoryObserver::LABEL, getWearableItemID(type,index));
		setWearable(type,index,NULL);

		//queryWearableCache(); // moved below

		if (old_wearable)
		{
			old_wearable->removeFromAvatar(TRUE);
		}

		// MULTI_WEARABLE: logic changes if null entries go away
		if (getWearableCount(type)>1)
		{
			// Have to shrink the vector and clean up the item.
			wearableentry_map_t::iterator wearable_iter = mWearableDatas.find(type);
			llassert_always(wearable_iter != mWearableDatas.end());
			wearableentry_vec_t& wearable_vec = wearable_iter->second;
			wearable_vec.erase( wearable_vec.begin() + index );
		}
	}

	queryWearableCache();

	// Update the server
	updateServer();
	gInventory.notifyObservers();
}

// Assumes existing wearables are not dirty.
// MULTI_WEARABLE: assumes one wearable per type.
void LLAgentWearables::setWearableOutfit(const LLInventoryItem::item_array_t& items,
										 const LLDynamicArray< LLWearable* >& wearables,
										 BOOL remove)
{
	lldebugs << "setWearableOutfit() start" << llendl;

	BOOL wearables_to_remove[WT_COUNT];
	wearables_to_remove[WT_SHAPE]		= FALSE;
	wearables_to_remove[WT_SKIN]		= FALSE;
	wearables_to_remove[WT_HAIR]		= FALSE;
	wearables_to_remove[WT_EYES]		= FALSE;
	wearables_to_remove[WT_SHIRT]		= remove;
	wearables_to_remove[WT_PANTS]		= remove;
	wearables_to_remove[WT_SHOES]		= remove;
	wearables_to_remove[WT_SOCKS]		= remove;
	wearables_to_remove[WT_JACKET]		= remove;
	wearables_to_remove[WT_GLOVES]		= remove;
	wearables_to_remove[WT_UNDERSHIRT]	= (!gAgent.isTeen()) & remove;
	wearables_to_remove[WT_UNDERPANTS]	= (!gAgent.isTeen()) & remove;
	wearables_to_remove[WT_SKIRT]		= remove;

	S32 count = wearables.count();
	llassert(items.count() == count);

	S32 i;
	for (i = 0; i < count; i++)
	{
		LLWearable* new_wearable = wearables[i];
		LLPointer<LLInventoryItem> new_item = items[i];

		const EWearableType type = new_wearable->getType();
		wearables_to_remove[type] = FALSE;

		// MULTI_WEARABLE: using 0th
		LLWearable* old_wearable = getWearable(type, 0);
		if (old_wearable)
		{
			const LLUUID& old_item_id = getWearableItemID(type, 0);
			if ((old_wearable->getAssetID() == new_wearable->getAssetID()) &&
				(old_item_id == new_item->getUUID()))
			{
				lldebugs << "No change to wearable asset and item: " << LLWearableDictionary::getInstance()->getWearableEntry(type) << llendl;
				continue;
			}

			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);

			// Assumes existing wearables are not dirty.
			if (old_wearable->isDirty())
			{
				llassert(0);
				continue;
			}
		}

		setWearable(type,0,new_wearable);
		if (new_wearable)
			new_wearable->setItemID(new_item->getUUID());
	}

	std::vector<LLWearable*> wearables_being_removed;

	for (i = 0; i < WT_COUNT; i++)
	{
		if (wearables_to_remove[i])
		{
			// MULTI_WEARABLE: assuming 0th
			LLWearable* wearable = getWearable((EWearableType)i, 0);
			gInventory.addChangedMask(LLInventoryObserver::LABEL, getWearableItemID((EWearableType)i,0));
			if (wearable)
			{
				wearables_being_removed.push_back(wearable);
			}
			setWearable((EWearableType)i,0,NULL);
		}
	}

	gInventory.notifyObservers();

	queryWearableCache();

	std::vector<LLWearable*>::iterator wearable_iter;

	for (wearable_iter = wearables_being_removed.begin(); 
		 wearable_iter != wearables_being_removed.end();
		 ++wearable_iter)
	{
		LLWearable* wearablep = *wearable_iter;
		if (wearablep)
		{
			wearablep->removeFromAvatar(TRUE);
		}
	}

	for (i = 0; i < count; i++)
	{
		wearables[i]->writeToAvatar(TRUE);
	}

	// Start rendering & update the server
	mWearablesLoaded = TRUE; 
	checkWearablesLoaded();
	updateServer();

	lldebugs << "setWearableOutfit() end" << llendl;
}


// User has picked "wear on avatar" from a menu.
void LLAgentWearables::setWearableItem(LLInventoryItem* new_item, LLWearable* new_wearable, bool do_append)
{
	//LLAgentDumper dumper("setWearableItem");
	if (isWearingItem(new_item->getUUID()))
	{
		llwarns << "wearable " << new_item->getUUID() << " is already worn" << llendl;
		return;
	}
	
	const EWearableType type = new_wearable->getType();

	if (!do_append)
	{
		// Remove old wearable, if any
		// MULTI_WEARABLE: hardwired to 0
		LLWearable* old_wearable = getWearable(type,0);
		if (old_wearable)
		{
			const LLUUID& old_item_id = old_wearable->getItemID();
			if ((old_wearable->getAssetID() == new_wearable->getAssetID()) &&
				(old_item_id == new_item->getUUID()))
			{
				lldebugs << "No change to wearable asset and item: " << LLWearableDictionary::getInstance()->getWearableEntry(type) << llendl;
				return;
			}
			
			if (old_wearable->isDirty())
			{
				// Bring up modal dialog: Save changes? Yes, No, Cancel
				LLSD payload;
				payload["item_id"] = new_item->getUUID();
				LLNotifications::instance().add("WearableSave", LLSD(), payload, boost::bind(onSetWearableDialog, _1, _2, new_wearable));
				return;
			}
		}
	}

	setWearableFinal(new_item, new_wearable, do_append);
}

// static 
bool LLAgentWearables::onSetWearableDialog(const LLSD& notification, const LLSD& response, LLWearable* wearable)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLInventoryItem* new_item = gInventory.getItem(notification["payload"]["item_id"].asUUID());
	if (!new_item)
	{
		delete wearable;
		return false;
	}

	switch(option)
	{
		case 0:  // "Save"
// MULTI_WEARABLE: assuming 0th
			gAgentWearables.saveWearable(wearable->getType(),0);
			gAgentWearables.setWearableFinal(new_item, wearable);
			break;

		case 1:  // "Don't Save"
			gAgentWearables.setWearableFinal(new_item, wearable);
			break;

		case 2: // "Cancel"
			break;

		default:
			llassert(0);
			break;
	}

	delete wearable;
	return false;
}

// Called from setWearableItem() and onSetWearableDialog() to actually set the wearable.
// MULTI_WEARABLE: unify code after null objects are gone.
void LLAgentWearables::setWearableFinal(LLInventoryItem* new_item, LLWearable* new_wearable, bool do_append)
{
	const EWearableType type = new_wearable->getType();

	if (do_append && getWearableItemID(type,0).notNull())
	{
		new_wearable->setItemID(new_item->getUUID());
		mWearableDatas[type].push_back(new_wearable);
		llinfos << "Added additional wearable for type " << type
				<< " size is now " << mWearableDatas[type].size() << llendl;
	}
	else
	{
		// Replace the old wearable with a new one.
		llassert(new_item->getAssetUUID() == new_wearable->getAssetID());

		LLWearable *old_wearable = getWearable(type,0);
		LLUUID old_item_id;
		if (old_wearable)
		{
			old_item_id = old_wearable->getItemID();
		}
		new_wearable->setItemID(new_item->getUUID());
		setWearable(type,0,new_wearable);

		if (old_item_id.notNull())
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
			gInventory.notifyObservers();
		}
		llinfos << "Replaced current element 0 for type " << type
				<< " size is now " << mWearableDatas[type].size() << llendl;
	}

	//llinfos << "LLVOAvatar::setWearableItem()" << llendl;
	queryWearableCache();
	new_wearable->writeToAvatar(TRUE);

	updateServer();
}

void LLAgentWearables::queryWearableCache()
{
	if (!areWearablesLoaded())
	{
		return;
	}

	// Look up affected baked textures.
	// If they exist:
	//		disallow updates for affected layersets (until dataserver responds with cache request.)
	//		If cache miss, turn updates back on and invalidate composite.
	//		If cache hit, modify baked texture entries.
	//
	// Cache requests contain list of hashes for each baked texture entry.
	// Response is list of valid baked texture assets. (same message)

	gMessageSystem->newMessageFast(_PREHASH_AgentCachedTexture);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->addS32Fast(_PREHASH_SerialNum, gAgentQueryManager.mWearablesCacheQueryID);

	S32 num_queries = 0;
	for (U8 baked_index = 0; baked_index < BAKED_NUM_INDICES; baked_index++)
	{
		const LLVOAvatarDictionary::BakedEntry *baked_dict = LLVOAvatarDictionary::getInstance()->getBakedTexture((EBakedTextureIndex)baked_index);
		LLUUID hash;
		for (U8 i=0; i < baked_dict->mWearables.size(); i++)
		{
			// EWearableType baked_type = gBakedWearableMap[baked_index][baked_num];
			const EWearableType baked_type = baked_dict->mWearables[i];
			// MULTI_WEARABLE: assuming 0th
			const LLWearable* wearable = getWearable(baked_type,0);
			if (wearable)
			{
				hash ^= wearable->getAssetID();
			}
		}
		if (hash.notNull())
		{
			hash ^= baked_dict->mWearablesHashID;
			num_queries++;
			// *NOTE: make sure at least one request gets packed

			//llinfos << "Requesting texture for hash " << hash << " in baked texture slot " << baked_index << llendl;
			gMessageSystem->nextBlockFast(_PREHASH_WearableData);
			gMessageSystem->addUUIDFast(_PREHASH_ID, hash);
			gMessageSystem->addU8Fast(_PREHASH_TextureIndex, (U8)baked_index);
		}

		gAgentQueryManager.mActiveCacheQueries[baked_index] = gAgentQueryManager.mWearablesCacheQueryID;
	}

	llinfos << "Requesting texture cache entry for " << num_queries << " baked textures" << llendl;
	gMessageSystem->sendReliable(gAgent.getRegion()->getHost());
	gAgentQueryManager.mNumPendingQueries++;
	gAgentQueryManager.mWearablesCacheQueryID++;
}

// MULTI_WEARABLE: need a way to specify by wearable rather than by type.
// User has picked "remove from avatar" from a menu.
// static
void LLAgentWearables::userRemoveWearable(void* userdata)
{
	EWearableType type = (EWearableType)(intptr_t)userdata;
	
	if (!(type==WT_SHAPE || type==WT_SKIN || type==WT_HAIR)) //&&
		//!((!gAgent.isTeen()) && (type==WT_UNDERPANTS || type==WT_UNDERSHIRT)))
	{
		// MULTI_WEARABLE: fixed to 0th for now.
		gAgentWearables.removeWearable(type,false,0);
	}
}

// static
void LLAgentWearables::userRemoveAllClothes(void* userdata)
{
	// We have to do this up front to avoid having to deal with the case of multiple wearables being dirty.
	if (gFloaterCustomize)
	{
		gFloaterCustomize->askToSaveIfDirty(userRemoveAllClothesStep2);
	}
	else
	{
		userRemoveAllClothesStep2(TRUE);
	}
}

// static
// MULTI_WEARABLE: removing all here.
void LLAgentWearables::userRemoveAllClothesStep2(BOOL proceed)
{
	if (proceed)
	{
		gAgentWearables.removeWearable(WT_SHIRT,true,0);
		gAgentWearables.removeWearable(WT_PANTS,true,0);
		gAgentWearables.removeWearable(WT_SHOES,true,0);
		gAgentWearables.removeWearable(WT_SOCKS,true,0);
		gAgentWearables.removeWearable(WT_JACKET,true,0);
		gAgentWearables.removeWearable(WT_GLOVES,true,0);
		gAgentWearables.removeWearable(WT_UNDERSHIRT,true,0);
		gAgentWearables.removeWearable(WT_UNDERPANTS,true,0);
		gAgentWearables.removeWearable(WT_SKIRT,true,0);
	}
}

void LLAgentWearables::userRemoveAllAttachments(void* userdata)
{
	LLVOAvatar* avatarp = gAgent.getAvatarObject();
	if (!avatarp)
	{
		llwarns << "No avatar found." << llendl;
		return;
	}

	gMessageSystem->newMessage("ObjectDetach");
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	for (LLVOAvatar::attachment_map_t::iterator iter = avatarp->mAttachmentPoints.begin(); 
		 iter != avatarp->mAttachmentPoints.end();)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		LLViewerObject* objectp = attachment->getObject();
		if (objectp)
		{
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
		}
	}
	gMessageSystem->sendReliable(gAgent.getRegionHost());
}

void LLAgentWearables::checkWearablesLoaded() const
{
#ifdef SHOW_ASSERT
	U32 item_pend_count = itemUpdatePendingCount();
	if (mWearablesLoaded)
	{
		llassert(item_pend_count==0);
	}
#endif
}

BOOL LLAgentWearables::areWearablesLoaded() const
{
	checkWearablesLoaded();
	return mWearablesLoaded;
}

// MULTI-WEARABLE: update for multiple indices.
void LLAgentWearables::updateWearablesLoaded()
{
	mWearablesLoaded = (itemUpdatePendingCount()==0);
}

void LLAgentWearables::updateServer()
{
	sendAgentWearablesUpdate();
	gAgent.sendAgentSetAppearance();
}

void LLAgentWearables::LLOutfitFolderFetch::done()
{
	// What we do here is get the complete information on the items in
	// the library, and set up an observer that will wait for that to
	// happen.
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	gInventory.collectDescendents(mCompleteFolders.front(),
								  cat_array,
								  item_array,
								  LLInventoryModel::EXCLUDE_TRASH);
	S32 count = item_array.count();
	LLAgentWearables::initial_wearable_data_vec_t current_outfit_links;
	current_outfit_links.reserve(count);
	
	for(S32 i = 0; i < count; ++i)
	{
		// A bit of a hack since wearables database doesn't contain asset types...
		// Perform indirection in case this assetID is in fact a link.  This only works
		// because of the assumption that all assetIDs and itemIDs are unique (i.e. 
		// no assetID is also used as an itemID elsewhere); therefore if the assetID
		// exists as an itemID in the user's inventory, then this must be a link.
		const LLInventoryItem *linked_item = gInventory.getItem(item_array.get(i)->getUUID());
		LLAssetType::EType asset_type = (LLAssetType::EType) 0;
		if (linked_item)
		{
			asset_type = linked_item->getType();
			LLInventoryItem * base_item = gInventory.getItem(linked_item->getLinkedUUID());
			if (base_item)
			{
				EWearableType type = (EWearableType) (base_item->getFlags() & LLInventoryItem::II_FLAGS_WEARABLES_MASK);
				// MULTI-WEARABLE: update
				InitialWearableData * temp_wearable_data = new InitialWearableData(type, 0, linked_item->getLinkedUUID(), base_item->getAssetUUID());
				current_outfit_links.push_back(temp_wearable_data);
			}
			else
			{
				llwarns << "Null base_item in LLOutfitFolderFetch::done, linkedUUID is " << linked_item->getLinkedUUID().asString() << llendl;
			}
		}
		else
		{
			llwarns << "Null linked_item in LLOutfitFolderFetch::done, UUID is " << item_array.get(i)->getUUID().asString() << llendl;
		}
	}
	
	gInventory.removeObserver(this);
	LLAgentWearables::fetchInitialWearables(current_outfit_links, mAgentInitialWearables);
	mAgentInitialWearables.clear();
	delete this;
}

