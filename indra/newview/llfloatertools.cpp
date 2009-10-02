/** 
 * @file llfloatertools.cpp
 * @brief The edit tools, including move, position, land, etc.
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

#include "llfloatertools.h"

#include "llfontgl.h"
#include "llcoord.h"
#include "llgl.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldraghandle.h"
#include "llfloaterbuildoptions.h"
#include "llfloatermediasettings.h"
#include "llfloateropenobject.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llmediaentry.h"
#include "llmenugl.h"
#include "llpanelcontents.h"
#include "llpanelface.h"
#include "llpanelland.h"
#include "llpanelinventory.h"
#include "llpanelobject.h"
#include "llpanelvolume.h"
#include "llpanelpermissions.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llslider.h"
#include "llstatusbar.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltoolbrush.h"
#include "lltoolcomp.h"
#include "lltooldraganddrop.h"
#include "lltoolface.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolgrab.h"
#include "lltoolindividual.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltoolpipette.h"
#include "lltoolplacer.h"
#include "lltoolselectland.h"
#include "llui.h"
#include "llviewermenu.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "llviewerjoystick.h"
#include "lluictrlfactory.h"

// Globals
LLFloaterTools *gFloaterTools = NULL;


const std::string PANEL_NAMES[LLFloaterTools::PANEL_COUNT] =
{
	std::string("General"), 	// PANEL_GENERAL,
	std::string("Object"), 	// PANEL_OBJECT,
	std::string("Features"),	// PANEL_FEATURES,
	std::string("Texture"),	// PANEL_FACE,
	std::string("Content"),	// PANEL_CONTENTS,
};

// Local prototypes
void commit_select_component(void *data);
void click_show_more(void*);
void click_popup_info(void*);
void click_popup_done(void*);
void click_popup_minimize(void*);
void click_popup_rotate_left(void*);
void click_popup_rotate_reset(void*);
void click_popup_rotate_right(void*);
void commit_slider_dozer_force(LLUICtrl *);
void click_apply_to_selection(void*);
void commit_radio_group_focus(LLUICtrl* ctrl);
void commit_radio_group_move(LLUICtrl* ctrl);
void commit_radio_group_edit(LLUICtrl* ctrl);
void commit_radio_group_land(LLUICtrl* ctrl);
void commit_grid_mode(LLUICtrl *);
void commit_slider_zoom(LLUICtrl *ctrl);


//static
void*	LLFloaterTools::createPanelPermissions(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelPermissions = new LLPanelPermissions();
	return floater->mPanelPermissions;
}
//static
void*	LLFloaterTools::createPanelObject(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelObject = new LLPanelObject();
	return floater->mPanelObject;
}

//static
void*	LLFloaterTools::createPanelVolume(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelVolume = new LLPanelVolume();
	return floater->mPanelVolume;
}

//static
void*	LLFloaterTools::createPanelFace(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelFace = new LLPanelFace();
	return floater->mPanelFace;
}

//static
void*	LLFloaterTools::createPanelContents(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelContents = new LLPanelContents();
	return floater->mPanelContents;
}

//static
void*	LLFloaterTools::createPanelLandInfo(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelLandInfo = new LLPanelLandInfo();
	return floater->mPanelLandInfo;
}

static	const std::string	toolNames[]={
	"ToolCube",
	"ToolPrism",
	"ToolPyramid",
	"ToolTetrahedron",
	"ToolCylinder",
	"ToolHemiCylinder",
	"ToolCone",
	"ToolHemiCone",
	"ToolSphere",
	"ToolHemiSphere",
	"ToolTorus",
	"ToolTube",
	"ToolRing",
	"ToolTree",
	"ToolGrass"};
LLPCode toolData[]={
	LL_PCODE_CUBE,
	LL_PCODE_PRISM,
	LL_PCODE_PYRAMID,
	LL_PCODE_TETRAHEDRON,
	LL_PCODE_CYLINDER,
	LL_PCODE_CYLINDER_HEMI,
	LL_PCODE_CONE,
	LL_PCODE_CONE_HEMI,
	LL_PCODE_SPHERE,
	LL_PCODE_SPHERE_HEMI,
	LL_PCODE_TORUS,
	LLViewerObject::LL_VO_SQUARE_TORUS,
	LLViewerObject::LL_VO_TRIANGLE_TORUS,
	LL_PCODE_LEGACY_TREE,
	LL_PCODE_LEGACY_GRASS};

BOOL	LLFloaterTools::postBuild()
{
	mCloseSignal.connect(boost::bind(&LLFloaterTools::onClose, this));
	
	// Hide until tool selected
	setVisible(FALSE);

	// Since we constantly show and hide this during drags, don't
	// make sounds on visibility changes.
	setSoundFlags(LLView::SILENT);

	getDragHandle()->setEnabled( !gSavedSettings.getBOOL("ToolboxAutoMove") );

	LLRect rect;
	mBtnFocus			= getChild<LLButton>("button focus");//btn;
	mBtnMove			= getChild<LLButton>("button move");
	mBtnEdit			= getChild<LLButton>("button edit");
	mBtnCreate			= getChild<LLButton>("button create");
	mBtnLand			= getChild<LLButton>("button land" );
	mTextStatus			= getChild<LLTextBox>("text status");
	mRadioGroupFocus	= getChild<LLRadioGroup>("focus_radio_group");
	mRadioGroupMove		= getChild<LLRadioGroup>("move_radio_group");
	mRadioGroupEdit		= getChild<LLRadioGroup>("edit_radio_group");
	mBtnGridOptions		= getChild<LLButton>("Options...");
	
	mCheckSelectIndividual	= getChild<LLCheckBoxCtrl>("checkbox edit linked parts");	
	childSetValue("checkbox edit linked parts",(BOOL)gSavedSettings.getBOOL("EditLinkedParts"));
	mCheckSnapToGrid		= getChild<LLCheckBoxCtrl>("checkbox snap to grid");
	childSetValue("checkbox snap to grid",(BOOL)gSavedSettings.getBOOL("SnapEnabled"));
	mCheckStretchUniform	= getChild<LLCheckBoxCtrl>("checkbox uniform");
	childSetValue("checkbox uniform",(BOOL)gSavedSettings.getBOOL("ScaleUniform"));
	mCheckStretchTexture	= getChild<LLCheckBoxCtrl>("checkbox stretch textures");
	childSetValue("checkbox stretch textures",(BOOL)gSavedSettings.getBOOL("ScaleStretchTextures"));
	mTextGridMode			= getChild<LLTextBox>("text ruler mode");
	mComboGridMode			= getChild<LLComboBox>("combobox grid mode");

	//
	// Create Buttons
	//

	for(size_t t=0; t<LL_ARRAY_SIZE(toolNames); ++t)
	{
		LLButton *found = getChild<LLButton>(toolNames[t]);
		if(found)
		{
			found->setClickedCallback(boost::bind(&LLFloaterTools::setObjectType, toolData[t]));
			mButtons.push_back( found );
		}else{
			llwarns << "Tool button not found! DOA Pending." << llendl;
		}
	}
	mCheckCopySelection = getChild<LLCheckBoxCtrl>("checkbox copy selection");
	childSetValue("checkbox copy selection",(BOOL)gSavedSettings.getBOOL("CreateToolCopySelection"));
	mCheckSticky = getChild<LLCheckBoxCtrl>("checkbox sticky");
	childSetValue("checkbox sticky",(BOOL)gSavedSettings.getBOOL("CreateToolKeepSelected"));
	mCheckCopyCenters = getChild<LLCheckBoxCtrl>("checkbox copy centers");
	childSetValue("checkbox copy centers",(BOOL)gSavedSettings.getBOOL("CreateToolCopyCenters"));
	mCheckCopyRotates = getChild<LLCheckBoxCtrl>("checkbox copy rotates");
	childSetValue("checkbox copy rotates",(BOOL)gSavedSettings.getBOOL("CreateToolCopyRotates"));

	mRadioGroupLand			= getChild<LLRadioGroup>("land_radio_group");
	mBtnApplyToSelection	= getChild<LLButton>("button apply to selection");
	mSliderDozerSize		= getChild<LLSlider>("slider brush size");
	childSetValue( "slider brush size", gSavedSettings.getF32("LandBrushSize"));
	mSliderDozerForce		= getChild<LLSlider>("slider force");
	// the setting stores the actual force multiplier, but the slider is logarithmic, so we convert here
	childSetValue( "slider force", log10(gSavedSettings.getF32("LandBrushForce")));

	mTab = getChild<LLTabContainer>("Object Info Tabs");
	if(mTab)
	{
		mTab->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
		mTab->setBorderVisible(FALSE);
		mTab->selectFirstTab();
	}

	mStatusText["rotate"] = getString("status_rotate");
	mStatusText["scale"] = getString("status_scale");
	mStatusText["move"] = getString("status_move");
	mStatusText["modifyland"] = getString("status_modifyland");
	mStatusText["camera"] = getString("status_camera");
	mStatusText["grab"] = getString("status_grab");
	mStatusText["place"] = getString("status_place");
	mStatusText["selectland"] = getString("status_selectland");
	
	return TRUE;
}

// Create the popupview with a dummy center.  It will be moved into place
// during LLViewerWindow's per-frame hover processing.
LLFloaterTools::LLFloaterTools(const LLSD& key)
:	LLFloater(key),
	mBtnFocus(NULL),
	mBtnMove(NULL),
	mBtnEdit(NULL),
	mBtnCreate(NULL),
	mBtnLand(NULL),
	mTextStatus(NULL),

	mRadioGroupFocus(NULL),
	mRadioGroupMove(NULL),
	mRadioGroupEdit(NULL),

	mCheckSelectIndividual(NULL),

	mCheckSnapToGrid(NULL),
	mBtnGridOptions(NULL),
	mTextGridMode(NULL),
	mComboGridMode(NULL),
	mCheckStretchUniform(NULL),
	mCheckStretchTexture(NULL),

	mBtnRotateLeft(NULL),
	mBtnRotateReset(NULL),
	mBtnRotateRight(NULL),

	mBtnDelete(NULL),
	mBtnDuplicate(NULL),
	mBtnDuplicateInPlace(NULL),

	mCheckSticky(NULL),
	mCheckCopySelection(NULL),
	mCheckCopyCenters(NULL),
	mCheckCopyRotates(NULL),
	mRadioGroupLand(NULL),
	mSliderDozerSize(NULL),
	mSliderDozerForce(NULL),
	mBtnApplyToSelection(NULL),

	mTab(NULL),
	mPanelPermissions(NULL),
	mPanelObject(NULL),
	mPanelVolume(NULL),
	mPanelContents(NULL),
	mPanelFace(NULL),
	mPanelLandInfo(NULL),

	mTabLand(NULL),
	mDirty(TRUE)
{
	gFloaterTools = this;
	
	setAutoFocus(FALSE);
	mFactoryMap["General"] = LLCallbackMap(createPanelPermissions, this);//LLPanelPermissions
	mFactoryMap["Object"] = LLCallbackMap(createPanelObject, this);//LLPanelObject
	mFactoryMap["Features"] = LLCallbackMap(createPanelVolume, this);//LLPanelVolume
	mFactoryMap["Texture"] = LLCallbackMap(createPanelFace, this);//LLPanelFace
	mFactoryMap["Contents"] = LLCallbackMap(createPanelContents, this);//LLPanelContents
	mFactoryMap["land info panel"] = LLCallbackMap(createPanelLandInfo, this);//LLPanelLandInfo
	
	//Called from floater reg: LLUICtrlFactory::getInstance()->buildFloater(this,"floater_tools.xml",FALSE);
	mCommitCallbackRegistrar.add("BuildTool.setTool",			boost::bind(&LLFloaterTools::setTool,this, _2));
	mCommitCallbackRegistrar.add("BuildTool.commitZoom",		boost::bind(&commit_slider_zoom, _1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioFocus",	boost::bind(&commit_radio_group_focus, _1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioMove",	boost::bind(&commit_radio_group_move,_1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioEdit",	boost::bind(&commit_radio_group_edit,_1));

	mCommitCallbackRegistrar.add("BuildTool.selectComponent",	boost::bind(&commit_select_component, this));
	mCommitCallbackRegistrar.add("BuildTool.gridOptions",		boost::bind(&LLFloaterTools::onClickGridOptions,this));
	mCommitCallbackRegistrar.add("BuildTool.applyToSelection",	boost::bind(&click_apply_to_selection, this));
	mCommitCallbackRegistrar.add("BuildTool.gridMode",			boost::bind(&commit_grid_mode,_1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioLand",	boost::bind(&commit_radio_group_land,_1));
	mCommitCallbackRegistrar.add("BuildTool.LandBrushForce",	boost::bind(&commit_slider_dozer_force,_1));
	mCommitCallbackRegistrar.add("BuildTool.AddMedia",			boost::bind(&LLFloaterTools::onClickBtnAddMedia,this));
	mCommitCallbackRegistrar.add("BuildTool.DeleteMedia",		boost::bind(&LLFloaterTools::onClickBtnDeleteMedia,this));
	mCommitCallbackRegistrar.add("BuildTool.EditMedia",			boost::bind(&LLFloaterTools::onClickBtnEditMedia,this));

}

LLFloaterTools::~LLFloaterTools()
{
	// children automatically deleted
	gFloaterTools = NULL;
}

void LLFloaterTools::setStatusText(const std::string& text)
{
	std::map<std::string, std::string>::iterator iter = mStatusText.find(text);
	if (iter != mStatusText.end())
	{
		mTextStatus->setText(iter->second);
	}
	else
	{
		mTextStatus->setText(text);
	}
}

void LLFloaterTools::refresh()
{
	const S32 INFO_WIDTH = getRect().getWidth();
	const S32 INFO_HEIGHT = 384;
	LLRect object_info_rect(0, 0, INFO_WIDTH, -INFO_HEIGHT);
	BOOL all_volume = LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME );

	S32 idx_features = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_FEATURES]);
	S32 idx_face = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_FACE]);
	S32 idx_contents = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_CONTENTS]);

	S32 selected_index = mTab->getCurrentPanelIndex();

	if (!all_volume && (selected_index == idx_features || selected_index == idx_face ||
		selected_index == idx_contents))
	{
		mTab->selectFirstTab();
	}

	mTab->enableTabButton(idx_features, all_volume);
	mTab->enableTabButton(idx_face, all_volume);
	mTab->enableTabButton(idx_contents, all_volume);

	// Refresh object and prim count labels
	LLLocale locale(LLLocale::USER_LOCALE);
	std::string obj_count_string;
	LLResMgr::getInstance()->getIntegerString(obj_count_string, LLSelectMgr::getInstance()->getSelection()->getRootObjectCount());
	childSetTextArg("obj_count",  "[COUNT]", obj_count_string);	
	std::string prim_count_string;
	LLResMgr::getInstance()->getIntegerString(prim_count_string, LLSelectMgr::getInstance()->getSelection()->getObjectCount());
	childSetTextArg("prim_count", "[COUNT]", prim_count_string);

	// Refresh child tabs
	mPanelPermissions->refresh();
	mPanelObject->refresh();
	mPanelVolume->refresh();
	mPanelFace->refresh();
	refreshMedia();
	mPanelContents->refresh();
	mPanelLandInfo->refresh();
}

void LLFloaterTools::draw()
{
	if (mDirty)
	{
		refresh();
		mDirty = FALSE;
	}

	//	mCheckSelectIndividual->set(gSavedSettings.getBOOL("EditLinkedParts"));
	LLFloater::draw();
}

void LLFloaterTools::dirty()
{
	mDirty = TRUE; 
	LLFloaterOpenObject* instance = LLFloaterReg::findTypedInstance<LLFloaterOpenObject>("openobject");
	if (instance) instance->dirty();
}

// Clean up any tool state that should not persist when the
// floater is closed.
void LLFloaterTools::resetToolState()
{
	gCameraBtnZoom = TRUE;
	gCameraBtnOrbit = FALSE;
	gCameraBtnPan = FALSE;

	gGrabBtnSpin = FALSE;
	gGrabBtnVertical = FALSE;
}

void LLFloaterTools::updatePopup(LLCoordGL center, MASK mask)
{
	LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();

	// HACK to allow seeing the buttons when you have the app in a window.
	// Keep the visibility the same as it 
	if (tool == gToolNull)
	{
		return;
	}

	if ( isMinimized() )
	{	// SL looks odd if we draw the tools while the window is minimized
		return;
	}
	
	// Focus buttons
	BOOL focus_visible = (	tool == LLToolCamera::getInstance() );

	mBtnFocus	->setToggleState( focus_visible );

	mRadioGroupFocus->setVisible( focus_visible );
	childSetVisible("slider zoom", focus_visible);
	childSetEnabled("slider zoom", gCameraBtnZoom);

	if (!gCameraBtnOrbit &&
		!gCameraBtnPan &&
		!(mask == MASK_ORBIT) &&
		!(mask == (MASK_ORBIT | MASK_ALT)) &&
		!(mask == MASK_PAN) &&
		!(mask == (MASK_PAN | MASK_ALT)) )
	{
		mRadioGroupFocus->setValue("radio zoom");
	}
	else if (	gCameraBtnOrbit || 
				(mask == MASK_ORBIT) ||
				(mask == (MASK_ORBIT | MASK_ALT)) )
	{
		mRadioGroupFocus->setValue("radio orbit");
	}
	else if (	gCameraBtnPan ||
				(mask == MASK_PAN) ||
				(mask == (MASK_PAN | MASK_ALT)) )
	{
		mRadioGroupFocus->setValue("radio pan");
	}

	// multiply by correction factor because volume sliders go [0, 0.5]
	childSetValue( "slider zoom", gAgent.getCameraZoomFraction() * 0.5f);

	// Move buttons
	BOOL move_visible = (tool == LLToolGrab::getInstance());

	if (mBtnMove) mBtnMove	->setToggleState( move_visible );

	// HACK - highlight buttons for next click
	mRadioGroupMove->setVisible(move_visible);
	if (!gGrabBtnSpin && 
		!gGrabBtnVertical &&
		!(mask == MASK_VERTICAL) && 
		!(mask == MASK_SPIN) )
	{
		mRadioGroupMove->setValue("radio move");
	}
	else if (gGrabBtnVertical || 
			 (mask == MASK_VERTICAL) )
	{
		mRadioGroupMove->setValue("radio lift");
	}
	else if (gGrabBtnSpin || 
			 (mask == MASK_SPIN) )
	{
		mRadioGroupMove->setValue("radio spin");
	}

	// Edit buttons
	BOOL edit_visible = tool == LLToolCompTranslate::getInstance() ||
						tool == LLToolCompRotate::getInstance() ||
						tool == LLToolCompScale::getInstance() ||
						tool == LLToolFace::getInstance() ||
						tool == LLToolIndividual::getInstance() ||
						tool == LLToolPipette::getInstance();

	mBtnEdit	->setToggleState( edit_visible );
	mRadioGroupEdit->setVisible( edit_visible );

	if (mCheckSelectIndividual)
	{
		mCheckSelectIndividual->setVisible(edit_visible);
		//mCheckSelectIndividual->set(gSavedSettings.getBOOL("EditLinkedParts"));
	}

	if ( tool == LLToolCompTranslate::getInstance() )
	{
		mRadioGroupEdit->setValue("radio position");
	}
	else if ( tool == LLToolCompRotate::getInstance() )
	{
		mRadioGroupEdit->setValue("radio rotate");
	}
	else if ( tool == LLToolCompScale::getInstance() )
	{
		mRadioGroupEdit->setValue("radio stretch");
	}
	else if ( tool == LLToolFace::getInstance() )
	{
		mRadioGroupEdit->setValue("radio select face");
	}

	if (mComboGridMode) 
	{
		mComboGridMode->setVisible( edit_visible );
		S32 index = mComboGridMode->getCurrentIndex();
		mComboGridMode->removeall();

		switch (mObjectSelection->getSelectType())
		{
		case SELECT_TYPE_HUD:
		  mComboGridMode->add(getString("grid_screen_text"));
		  mComboGridMode->add(getString("grid_local_text"));
		  //mComboGridMode->add(getString("grid_reference_text"));
		  break;
		case SELECT_TYPE_WORLD:
		  mComboGridMode->add(getString("grid_world_text"));
		  mComboGridMode->add(getString("grid_local_text"));
		  mComboGridMode->add(getString("grid_reference_text"));
		  break;
		case SELECT_TYPE_ATTACHMENT:
		  mComboGridMode->add(getString("grid_attachment_text"));
		  mComboGridMode->add(getString("grid_local_text"));
		  mComboGridMode->add(getString("grid_reference_text"));
		  break;
		}

		mComboGridMode->setCurrentByIndex(index);
	}
	if (mTextGridMode) mTextGridMode->setVisible( edit_visible );

	// Snap to grid disabled for grab tool - very confusing
	if (mCheckSnapToGrid) mCheckSnapToGrid->setVisible( edit_visible /* || tool == LLToolGrab::getInstance() */ );
	if (mBtnGridOptions) mBtnGridOptions->setVisible( edit_visible /* || tool == LLToolGrab::getInstance() */ );

	//mCheckSelectLinked	->setVisible( edit_visible );
	if (mCheckStretchUniform) mCheckStretchUniform->setVisible( edit_visible );
	if (mCheckStretchTexture) mCheckStretchTexture->setVisible( edit_visible );

	// Create buttons
	BOOL create_visible = (tool == LLToolCompCreate::getInstance());

	mBtnCreate	->setToggleState(	tool == LLToolCompCreate::getInstance() );

	if (mCheckCopySelection
		&& mCheckCopySelection->get())
	{
		// don't highlight any placer button
		for (std::vector<LLButton*>::size_type i = 0; i < mButtons.size(); i++)
		{
			mButtons[i]->setToggleState(FALSE);
			mButtons[i]->setVisible( create_visible );
		}
	}
	else
	{
		// Highlight the correct placer button
		for( S32 t = 0; t < (S32)mButtons.size(); t++ )
		{
			LLPCode pcode = LLToolPlacer::getObjectType();
			LLPCode button_pcode = toolData[t];
			BOOL state = (pcode == button_pcode);
			mButtons[t]->setToggleState( state );
			mButtons[t]->setVisible( create_visible );
		}
	}

	if (mCheckSticky) mCheckSticky		->setVisible( create_visible );
	if (mCheckCopySelection) mCheckCopySelection	->setVisible( create_visible );
	if (mCheckCopyCenters) mCheckCopyCenters	->setVisible( create_visible );
	if (mCheckCopyRotates) mCheckCopyRotates	->setVisible( create_visible );

	if (mCheckCopyCenters) mCheckCopyCenters->setEnabled( mCheckCopySelection->get() );
	if (mCheckCopyRotates) mCheckCopyRotates->setEnabled( mCheckCopySelection->get() );

	// Land buttons
	BOOL land_visible = (tool == LLToolBrushLand::getInstance() || tool == LLToolSelectLand::getInstance() );

	if (mBtnLand)	mBtnLand	->setToggleState( land_visible );

	mRadioGroupLand->setVisible( land_visible );
	if ( tool == LLToolSelectLand::getInstance() )
	{
		mRadioGroupLand->setValue("radio select land");
	}
	else if ( tool == LLToolBrushLand::getInstance() )
	{
		S32 dozer_mode = gSavedSettings.getS32("RadioLandBrushAction");
		switch(dozer_mode)
		{
		case 0:
			mRadioGroupLand->setValue("radio flatten");
			break;
		case 1:
			mRadioGroupLand->setValue("radio raise");
			break;
		case 2:
			mRadioGroupLand->setValue("radio lower");
			break;
		case 3:
			mRadioGroupLand->setValue("radio smooth");
			break;
		case 4:
			mRadioGroupLand->setValue("radio noise");
			break;
		case 5:
			mRadioGroupLand->setValue("radio revert");
			break;
		default:
			break;
		}
	}

	if (mBtnApplyToSelection)
	{
		mBtnApplyToSelection->setVisible( land_visible );
		mBtnApplyToSelection->setEnabled( land_visible && !LLViewerParcelMgr::getInstance()->selectionEmpty() && tool != LLToolSelectLand::getInstance());
	}
	if (mSliderDozerSize)
	{
		mSliderDozerSize	->setVisible( land_visible );
		childSetVisible("Bulldozer:", land_visible);
		childSetVisible("Dozer Size:", land_visible);
	}
	if (mSliderDozerForce)
	{
		mSliderDozerForce	->setVisible( land_visible );
		childSetVisible("Strength:", land_visible);
	}

	childSetVisible("obj_count", !land_visible);
	childSetVisible("prim_count", !land_visible);
	mTab->setVisible(!land_visible);
	mPanelLandInfo->setVisible(land_visible);
}


