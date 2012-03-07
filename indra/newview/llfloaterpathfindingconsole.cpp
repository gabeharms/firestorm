/** 
* @file llfloaterpathfindingconsole.cpp
* @author William Todd Stinson
* @brief "Pathfinding console" floater, allowing manipulation of the Havok AI pathfinding settings.
*
* $LicenseInfo:firstyear=2002&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2010, Linden Research, Inc.
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
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"
#include "llfloaterpathfindingconsole.h"
#include "llfloaterpathfindinglinksets.h"
#include "llfloaterpathfindingcharacters.h"

#include "llsd.h"
#include "llhandle.h"
#include "llagent.h"
#include "llpanel.h"
#include "llbutton.h"
#include "llradiogroup.h"
#include "llsliderctrl.h"
#include "lllineeditor.h"
#include "lltextbase.h"
#include "lltabcontainer.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llpathfindingnavmeshzone.h"
#include "llpathfindingmanager.h"

#include "LLPathingLib.h"

#define XUI_RENDER_HEATMAP_NONE 0
#define XUI_RENDER_HEATMAP_A 1
#define XUI_RENDER_HEATMAP_B 2
#define XUI_RENDER_HEATMAP_C 3
#define XUI_RENDER_HEATMAP_D 4

#define XUI_CHARACTER_TYPE_A 1
#define XUI_CHARACTER_TYPE_B 2
#define XUI_CHARACTER_TYPE_C 3
#define XUI_CHARACTER_TYPE_D 4

#define XUI_TEST_TAB_INDEX 1

LLHandle<LLFloaterPathfindingConsole> LLFloaterPathfindingConsole::sInstanceHandle;

//---------------------------------------------------------------------------
// LLFloaterPathfindingConsole
//---------------------------------------------------------------------------

BOOL LLFloaterPathfindingConsole::postBuild()
{
	mShowNavMeshCheckBox = findChild<LLCheckBoxCtrl>("show_navmesh");
	llassert(mShowNavMeshCheckBox != NULL);

	mShowNavMeshWalkabilityComboBox = findChild<LLComboBox>("show_heatmap_mode");
	llassert(mShowNavMeshWalkabilityComboBox != NULL);
	mShowNavMeshWalkabilityComboBox->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onShowWalkabilitySet, this));

	mShowWalkablesCheckBox = findChild<LLCheckBoxCtrl>("show_walkables");
	llassert(mShowWalkablesCheckBox != NULL);

	mShowStaticObstaclesCheckBox = findChild<LLCheckBoxCtrl>("show_static_obstacles");
	llassert(mShowStaticObstaclesCheckBox != NULL);

	mShowMaterialVolumesCheckBox = findChild<LLCheckBoxCtrl>("show_material_volumes");
	llassert(mShowMaterialVolumesCheckBox != NULL);

	mShowExclusionVolumesCheckBox = findChild<LLCheckBoxCtrl>("show_exclusion_volumes");
	llassert(mShowExclusionVolumesCheckBox != NULL);

	mShowWorldCheckBox = findChild<LLCheckBoxCtrl>("show_world");
	llassert(mShowWorldCheckBox != NULL);
	mShowWorldCheckBox->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onShowWorldToggle, this));

	mViewCharactersButton = findChild<LLButton>("view_characters_floater");
	llassert(mViewCharactersButton != NULL);
	mViewCharactersButton->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onViewCharactersClicked, this));

	mEditTestTabContainer = findChild<LLTabContainer>("edit_test_tab_container");
	llassert(mEditTestTabContainer != NULL);

	mEditTab = findChild<LLPanel>("edit_panel");
	llassert(mEditTab != NULL);

	mTestTab = findChild<LLPanel>("test_panel");
	llassert(mTestTab != NULL);

	mUnfreezeLabel = findChild<LLTextBase>("unfreeze_label");
	llassert(mUnfreezeLabel != NULL);

	mUnfreezeButton = findChild<LLButton>("enter_unfrozen_mode");
	llassert(mUnfreezeButton != NULL);
	mUnfreezeButton->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onUnfreezeClicked, this));

	mLinksetsLabel = findChild<LLTextBase>("edit_linksets_label");
	llassert(mLinksetsLabel != NULL);

	mLinksetsButton = findChild<LLButton>("view_and_edit_linksets");
	llassert(mLinksetsButton != NULL);
	mLinksetsButton->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onViewEditLinksetClicked, this));

	mFreezeLabel = findChild<LLTextBase>("freeze_label");
	llassert(mFreezeLabel != NULL);

	mFreezeButton = findChild<LLButton>("enter_frozen_mode");
	llassert(mFreezeButton != NULL);
	mFreezeButton->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onFreezeClicked, this));

	mPathfindingStatus = findChild<LLTextBase>("pathfinding_status");
	llassert(mPathfindingStatus != NULL);

	mCharacterWidthSlider = findChild<LLSliderCtrl>("character_width");
	llassert(mCharacterWidthSlider != NULL);
	mCharacterWidthSlider->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onCharacterWidthSet, this));

	mCharacterTypeRadioGroup = findChild<LLRadioGroup>("character_type");
	llassert(mCharacterTypeRadioGroup != NULL);
	mCharacterTypeRadioGroup->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onCharacterTypeSwitch, this));

	mPathTestingStatus = findChild<LLTextBase>("path_test_status");
	llassert(mPathTestingStatus != NULL);

	mClearPathButton = findChild<LLButton>("clear_path");
	llassert(mClearPathButton != NULL);
	mClearPathButton->setCommitCallback(boost::bind(&LLFloaterPathfindingConsole::onClearPathClicked, this));

	return LLFloater::postBuild();
}

void LLFloaterPathfindingConsole::onOpen(const LLSD& pKey)
{
	LLFloater::onOpen(pKey);
	setHeartBeat( true );
	//make sure we have a pathing system
	if ( !LLPathingLib::getInstance() )
	{
		LLPathingLib::initSystem();
	}	
	if ( LLPathingLib::getInstance() == NULL )
	{ 
		setConsoleState(kConsoleStateLibraryNotImplemented);
		llwarns <<"Errror: cannot find pathing library implementation."<<llendl;
	}
	else
	{	
		if (!mNavMeshZoneSlot.connected())
		{
			mNavMeshZoneSlot = mNavMeshZone.registerNavMeshZoneListener(boost::bind(&LLFloaterPathfindingConsole::onNavMeshZoneCB, this, _1));
		}

		mNavMeshZone.initialize();

		mNavMeshZone.enable();
		mNavMeshZone.refresh();
	}		

	if (!mAgentStateSlot.connected())
	{
		mAgentStateSlot = LLPathfindingManager::getInstance()->registerAgentStateListener(boost::bind(&LLFloaterPathfindingConsole::onAgentStateCB, this, _1));
	}

	setAgentState(LLPathfindingManager::getInstance()->getAgentState());
	updatePathTestStatus();
}

void LLFloaterPathfindingConsole::onClose(bool pIsAppQuitting)
{
	if (mAgentStateSlot.connected())
	{
		mAgentStateSlot.disconnect();
	}

	if (mNavMeshZoneSlot.connected())
	{
		mNavMeshZoneSlot.disconnect();
	}

	if (LLPathingLib::getInstance() != NULL)
	{
		mNavMeshZone.disable();
	}

	LLFloater::onClose(pIsAppQuitting);
	setHeartBeat( false );
	setConsoleState(kConsoleStateUnknown);
}

BOOL LLFloaterPathfindingConsole::handleAnyMouseClick(S32 x, S32 y, MASK mask, EClickType clicktype, BOOL down)
{
	if (isGeneratePathMode(mask, clicktype, down))
	{
		LLVector3 dv = gViewerWindow->mouseDirectionGlobal(x, y);
		LLVector3 mousePos = LLViewerCamera::getInstance()->getOrigin();
		LLVector3 rayStart = mousePos;
		LLVector3 rayEnd = mousePos + dv * 150;

		if (mask & MASK_CONTROL)
		{
			mPathData.mStartPointA = rayStart;
			mPathData.mEndPointA = rayEnd;
			mHasStartPoint = true;
		}
		else if (mask & MASK_SHIFT)
		{
			mPathData.mStartPointB = rayStart;
			mPathData.mEndPointB = rayEnd;
			mHasEndPoint = true;
		}
		generatePath();
		updatePathTestStatus();

		return TRUE;
	}
	else
	{
		return LLFloater::handleAnyMouseClick(x, y, mask, clicktype, down);
	}
}

BOOL LLFloaterPathfindingConsole::isGeneratePathMode(MASK mask, EClickType clicktype, BOOL down) const
{
	return (isShown() && (mEditTestTabContainer->getCurrentPanelIndex() == XUI_TEST_TAB_INDEX) &&
		(clicktype == LLMouseHandler::CLICK_LEFT) && down && 
		(((mask & MASK_CONTROL) && !(mask & (~MASK_CONTROL))) ||
		((mask & MASK_SHIFT) && !(mask & (~MASK_SHIFT)))));
}

LLHandle<LLFloaterPathfindingConsole> LLFloaterPathfindingConsole::getInstanceHandle()
{
	if (sInstanceHandle.isDead())
	{
		LLFloaterPathfindingConsole *floaterInstance = LLFloaterReg::getTypedInstance<LLFloaterPathfindingConsole>("pathfinding_console");
		if (floaterInstance != NULL)
		{
			sInstanceHandle = floaterInstance->mSelfHandle;
		}
	}

	return sInstanceHandle;
}

BOOL LLFloaterPathfindingConsole::isRenderPath() const
{
	return (mHasStartPoint && mHasEndPoint);
}

BOOL LLFloaterPathfindingConsole::isRenderNavMesh() const
{
	return mShowNavMeshCheckBox->get();
}

void LLFloaterPathfindingConsole::setRenderNavMesh(BOOL pIsRenderNavMesh)
{
	mShowNavMeshCheckBox->set(pIsRenderNavMesh);
}

BOOL LLFloaterPathfindingConsole::isRenderWalkables() const
{
	return mShowWalkablesCheckBox->get();
}

void LLFloaterPathfindingConsole::setRenderWalkables(BOOL pIsRenderWalkables)
{
	mShowWalkablesCheckBox->set(pIsRenderWalkables);
}

BOOL LLFloaterPathfindingConsole::isRenderStaticObstacles() const
{
	return mShowStaticObstaclesCheckBox->get();
}

void LLFloaterPathfindingConsole::setRenderStaticObstacles(BOOL pIsRenderStaticObstacles)
{
	mShowStaticObstaclesCheckBox->set(pIsRenderStaticObstacles);
}

BOOL LLFloaterPathfindingConsole::isRenderMaterialVolumes() const
{
	return mShowMaterialVolumesCheckBox->get();
}

void LLFloaterPathfindingConsole::setRenderMaterialVolumes(BOOL pIsRenderMaterialVolumes)
{
	mShowMaterialVolumesCheckBox->set(pIsRenderMaterialVolumes);
}

BOOL LLFloaterPathfindingConsole::isRenderExclusionVolumes() const
{
	return mShowExclusionVolumesCheckBox->get();
}

void LLFloaterPathfindingConsole::setRenderExclusionVolumes(BOOL pIsRenderExclusionVolumes)
{
	mShowExclusionVolumesCheckBox->set(pIsRenderExclusionVolumes);
}

BOOL LLFloaterPathfindingConsole::isRenderWorld() const
{
	return mShowWorldCheckBox->get();
}

void LLFloaterPathfindingConsole::setRenderWorld(BOOL pIsRenderWorld)
{
	mShowWorldCheckBox->set(pIsRenderWorld);
}

LLFloaterPathfindingConsole::ERenderHeatmapType LLFloaterPathfindingConsole::getRenderHeatmapType() const
{
	ERenderHeatmapType renderHeatmapType;

	switch (mShowNavMeshWalkabilityComboBox->getValue().asInteger())
	{
	case XUI_RENDER_HEATMAP_NONE :
		renderHeatmapType = kRenderHeatmapNone;
		break;
	case XUI_RENDER_HEATMAP_A :
		renderHeatmapType = kRenderHeatmapA;
		break;
	case XUI_RENDER_HEATMAP_B :
		renderHeatmapType = kRenderHeatmapB;
		break;
	case XUI_RENDER_HEATMAP_C :
		renderHeatmapType = kRenderHeatmapC;
		break;
	case XUI_RENDER_HEATMAP_D :
		renderHeatmapType = kRenderHeatmapD;
		break;
	default :
		renderHeatmapType = kRenderHeatmapNone;
		llassert(0);
		break;
	}

	return renderHeatmapType;
}

void LLFloaterPathfindingConsole::setRenderHeatmapType(ERenderHeatmapType pRenderHeatmapType)
{
	LLSD comboBoxValue;

	switch (pRenderHeatmapType)
	{
	case kRenderHeatmapNone :
		comboBoxValue = XUI_RENDER_HEATMAP_NONE;
		break;
	case kRenderHeatmapA :
		comboBoxValue = XUI_RENDER_HEATMAP_A;
		break;
	case kRenderHeatmapB :
		comboBoxValue = XUI_RENDER_HEATMAP_B;
		break;
	case kRenderHeatmapC :
		comboBoxValue = XUI_RENDER_HEATMAP_C;
		break;
	case kRenderHeatmapD :
		comboBoxValue = XUI_RENDER_HEATMAP_D;
		break;
	default :
		comboBoxValue = XUI_RENDER_HEATMAP_NONE;
		llassert(0);
		break;
	}

	return mShowNavMeshWalkabilityComboBox->setValue(comboBoxValue);
}

F32 LLFloaterPathfindingConsole::getCharacterWidth() const
{
	return mCharacterWidthSlider->getValueF32();
}

void LLFloaterPathfindingConsole::setCharacterWidth(F32 pCharacterWidth)
{
	mCharacterWidthSlider->setValue(LLSD(pCharacterWidth));
}

LLFloaterPathfindingConsole::ECharacterType LLFloaterPathfindingConsole::getCharacterType() const
{
	ECharacterType characterType;

	switch (mCharacterTypeRadioGroup->getValue().asInteger())
	{
	case XUI_CHARACTER_TYPE_A :
		characterType = kCharacterTypeA;
		break;
	case XUI_CHARACTER_TYPE_B :
		characterType = kCharacterTypeB;
		break;
	case XUI_CHARACTER_TYPE_C :
		characterType = kCharacterTypeC;
		break;
	case XUI_CHARACTER_TYPE_D :
		characterType = kCharacterTypeD;
		break;
	default :
		characterType = kCharacterTypeA;
		llassert(0);
		break;
	}

	return characterType;
}

void LLFloaterPathfindingConsole::setCharacterType(ECharacterType pCharacterType)
{
	LLSD radioGroupValue;

	switch (pCharacterType)
	{
	case kCharacterTypeA :
		radioGroupValue = XUI_CHARACTER_TYPE_A;
		break;
	case kCharacterTypeB :
		radioGroupValue = XUI_CHARACTER_TYPE_B;
		break;
	case kCharacterTypeC :
		radioGroupValue = XUI_CHARACTER_TYPE_C;
		break;
	case kCharacterTypeD :
		radioGroupValue = XUI_CHARACTER_TYPE_D;
		break;
	default :
		radioGroupValue = XUI_CHARACTER_TYPE_A;
		llassert(0);
		break;
	}

	mCharacterTypeRadioGroup->setValue(radioGroupValue);
}

LLFloaterPathfindingConsole::LLFloaterPathfindingConsole(const LLSD& pSeed)
	: LLFloater(pSeed),
	mSelfHandle(),
	mShowNavMeshCheckBox(NULL),
	mShowNavMeshWalkabilityComboBox(NULL),
	mShowWalkablesCheckBox(NULL),
	mShowStaticObstaclesCheckBox(NULL),
	mShowMaterialVolumesCheckBox(NULL),
	mShowExclusionVolumesCheckBox(NULL),
	mShowWorldCheckBox(NULL),
	mPathfindingStatus(NULL),
	mViewCharactersButton(NULL),
	mEditTestTabContainer(NULL),
	mEditTab(NULL),
	mTestTab(NULL),
	mUnfreezeLabel(NULL),
	mUnfreezeButton(NULL),
	mLinksetsLabel(NULL),
	mLinksetsButton(NULL),
	mFreezeLabel(NULL),
	mFreezeButton(NULL),
	mCharacterWidthSlider(NULL),
	mCharacterTypeRadioGroup(NULL),
	mPathTestingStatus(NULL),
	mClearPathButton(NULL),
	mNavMeshZoneSlot(),
	mNavMeshZone(),
	mAgentStateSlot(),
	mConsoleState(kConsoleStateUnknown),
	mPathData(),
	mHasStartPoint(false),
	mHasEndPoint(false),
	mHeartBeat( false )
{
	mSelfHandle.bind(this);
}

LLFloaterPathfindingConsole::~LLFloaterPathfindingConsole()
{
}

void LLFloaterPathfindingConsole::onShowWalkabilitySet()
{
	switch (getRenderHeatmapType())
	{
	case kRenderHeatmapNone :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mShowNavMeshWalkabilityComboBox->getName() << "' to RenderHeatmapNone"
			<< llendl;
		break;
	case kRenderHeatmapA :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mShowNavMeshWalkabilityComboBox->getName() << "' to RenderHeatmapA"
			<< llendl;
		break;
	case kRenderHeatmapB :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mShowNavMeshWalkabilityComboBox->getName() << "' to RenderHeatmapB"
			<< llendl;
		break;
	case kRenderHeatmapC :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mShowNavMeshWalkabilityComboBox->getName() << "' to RenderHeatmapC"
			<< llendl;
		break;
	case kRenderHeatmapD :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mShowNavMeshWalkabilityComboBox->getName() << "' to RenderHeatmapD"
			<< llendl;
		break;
	default :
		llassert(0);
		break;
	}
}

void LLFloaterPathfindingConsole::onShowWorldToggle()
{
	BOOL checkBoxValue = mShowWorldCheckBox->get();

	LLPathingLib *llPathingLibInstance = LLPathingLib::getInstance();
	if (llPathingLibInstance != NULL)
	{
		llPathingLibInstance->setRenderWorld(checkBoxValue);
	}
	else
	{
		mShowWorldCheckBox->set(FALSE);
		llwarns << "cannot find LLPathingLib instance" << llendl;
	}
}

void LLFloaterPathfindingConsole::onCharacterWidthSet()
{
	generatePath();
	updatePathTestStatus();
}

void LLFloaterPathfindingConsole::onCharacterTypeSwitch()
{
	switch (getCharacterType())
	{
	case kCharacterTypeA :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mCharacterTypeRadioGroup->getName() << "' to CharacterTypeA"
			<< llendl;
		break;
	case kCharacterTypeB :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mCharacterTypeRadioGroup->getName() << "' to CharacterTypeB"
			<< llendl;
		break;
	case kCharacterTypeC :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mCharacterTypeRadioGroup->getName() << "' to CharacterTypeC"
			<< llendl;
		break;
	case kCharacterTypeD :
		llwarns << "functionality has not yet been implemented to toggle '"
			<< mCharacterTypeRadioGroup->getName() << "' to CharacterTypeD"
			<< llendl;
		break;
	default :
		llassert(0);
		break;
	}
	updatePathTestStatus();
}

void LLFloaterPathfindingConsole::onViewCharactersClicked()
{
	LLFloaterPathfindingCharacters::openCharactersViewer();
}

void LLFloaterPathfindingConsole::onUnfreezeClicked()
{
	mUnfreezeButton->setEnabled(FALSE);
	LLPathfindingManager::getInstance()->requestSetAgentState(LLPathfindingManager::kAgentStateUnfrozen);
}

void LLFloaterPathfindingConsole::onFreezeClicked()
{
	mFreezeButton->setEnabled(FALSE);
	LLPathfindingManager::getInstance()->requestSetAgentState(LLPathfindingManager::kAgentStateFrozen);
}

void LLFloaterPathfindingConsole::onViewEditLinksetClicked()
{
	LLFloaterPathfindingLinksets::openLinksetsEditor();
}

void LLFloaterPathfindingConsole::onClearPathClicked()
{
	mHasStartPoint = false;
	mHasEndPoint = false;
	updatePathTestStatus();
}

void LLFloaterPathfindingConsole::onNavMeshZoneCB(LLPathfindingNavMeshZone::ENavMeshZoneRequestStatus pNavMeshZoneRequestStatus)
{
	switch (pNavMeshZoneRequestStatus)
	{
	case LLPathfindingNavMeshZone::kNavMeshZoneRequestUnknown :
		setConsoleState(kConsoleStateUnknown);
		break;
	case LLPathfindingNavMeshZone::kNavMeshZoneRequestStarted :
		setConsoleState(kConsoleStateDownloading);
		break;
	case LLPathfindingNavMeshZone::kNavMeshZoneRequestCompleted :
		setConsoleState(kConsoleStateHasNavMesh);
		break;
	case LLPathfindingNavMeshZone::kNavMeshZoneRequestNotEnabled :
		setConsoleState(kConsoleStateRegionNotEnabled);
		break;
	case LLPathfindingNavMeshZone::kNavMeshZoneRequestError :
		setConsoleState(kConsoleStateError);
		break;
	default:
		setConsoleState(kConsoleStateUnknown);
		llassert(0);
		break;
	}
}

void LLFloaterPathfindingConsole::onAgentStateCB(LLPathfindingManager::EAgentState pAgentState)
{
	setAgentState(pAgentState);
}

void LLFloaterPathfindingConsole::setConsoleState(EConsoleState pConsoleState)
{
	mConsoleState = pConsoleState;
	updateControlsOnConsoleState();
	updateStatusOnConsoleState();
}

void LLFloaterPathfindingConsole::updateControlsOnConsoleState()
{
	switch (mConsoleState)
	{
	case kConsoleStateUnknown :
	case kConsoleStateLibraryNotImplemented :
	case kConsoleStateRegionNotEnabled :
		mShowNavMeshCheckBox->setEnabled(FALSE);
		mShowNavMeshWalkabilityComboBox->setEnabled(FALSE);
		mShowWalkablesCheckBox->setEnabled(FALSE);
		mShowStaticObstaclesCheckBox->setEnabled(FALSE);
		mShowMaterialVolumesCheckBox->setEnabled(FALSE);
		mShowExclusionVolumesCheckBox->setEnabled(FALSE);
		mShowWorldCheckBox->setEnabled(FALSE);
		mViewCharactersButton->setEnabled(FALSE);
		mEditTestTabContainer->selectTab(0);
		mTestTab->setEnabled(FALSE);
		mCharacterWidthSlider->setEnabled(FALSE);
		mCharacterTypeRadioGroup->setEnabled(FALSE);
		mClearPathButton->setEnabled(FALSE);
		mHasStartPoint = false;
		mHasEndPoint = false;
		break;
	case kConsoleStateDownloading :
	case kConsoleStateError :
		mShowNavMeshCheckBox->setEnabled(FALSE);
		mShowNavMeshWalkabilityComboBox->setEnabled(FALSE);
		mShowWalkablesCheckBox->setEnabled(FALSE);
		mShowStaticObstaclesCheckBox->setEnabled(FALSE);
		mShowMaterialVolumesCheckBox->setEnabled(FALSE);
		mShowExclusionVolumesCheckBox->setEnabled(FALSE);
		mShowWorldCheckBox->setEnabled(FALSE);
		mViewCharactersButton->setEnabled(TRUE);
		mEditTestTabContainer->selectTab(0);
		mTestTab->setEnabled(FALSE);
		mCharacterWidthSlider->setEnabled(FALSE);
		mCharacterTypeRadioGroup->setEnabled(FALSE);
		mClearPathButton->setEnabled(FALSE);
		mHasStartPoint = false;
		mHasEndPoint = false;
		break;
	case kConsoleStateHasNavMesh :
	case kConsoleStateHasNavMeshDownloading :
		mShowNavMeshCheckBox->setEnabled(TRUE);
		mShowNavMeshWalkabilityComboBox->setEnabled(TRUE);
		mShowWalkablesCheckBox->setEnabled(TRUE);
		mShowStaticObstaclesCheckBox->setEnabled(TRUE);
		mShowMaterialVolumesCheckBox->setEnabled(TRUE);
		mShowExclusionVolumesCheckBox->setEnabled(TRUE);
		mShowWorldCheckBox->setEnabled(TRUE);
		mViewCharactersButton->setEnabled(TRUE);
		mTestTab->setEnabled(TRUE);
		mCharacterWidthSlider->setEnabled(TRUE);
		mCharacterTypeRadioGroup->setEnabled(TRUE);
		mClearPathButton->setEnabled(TRUE);
		mTestTab->setEnabled(TRUE);
		break;
	default :
		llassert(0);
		break;
	}
}

void LLFloaterPathfindingConsole::updateStatusOnConsoleState()
{
	static const LLColor4 warningColor = LLUIColorTable::instance().getColor("DrYellow");

	std::string statusText("");
	LLStyle::Params styleParams;

	switch (mConsoleState)
	{
	case kConsoleStateUnknown :
		statusText = getString("navmesh_status_unknown");
		break;
	case kConsoleStateLibraryNotImplemented :
		statusText = getString("navmesh_status_library_not_implemented");
		styleParams.color = warningColor;
		break;
	case kConsoleStateRegionNotEnabled :
		statusText = getString("navmesh_status_region_not_enabled");
		styleParams.color = warningColor;
		break;
	case kConsoleStateDownloading :
		statusText = getString("navmesh_status_downloading");
		break;
	case kConsoleStateHasNavMesh :
		statusText = getString("navmesh_status_has_navmesh");
		break;
	case kConsoleStateHasNavMeshDownloading :
		statusText = getString("navmesh_status_has_navmesh_downloading");
		break;
	case kConsoleStateError :
		statusText = getString("navmesh_status_error");
		styleParams.color = warningColor;
		break;
	default :
		statusText = getString("navmesh_status_unknown");
		llassert(0);
		break;
	}

	mPathfindingStatus->setText((LLStringExplicit)statusText, styleParams);
}

void LLFloaterPathfindingConsole::setAgentState(LLPathfindingManager::EAgentState pAgentState)
{
	switch (LLPathfindingManager::getInstance()->getLastKnownNonErrorAgentState())
	{
	case LLPathfindingManager::kAgentStateUnknown :
	case LLPathfindingManager::kAgentStateNotEnabled :
		mEditTab->setEnabled(FALSE);
		mUnfreezeLabel->setEnabled(FALSE);
		mUnfreezeButton->setEnabled(FALSE);
		mLinksetsLabel->setEnabled(FALSE);
		mLinksetsButton->setEnabled(FALSE);
		mFreezeLabel->setEnabled(FALSE);
		mFreezeButton->setEnabled(FALSE);
		break;
	case LLPathfindingManager::kAgentStateFrozen :
		mEditTab->setEnabled(TRUE);
		mUnfreezeLabel->setEnabled(TRUE);
		mUnfreezeButton->setEnabled(TRUE);
		mLinksetsLabel->setEnabled(FALSE);
		mLinksetsButton->setEnabled(FALSE);
		mFreezeLabel->setEnabled(FALSE);
		mFreezeButton->setEnabled(FALSE);
		break;
	case LLPathfindingManager::kAgentStateUnfrozen :
		mEditTab->setEnabled(TRUE);
		mUnfreezeLabel->setEnabled(FALSE);
		mUnfreezeButton->setEnabled(FALSE);
		mLinksetsLabel->setEnabled(TRUE);
		mLinksetsButton->setEnabled(TRUE);
		mFreezeLabel->setEnabled(TRUE);
		mFreezeButton->setEnabled(TRUE);
		break;
	default :
		llassert(0);
		break;
	}
}
 
void LLFloaterPathfindingConsole::generatePath()
{
	if (mHasStartPoint && mHasEndPoint)
	{
		mPathData.mCharacterWidth = getCharacterWidth();
		LLPathingLib::getInstance()->generatePath(mPathData);
	}
}

void LLFloaterPathfindingConsole::updatePathTestStatus()
{
	static const LLColor4 warningColor = LLUIColorTable::instance().getColor("DrYellow");

	std::string statusText("");
	LLStyle::Params styleParams;

	if (!mHasStartPoint && !mHasEndPoint)
	{
		statusText = getString("pathing_choose_start_and_end_points");
		styleParams.color = warningColor;
	}
	else if (!mHasStartPoint && mHasEndPoint)
	{
		statusText = getString("pathing_choose_start_point");
		styleParams.color = warningColor;
	}
	else if (mHasStartPoint && !mHasEndPoint)
	{
		statusText = getString("pathing_choose_end_point");
		styleParams.color = warningColor;
	}
	else
	{
		statusText = getString("pathing_path_valid");
	}

	mPathTestingStatus->setText((LLStringExplicit)statusText, styleParams);
}


BOOL LLFloaterPathfindingConsole::isRenderAnyShapes() const
{
	if ( isRenderWalkables() || isRenderStaticObstacles() ||
		 isRenderMaterialVolumes() ||  isRenderExclusionVolumes() )
	{
		return true;
	}
	
	return false;
}

U32 LLFloaterPathfindingConsole::getRenderShapeFlags()
{
	resetShapeRenderFlags();

	if ( isRenderWalkables() )			
	{ 
		setShapeRenderFlag( LLPathingLib::LLST_WalkableObjects ); 
	}
	if ( isRenderStaticObstacles() )	
	{ 
		setShapeRenderFlag( LLPathingLib::LLST_ObstacleObjects ); 
	}
	if ( isRenderMaterialVolumes() )	
	{ 
		setShapeRenderFlag( LLPathingLib::LLST_MaterialPhantoms ); 
	}
	if ( isRenderExclusionVolumes() )	
	{ 
		setShapeRenderFlag( LLPathingLib::LLST_ExclusionPhantoms ); 
	}
	return mShapeRenderFlags;
}
