/** 
 * @file llviewerinventory.cpp
 * @brief Implementation of the viewer side inventory objects.
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

#include "llviewerprecompiledheaders.h"
#include "llviewerinventory.h"

#include "message.h"
#include "indra_constants.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llconsole.h"
#include "llinventorymodel.h"
#include "llnotify.h"
#include "llimview.h"
#include "llgesturemgr.h"

#include "llinventorybridge.h"
#include "llfloaterinventory.h"

#include "llviewerregion.h"
#include "llviewerobjectlist.h"
#include "llpreviewgesture.h"
#include "llviewerwindow.h"
#include "lltrans.h"

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

///----------------------------------------------------------------------------
/// Class LLViewerInventoryItem
///----------------------------------------------------------------------------

LLViewerInventoryItem::LLViewerInventoryItem(const LLUUID& uuid,
											 const LLUUID& parent_uuid,
											 const LLPermissions& perm,
											 const LLUUID& asset_uuid,
											 LLAssetType::EType type,
											 LLInventoryType::EType inv_type,
											 const std::string& name,
											 const std::string& desc,
											 const LLSaleInfo& sale_info,
											 U32 flags,
											 time_t creation_date_utc) :
	LLInventoryItem(uuid, parent_uuid, perm, asset_uuid, type, inv_type,
					name, desc, sale_info, flags, creation_date_utc),
	mIsComplete(TRUE)
{
}

LLViewerInventoryItem::LLViewerInventoryItem(const LLUUID& item_id,
											 const LLUUID& parent_id,
											 const std::string& name,
											 LLInventoryType::EType inv_type) :
	LLInventoryItem(),
	mIsComplete(FALSE)
{
	mUUID = item_id;
	mParentUUID = parent_id;
	mInventoryType = inv_type;
	mName = name;
}

LLViewerInventoryItem::LLViewerInventoryItem() :
	LLInventoryItem(),
	mIsComplete(FALSE)
{
}

LLViewerInventoryItem::LLViewerInventoryItem(const LLViewerInventoryItem* other) :
	LLInventoryItem()
{
	copyViewerItem(other);
	if (!mIsComplete)
	{
		llwarns << "LLViewerInventoryItem copy constructor for incomplete item"
			<< mUUID << llendl;
	}
}

LLViewerInventoryItem::LLViewerInventoryItem(const LLInventoryItem *other) :
	LLInventoryItem(other),
	mIsComplete(TRUE)
{
}


LLViewerInventoryItem::~LLViewerInventoryItem()
{
}

void LLViewerInventoryItem::copyViewerItem(const LLViewerInventoryItem* other)
{
	LLInventoryItem::copyItem(other);
	mIsComplete = other->mIsComplete;
	mTransactionID = other->mTransactionID;
}

// virtual
void LLViewerInventoryItem::copyItem(const LLInventoryItem *other)
{
	LLInventoryItem::copyItem(other);
	mIsComplete = true;
	mTransactionID.setNull();
}

void LLViewerInventoryItem::cloneViewerItem(LLPointer<LLViewerInventoryItem>& newitem) const
{
	newitem = new LLViewerInventoryItem(this);
	if(newitem.notNull())
	{
		LLUUID item_id;
		item_id.generate();
		newitem->setUUID(item_id);
	}
}

void LLViewerInventoryItem::removeFromServer()
{
	llinfos << "Removing inventory item " << mUUID << " from server."
			<< llendl;

	LLInventoryModel::LLCategoryUpdate up(mParentUUID, -1);
	gInventory.accountForUpdate(up);

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_RemoveInventoryItem);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID()); 
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addUUIDFast(_PREHASH_ItemID, mUUID);
	gAgent.sendReliableMessage();
}

void LLViewerInventoryItem::updateServer(BOOL is_new) const
{
	if(!mIsComplete)
	{
		// *FIX: deal with this better.
		// If we're crashing here then the UI is incorrectly enabled.
		llerrs << "LLViewerInventoryItem::updateServer() - for incomplete item"
			   << llendl;
		return;
	}
	if(gAgent.getID() != mPermissions.getOwner())
	{
		// *FIX: deal with this better.
		llwarns << "LLViewerInventoryItem::updateServer() - for unowned item"
				<< llendl;
		return;
	}
	LLInventoryModel::LLCategoryUpdate up(mParentUUID, is_new ? 1 : 0);
	gInventory.accountForUpdate(up);

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_UpdateInventoryItem);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, mTransactionID);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addU32Fast(_PREHASH_CallbackID, 0);
	packMessage(msg);
	gAgent.sendReliableMessage();
}

void LLViewerInventoryItem::fetchFromServer(void) const
{
	if(!mIsComplete)
	{
		std::string url; 

		LLViewerRegion* region = gAgent.getRegion();
		// we have to check region. It can be null after region was destroyed. See EXT-245
		if (region)
		{
			if( ALEXANDRIA_LINDEN_ID.getString() == mPermissions.getOwner().getString())
				url = region->getCapability("FetchLib");
			else	
				url = region->getCapability("FetchInventory");
		}
		else
		{
			llwarns << "Agent Region is absent" << llendl;
		}

		if (!url.empty())
		{
			LLSD body;
			body["agent_id"]	= gAgent.getID();
			body["items"][0]["owner_id"]	= mPermissions.getOwner();
			body["items"][0]["item_id"]		= mUUID;

			LLHTTPClient::post(url, body, new LLInventoryModel::fetchInventoryResponder(body));
		}
		else
		{
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessage("FetchInventory");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgent.getID());
			msg->addUUID("SessionID", gAgent.getSessionID());
			msg->nextBlock("InventoryData");
			msg->addUUID("OwnerID", mPermissions.getOwner());
			msg->addUUID("ItemID", mUUID);
			gAgent.sendReliableMessage();
		}
	}
	else
	{
		// *FIX: this can be removed after a bit.
		llwarns << "request to fetch complete item" << llendl;
	}
}

// virtual
BOOL LLViewerInventoryItem::unpackMessage(LLSD item)
{
	BOOL rv = LLInventoryItem::fromLLSD(item);
	mIsComplete = TRUE;
	return rv;
}

// virtual
BOOL LLViewerInventoryItem::unpackMessage(LLMessageSystem* msg, const char* block, S32 block_num)
{
	BOOL rv = LLInventoryItem::unpackMessage(msg, block, block_num);
	mIsComplete = TRUE;
	return rv;
}

void LLViewerInventoryItem::setTransactionID(const LLTransactionID& transaction_id)
{
	mTransactionID = transaction_id;
}
// virtual
void LLViewerInventoryItem::packMessage(LLMessageSystem* msg) const
{
	msg->addUUIDFast(_PREHASH_ItemID, mUUID);
	msg->addUUIDFast(_PREHASH_FolderID, mParentUUID);
	mPermissions.packMessage(msg);
	msg->addUUIDFast(_PREHASH_TransactionID, mTransactionID);
	S8 type = static_cast<S8>(mType);
	msg->addS8Fast(_PREHASH_Type, type);
	type = static_cast<S8>(mInventoryType);
	msg->addS8Fast(_PREHASH_InvType, type);
	msg->addU32Fast(_PREHASH_Flags, mFlags);
	mSaleInfo.packMessage(msg);
	msg->addStringFast(_PREHASH_Name, mName);
	msg->addStringFast(_PREHASH_Description, mDescription);
	msg->addS32Fast(_PREHASH_CreationDate, mCreationDate);
	U32 crc = getCRC32();
	msg->addU32Fast(_PREHASH_CRC, crc);
}
// virtual
BOOL LLViewerInventoryItem::importFile(LLFILE* fp)
{
	BOOL rv = LLInventoryItem::importFile(fp);
	mIsComplete = TRUE;
	return rv;
}

// virtual
BOOL LLViewerInventoryItem::importLegacyStream(std::istream& input_stream)
{
	BOOL rv = LLInventoryItem::importLegacyStream(input_stream);
	mIsComplete = TRUE;
	return rv;
}

bool LLViewerInventoryItem::importFileLocal(LLFILE* fp)
{
	// TODO: convert all functions that return BOOL to return bool
	bool rv = (LLInventoryItem::importFile(fp) ? true : false);
	mIsComplete = false;
	return rv;
}

bool LLViewerInventoryItem::exportFileLocal(LLFILE* fp) const
{
	std::string uuid_str;
	fprintf(fp, "\tinv_item\t0\n\t{\n");
	mUUID.toString(uuid_str);
	fprintf(fp, "\t\titem_id\t%s\n", uuid_str.c_str());
	mParentUUID.toString(uuid_str);
	fprintf(fp, "\t\tparent_id\t%s\n", uuid_str.c_str());
	mPermissions.exportFile(fp);
	fprintf(fp, "\t\ttype\t%s\n", LLAssetType::lookup(mType));
	const std::string inv_type_str = LLInventoryType::lookup(mInventoryType);
	if(!inv_type_str.empty()) fprintf(fp, "\t\tinv_type\t%s\n", inv_type_str.c_str());
	fprintf(fp, "\t\tname\t%s|\n", mName.c_str());
	fprintf(fp, "\t\tcreation_date\t%d\n", (S32) mCreationDate);
	fprintf(fp,"\t}\n");
	return true;
}

void LLViewerInventoryItem::updateParentOnServer(BOOL restamp) const
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_MoveInventoryItem);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addBOOLFast(_PREHASH_Stamp, restamp);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addUUIDFast(_PREHASH_ItemID, mUUID);
	msg->addUUIDFast(_PREHASH_FolderID, mParentUUID);
	msg->addString("NewName", NULL);
	gAgent.sendReliableMessage();
}

//void LLViewerInventoryItem::setCloneCount(S32 clones)
//{
//	mClones = clones;
//}

//S32 LLViewerInventoryItem::getCloneCount() const
//{
//	return mClones;
//}

///----------------------------------------------------------------------------
/// Class LLViewerInventoryCategory
///----------------------------------------------------------------------------

LLViewerInventoryCategory::LLViewerInventoryCategory(const LLUUID& uuid,
													 const LLUUID& parent_uuid,
													 LLAssetType::EType pref,
													 const std::string& name,
													 const LLUUID& owner_id) :
	LLInventoryCategory(uuid, parent_uuid, pref, name),
	mOwnerID(owner_id),
	mVersion(LLViewerInventoryCategory::VERSION_UNKNOWN),
	mDescendentCount(LLViewerInventoryCategory::DESCENDENT_COUNT_UNKNOWN)
{
	mDescendentsRequested.reset();
}

LLViewerInventoryCategory::LLViewerInventoryCategory(const LLUUID& owner_id) :
	mOwnerID(owner_id),
	mVersion(LLViewerInventoryCategory::VERSION_UNKNOWN),
	mDescendentCount(LLViewerInventoryCategory::DESCENDENT_COUNT_UNKNOWN)
{
	mDescendentsRequested.reset();
}

LLViewerInventoryCategory::LLViewerInventoryCategory(const LLViewerInventoryCategory* other)
{
	copyViewerCategory(other);
}

LLViewerInventoryCategory::~LLViewerInventoryCategory()
{
}

void LLViewerInventoryCategory::copyViewerCategory(const LLViewerInventoryCategory* other)
{
	copyCategory(other);
	mOwnerID = other->mOwnerID;
	mVersion = other->mVersion;
	mDescendentCount = other->mDescendentCount;
	mDescendentsRequested = other->mDescendentsRequested;
}


void LLViewerInventoryCategory::updateParentOnServer(BOOL restamp) const
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_MoveInventoryFolder);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	msg->addBOOL("Stamp", restamp);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addUUIDFast(_PREHASH_FolderID, mUUID);
	msg->addUUIDFast(_PREHASH_ParentID, mParentUUID);
	gAgent.sendReliableMessage();
}

void LLViewerInventoryCategory::updateServer(BOOL is_new) const
{
	// communicate that change with the server.

	if (LLAssetType::lookupIsProtectedCategoryType(mPreferredType))
	{
		LLNotifications::instance().add("CannotModifyProtectedCategories");
		return;
	}

	LLInventoryModel::LLCategoryUpdate up(mParentUUID, is_new ? 1 : 0);
	gInventory.accountForUpdate(up);

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_UpdateInventoryFolder);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_FolderData);
	packMessage(msg);
	gAgent.sendReliableMessage();
}

void LLViewerInventoryCategory::removeFromServer( void )
{
	llinfos << "Removing inventory category " << mUUID << " from server."
			<< llendl;
	// communicate that change with the server.
	if(LLAssetType::lookupIsProtectedCategoryType(mPreferredType))
	{
		LLNotifications::instance().add("CannotRemoveProtectedCategories");
		return;
	}

	LLInventoryModel::LLCategoryUpdate up(mParentUUID, -1);
	gInventory.accountForUpdate(up);

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_RemoveInventoryFolder);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_FolderData);
	msg->addUUIDFast(_PREHASH_FolderID, mUUID);
	gAgent.sendReliableMessage();
}

bool LLViewerInventoryCategory::fetchDescendents()
{
	if((VERSION_UNKNOWN == mVersion)
	   && mDescendentsRequested.hasExpired())	//Expired check prevents multiple downloads.
	{
		const F32 FETCH_TIMER_EXPIRY = 10.0f;
		mDescendentsRequested.reset();
		mDescendentsRequested.setTimerExpirySec(FETCH_TIMER_EXPIRY);

		// bitfield
		// 1 = by date
		// 2 = folders by date
		// Need to mask off anything but the first bit.
		// This comes from LLInventoryFilter from llfolderview.h
		U32 sort_order = gSavedSettings.getU32("InventorySortOrder") & 0x1;

		std::string url = gAgent.getRegion()->getCapability("WebFetchInventoryDescendents");
   
		if (!url.empty()) //Capability found.  Build up LLSD and use it.
		{
			LLInventoryModel::startBackgroundFetch(mUUID);			
		}
		else
		{	//Deprecated, but if we don't have a capability, use the old system.
			llinfos << "WebFetchInventoryDescendents capability not found.  Using deprecated UDP message." << llendl;
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessage("FetchInventoryDescendents");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgent.getID());
			msg->addUUID("SessionID", gAgent.getSessionID());
			msg->nextBlock("InventoryData");
			msg->addUUID("FolderID", mUUID);
			msg->addUUID("OwnerID", mOwnerID);

			msg->addS32("SortOrder", sort_order);
			msg->addBOOL("FetchFolders", FALSE);
			msg->addBOOL("FetchItems", TRUE);
			gAgent.sendReliableMessage();
		}
		return true;
	}
	return false;
}

bool LLViewerInventoryCategory::importFileLocal(LLFILE* fp)
{
	// *NOTE: This buffer size is hard coded into scanf() below.
	char buffer[MAX_STRING];		/* Flawfinder: ignore */
	char keyword[MAX_STRING];		/* Flawfinder: ignore */
	char valuestr[MAX_STRING];		/* Flawfinder: ignore */

	keyword[0] = '\0';
	valuestr[0] = '\0';
	while(!feof(fp))
	{
		if (fgets(buffer, MAX_STRING, fp) == NULL)
		{
			buffer[0] = '\0';
		}
		
		sscanf(	/* Flawfinder: ignore */
			buffer, " %254s %254s", keyword, valuestr); 
		if(0 == strcmp("{",keyword))
		{
			continue;
		}
		if(0 == strcmp("}", keyword))
		{
			break;
		}
		else if(0 == strcmp("cat_id", keyword))
		{
			mUUID.set(valuestr);
		}
		else if(0 == strcmp("parent_id", keyword))
		{
			mParentUUID.set(valuestr);
		}
		else if(0 == strcmp("type", keyword))
		{
			mType = LLAssetType::lookup(valuestr);
		}
		else if(0 == strcmp("pref_type", keyword))
		{
			mPreferredType = LLAssetType::lookup(valuestr);
		}
		else if(0 == strcmp("name", keyword))
		{
			//strcpy(valuestr, buffer + strlen(keyword) + 3);
			// *NOTE: Not ANSI C, but widely supported.
			sscanf(	/* Flawfinder: ignore */
				buffer, " %254s %254[^|]", keyword, valuestr);
			mName.assign(valuestr);
			LLStringUtil::replaceNonstandardASCII(mName, ' ');
			LLStringUtil::replaceChar(mName, '|', ' ');
		}
		else if(0 == strcmp("owner_id", keyword))
		{
			mOwnerID.set(valuestr);
		}
		else if(0 == strcmp("version", keyword))
		{
			sscanf(valuestr, "%d", &mVersion);
		}
		else
		{
			llwarns << "unknown keyword '" << keyword
					<< "' in inventory import category "  << mUUID << llendl;
		}
	}
	return true;
}