// virtual
BOOL LLFloaterTools::canClose()
{
	// don't close when quitting, so camera will stay put
	return !LLApp::isExiting();
}

// virtual
void LLFloaterTools::onOpen(const LLSD& key)
{
	mParcelSelection = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();
	mObjectSelection = LLSelectMgr::getInstance()->getEditSelection();
	
	std::string panel = key.asString();
	if (!panel.empty())
	{
		mTab->selectTabByName(panel);
	}
	
	//gMenuBarView->setItemVisible("BuildTools", TRUE);
}

void LLFloaterTools::onClose()
{
	mTab->setVisible(FALSE);

	LLViewerJoystick::getInstance()->moveAvatar(false);

    // Different from handle_reset_view in that it doesn't actually 
	//   move the camera if EditCameraMovement is not set.
	gAgent.resetView(gSavedSettings.getBOOL("EditCameraMovement"));
	
	// exit component selection mode
	LLSelectMgr::getInstance()->promoteSelectionToRoot();
	gSavedSettings.setBOOL("EditLinkedParts", FALSE);

	gViewerWindow->showCursor();

	resetToolState();

	mParcelSelection = NULL;
	mObjectSelection = NULL;

	// Switch back to basic toolset
	LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
	// we were already in basic toolset, using build tools
	// so manually reset tool to default (pie menu tool)
	LLToolMgr::getInstance()->getCurrentToolset()->selectFirstTool();

	//gMenuBarView->setItemVisible("BuildTools", FALSE);
	LLFloaterReg::hideInstance("media_settings");
}

