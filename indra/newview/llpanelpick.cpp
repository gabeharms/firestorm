/** 
 * @file llpanelpick.cpp
 * @brief LLPanelPick class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

// Display of a "Top Pick" used both for the global top picks in the 
// Find directory, and also for each individual user's picks in their
// profile.

#include "llviewerprecompiledheaders.h"
#include "llpanel.h"
#include "message.h"
#include "llagent.h"
#include "llagentpicksinfo.h"
#include "llbutton.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lluiconstants.h"
#include "llviewerregion.h"
#include "llworldmap.h"
#include "llfloaterworldmap.h"
#include "llfloaterreg.h"
#include "llavatarpropertiesprocessor.h"
#include "llpanelpick.h"


#define XML_PANEL_EDIT_PICK "panel_edit_pick.xml"
#define XML_PANEL_PICK_INFO "panel_pick_info.xml"

#define XML_NAME		"pick_name"
#define XML_DESC		"pick_desc"
#define XML_SNAPSHOT	"pick_snapshot"
#define XML_LOCATION	"pick_location"

#define XML_BTN_ON_TXTR "edit_icon"
#define XML_BTN_SAVE "save_changes_btn"

#define SAVE_BTN_LABEL "[WHAT]"
#define LABEL_PICK = "Pick"
#define LABEL_CHANGES = "Changes"

std::string SET_LOCATION_NOTICE("(will update after save)");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static
LLPanelPickInfo* LLPanelPickInfo::create()
{
	LLPanelPickInfo* panel = new LLPanelPickInfo();
	LLUICtrlFactory::getInstance()->buildPanel(panel, XML_PANEL_PICK_INFO);
	return panel;
}

LLPanelPickInfo::LLPanelPickInfo()
 : LLPanel()
 , LLAvatarPropertiesObserver()
 , LLRemoteParcelInfoObserver()
 , mAvatarId(LLUUID::null)
 , mSnapshotCtrl(NULL)
 , mPickId(LLUUID::null)
{
}

LLPanelPickInfo::~LLPanelPickInfo()
{
	LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(), this);
}

void LLPanelPickInfo::onOpen(const LLSD& key)
{
	LLUUID avatar_id = key["avatar_id"];
	if(avatar_id.isNull())
	{
		return;
	}

	if(getAvatarId().notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(
			getAvatarId(), this);
	}

	setAvatarId(avatar_id);

	resetData();
	resetControls();

	setPickId(key["pick_id"]);
	setPickName(key["pick_name"]);
	setPickDesc(key["pick_desc"]);
	setSnapshotId(key["snapshot_id"]);

	LLAvatarPropertiesProcessor::getInstance()->addObserver(
		getAvatarId(), this);
	LLAvatarPropertiesProcessor::getInstance()->sendPickInfoRequest(
		getAvatarId(), getPickId());
}

BOOL LLPanelPickInfo::postBuild()
{
	mSnapshotCtrl = getChild<LLTextureCtrl>(XML_SNAPSHOT);

	childSetLabelArg(XML_BTN_SAVE, SAVE_BTN_LABEL, std::string("Pick"));

	childSetAction("teleport_btn", boost::bind(&LLPanelPickInfo::onClickTeleport, this));
	childSetAction("show_on_map_btn", boost::bind(&LLPanelPickInfo::onClickMap, this));
	childSetAction("back_btn", boost::bind(&LLPanelPickInfo::onClickBack, this));

	return TRUE;
}

void LLPanelPickInfo::processProperties(void* data, EAvatarProcessorType type)
{
	if(APT_PICK_INFO != type)
	{
		return;
	}
	LLPickData* pick_info = static_cast<LLPickData*>(data);
	if(!pick_info 
		|| pick_info->creator_id != getAvatarId() 
		|| pick_info->pick_id != getPickId())
	{
		return;
	}

	setSnapshotId(pick_info->snapshot_id);
	setPickName(pick_info->name);
	setPickDesc(pick_info->desc);
	setPosGlobal(pick_info->pos_global);
	setPickLocation(createLocationText(pick_info->user_name, pick_info->original_name, 
		pick_info->sim_name, pick_info->pos_global));

	// *NOTE dzaporozhan
	// We want to keep listening to APT_PICK_INFO because user may 
	// edit the Pick and we have to update Pick info panel.
	// revomeObserver is called from onClickBack
}

void LLPanelPickInfo::setExitCallback(const commit_callback_t& cb)
{
	getChild<LLButton>("back_btn")->setClickedCallback(cb);
}

void LLPanelPickInfo::processParcelInfo(const LLParcelData& parcel_data)
{
	// HACK: Flag 0x2 == adult region,
	// Flag 0x1 == mature region, otherwise assume PG
	std::string rating_icon = "icon_event.tga";
	if (parcel_data.flags & 0x2)
	{
		rating_icon = "icon_event_adult.tga";
	}
	else if (parcel_data.flags & 0x1)
	{
		rating_icon = "icon_event_mature.tga";
	}

	childSetValue("maturity", rating_icon);

	//*NOTE we don't removeObserver(...) ourselves cause LLRemoveParcelProcessor does it for us
}

void LLPanelPickInfo::setEditPickCallback(const commit_callback_t& cb)
{
	getChild<LLButton>("edit_btn")->setClickedCallback(cb);
}

// PROTECTED AREA

void LLPanelPickInfo::resetControls()
{
	if(getAvatarId() == gAgent.getID())
	{
		childSetEnabled("edit_btn", TRUE);
		childSetVisible("edit_btn", TRUE);
	}
	else
	{
		childSetEnabled("edit_btn", FALSE);
		childSetVisible("edit_btn", FALSE);
	}
}

void LLPanelPickInfo::resetData()
{
	setPickName(LLStringUtil::null);
	setPickDesc(LLStringUtil::null);
	setPickLocation(LLStringUtil::null);
	setPickId(LLUUID::null);
	setSnapshotId(LLUUID::null);
	mPosGlobal.clearVec();
	childSetValue("maturity", LLStringUtil::null);
}

// static
std::string LLPanelPickInfo::createLocationText(const std::string& owner_name, const std::string& original_name, const std::string& sim_name, const LLVector3d& pos_global)
{
	std::string location_text;
	location_text.append(owner_name);
	if (!original_name.empty())
	{
		if (!location_text.empty()) location_text.append(", ");
		location_text.append(original_name);

	}
	if (!sim_name.empty())
	{
		if (!location_text.empty()) location_text.append(", ");
		location_text.append(sim_name);
	}

	if (!location_text.empty()) location_text.append(" ");

	if (!pos_global.isNull())
	{
		S32 region_x = llround((F32)pos_global.mdV[VX]) % REGION_WIDTH_UNITS;
		S32 region_y = llround((F32)pos_global.mdV[VY]) % REGION_WIDTH_UNITS;
		S32 region_z = llround((F32)pos_global.mdV[VZ]);
		location_text.append(llformat(" (%d, %d, %d)", region_x, region_y, region_z));
	}
	return location_text;
}

void LLPanelPickInfo::setSnapshotId(const LLUUID& id) 
{ 
	mSnapshotCtrl->setImageAssetID(id);
	mSnapshotCtrl->setValid(TRUE);
}

void LLPanelPickInfo::setPickName(const std::string& name)
{
	childSetValue(XML_NAME, name);
}

void LLPanelPickInfo::setPickDesc(const std::string& desc)
{
	childSetValue(XML_DESC, desc);
}

void LLPanelPickInfo::setPickLocation(const std::string& location)
{
	childSetValue(XML_LOCATION, location);

	//preserving non-wrapped text for info/edit modes switching
	mLocation = location;
}

void LLPanelPickInfo::onClickMap()
{
	LLFloaterWorldMap::getInstance()->trackLocation(getPosGlobal());
	LLFloaterReg::showInstance("world_map", "center");
}

void LLPanelPickInfo::onClickTeleport()
{
	if (!getPosGlobal().isExactlyZero())
	{
		gAgent.teleportViaLocation(getPosGlobal());
		LLFloaterWorldMap::getInstance()->trackLocation(getPosGlobal());
	}
}

void LLPanelPickInfo::onClickBack()
{
	LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(), this);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static
LLPanelPickEdit* LLPanelPickEdit::create()
{
	LLPanelPickEdit* panel = new LLPanelPickEdit();
	LLUICtrlFactory::getInstance()->buildPanel(panel, XML_PANEL_EDIT_PICK);
	return panel;
}

LLPanelPickEdit::LLPanelPickEdit()
 : LLPanelPickInfo()
 , mLocationChanged(false)
 , mNeedData(true)
 , mNewPick(false)
{
}

LLPanelPickEdit::~LLPanelPickEdit()
{
}

void LLPanelPickEdit::onOpen(const LLSD& key)
{
	LLUUID pick_id = key["pick_id"];
	mNeedData = true;

	// creating new Pick
	if(pick_id.isNull())
	{
		mNewPick = true;

		setAvatarId(gAgent.getID());

		resetData();
		resetControls();

		setPosGlobal(gAgent.getPositionGlobal());

		LLUUID parcel_id = LLUUID::null, snapshot_id = LLUUID::null;
		std::string pick_name, pick_desc;

		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if(parcel)
		{
			parcel_id = parcel->getID();
			pick_name = parcel->getName();
			pick_desc = parcel->getDesc();
			snapshot_id = parcel->getSnapshotID();
		}

		if(pick_name.empty())
		{
			LLViewerRegion* region = gAgent.getRegion();
			if(region)
			{
				pick_name = region->getName();
			}
		}

		setParcelID(parcel_id);
		childSetValue("pick_name", pick_name);
		childSetValue("pick_desc", pick_desc);
		setSnapshotId(snapshot_id);
		setPickLocation(createLocationText(LLStringUtil::null, SET_LOCATION_NOTICE, 
			pick_name, getPosGlobal()));

		enableSaveButton(true);
	}
	// editing existing pick
	else
	{
		mNewPick = false;
		LLPanelPickInfo::onOpen(key);

		enableSaveButton(false);
	}

	resetDirty();
}

void LLPanelPickEdit::setPickData(const LLPickData* pick_data)
{
	if(!pick_data)
	{
		return;
	}

	mNeedData = false;

	setParcelID(pick_data->parcel_id);
	childSetValue("pick_name", pick_data->name);
	childSetValue("pick_desc", pick_data->desc);
	setSnapshotId(pick_data->snapshot_id);
	setPickLocation(createLocationText(pick_data->user_name, pick_data->original_name, /*pick_data->sim_name,*/ 
		pick_data->name, pick_data->pos_global));
}