bool LLViewerInventoryCategory::exportFileLocal(LLFILE* fp) const
{
	std::string uuid_str;
	fprintf(fp, "\tinv_category\t0\n\t{\n");
	mUUID.toString(uuid_str);
	fprintf(fp, "\t\tcat_id\t%s\n", uuid_str.c_str());
	mParentUUID.toString(uuid_str);
	fprintf(fp, "\t\tparent_id\t%s\n", uuid_str.c_str());
	fprintf(fp, "\t\ttype\t%s\n", LLAssetType::lookup(mType));
	fprintf(fp, "\t\tpref_type\t%s\n", LLAssetType::lookup(mPreferredType));
	fprintf(fp, "\t\tname\t%s|\n", mName.c_str());
	mOwnerID.toString(uuid_str);
	fprintf(fp, "\t\towner_id\t%s\n", uuid_str.c_str());
	fprintf(fp, "\t\tversion\t%d\n", mVersion);
	fprintf(fp,"\t}\n");
	return true;
}

///----------------------------------------------------------------------------
/// Local function definitions
///----------------------------------------------------------------------------

LLInventoryCallbackManager *LLInventoryCallbackManager::sInstance = NULL;

LLInventoryCallbackManager::LLInventoryCallbackManager() :
	mLastCallback(0)
{
	if( sInstance != NULL )
	{
		llwarns << "LLInventoryCallbackManager::LLInventoryCallbackManager: unexpected multiple instances" << llendl;
		return;
	}
	sInstance = this;
}