void click_popup_info(void*)
{
}

void click_popup_done(void*)
{
	handle_reset_view();
}

void commit_radio_group_move(LLUICtrl* ctrl)
{
	LLRadioGroup* group = (LLRadioGroup*)ctrl;
	std::string selected = group->getValue().asString();
	if (selected == "radio move")
	{
		gGrabBtnVertical = FALSE;
		gGrabBtnSpin = FALSE;
	}
	else if (selected == "radio lift")
	{
		gGrabBtnVertical = TRUE;
		gGrabBtnSpin = FALSE;
	}
	else if (selected == "radio spin")
	{
		gGrabBtnVertical = FALSE;
		gGrabBtnSpin = TRUE;
	}
}

void commit_radio_group_focus(LLUICtrl* ctrl)
{
	LLRadioGroup* group = (LLRadioGroup*)ctrl;
	std::string selected = group->getValue().asString();
	if (selected == "radio zoom")
	{
		gCameraBtnZoom = TRUE;
		gCameraBtnOrbit = FALSE;
		gCameraBtnPan = FALSE;
	}
	else if (selected == "radio orbit")
	{
		gCameraBtnZoom = FALSE;
		gCameraBtnOrbit = TRUE;
		gCameraBtnPan = FALSE;
	}
	else if (selected == "radio pan")
	{
		gCameraBtnZoom = FALSE;
		gCameraBtnOrbit = FALSE;
		gCameraBtnPan = TRUE;
	}
}