BOOL LLPanelPickEdit::postBuild()
{
	LLPanelPickInfo::postBuild();

	mSnapshotCtrl->setOnSelectCallback(boost::bind(&LLPanelPickEdit::onPickChanged, this, _1));

	LLLineEditor* line_edit = getChild<LLLineEditor>("pick_name");
	line_edit->setKeystrokeCallback(boost::bind(&LLPanelPickEdit::onPickChanged, this, _1), NULL);

	LLTextEditor* text_edit = getChild<LLTextEditor>("pick_desc");
	text_edit->setKeystrokeCallback(boost::bind(&LLPanelPickEdit::onPickChanged, this, _1));

	childSetAction(XML_BTN_SAVE, boost::bind(&LLPanelPickEdit::onClickSave, this));
	childSetAction("set_to_curr_location_btn", boost::bind(&LLPanelPickEdit::onClickSetLocation, this));

	initTexturePickerMouseEvents();

	return TRUE;
}

void LLPanelPickEdit::setSaveCallback(const commit_callback_t& cb)
{
	getChild<LLButton>("save_changes_btn")->setClickedCallback(cb);
}

void LLPanelPickEdit::setCancelCallback(const commit_callback_t& cb)
{
	getChild<LLButton>("cancel_btn")->setClickedCallback(cb);
}