LLInventoryCallbackManager::~LLInventoryCallbackManager()
{
	if( sInstance != this )
	{
		llwarns << "LLInventoryCallbackManager::~LLInventoryCallbackManager: unexpected multiple instances" << llendl;
		return;
	}
	sInstance = NULL;
}

U32 LLInventoryCallbackManager::registerCB(LLPointer<LLInventoryCallback> cb)
{
	if (cb.isNull())
		return 0;

	mLastCallback++;
	if (!mLastCallback)
		mLastCallback++;

	mMap[mLastCallback] = cb;
	return mLastCallback;
}

void LLInventoryCallbackManager::fire(U32 callback_id, const LLUUID& item_id)
{
	if (!callback_id || item_id.isNull())
		return;

	std::map<U32, LLPointer<LLInventoryCallback> >::iterator i;

	i = mMap.find(callback_id);
	if (i != mMap.end())
	{
		(*i).second->fire(item_id);
		mMap.erase(i);
	}
}

void WearOnAvatarCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
		return;

	LLViewerInventoryItem *item = gInventory.getItem(inv_item);
	if (item)
	{
		wear_inventory_item_on_avatar(item);
	}
}

RezAttachmentCallback::RezAttachmentCallback(LLViewerJointAttachment *attachmentp)
{
	mAttach = attachmentp;
}
RezAttachmentCallback::~RezAttachmentCallback()
{
}

void RezAttachmentCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
		return;

	LLViewerInventoryItem *item = gInventory.getItem(inv_item);
	if (item)
	{
		rez_attachment(item, mAttach);
	}
}

void ActivateGestureCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
		return;

	LLGestureManager::instance().activateGesture(inv_item);
}

void CreateGestureCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
		return;

	LLGestureManager::instance().activateGesture(inv_item);
	
	LLViewerInventoryItem* item = gInventory.getItem(inv_item);
	if (!item) return;
    gInventory.updateItem(item);
    gInventory.notifyObservers();

	LLPreviewGesture* preview = LLPreviewGesture::show(inv_item,  LLUUID::null);
	// Force to be entirely onscreen.
	gFloaterView->adjustToFitScreen(preview, FALSE);
}

LLInventoryCallbackManager gInventoryCallbacks;

void create_inventory_item(const LLUUID& agent_id, const LLUUID& session_id,
						   const LLUUID& parent, const LLTransactionID& transaction_id,
						   const std::string& name,
						   const std::string& desc, LLAssetType::EType asset_type,
						   LLInventoryType::EType inv_type, EWearableType wtype,
						   U32 next_owner_perm,
						   LLPointer<LLInventoryCallback> cb)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_CreateInventoryItem);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, agent_id);
	msg->addUUIDFast(_PREHASH_SessionID, session_id);
	msg->nextBlock(_PREHASH_InventoryBlock);
	msg->addU32Fast(_PREHASH_CallbackID, gInventoryCallbacks.registerCB(cb));
	msg->addUUIDFast(_PREHASH_FolderID, parent);
	msg->addUUIDFast(_PREHASH_TransactionID, transaction_id);
	msg->addU32Fast(_PREHASH_NextOwnerMask, next_owner_perm);
	msg->addS8Fast(_PREHASH_Type, (S8)asset_type);
	msg->addS8Fast(_PREHASH_InvType, (S8)inv_type);
	msg->addU8Fast(_PREHASH_WearableType, (U8)wtype);
	msg->addStringFast(_PREHASH_Name, name);
	msg->addStringFast(_PREHASH_Description, desc);
	
	gAgent.sendReliableMessage();
}