void commit_slider_zoom(LLUICtrl *ctrl)
{
	// renormalize value, since max "volume" level is 0.5 for some reason
	F32 zoom_level = (F32)ctrl->getValue().asReal() * 2.f; // / 0.5f;
	gAgent.setCameraZoomFraction(zoom_level);
}

void click_popup_rotate_left(void*)
{
	LLSelectMgr::getInstance()->selectionRotateAroundZ( 45.f );
	dialog_refresh_all();
}

void click_popup_rotate_reset(void*)
{
	LLSelectMgr::getInstance()->selectionResetRotation();
	dialog_refresh_all();
}

void click_popup_rotate_right(void*)
{
	LLSelectMgr::getInstance()->selectionRotateAroundZ( -45.f );
	dialog_refresh_all();
}

void commit_slider_dozer_force(LLUICtrl *ctrl)
{
	// the slider is logarithmic, so we exponentiate to get the actual force multiplier
	F32 dozer_force = pow(10.f, (F32)ctrl->getValue().asReal());
	gSavedSettings.setF32("LandBrushForce", dozer_force);
}

void click_apply_to_selection(void*)
{
	LLToolBrushLand::getInstance()->modifyLandInSelectionGlobal();
}

void commit_radio_group_edit(LLUICtrl *ctrl)
{
	S32 show_owners = gSavedSettings.getBOOL("ShowParcelOwners");

	LLRadioGroup* group = (LLRadioGroup*)ctrl;
	std::string selected = group->getValue().asString();
	if (selected == "radio position")
	{
		LLFloaterTools::setEditTool( LLToolCompTranslate::getInstance() );
	}
	else if (selected == "radio rotate")
	{
		LLFloaterTools::setEditTool( LLToolCompRotate::getInstance() );
	}
	else if (selected == "radio stretch")
	{
		LLFloaterTools::setEditTool( LLToolCompScale::getInstance() );
	}
	else if (selected == "radio select face")
	{
		LLFloaterTools::setEditTool( LLToolFace::getInstance() );
	}
	gSavedSettings.setBOOL("ShowParcelOwners", show_owners);
}