void LLPanelPickEdit::resetDirty()
{
	LLPanelPickInfo::resetDirty();

	getChild<LLLineEditor>("pick_name")->resetDirty();
	getChild<LLTextEditor>("pick_desc")->resetDirty();
	mSnapshotCtrl->resetDirty();
	mLocationChanged = false;
}

BOOL LLPanelPickEdit::isDirty() const
{
	if( mNewPick
		|| LLPanelPickInfo::isDirty()
		|| mLocationChanged
		|| mSnapshotCtrl->isDirty()
		|| getChild<LLLineEditor>("pick_name")->isDirty()
		|| getChild<LLTextEditor>("pick_desc")->isDirty())
	{
		return TRUE;
	}
	return FALSE;
}

// PROTECTED AREA

void LLPanelPickEdit::sendUpdate()
{
	LLPickData pick_data;

	// If we don't have a pick id yet, we'll need to generate one,
	// otherwise we'll keep overwriting pick_id 00000 in the database.
	if (getPickId().isNull()) 
	{
		getPickId().generate();
	}

	pick_data.agent_id = gAgent.getID();
	pick_data.session_id = gAgent.getSessionID();
	pick_data.pick_id = getPickId();
	pick_data.creator_id = gAgent.getID();;

	//legacy var  need to be deleted
	pick_data.top_pick = FALSE; 
	pick_data.parcel_id = mParcelId;
	pick_data.name = childGetValue(XML_NAME).asString();
	pick_data.desc = childGetValue(XML_DESC).asString();
	pick_data.snapshot_id = mSnapshotCtrl->getImageAssetID();
	pick_data.pos_global = getPosGlobal();
	pick_data.sort_order = 0;
	pick_data.enabled = TRUE;

	LLAvatarPropertiesProcessor::instance().sendPickInfoUpdate(&pick_data);

	if(mNewPick)
	{
		// Assume a successful create pick operation, make new number of picks
		// available immediately. Actual number of picks will be requested in 
		// LLAvatarPropertiesProcessor::sendPickInfoUpdate and updated upon server respond.
		LLAgentPicksInfo::getInstance()->incrementNumberOfPicks();
	}
}