void copy_inventory_item(
	const LLUUID& agent_id,
	const LLUUID& current_owner,
	const LLUUID& item_id,
	const LLUUID& parent_id,
	const std::string& new_name,
	LLPointer<LLInventoryCallback> cb)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_CopyInventoryItem);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, agent_id);
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addU32Fast(_PREHASH_CallbackID, gInventoryCallbacks.registerCB(cb));
	msg->addUUIDFast(_PREHASH_OldAgentID, current_owner);
	msg->addUUIDFast(_PREHASH_OldItemID, item_id);
	msg->addUUIDFast(_PREHASH_NewFolderID, parent_id);
	msg->addStringFast(_PREHASH_NewName, new_name);
	gAgent.sendReliableMessage();
}

void link_inventory_item(
	const LLUUID& agent_id,
	const LLUUID& item_id,
	const LLUUID& parent_id,
	const std::string& new_name,
	const LLAssetType::EType asset_type,
	LLPointer<LLInventoryCallback> cb)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_LinkInventoryItem);
	msg->nextBlockFast(_PREHASH_AgentData);
	{
		msg->addUUIDFast(_PREHASH_AgentID, agent_id);
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	}
	msg->nextBlockFast(_PREHASH_InventoryData);
	{
		msg->addU32Fast(_PREHASH_CallbackID, gInventoryCallbacks.registerCB(cb));
		msg->addUUIDFast(_PREHASH_FolderID, parent_id);
		msg->addUUIDFast(_PREHASH_OldItemID, item_id);
		msg->addStringFast(_PREHASH_Name, new_name);
		msg->addU8Fast(_PREHASH_AssetType, asset_type);
	}
	gAgent.sendReliableMessage();
}