void commit_radio_group_land(LLUICtrl* ctrl)
{
	LLRadioGroup* group = (LLRadioGroup*)ctrl;
	std::string selected = group->getValue().asString();
	if (selected == "radio select land")
	{
		LLFloaterTools::setEditTool( LLToolSelectLand::getInstance() );
	}
	else
	{
		LLFloaterTools::setEditTool( LLToolBrushLand::getInstance() );
		S32 dozer_mode = gSavedSettings.getS32("RadioLandBrushAction");
		if (selected == "radio flatten")
			dozer_mode = 0;
		else if (selected == "radio raise")
			dozer_mode = 1;
		else if (selected == "radio lower")
			dozer_mode = 2;
		else if (selected == "radio smooth")
			dozer_mode = 3;
		else if (selected == "radio noise")
			dozer_mode = 4;
		else if (selected == "radio revert")
			dozer_mode = 5;
		gSavedSettings.setS32("RadioLandBrushAction", dozer_mode);
	}
}

void commit_select_component(void *data)
{
	LLFloaterTools* floaterp = (LLFloaterTools*)data;

	//forfeit focus
	if (gFocusMgr.childHasKeyboardFocus(floaterp))
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}

	BOOL select_individuals = floaterp->mCheckSelectIndividual->get();
	gSavedSettings.setBOOL("EditLinkedParts", select_individuals);
	floaterp->dirty();

	if (select_individuals)
	{
		LLSelectMgr::getInstance()->demoteSelectionToIndividuals();
	}
	else
	{
		LLSelectMgr::getInstance()->promoteSelectionToRoot();
	}
}