void LLPanelPickEdit::onPickChanged(LLUICtrl* ctrl)
{
	if(isDirty())
	{
		enableSaveButton(true);
	}
	else
	{
		enableSaveButton(false);
	}
}

void LLPanelPickEdit::resetData()
{
	LLPanelPickInfo::resetData();
	mLocationChanged = false;
}

void LLPanelPickEdit::enableSaveButton(bool enable)
{
	childSetEnabled(XML_BTN_SAVE, enable);
}

void LLPanelPickEdit::onClickSetLocation()
{
	// Save location for later use.
	setPosGlobal(gAgent.getPositionGlobal());

	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (parcel)
	{
		mParcelId = parcel->getID();
		mSimName = parcel->getName();
	}
	setPickLocation(createLocationText(
		LLStringUtil::null, SET_LOCATION_NOTICE, mSimName, getPosGlobal()));

	mLocationChanged = true;
	enableSaveButton(TRUE);
}

void LLPanelPickEdit::onClickSave()
{
	sendUpdate();

	mLocationChanged = false;

	LLSD params;
	params["action"] = "save_new_pick";
	notifyParent(params);
}

void LLPanelPickEdit::processProperties(void* data, EAvatarProcessorType type)
{
	if(mNeedData)
	{
		LLPanelPickInfo::processProperties(data, type);
	}
}

// PRIVATE AREA

void LLPanelPickEdit::initTexturePickerMouseEvents()
{
	text_icon = getChild<LLIconCtrl>(XML_BTN_ON_TXTR);
	mSnapshotCtrl->setMouseEnterCallback(boost::bind(&LLPanelPickEdit::onTexturePickerMouseEnter, this, _1));
	mSnapshotCtrl->setMouseLeaveCallback(boost::bind(&LLPanelPickEdit::onTexturePickerMouseLeave, this, _1));
	
	// *WORKAROUND: Needed for EXT-1625: enabling save button each time when picker is opened, even if 
	// texture wasn't changed (see Steve's comment).
	mSnapshotCtrl->setMouseDownCallback(boost::bind(&LLPanelPickEdit::enableSaveButton, this, true));
	
	text_icon->setVisible(FALSE);
}
		
void LLPanelPickEdit::onTexturePickerMouseEnter(LLUICtrl* ctrl)
{
        text_icon->setVisible(TRUE);
}

void LLPanelPickEdit::onTexturePickerMouseLeave(LLUICtrl* ctrl)
{
	text_icon->setVisible(FALSE);
}