void move_inventory_item(
	const LLUUID& agent_id,
	const LLUUID& session_id,
	const LLUUID& item_id,
	const LLUUID& parent_id,
	const std::string& new_name,
	LLPointer<LLInventoryCallback> cb)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_MoveInventoryItem);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, agent_id);
	msg->addUUIDFast(_PREHASH_SessionID, session_id);
	msg->addBOOLFast(_PREHASH_Stamp, FALSE);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addUUIDFast(_PREHASH_ItemID, item_id);
	msg->addUUIDFast(_PREHASH_FolderID, parent_id);
	msg->addStringFast(_PREHASH_NewName, new_name);
	gAgent.sendReliableMessage();
}

void copy_inventory_from_notecard(const LLUUID& object_id, const LLUUID& notecard_inv_id, const LLInventoryItem *src, U32 callback_id)
{
	LLViewerRegion* viewer_region = NULL;
    LLViewerObject* vo = NULL;
	if (object_id.notNull() && (vo = gObjectList.findObject(object_id)) != NULL)
    {
        viewer_region = vo->getRegion();
	}

	// Fallback to the agents region if for some reason the 
	// object isn't found in the viewer.
	if (! viewer_region)
	{
		viewer_region = gAgent.getRegion();
	}

	if (! viewer_region)
	{
        LL_WARNS("copy_inventory_from_notecard") << "Can't find region from object_id "
                                                 << object_id << " or gAgent"
                                                 << LL_ENDL;
        return;
    }

    LLSD request, body;
    body["notecard-id"] = notecard_inv_id;
    body["object-id"] = object_id;
    body["item-id"] = src->getUUID();
    body["folder-id"] = gInventory.findCategoryUUIDForType(src->getType());
    body["callback-id"] = (LLSD::Integer)callback_id;

    request["message"] = "CopyInventoryFromNotecard";
    request["payload"] = body;

    viewer_region->getCapAPI().post(request);
}