void commit_grid_mode(LLUICtrl *ctrl)   
{   
	LLComboBox* combo = (LLComboBox*)ctrl;   
    
	LLSelectMgr::getInstance()->setGridMode((EGridMode)combo->getCurrentIndex());
} 

// static 
void LLFloaterTools::setObjectType( LLPCode pcode )
{
	LLToolPlacer::setObjectType( pcode );
	gSavedSettings.setBOOL("CreateToolCopySelection", FALSE);
	gFocusMgr.setMouseCapture(NULL);
}


void LLFloaterTools::onClickGridOptions()
{
	LLFloaterReg::showInstance("build_options");
	// RN: this makes grid options dependent on build tools window
	//floaterp->addDependentFloater(LLFloaterBuildOptions::getInstance(), FALSE);
}

// static
void LLFloaterTools::setEditTool(void* tool_pointer)
{
	LLTool *tool = (LLTool *)tool_pointer;
	LLToolMgr::getInstance()->getCurrentToolset()->selectTool( tool );
}

void LLFloaterTools::setTool(const LLSD& user_data)
{
	std::string control_name = user_data.asString();
	if(control_name == "Focus")
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool((LLTool *) LLToolCamera::getInstance() );
	else if (control_name == "Move" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *)LLToolGrab::getInstance() );
	else if (control_name == "Edit" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolCompTranslate::getInstance());
	else if (control_name == "Create" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolCompCreate::getInstance());
	else if (control_name == "Land" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolSelectLand::getInstance());
	else
		llwarns<<" no parameter name "<<control_name<<" found!! No Tool selected!!"<< llendl;
}

void LLFloaterTools::onFocusReceived()
{
	LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
	LLFloater::onFocusReceived();
}

// Media stuff
void LLFloaterTools::refreshMedia()
{
	getMediaState();	
	LLFloaterMediaSettings::getInstance();
	LLFloaterMediaSettings::initValues(mMediaSettings );
}



void LLFloaterTools::getMediaState()
{
	LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
	
	if( !objectp )
	{
		childSetEnabled("media_tex", FALSE);
		childSetEnabled("add_media", FALSE);
		childSetEnabled("delete_media", FALSE);
		childSetEnabled("edit_media", FALSE);
		updateMediaSettings();
		return;
	}
	
	bool editable = gAgent.isGodlike() || (objectp->permModify() && objectp->getPCode() == LL_PCODE_VOLUME);
	
	// Media settings
	U8 has_media = (U8)0;
	struct media_functor : public LLSelectedTEGetFunctor<U8>
	{
		U8 get(LLViewerObject* object, S32 face)
		{
			return (object->getTE(face)->getMediaTexGen());
		}
	} func;
	bool identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, has_media );
	
	// update UI depending on whether "object" (prim or face) has media
	// and whether or not you are allowed to edit it.
	bool bool_has_media = (has_media & LLTextureEntry::MF_HAS_MEDIA);
	childSetEnabled("media_tex",  bool_has_media & editable);
	childSetEnabled( "edit_media", bool_has_media & editable );
	childSetEnabled( "delete_media", bool_has_media & editable );
	childSetEnabled( "add_media", ( ! bool_has_media ) & editable );
	
	// load values for media settings
	updateMediaSettings();
	
	// if identical is set, all faces are same
	if ( identical )
	{
		// TODO: display a list of all media on the face - use 'identical' flag
	};
}


