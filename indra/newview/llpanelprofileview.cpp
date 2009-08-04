/** 
* @file llpanelprofileview.cpp
* @brief Side tray "Profile View" panel
*
* $LicenseInfo:firstyear=2009&license=viewergpl$
* 
* Copyright (c) 2009, Linden Research, Inc.
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
#include "llpanelprofileview.h"

#include "llpanelavatar.h"
#include "llpanelpicks.h"
#include "llsidetraypanelcontainer.h"
#include "llpanelprofile.h"

static LLRegisterPanelClassWrapper<LLPanelProfileView> t_panel_target_profile("panel_profile_view");
static LLRegisterPanelClassWrapper<LLPanelAvatarNotes> t_panel_notes("panel_notes");

static std::string PANEL_PROFILE = "panel_profile";
static std::string PANEL_PICKS = "panel_picks";
static std::string PANEL_NOTES = "panel_notes";

LLPanelProfileView::LLPanelProfileView()
:	LLPanelProfile()
{
}

LLPanelProfileView::~LLPanelProfileView(void)
{
}

/*virtual*/ 
void LLPanelProfileView::onOpen(const LLSD& key)
{
	LLUUID id = key["id"];
	if (key.has("open_tab_name"))
		mTabContainer->selectTabByName(key["open_tab_name"]);

	if(id.notNull() && mAvatarId != id)
	{
		mAvatarId = id;

		mTabs[PANEL_PROFILE]->clear();
		mTabs[PANEL_PICKS]->clear();
		mTabs[PANEL_NOTES]->clear();
	}

	mTabContainer->getCurrentPanel()->onOpen(mAvatarId);

	std::string full_name;
	gCacheName->getFullName(mAvatarId,full_name);
	childSetValue("user_name",full_name);
}


BOOL LLPanelProfileView::postBuild()
{
	LLPanelProfile::postBuild();

	mTabs[PANEL_NOTES] = (getChild<LLPanelAvatarNotes>(PANEL_NOTES));

	childSetCommitCallback("back",boost::bind(&LLPanelProfileView::onBackBtnClick,this),NULL);
	
	return TRUE;
}


//private

void LLPanelProfileView::onBackBtnClick()
{
	LLSideTrayPanelContainer* parent = dynamic_cast<LLSideTrayPanelContainer*>(getParent());
	if(parent)
	{
		parent->openPreviousPanel();
	}
}