void create_new_item(const std::string& name,
				   const LLUUID& parent_id,
				   LLAssetType::EType asset_type,
				   LLInventoryType::EType inv_type,
				   U32 next_owner_perm)
{
	std::string desc;
	LLAssetType::generateDescriptionFor(asset_type, desc);
	next_owner_perm = (next_owner_perm) ? next_owner_perm : PERM_MOVE | PERM_TRANSFER;

	
	if (inv_type == LLInventoryType::IT_GESTURE)
	{
		LLPointer<LLInventoryCallback> cb = new CreateGestureCallback();
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							  parent_id, LLTransactionID::tnull, name, desc, asset_type, inv_type,
							  NOT_WEARABLE, next_owner_perm, cb);
	}
	else
	{
		LLPointer<LLInventoryCallback> cb = NULL;
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							  parent_id, LLTransactionID::tnull, name, desc, asset_type, inv_type,
							  NOT_WEARABLE, next_owner_perm, cb);
	}
	
}	

const std::string NEW_LSL_NAME = "New Script"; // *TODO:Translate? (probably not)
const std::string NEW_NOTECARD_NAME = "New Note"; // *TODO:Translate? (probably not)
const std::string NEW_GESTURE_NAME = "New Gesture"; // *TODO:Translate? (probably not)

void menu_create_inventory_item(LLFolderView* folder, LLFolderBridge *bridge, const LLSD& userdata)
{
	std::string type = userdata.asString();
	
	if ("category" == type)
	{
		LLUUID category;
		if (bridge)
		{
			category = gInventory.createNewCategory(bridge->getUUID(), LLAssetType::AT_NONE, LLStringUtil::null);
		}
		else
		{
			category = gInventory.createNewCategory(gInventory.getRootFolderID(), LLAssetType::AT_NONE, LLStringUtil::null);
		}
		gInventory.notifyObservers();
		folder->setSelectionByID(category, TRUE);
	}
	else if ("lsl" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_LSL_TEXT);
		create_new_item(NEW_LSL_NAME,
					  parent_id,
					  LLAssetType::AT_LSL_TEXT,
					  LLInventoryType::IT_LSL,
					  PERM_MOVE | PERM_TRANSFER);
	}
	else if ("notecard" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_NOTECARD);
		create_new_item(NEW_NOTECARD_NAME,
					  parent_id,
					  LLAssetType::AT_NOTECARD,
					  LLInventoryType::IT_NOTECARD,
					  PERM_ALL);
	}
	else if ("gesture" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_GESTURE);
		create_new_item(NEW_GESTURE_NAME,
					  parent_id,
					  LLAssetType::AT_GESTURE,
					  LLInventoryType::IT_GESTURE,
					  PERM_ALL);
	}
	else if ("shirt" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_SHIRT);
	}
	else if ("pants" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_PANTS);
	}
	else if ("shoes" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_SHOES);
	}
	else if ("socks" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_SOCKS);
	}
	else if ("jacket" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_JACKET);
	}
	else if ("skirt" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_SKIRT);
	}
	else if ("gloves" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_GLOVES);
	}
	else if ("undershirt" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_UNDERSHIRT);
	}
	else if ("underpants" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_CLOTHING);
		LLFolderBridge::createWearable(parent_id, WT_UNDERPANTS);
	}
	else if ("shape" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_BODYPART);
		LLFolderBridge::createWearable(parent_id, WT_SHAPE);
	}
	else if ("skin" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_BODYPART);
		LLFolderBridge::createWearable(parent_id, WT_SKIN);
	}
	else if ("hair" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_BODYPART);
		LLFolderBridge::createWearable(parent_id, WT_HAIR);
	}
	else if ("eyes" == type)
	{
		LLUUID parent_id = bridge ? bridge->getUUID() : gInventory.findCategoryUUIDForType(LLAssetType::AT_BODYPART);
		LLFolderBridge::createWearable(parent_id, WT_EYES);
	}
	
	folder->setNeedsAutoRename(TRUE);	
}

