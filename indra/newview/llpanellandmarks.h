/**
 * @file llpanellandmarks.h
 * @brief Landmarks tab for Side Bar "Places" panel
 * class definition
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef LL_LLPANELLANDMARKS_H
#define LL_LLPANELLANDMARKS_H

#include "lllandmark.h"

// newview
#include "llinventorymodel.h"
#include "llpanelplacestab.h"
#include "llpanelpick.h"
#include "llremoteparcelrequest.h"

class LLFolderViewItem;
class LLMenuGL;
class LLInventoryPanel;
class LLInventorySubTreePanel;

class LLLandmarksPanel : public LLPanelPlacesTab, LLRemoteParcelInfoObserver
{
public:
	LLLandmarksPanel();
	virtual ~LLLandmarksPanel();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onSearchEdit(const std::string& string);
	/*virtual*/ void onShowOnMap();
	/*virtual*/ void onTeleport();
	/*virtual*/ void updateVerbs();

	void onSelectionChange(LLInventorySubTreePanel* inventory_list, const std::deque<LLFolderViewItem*> &items, BOOL user_action);
	void onSelectorButtonClicked();
	
protected:
	/**
	 * @return true - if current selected panel is not null and selected item is a landmark
	 */
	bool isLandmarkSelected() const;
	LLLandmark* getCurSelectedLandmark() const;
	LLFolderViewItem* getCurSelectedItem () const;

	//LLRemoteParcelInfoObserver interface
	/*virtual*/ void processParcelInfo(const LLParcelData& parcel_data);
	/*virtual*/ void setParcelID(const LLUUID& parcel_id);
	/*virtual*/ void setErrorStatus(U32 status, const std::string& reason);
	
private:
	void initFavoritesInventroyPanel();
	void initLandmarksInventroyPanel();
	void initMyInventroyPanel();
	void initLibraryInventroyPanel();
	void initLandmarksPanel(LLInventorySubTreePanel* inventory_list, const LLUUID& start_folder_id);
	void initAccordion(const std::string& accordion_tab_name, LLInventorySubTreePanel* inventory_list);
	void onAccordionExpandedCollapsed(const LLSD& param, LLInventorySubTreePanel* inventory_list);
	void deselectOtherThan(const LLInventorySubTreePanel* inventory_list);

	// List Commands Handlers
	void initListCommandsHandlers();
	void updateListCommands();
	void onActionsButtonClick();
	void onAddLandmarkButtonClick() const;
	void onAddFolderButtonClick() const;
	void onTrashButtonClick() const;
	void onAddAction(const LLSD& command_name) const;
	void onCopyPasteAction(const LLSD& command_name) const;
	void onFoldingAction(const LLSD& command_name) const;
	bool isActionEnabled(const LLSD& command_name) const;
	void onCustomAction(const LLSD& command_name);
	void onPickPanelExit( LLPanelPick* pick_panel, LLView* owner, const LLSD& params);

private:
	LLInventorySubTreePanel*	mFavoritesInventoryPanel;
	LLInventorySubTreePanel*	mLandmarksInventoryPanel;
	LLInventorySubTreePanel*	mMyInventoryPanel;
	LLInventorySubTreePanel*	mLibraryInventoryPanel;
	LLMenuGL*					mGearLandmarkMenu;
	LLMenuGL*					mGearFolderMenu;
	LLInventorySubTreePanel*	mCurrentSelectedList;

	LLPanel*					mListCommands;
};

#endif //LL_LLPANELLANDMARKS_H
