/** 
 * @file llfloatermediasettings.cpp
 * @brief Tabbed dialog for media settings - class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
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

#include "llfloaterreg.h"
#include "llfloatermediasettings.h"
#include "llpanelmediasettingsgeneral.h"
#include "llpanelmediasettingssecurity.h"
#include "llpanelmediasettingspermissions.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llbutton.h"
#include "llselectmgr.h"
#include "llsdutil.h"

LLFloaterMediaSettings* LLFloaterMediaSettings::sInstance = NULL;

////////////////////////////////////////////////////////////////////////////////
// 
LLFloaterMediaSettings::LLFloaterMediaSettings(const LLSD& key)
	: LLFloater(key),
	mTabContainer(NULL),
	mPanelMediaSettingsGeneral(NULL),
	mPanelMediaSettingsSecurity(NULL),
	mPanelMediaSettingsPermissions(NULL),
	mWaitingToClose( false ),
	mIdenticalHasMediaInfo( true ),
	mMultipleMedia(false),
	mMultipleValidMedia(false)
{
//	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_media_settings.xml");
}

////////////////////////////////////////////////////////////////////////////////
//
LLFloaterMediaSettings::~LLFloaterMediaSettings()
{
	if ( mPanelMediaSettingsGeneral )
	{
		delete mPanelMediaSettingsGeneral;
		mPanelMediaSettingsGeneral = NULL;
	}

	if ( mPanelMediaSettingsSecurity )
	{
		delete mPanelMediaSettingsSecurity;
		mPanelMediaSettingsSecurity = NULL;
	}

	if ( mPanelMediaSettingsPermissions )
	{
		delete mPanelMediaSettingsPermissions;
		mPanelMediaSettingsPermissions = NULL;
	}

	sInstance = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLFloaterMediaSettings::postBuild()
{
	mApplyBtn = getChild<LLButton>("Apply");
	mApplyBtn->setClickedCallback(onBtnApply, this);
		
	mCancelBtn = getChild<LLButton>("Cancel");
	mCancelBtn->setClickedCallback(onBtnCancel, this);

	mOKBtn = getChild<LLButton>("OK");
	mOKBtn->setClickedCallback(onBtnOK, this);
			
	mTabContainer = getChild<LLTabContainer>( "tab_container" );
	
	mPanelMediaSettingsGeneral = new LLPanelMediaSettingsGeneral();
	mTabContainer->addTabPanel( 
			LLTabContainer::TabPanelParams().
			panel(mPanelMediaSettingsGeneral));
	mPanelMediaSettingsGeneral->setParent( this );

	// note that "permissions" tab is really "Controls" tab - refs to 'perms' and
	// 'permissions' not changed to 'controls' since we don't want to change 
	// shared files in server code and keeping everything the same seemed best.
	mPanelMediaSettingsPermissions = new LLPanelMediaSettingsPermissions();
	mTabContainer->addTabPanel( 
			LLTabContainer::TabPanelParams().
			panel(mPanelMediaSettingsPermissions));

	mPanelMediaSettingsSecurity = new LLPanelMediaSettingsSecurity();
	mTabContainer->addTabPanel( 
			LLTabContainer::TabPanelParams().
			panel(mPanelMediaSettingsSecurity));
	mPanelMediaSettingsSecurity->setParent( this );
		
	// restore the last tab viewed from persistance variable storage
	if (!mTabContainer->selectTab(gSavedSettings.getS32("LastMediaSettingsTab")))
	{
		mTabContainer->selectFirstTab();
	};

	sInstance = this;

	return TRUE;
}

//static 
LLFloaterMediaSettings* LLFloaterMediaSettings::getInstance()
{
	if ( !sInstance )
	{
		sInstance = (LLFloaterReg::getTypedInstance<LLFloaterMediaSettings>("media_settings"));
	}
	
	return sInstance;
}

//static 
void LLFloaterMediaSettings::apply()
{
	LLSD settings;
	sInstance->mPanelMediaSettingsGeneral->preApply();
	sInstance->mPanelMediaSettingsGeneral->getValues( settings );
	sInstance->mPanelMediaSettingsSecurity->preApply();
	sInstance->mPanelMediaSettingsSecurity->getValues( settings );
	sInstance->mPanelMediaSettingsPermissions->preApply();
	sInstance->mPanelMediaSettingsPermissions->getValues( settings );
	LLSelectMgr::getInstance()->selectionSetMedia( LLTextureEntry::MF_HAS_MEDIA );
	LLSelectMgr::getInstance()->selectionSetMediaData(settings);
	sInstance->mPanelMediaSettingsGeneral->postApply();
	sInstance->mPanelMediaSettingsSecurity->postApply();
	sInstance->mPanelMediaSettingsPermissions->postApply();
}

////////////////////////////////////////////////////////////////////////////////
void LLFloaterMediaSettings::onClose(bool app_quitting)
{
	if(mPanelMediaSettingsGeneral)
	{
		mPanelMediaSettingsGeneral->onClose(app_quitting);
	}
	LLFloaterReg::hideInstance("whitelist_entry");
}

////////////////////////////////////////////////////////////////////////////////
//static 
void LLFloaterMediaSettings::initValues( const LLSD& media_settings, bool editable )
{
	sInstance->clearValues(editable);
	// update all panels with values from simulator
	sInstance->mPanelMediaSettingsGeneral->
		initValues( sInstance->mPanelMediaSettingsGeneral, media_settings, editable );

	sInstance->mPanelMediaSettingsSecurity->
		initValues( sInstance->mPanelMediaSettingsSecurity, media_settings, editable );

	sInstance->mPanelMediaSettingsPermissions->
		initValues( sInstance->mPanelMediaSettingsPermissions, media_settings, editable );
	
	// Squirrel away initial values 
	sInstance->mInitialValues.clear();
	sInstance->mPanelMediaSettingsGeneral->getValues( sInstance->mInitialValues );
	sInstance->mPanelMediaSettingsSecurity->getValues( sInstance->mInitialValues );
	sInstance->mPanelMediaSettingsPermissions->getValues( sInstance->mInitialValues );
}

////////////////////////////////////////////////////////////////////////////////
// 
void LLFloaterMediaSettings::commitFields()
{
	if (hasFocus())
	{
		LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());
		if (cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
		};
	};
}

////////////////////////////////////////////////////////////////////////////////
//static 
void LLFloaterMediaSettings::clearValues( bool editable)
{
	// clean up all panels before updating
	sInstance->mPanelMediaSettingsGeneral	 ->clearValues(sInstance->mPanelMediaSettingsGeneral,  editable);
	sInstance->mPanelMediaSettingsSecurity	 ->clearValues(sInstance->mPanelMediaSettingsSecurity,	editable);
	sInstance->mPanelMediaSettingsPermissions->clearValues(sInstance->mPanelMediaSettingsPermissions,  editable);	
}


////////////////////////////////////////////////////////////////////////////////
// static
void LLFloaterMediaSettings::onBtnOK( void* userdata )
{
	sInstance->commitFields();

	sInstance->apply();

	sInstance->closeFloater();
}

////////////////////////////////////////////////////////////////////////////////
// static
void LLFloaterMediaSettings::onBtnApply( void* userdata )
{
	sInstance->commitFields();

	sInstance->apply();
}

////////////////////////////////////////////////////////////////////////////////
// static
void LLFloaterMediaSettings::onBtnCancel( void* userdata )
{
	sInstance->closeFloater(); 
}

////////////////////////////////////////////////////////////////////////////////
// static
void LLFloaterMediaSettings::onTabChanged(void* user_data, bool from_click)
{
	LLTabContainer* self = (LLTabContainer*)user_data;
	gSavedSettings.setS32("LastMediaSettingsTab", self->getCurrentPanelIndex());
}

////////////////////////////////////////////////////////////////////////////////
//
void LLFloaterMediaSettings::enableOkApplyBtns( bool enable )
{
	childSetEnabled( "OK", enable );
	childSetEnabled( "Apply", enable );
}

////////////////////////////////////////////////////////////////////////////////
//
const std::string LLFloaterMediaSettings::getHomeUrl()
{
	if ( mPanelMediaSettingsGeneral )
		return mPanelMediaSettingsGeneral->getHomeUrl();
	else
		return std::string( "" );
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLFloaterMediaSettings::passesWhiteList( const std::string& test_url )
{
	// sanity check - don't think this can happen
	if ( mPanelMediaSettingsSecurity )
		// version in security dialog code is specialized so we pass in 
		// empty string for first parameter since it's not used
		return mPanelMediaSettingsSecurity->passesWhiteList( "", test_url );
	else
		// this is all we can do
		return false;
}

////////////////////////////////////////////////////////////////////////////////
// virtual 
void LLFloaterMediaSettings::draw()
{
	// *NOTE: The code below is very inefficient.  Better to do this
	// only when data change.
	// Every frame, check to see what the values are.  If they are not
	// the same as the default media data, enable the OK/Apply buttons
	LLSD settings;
	sInstance->mPanelMediaSettingsGeneral->getValues( settings );
	sInstance->mPanelMediaSettingsSecurity->getValues( settings );
	sInstance->mPanelMediaSettingsPermissions->getValues( settings );

	bool values_changed = false;
	
	LLSD::map_const_iterator iter = settings.beginMap();
	LLSD::map_const_iterator end = settings.endMap();
	for ( ; iter != end; ++iter )
	{
		const std::string &current_key = iter->first;
		const LLSD &current_value = iter->second;
		if ( ! llsd_equals(current_value, mInitialValues[current_key]))
		{
			values_changed = true;
			break;
		}
	}
	
	enableOkApplyBtns(values_changed);
	
	LLFloater::draw();
}
