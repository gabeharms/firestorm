/**
 * @file llpanelplaceinfo.cpp
 * @brief Base class for place information in Side Tray.
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
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

#include "llviewerprecompiledheaders.h"

#include "llpanelplaceinfo.h"

#include "roles_constants.h"
#include "llsdutil.h"
#include "llsecondlifeurls.h"

#include "llinventory.h"

#include "llsdutil_math.h"

#include "llscrollcontainer.h"
#include "lltextbox.h"

#include "llagent.h"
#include "llavatarpropertiesprocessor.h"
#include "llexpandabletextbox.h"
#include "llfloaterworldmap.h"
#include "llinventorymodel.h"
#include "llpanelpick.h"
#include "lltexturectrl.h"
#include "llviewerinventory.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llworldmap.h"

LLPanelPlaceInfo::LLPanelPlaceInfo()
:	LLPanel(),
	mParcelID(),
	mRequestedID(),
	mPosRegion(),
	mMinHeight(0)
{}

//virtual
LLPanelPlaceInfo::~LLPanelPlaceInfo()
{
	if (mParcelID.notNull())
	{
		LLRemoteParcelInfoProcessor::getInstance()->removeObserver(mParcelID, this);
	}
}

//virtual
BOOL LLPanelPlaceInfo::postBuild()
{
	mTitle = getChild<LLTextBox>("title");
	mCurrentTitle = mTitle->getText();

	mSnapshotCtrl = getChild<LLTextureCtrl>("logo");
	mRegionName = getChild<LLTextBox>("region_title");
	mParcelName = getChild<LLTextBox>("parcel_title");
	mDescEditor = getChild<LLExpandableTextBox>("description");

	mMaturityRatingText = getChild<LLTextBox>("maturity_value");

	LLScrollContainer* scroll_container = getChild<LLScrollContainer>("place_scroll");
	scroll_container->setBorderVisible(FALSE);
	mMinHeight = scroll_container->getScrolledViewRect().getHeight();

	return TRUE;
}

//virtual
void LLPanelPlaceInfo::resetLocation()
{
	mParcelID.setNull();
	mRequestedID.setNull();
	mPosRegion.clearVec();

	std::string not_available = getString("not_available");
	mMaturityRatingText->setValue(not_available);
	mRegionName->setText(not_available);
	mParcelName->setText(not_available);
	mDescEditor->setText(not_available);

	mSnapshotCtrl->setImageAssetID(LLUUID::null);
	mSnapshotCtrl->setFallbackImageName("default_land_picture.j2c");
}

//virtual
void LLPanelPlaceInfo::setParcelID(const LLUUID& parcel_id)
{
	mParcelID = parcel_id;
	sendParcelInfoRequest();
}

//virtual
void LLPanelPlaceInfo::setInfoType(INFO_TYPE type)
{
	mTitle->setText(mCurrentTitle);

	mInfoType = type;
}

void LLPanelPlaceInfo::sendParcelInfoRequest()
{
	if (mParcelID != mRequestedID)
	{
		LLRemoteParcelInfoProcessor::getInstance()->addObserver(mParcelID, this);
		LLRemoteParcelInfoProcessor::getInstance()->sendParcelInfoRequest(mParcelID);

		mRequestedID = mParcelID;
	}
}

void LLPanelPlaceInfo::displayParcelInfo(const LLUUID& region_id,
										 const LLVector3d& pos_global)
{
	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
		return;

	mPosRegion.setVec((F32)fmod(pos_global.mdV[VX], (F64)REGION_WIDTH_METERS),
					  (F32)fmod(pos_global.mdV[VY], (F64)REGION_WIDTH_METERS),
					  (F32)pos_global.mdV[VZ]);

	LLSD body;
	std::string url = region->getCapability("RemoteParcelRequest");
	if (!url.empty())
	{
		body["location"] = ll_sd_from_vector3(mPosRegion);
		if (!region_id.isNull())
		{
			body["region_id"] = region_id;
		}
		if (!pos_global.isExactlyZero())
		{
			U64 region_handle = to_region_handle(pos_global);
			body["region_handle"] = ll_sd_from_U64(region_handle);
		}
		LLHTTPClient::post(url, body, new LLRemoteParcelRequestResponder(getObserverHandle()));
	}
	else
	{
		mDescEditor->setText(getString("server_update_text"));
	}
}

// virtual
void LLPanelPlaceInfo::setErrorStatus(U32 status, const std::string& reason)
{
	// We only really handle 404 and 499 errors
	std::string error_text;
	if(status == 404)
	{
		error_text = getString("server_error_text");
	}
	else if(status == 499)
	{
		error_text = getString("server_forbidden_text");
	}
	mDescEditor->setText(error_text);
}

// virtual
void LLPanelPlaceInfo::processParcelInfo(const LLParcelData& parcel_data)
{
	if(parcel_data.snapshot_id.notNull())
	{
		mSnapshotCtrl->setImageAssetID(parcel_data.snapshot_id);
	}

	if(!parcel_data.sim_name.empty())
	{
		mRegionName->setText(parcel_data.sim_name);
	}
	else
	{
		mRegionName->setText(LLStringUtil::null);
	}

	if(!parcel_data.desc.empty())
	{
		mDescEditor->setText(parcel_data.desc);
	}

	S32 region_x;
	S32 region_y;
	S32 region_z;

	// If the region position is zero, grab position from the global
	if(mPosRegion.isExactlyZero())
	{
		region_x = llround(parcel_data.global_x) % REGION_WIDTH_UNITS;
		region_y = llround(parcel_data.global_y) % REGION_WIDTH_UNITS;
		region_z = llround(parcel_data.global_z);
	}
	else
	{
		region_x = llround(mPosRegion.mV[VX]);
		region_y = llround(mPosRegion.mV[VY]);
		region_z = llround(mPosRegion.mV[VZ]);
	}

	if (!parcel_data.name.empty())
	{
		mParcelName->setText(llformat("%s (%d, %d, %d)",
							 parcel_data.name.c_str(), region_x, region_y, region_z));
	}
	else
	{
		mParcelName->setText(getString("not_available"));
	}
}

// virtual
void LLPanelPlaceInfo::handleVisibilityChange(BOOL new_visibility)
{
	LLPanel::handleVisibilityChange(new_visibility);

	LLViewerParcelMgr* parcel_mgr = LLViewerParcelMgr::getInstance();
	if (!parcel_mgr)
		return;

	// Remove land selection when panel hides.
	if (!new_visibility)
	{
		if (!parcel_mgr->selectionEmpty())
		{
			parcel_mgr->deselectLand();
		}
	}
}

void LLPanelPlaceInfo::createPick(const LLVector3d& pos_global, LLPanelPickEdit* pick_panel)
{
	std::string name = mParcelName->getText();
	if (name.empty())
	{
		name = mRegionName->getText();
	}

	LLPickData data;
	data.pos_global = pos_global;
	data.name = name;
	data.desc = mDescEditor->getText();
	data.snapshot_id = mSnapshotCtrl->getImageAssetID();
	data.parcel_id = mParcelID;
	pick_panel->setPickData(&data);
}

// static
void LLPanelPlaceInfo::nameUpdatedCallback(LLTextBox* text,
										   const std::string& first,
										   const std::string& last)
{
	text->setText(first + " " + last);
}
