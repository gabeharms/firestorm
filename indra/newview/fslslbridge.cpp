/** 
 * @fslslbridge.cpp 
 * @FSLSLBridge implementation 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Viewer Project, Inc.
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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "fslslbridge.h"
#include "fslslbridgerequest.h"
#include "imageids.h"
#include "llxmlnode.h"
#include "llbufferstream.h"
#include "llsdserialize.h"
#include "llviewerinventory.h"
#include "llagent.h"
#include "llvoavatar.h"
#include "llattachmentsmgr.h"
#include "llinventorymodel.h"
#include "llinventoryfunctions.h"
#include "llviewerassettype.h"
#include "llfloaterreg.h"
#include "llinventorybridge.h"
#include "llpreviewscript.h"
#include "llselectmgr.h"
#include "llinventorydefines.h"
#include "llviewerregion.h"
#include "llfoldertype.h"
#include "llhttpclient.h"
#include "llassetuploadresponders.h"
#include "llnearbychatbar.h"
#include "llnotificationmanager.h"
#include "llviewerobject.h"

#include <boost/regex.hpp>

//#define phoenix_bridge_name "#LSL<->Client Bridge v0.12"
#define phoenix_folder_name "#Phoenix"

#define LIB_ROCK_NAME "Rock - medium, round"

//#define ROOT_FIRESTORM_FOLDER "#Firestorm"	//moved to llinventoryfunctions to synch with the AO object
#define FS_BRIDGE_FOLDER "#LSL Bridge"
#define FS_BRIDGE_NAME "#Firestorm LSL Bridge v"
#define FS_BRIDGE_MAJOR_VERSION 1
#define FS_BRIDGE_MINOR_VERSION 9

//current script version is 1.9
const std::string UPLOAD_SCRIPT_CURRENT = "EBEDD1D2-A320-43f5-88CF-DD47BBCA5DFB.lsltxt";

const boost::regex FSBridgePattern("^#Firestorm LSL Bridge v*.*");


//
//-TT Client LSL Bridge File
//
class NameCollectFunctor : public LLInventoryCollectFunctor
{
public:
	NameCollectFunctor(std::string name)
	{
		sName = name;
	}
	virtual ~NameCollectFunctor() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		if(item)
		{
			return (item->getName() == sName);
		}
		return false;
	}
private:
	std::string sName;
};

//
//
// Bridge functionality
//
FSLSLBridge :: FSLSLBridge():
					mBridgeCreating(false),
					mpBridge(NULL),
					mIsFirstCallDone(false)
{
	llinfos << "Initializing fsbridge class" << llendl;
	std::stringstream sstr;
	
	sstr << FS_BRIDGE_NAME;
	sstr << FS_BRIDGE_MAJOR_VERSION;
	sstr << ".";
	sstr << FS_BRIDGE_MINOR_VERSION;

	mCurrentFullName = sstr.str();

	//mBridgeCreating = false;
	//mpBridge = NULL;
}

FSLSLBridge :: ~FSLSLBridge()
{
}

bool FSLSLBridge :: lslToViewer(std::string message, LLUUID fromID, LLUUID ownerID)
{
	if (!gSavedSettings.getBOOL("UseLSLBridge"))
		return false;

	llinfos << message << llendl;

	std::string tag = message.substr(0,11);
	bool status = false;

	if (tag == "<bridgeURL>")
	{
		// get the content of the message, between <tag> and </tag>
		mCurrentURL = message.substr(tag.length(), message.length() - ((tag.length() * 2) + 1));
		llinfos << "New URL is: " << mCurrentURL << llendl;
		
		if (mpBridge == NULL)
		{
			LLUUID catID = findFSCategory();
			LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID, LLAssetType::AT_OBJECT);

			if (fsBridge != NULL)
				mpBridge = fsBridge;
		}
		status = true;
		if (!mIsFirstCallDone)
		{
			//on first call from bridge, confirm that we are here
			//then check options use
			viewerToLSL("URL Confirmed", new FSLSLBridgeRequestResponder());
			updateBoolSettingValue("UseLSLFlightAssist");

			mIsFirstCallDone = true;
		}
	}
	return status;
}

bool FSLSLBridge :: viewerToLSL(std::string message, FSLSLBridgeRequestResponder *responder)
{
	if (!gSavedSettings.getBOOL("UseLSLBridge"))
		return false;

	if (responder == NULL)
		responder = new FSLSLBridgeRequestResponder();
	LLHTTPClient::post(mCurrentURL, LLSD(message), responder);

	return true;
}

bool FSLSLBridge :: updateBoolSettingValue(std::string msgVal)
{
	std::string boolVal = "0";

	if (gSavedSettings.getBOOL(msgVal))
		boolVal = "1";

	return viewerToLSL( msgVal+"|"+boolVal, new FSLSLBridgeRequestResponder());
}

bool FSLSLBridge :: updateBoolSettingValue(std::string msgVal, bool contentVal)
{
	std::string boolVal = "0";

	if (contentVal)
		boolVal = "1";

	return viewerToLSL( msgVal+"|"+boolVal, new FSLSLBridgeRequestResponder());
}

//
//Bridge initialization
//
void FSLSLBridge :: recreateBridge()
{
	if (!gSavedSettings.getBOOL("UseLSLBridge"))
		return;

	if (mBridgeCreating)
	{
		reportToNearbyChat("Bridge creation in process, can't start another. Please wait a few minutes.");
		return;
	}

	LLUUID catID = findFSCategory();

	LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID, LLAssetType::AT_OBJECT);
	if (fsBridge != NULL)
	{
		if (get_is_item_worn(fsBridge->getUUID()))
		{
			LLVOAvatarSelf::detachAttachmentIntoInventory(fsBridge->getUUID());
		}
	}
	// clear the stored bridge ID - we are starting over.
	if (mpBridge != NULL)
		mpBridge = NULL; //the object itself will get cleaned up when new one is created.

	initCreationStep();
}

void FSLSLBridge :: initBridge()
{
	if (!gSavedSettings.getBOOL("UseLSLBridge"))
		return;

	LLUUID catID = findFSCategory();

	//check for inventory load
	FSLSLBridgeInventoryObserver *bridgeInventoryObserver = new FSLSLBridgeInventoryObserver(catID);
	gInventory.addObserver(bridgeInventoryObserver);
}


// Gets called by the Init, when inventory loaded.
void FSLSLBridge :: startCreation()
{
	//if bridge object doesn't exist - create and attach it, update script.
	LLUUID catID = findFSCategory();

	LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID, LLAssetType::AT_OBJECT);
	if (fsBridge == NULL)
	{
		llinfos << "bridge not found, creating new" << llendl;
		initCreationStep();
	}
	else
	{
		//TODO need versioning - see isOldBridgeVersion()
		mpBridge = fsBridge;
		if (!isItemAttached(mpBridge->getUUID()))
		{
			//Is this a valid bridge - wear it. 
			LLAttachmentsMgr::instance().addAttachment(mpBridge->getUUID(), BRIDGE_POINT, FALSE, TRUE);	
			llinfos << "found a bridge, reattached it" << llendl;
			//from here, the attach shoould report to ProcessAttach and make sure bridge is valid.
		}
	}
}

void FSLSLBridge :: initCreationStep()
{
	mBridgeCreating = true;
	//announce yourself
	reportToNearbyChat("Creating the bridge. This might take a few moments, please wait");

	if (gSavedSettings.getBOOL("NoInventoryLibrary"))
	{
		llwarns << "Asked to create bridge, but we don't have a library" << llendl;
		reportToNearbyChat("Firestorm could not create an LSL bridge. Please enable your library and relog");
		return;
	}
	createNewBridge();
}

void FSLSLBridge :: createNewBridge() 
{
	//check if user has a bridge
	LLUUID catID = findFSCategory();

	//attach the Linden rock from the library (will resize as soon as attached)
	LLUUID libID = gInventory.getLibraryRootFolderID();
	LLViewerInventoryItem* libRock = findInvObject(LIB_ROCK_NAME, libID, LLAssetType::AT_OBJECT);
	//shouldn't happen but just in case
	if (libRock != NULL)
	{
		//copy the library item to inventory and put it on 
		LLPointer<LLInventoryCallback> cb = new FSLSLBridgeRezCallback();
		llinfos << "attach the Linden rock from the library" << llendl;
		copy_inventory_item(gAgent.getID(),libRock->getPermissions().getOwner(),libRock->getUUID(),catID,mCurrentFullName,cb);
	}
}

void FSLSLBridge :: processAttach(LLViewerObject *object, const LLViewerJointAttachment *attachment)
{
	llinfos << "enter process attach, checking the rock" << llendl;

	if ((!gAgentAvatarp->isSelf()) || (attachment->getName() != "Bridge"))
		return;

	LLViewerInventoryItem *fsObject = gInventory.getItem(object->getAttachmentItemID());
	if (fsObject == NULL) //just in case
		return;
	if (mpBridge == NULL) //user is attaching an existing bridge?
	{
		//is it in the right place?
		LLUUID catID = findFSCategory();
		if (catID != fsObject->getParentUUID())
		{
			//the object is not where we think it is. Kick it off.
			LLVOAvatarSelf::detachAttachmentIntoInventory(fsObject->getUUID());
			return;
		}
		mpBridge = fsObject;
	}
	llinfos << "rock is attached, mpBridge not NULL, avatar is self, point is bridge" << llendl;


	if (fsObject->getUUID() != mpBridge->getUUID())
	{
		//something odd just got attached to bridge?
		llinfos << "Something unknown just got attached to bridge point, detaching." << llendl;
		LLVOAvatarSelf::detachAttachmentIntoInventory(mpBridge->getUUID());
		return;
	}
	llinfos << "rock found is the same rock we saved, id matched" << llendl;

	if (!mBridgeCreating) //just an attach. See what it is
	{
		//are we attaching the right thing? Check size and script
		LLInventoryObject::object_list_t inventory_objects;
		object->getInventoryContents(inventory_objects);

		if (object->flagInventoryEmpty())
		{
			llinfos << "Empty bridge - re-enter creation process" << llendl;
			mBridgeCreating = true;
		}
		else if (inventory_objects.size() > 0)
		{
			LLInventoryObject::object_list_t::iterator it = inventory_objects.begin();
			LLInventoryObject::object_list_t::iterator end = inventory_objects.end();
			bool isOurScript = false;
			for ( ; it != end; ++it)
			{
				LLInventoryItem* item = ((LLInventoryItem*)((LLInventoryObject*)(*it)));
				if (item->getType() == LLAssetType::AT_LSL_TEXT)
				{
					if (item->getCreatorUUID() == gAgent.getID()) 
						isOurScript = true;
					else //problem, not our script
						llwarns << "The bridge inventory contains a script not created by user" << llendl;
				}
			}
			if ((inventory_objects.size() == 1) && isOurScript) //We attached a valid bridge. Run along.
				return;
			else 
			{
				reportToNearbyChat("The bridge inventory contains unexpected items");
				llinfos << "The bridge inventory contains items other than bridge script" << llendl;
				if (!isOurScript)	//some junk but no valid script? Unlikely to happen, but lets add script anyway.
					mBridgeCreating = true;
				else //Let the script disable competitors 
					return;
			}
		}
		else
			llinfos << "Bridge not empty, but we're unable to retrieve contents." << llendl;
	}

	//modify the rock size and texture
	if ((object != NULL) && (mBridgeCreating))
	{
		llinfos << "rock object found after second attachment, resize" << llendl;
		setupBridgePrim(object);

		mpBridge->setDescription(mCurrentFullName);
		mpBridge->setComplete(TRUE);
		mpBridge->updateServer(FALSE);

		gInventory.updateItem(mpBridge);
		gInventory.notifyObservers();

		//add bridge script to object
		llinfos << "go on to create script" << llendl;
		create_script_inner(object);
	}
}

void FSLSLBridge :: processDetach(LLViewerObject *object, const LLViewerJointAttachment *attachment)
{
	llinfos << "enter process detach" << llendl;

	if ((!gAgentAvatarp->isSelf()) || (attachment->getName() != "Bridge"))
		return;

	LLViewerInventoryItem *fsObject = gInventory.getItem(object->getAttachmentItemID());
	if (fsObject == NULL) //just in case
		return;
	//is it in the right place?
	LLUUID catID = findFSCategory();
	if (catID != fsObject->getParentUUID())
	{
		//that was in the wrong place. It's not ours.
		return;
	}
	if (mpBridge != NULL && mpBridge->getUUID() == fsObject->getUUID()) 
	{
		mpBridge = NULL;
		reportToNearbyChat("Bridge detached.");
		if (mBridgeCreating)
		{
			reportToNearbyChat("Bridge has not finished creating, you might need to recreate it before using");
			mBridgeCreating = false; //in case we interrupted the creation
		}
	}

}

void FSLSLBridge :: setupBridgePrim(LLViewerObject *object)
{
	llinfos << "enter rock change" << llendl;

	LLProfileParams profParams(LL_PCODE_PROFILE_CIRCLE, F32(0.230), F32(0.250), F32(0.95));
	LLPathParams pathParams(LL_PCODE_PATH_CIRCLE, F32(0.2), F32(0.22), 
		F32(0.0), F32(350.0),	//scale
		F32(0.0), F32(0.0),		//shear
		F32(0), F32(0),			//twist
		F32(0),					//offset
		F32(0), F32(0.0),		//taper
		F32(0.05), F32(0.05));	//revolutions, skew
	pathParams.setRevolutions(F32(1.0));
	object->setVolume(LLVolumeParams(profParams, pathParams), 0);

	object->setScale(LLVector3(10.0f, 10.0f, 10.0f), TRUE);
	for (int i = 0; i < object->getNumTEs(); i++)
	{
		LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture( IMG_INVISIBLE );
		object->setTEImage(i, image); //transparent texture
	}
	object->setChanged(LLXform::MOVED | LLXform::SILHOUETTE | LLXform::TEXTURE);

	//object->setTETexture(0, LLUUID("29de489d-0491-fb00-7dab-f9e686d31e83")); //another test texture
	object->sendShapeUpdate();
	object->markForUpdate(TRUE);

	//object->setFlags(FLAGS_TEMPORARY_ON_REZ, true);
	object->addFlags(FLAGS_TEMPORARY_ON_REZ);
	object->updateFlags();

	llinfos << "end rock change" << llendl;
}

void FSLSLBridge :: create_script_inner(LLViewerObject* object)
{
	LLUUID catID = findFSCategory();

	LLPointer<LLInventoryCallback> cb = new FSLSLBridgeScriptCallback();
	create_inventory_item(gAgent.getID(), 
							gAgent.getSessionID(),
							catID,	//LLUUID::null, 
							LLTransactionID::tnull, 
							mCurrentFullName, 
							mCurrentFullName, 
							LLAssetType::AT_LSL_TEXT, 
							LLInventoryType::IT_LSL,
							NOT_WEARABLE, 
							mpBridge->getPermissions().getMaskNextOwner(), 
							cb);

}

//
// Bridge rez callback
//
FSLSLBridgeRezCallback :: FSLSLBridgeRezCallback()
{
}
FSLSLBridgeRezCallback :: ~FSLSLBridgeRezCallback()
{
}

void FSLSLBridgeRezCallback :: fire(const LLUUID& inv_item)
{
	// this is the first attach - librock got copied and worn on hand - but the ID is now real.
	if ((FSLSLBridge::instance().getBridge() != NULL) || inv_item.isNull() || !FSLSLBridge::instance().getBridgeCreating())
		return;

	llinfos << "rock attach callback fired, look for object" << llendl;

	LLViewerObject* obj = gAgentAvatarp->getWornAttachment(inv_item);
	if (obj != NULL)
	{
		llinfos << "rock object found, resize" << llendl;
		FSLSLBridge::instance().setupBridgePrim(obj);
	}
	else
		llinfos << "rock object not found yet, keep going" << llendl;

	//detach from default and put on the right point
	LLVOAvatarSelf::detachAttachmentIntoInventory(inv_item);
	LLViewerInventoryItem *item = gInventory.getItem(inv_item);
	//from this point on, this is our bridge - accept no substitutes!
	FSLSLBridge::instance().setBridge(item);
	
	llinfos << "attaching rock to the right spot now" << llendl;
	LLAttachmentsMgr::instance().addAttachment(inv_item, FSLSLBridge::BRIDGE_POINT, TRUE, TRUE);
}


//
// Bridge script creation callback
//
FSLSLBridgeScriptCallback :: FSLSLBridgeScriptCallback()
{
}
FSLSLBridgeScriptCallback :: ~FSLSLBridgeScriptCallback()
{
}

void FSLSLBridgeScriptCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull() || !FSLSLBridge::instance().getBridgeCreating())
		return;

	LLViewerInventoryItem* item = gInventory.getItem(inv_item);
	if (!item) 
	{
		return;
	}
    gInventory.updateItem(item);
    gInventory.notifyObservers();

	LLViewerObject* obj = gAgentAvatarp->getWornAttachment(FSLSLBridge::instance().getBridge()->getUUID());

	//caps import 
	std::string url = gAgent.getRegion()->getCapability("UpdateScriptAgent");
	std::string isMono = "lsl2";  //could also be "mono"
	if (!url.empty() && obj != NULL)  
	{
		const std::string fName = prepUploadFile();
		LLLiveLSLEditor::uploadAssetViaCapsStatic(url, fName, 
			obj->getID(), inv_item, isMono, true);
		llinfos << "updating script ID for bridge" << llendl;
		FSLSLBridge::instance().mScriptItemID = inv_item;
	}
	else
	{
		//can't complete bridge creation - detach and remove object, remove script
		//try to clean up and go away. Fail.
		LLVOAvatarSelf::detachAttachmentIntoInventory(FSLSLBridge::instance().getBridge()->getUUID());
		FSLSLBridge::instance().cleanUpBridge();
		//also clean up script remains
		gInventory.purgeObject(item->getUUID());
		gInventory.notifyObservers();
		return;
	}
}

std::string FSLSLBridgeScriptCallback::prepUploadFile()
{
	std::string fName = gDirUtilp->getExpandedFilename(LL_PATH_FS_RESOURCES, UPLOAD_SCRIPT_CURRENT);
	std::string fNew = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,UPLOAD_SCRIPT_CURRENT);

	//open script text file
	typedef std::istream_iterator<char> istream_iterator;
	std::ifstream file(fName.c_str());

	typedef std::ostream_iterator<char> ostream_iterator;
	std::ofstream tempFile(fNew.c_str());
	
	file >> std::noskipws;
	std::copy(istream_iterator(file), istream_iterator(), ostream_iterator(tempFile));

	return fNew;
}

void FSLSLBridge :: checkBridgeScriptName(std::string fileName)
{
	if ((fileName.length() == 0) || !mBridgeCreating)
		return;
	//need to parse out the last length of a GUID and compare to saved possible names.
	std::string fileOnly = fileName.substr(fileName.length()-UPLOAD_SCRIPT_CURRENT.length(), UPLOAD_SCRIPT_CURRENT.length());

	if (fileOnly == UPLOAD_SCRIPT_CURRENT)
	{
		//this is our script upload
		LLViewerObject* obj = gAgentAvatarp->getWornAttachment(mpBridge->getUUID());
		if (obj == NULL)
		{
			//something happened to our object. Try to fail gracefully.
			cleanUpBridge();
			return;
		}
		//registerVOInventoryListener(obj, NULL);
		obj->saveScript(gInventory.getItem(mScriptItemID), TRUE, false);
		FSLSLBridgeCleanupTimer *objTimer = new FSLSLBridgeCleanupTimer((F32)1.0);
		objTimer->startTimer();
		//obj->doInventoryCallback();
		//requestVOInventory();
	}
}

BOOL FSLSLBridgeCleanupTimer::tick()
{
	FSLSLBridge::instance().finishBridge();
	stopTimer();
	return TRUE;
}

void FSLSLBridge :: cleanUpBridge()
{
	//something unexpected went wrong. Try to clean up and not crash.
	reportToNearbyChat("Bridge object not found. Can't proceed with creation, exiting.");
	gInventory.purgeObject(mpBridge->getUUID());
	gInventory.notifyObservers();
	mpBridge = NULL;
	mBridgeCreating = false;
}

void FSLSLBridge :: finishBridge()
{
	//announce yourself
	reportToNearbyChat("Bridge created.");

	mBridgeCreating = false;
	//removeVOInventoryListener();
	cleanUpOldVersions();
	cleanUpBridgeFolder();
}
//
// Helper functions
///
bool FSLSLBridge :: isItemAttached(LLUUID iID)
{
	return (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(iID));
}

LLUUID FSLSLBridge :: findFSCategory()
{
	if (!mBridgeFolderID.isNull())
		return mBridgeFolderID;

	LLUUID fsCatID;
	LLUUID bridgeCatID;

	fsCatID = gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);
	if(!fsCatID.isNull())
	{
		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(fsCatID, cats, items);
		if(cats)
		{
			S32 count = cats->count();
			for(S32 i = 0; i < count; ++i)
			{
				if(cats->get(i)->getName() == FS_BRIDGE_FOLDER)
				{
					bridgeCatID = cats->get(i)->getUUID();
				}
			}
		}
	}
	else
	{
		fsCatID = gInventory.createNewCategory(gInventory.getRootFolderID(), LLFolderType::FT_NONE, ROOT_FIRESTORM_FOLDER);
	}

	if (bridgeCatID.isNull())
	{
		bridgeCatID = gInventory.createNewCategory(fsCatID, LLFolderType::FT_NONE, FS_BRIDGE_FOLDER);
	}

	mBridgeFolderID = bridgeCatID;

	return mBridgeFolderID;
}
LLViewerInventoryItem* FSLSLBridge :: findInvObject(std::string obj_name, LLUUID catID, LLAssetType::EType type)
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;

	//gInventory.findCategoryByName
	LLUUID itemID;
	NameCollectFunctor namefunctor(obj_name);

	gInventory.collectDescendentsIf(catID,cats,items,FALSE,namefunctor);

	for (S32 iIndex = 0; iIndex < items.count(); iIndex++)
	{
		const LLViewerInventoryItem* itemp = items.get(iIndex);
		if (!itemp->getIsLinkType()  && (itemp->getType() == LLAssetType::AT_OBJECT))
		{
			itemID = itemp->getUUID();
			break;
		}
	}

	if (itemID.notNull())
	{
		LLViewerInventoryItem* item = gInventory.getItem(itemID);
		return item;
	}
	return NULL;
}

void FSLSLBridge :: reportToNearbyChat(std::string message)
// AO small utility method for chat alerts.
{	
	LLChat chat;
    chat.mText = message;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLSD args;
	args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
	LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
}

void FSLSLBridge :: cleanUpBridgeFolder(std::string nameToCleanUp)
{
	llinfos << "Cleaning leftover scripts and bridges... for " << nameToCleanUp << llendl;
	
	LLUUID catID = findFSCategory();
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;

	//find all bridge and script duplicates and delete them
	//NameCollectFunctor namefunctor(mCurrentFullName);
	NameCollectFunctor namefunctor(nameToCleanUp);
	gInventory.collectDescendentsIf(catID,cats,items,FALSE,namefunctor);

	for (S32 iIndex = 0; iIndex < items.count(); iIndex++)
	{
		const LLViewerInventoryItem* itemp = items.get(iIndex);
		if (get_is_item_worn(itemp->getUUID()))
		{
			LLVOAvatarSelf::detachAttachmentIntoInventory(itemp->getUUID());
		}

		if (!itemp->getIsLinkType()  && (itemp->getUUID() != mpBridge->getUUID()))
		{
			gInventory.purgeObject(itemp->getUUID());
		}
	}
	gInventory.notifyObservers();
}

void FSLSLBridge :: cleanUpBridgeFolder()
{
	cleanUpBridgeFolder(mCurrentFullName);

}

void FSLSLBridge :: cleanUpOldVersions()
{
	std::string mProcessingName;

	for(int i = 1; i <= FS_BRIDGE_MAJOR_VERSION; i++)
	{
		for (int j = 1; j < FS_BRIDGE_MINOR_VERSION; j++)
		{
			std::stringstream sstr;
	
			sstr << FS_BRIDGE_NAME;
			sstr << i;
			sstr << ".";
			sstr << j;

			mProcessingName = sstr.str();
			cleanUpBridgeFolder(mProcessingName);
		}
	}
}

bool FSLSLBridge :: isOldBridgeVersion(LLInventoryItem *item)
{
	//if (!item)
	//	return false;
	////if (!boost::regex_match(item->getName(), FSBridgePattern))

	//std::string str = item->getName();

	//(item) && boost::regex_match(item->getName(), FSBridgePattern);

	//std::string tmpl = FS_BRIDGE_NAME;

	//std::string::size_type found = str.find_first_of(".");
 //
	//while( found != std::string::npos ) {}

	////std::string sMajor = str.substr(strlen(tmpl.c_str)-1, dotPos);
	////std::string sMinor = str.substr(strlen(tmpl.c_str)+strlen(sMajor));

	////int iMajor = atoi(sMajor);
	////float fMinor = atof(sMinor);

	return false;
}