LLAssetType::EType LLViewerInventoryItem::getType() const
{
	if (const LLViewerInventoryItem *linked_item = getLinkedItem())
	{
		return linked_item->getType();
	}
	if (const LLViewerInventoryCategory *linked_category = getLinkedCategory())
	{
		return linked_category->getType();
	}	
	return LLInventoryItem::getType();
}

const LLUUID& LLViewerInventoryItem::getAssetUUID() const
{
	if (const LLViewerInventoryItem *linked_item = getLinkedItem())
	{
		return linked_item->getAssetUUID();
	}

	return LLInventoryItem::getAssetUUID();
}

const std::string& LLViewerInventoryItem::getName() const
{
	if (const LLViewerInventoryItem *linked_item = getLinkedItem())
	{
		return linked_item->getName();
	}
	if (const LLViewerInventoryCategory *linked_category = getLinkedCategory())
	{
		return linked_category->getName();
	}

	return LLInventoryItem::getName();
}

const LLPermissions& LLViewerInventoryItem::getPermissions() const
{
	// Use the actual permissions of the symlink, not its parent.
	return LLInventoryItem::getPermissions();	
}

const LLUUID& LLViewerInventoryItem::getCreatorUUID() const
{
	if (const LLViewerInventoryItem *linked_item = getLinkedItem())
	{
		return linked_item->getCreatorUUID();
	}

	return LLInventoryItem::getCreatorUUID();
}

const std::string& LLViewerInventoryItem::getDescription() const
{
	if (const LLViewerInventoryItem *linked_item = getLinkedItem())
	{
		return linked_item->getDescription();
	}

	return LLInventoryItem::getDescription();
}

const LLSaleInfo& LLViewerInventoryItem::getSaleInfo() const
{	
	if (const LLViewerInventoryItem *linked_item = getLinkedItem())
	{
		return linked_item->getSaleInfo();
	}

	return LLInventoryItem::getSaleInfo();
}

LLInventoryType::EType LLViewerInventoryItem::getInventoryType() const
{
	if (const LLViewerInventoryItem *linked_item = getLinkedItem())
	{
		return linked_item->getInventoryType();
	}

	return LLInventoryItem::getInventoryType();
}

U32 LLViewerInventoryItem::getFlags() const
{
	if (const LLViewerInventoryItem *linked_item = getLinkedItem())
	{
		return linked_item->getFlags();
	}

	return LLInventoryItem::getFlags();
}

time_t LLViewerInventoryItem::getCreationDate() const
{
	return LLInventoryItem::getCreationDate();
}

U32 LLViewerInventoryItem::getCRC32() const
{
	return LLInventoryItem::getCRC32();	
}

const LLViewerInventoryItem *LLViewerInventoryItem::getLinkedItem() const
{
	if (mType == LLAssetType::AT_LINK)
	{
		const LLViewerInventoryItem *linked_item = gInventory.getItem(mAssetUUID);
		return linked_item;
	}
	return NULL;
}

const LLViewerInventoryCategory *LLViewerInventoryItem::getLinkedCategory() const
{
	if (mType == LLAssetType::AT_LINK_FOLDER)
	{
		const LLViewerInventoryCategory *linked_category = gInventory.getCategory(mAssetUUID);
		return linked_category;
	}
	return NULL;
}