//////////////////////////////////////////////////////////////////////////////
// called when a user wants to add media to a prim or prim face
void LLFloaterTools::onClickBtnAddMedia()
{
	// check for the edit tool and now many faces are selected
	LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();
	if((tool != LLToolFace::getInstance()) || LLSelectMgr::getInstance()->getSelection()->isMultipleTESelected())
	{
		LLNotifications::instance().add("MultipleFacesSelected",LLSD(), LLSD(), multipleFacesSelectedConfirm);
		
	}
	else
	{
		onClickBtnEditMedia();
	}

}

// static
bool LLFloaterTools::multipleFacesSelectedConfirm(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch( option )
	{
		case 0:  // "Yes"
			gFloaterTools->onClickBtnEditMedia();
			break;
		case 1:  // "No"
		default:
			break;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to edit existing media settings on a prim or prim face
// TODO: test if there is media on the item and only allow editing if present
void LLFloaterTools::onClickBtnEditMedia()
{
	refreshMedia();
	LLFloaterReg::showInstance("media_settings");	
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to delete media from a prim or prim face
void LLFloaterTools::onClickBtnDeleteMedia()
{
	LLNotifications::instance().add("DeleteMedia", LLSD(), LLSD(), deleteMediaConfirm);
}


// static
bool LLFloaterTools::deleteMediaConfirm(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch( option )
	{
		case 0:  // "Yes"
			LLSelectMgr::getInstance()->selectionSetMedia( 0 );
			if(LLFloaterReg::instanceVisible("media_settings"))
			{
				LLFloaterReg::hideInstance("media_settings");
			}
			break;
			
		case 1:  // "No"
		default:
			break;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
//
void LLFloaterTools::updateMediaSettings()
{
    bool identical( false );
    std::string base_key( "" );
    std::string value_str( "" );
    int value_int = 0;
    bool value_bool = false;
	LLObjectSelectionHandle selected_objects =LLSelectMgr::getInstance()->getSelection();
    // TODO: (CP) refactor this using something clever or boost or both !!

    LLMediaEntry default_media_data;

    // controls 
    U8 value_u8 = default_media_data.getControls();
    struct functor_getter_controls : public LLSelectedTEGetFunctor< U8 >
    {
        U8 get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getControls();
            LLMediaEntry default_media_data;
            return default_media_data.getControls();
        };
		
    } func_controls;
    identical = selected_objects->getSelectedTEValue( &func_controls, value_u8 );
    base_key = std::string( LLMediaEntry::CONTROLS_KEY );
    mMediaSettings[ base_key ] = value_u8;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // First click (formerly left click)
    value_bool = default_media_data.getFirstClickInteract();
    struct functor_getter_first_click : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getFirstClickInteract();
            LLMediaEntry default_media_data;
            return default_media_data.getFirstClickInteract();
        };
		
    } func_first_click;
    identical = selected_objects->getSelectedTEValue( &func_first_click, value_bool );
    base_key = std::string( LLMediaEntry::FIRST_CLICK_INTERACT_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Home URL
    value_str = default_media_data.getHomeURL();
    struct functor_getter_home_url : public LLSelectedTEGetFunctor< std::string >
    {
        std::string get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getHomeURL();
            LLMediaEntry default_media_data;
            return default_media_data.getHomeURL();
        };
		
    } func_home_url;
    identical = selected_objects->getSelectedTEValue( &func_home_url, value_str );
    base_key = std::string( LLMediaEntry::HOME_URL_KEY );
    mMediaSettings[ base_key ] = value_str;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	llwarns<<"Angela debug : home url string == "<<value_str<<llendl;
	
    // Current URL
    value_str = default_media_data.getCurrentURL();
    struct functor_getter_current_url : public LLSelectedTEGetFunctor< std::string >
    {
        std::string get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getCurrentURL();
            LLMediaEntry default_media_data;
            return default_media_data.getCurrentURL();
        };
		
    } func_current_url;
    identical = selected_objects->getSelectedTEValue( &func_current_url, value_str );
    base_key = std::string( LLMediaEntry::CURRENT_URL_KEY );
    mMediaSettings[ base_key ] = value_str;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Auto zoom
    value_bool = default_media_data.getAutoZoom();
    struct functor_getter_auto_zoom : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAutoZoom();
            LLMediaEntry default_media_data;
            return default_media_data.getAutoZoom();
        };
		
    } func_auto_zoom;
    identical = selected_objects->getSelectedTEValue( &func_auto_zoom, value_bool );
    base_key = std::string( LLMediaEntry::AUTO_ZOOM_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Auto play
    value_bool = default_media_data.getAutoPlay();
    struct functor_getter_auto_play : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAutoPlay();
			LLMediaEntry default_media_data;
            return default_media_data.getAutoPlay();
        };
		
    } func_auto_play;
    identical = selected_objects->getSelectedTEValue( &func_auto_play, value_bool );
    base_key = std::string( LLMediaEntry::AUTO_PLAY_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Auto scale
    value_bool = default_media_data.getAutoScale();
    struct functor_getter_auto_scale : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAutoScale();
			LLMediaEntry default_media_data;
            return default_media_data.getAutoScale();;
        };
		
    } func_auto_scale;
    identical = selected_objects->getSelectedTEValue( &func_auto_scale, value_bool );
    base_key = std::string( LLMediaEntry::AUTO_SCALE_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Auto loop
    value_bool = default_media_data.getAutoLoop();
    struct functor_getter_auto_loop : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAutoLoop();
			LLMediaEntry default_media_data;
            return default_media_data.getAutoLoop();
        };
		
    } func_auto_loop;
    identical = selected_objects->getSelectedTEValue( &func_auto_loop, value_bool );
    base_key = std::string( LLMediaEntry::AUTO_LOOP_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // width pixels (if not auto scaled)
    value_int = default_media_data.getWidthPixels();
    struct functor_getter_width_pixels : public LLSelectedTEGetFunctor< int >
    {
        int get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getWidthPixels();
			LLMediaEntry default_media_data;
            return default_media_data.getWidthPixels();
        };
		
    } func_width_pixels;
    identical = selected_objects->getSelectedTEValue( &func_width_pixels, value_int );
    base_key = std::string( LLMediaEntry::WIDTH_PIXELS_KEY );
    mMediaSettings[ base_key ] = value_int;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // height pixels (if not auto scaled)
    value_int = default_media_data.getHeightPixels();
    struct functor_getter_height_pixels : public LLSelectedTEGetFunctor< int >
    {
        int get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getHeightPixels();
			LLMediaEntry default_media_data;
            return default_media_data.getHeightPixels();
        };
		
    } func_height_pixels;
    identical = selected_objects->getSelectedTEValue( &func_height_pixels, value_int );
    base_key = std::string( LLMediaEntry::HEIGHT_PIXELS_KEY );
    mMediaSettings[ base_key ] = value_int;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Enable Alt image
    value_bool = default_media_data.getAltImageEnable();
    struct functor_getter_enable_alt_image : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAltImageEnable();
			LLMediaEntry default_media_data;
            return default_media_data.getAltImageEnable();
        };
		
    } func_enable_alt_image;
    identical = selected_objects->getSelectedTEValue( &func_enable_alt_image, value_bool );
    base_key = std::string( LLMediaEntry::ALT_IMAGE_ENABLE_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - owner interact
    value_bool = 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_OWNER );
    struct functor_getter_perms_owner_interact : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_OWNER));
			LLMediaEntry default_media_data;
            return 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_OWNER );
        };
		
    } func_perms_owner_interact;
    identical = selected_objects->getSelectedTEValue( &func_perms_owner_interact, value_bool );
    base_key = std::string( LLPanelContents::PERMS_OWNER_INTERACT_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - owner control
    value_bool = 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_OWNER );
    struct functor_getter_perms_owner_control : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_OWNER));
			LLMediaEntry default_media_data;
            return 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_OWNER );
        };
		
    } func_perms_owner_control;
    identical = selected_objects ->getSelectedTEValue( &func_perms_owner_control, value_bool );
    base_key = std::string( LLPanelContents::PERMS_OWNER_CONTROL_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - group interact
    value_bool = 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_GROUP );
    struct functor_getter_perms_group_interact : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_GROUP));
			LLMediaEntry default_media_data;
            return 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_GROUP );
        };
		
    } func_perms_group_interact;
    identical = selected_objects->getSelectedTEValue( &func_perms_group_interact, value_bool );
    base_key = std::string( LLPanelContents::PERMS_GROUP_INTERACT_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - group control
    value_bool = 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_GROUP );
    struct functor_getter_perms_group_control : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_GROUP));
			LLMediaEntry default_media_data;
            return 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_GROUP );
        };
		
    } func_perms_group_control;
    identical = selected_objects->getSelectedTEValue( &func_perms_group_control, value_bool );
    base_key = std::string( LLPanelContents::PERMS_GROUP_CONTROL_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - anyone interact
    value_bool = 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_ANYONE );
    struct functor_getter_perms_anyone_interact : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_ANYONE));
			LLMediaEntry default_media_data;
            return 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_ANYONE );
        };
		
    } func_perms_anyone_interact;
    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func_perms_anyone_interact, value_bool );
    base_key = std::string( LLPanelContents::PERMS_ANYONE_INTERACT_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - anyone control
    value_bool = 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_ANYONE );
    struct functor_getter_perms_anyone_control : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_ANYONE));
			LLMediaEntry default_media_data;
            return 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_ANYONE );
        };
		
    } func_perms_anyone_control;
    identical = selected_objects->getSelectedTEValue( &func_perms_anyone_control, value_bool );
    base_key = std::string( LLPanelContents::PERMS_ANYONE_CONTROL_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // security - whitelist enable
    value_bool = default_media_data.getWhiteListEnable();
    struct functor_getter_whitelist_enable : public LLSelectedTEGetFunctor< bool >
    {
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getWhiteListEnable();
			LLMediaEntry default_media_data;
            return default_media_data.getWhiteListEnable();
        };
		
    } func_whitelist_enable;
    identical = selected_objects->getSelectedTEValue( &func_whitelist_enable, value_bool );
    base_key = std::string( LLMediaEntry::WHITELIST_ENABLE_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // security - whitelist URLs
    std::vector<std::string> value_vector_str = default_media_data.getWhiteList();
    struct functor_getter_whitelist_urls : public LLSelectedTEGetFunctor< std::vector<std::string> >
    {
        std::vector<std::string> get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getWhiteList();
            LLMediaEntry default_media_data;
            return default_media_data.getWhiteList();
        };
		
    } func_whitelist_urls;
    identical = selected_objects->getSelectedTEValue( &func_whitelist_urls, value_vector_str );
    base_key = std::string( LLMediaEntry::WHITELIST_KEY );
	mMediaSettings[ base_key ].clear();
    std::vector< std::string >::iterator iter = value_vector_str.begin();
    while( iter != value_vector_str.end() )
    {
        std::string white_list_url = *iter;
        mMediaSettings[ base_key ].append( white_list_url );
        ++iter;
    };
	
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
}

