/** 
 * @file llpanelplaces.h
 * @brief Side Bar "Places" panel
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

#ifndef LL_LLPANELPLACES_H
#define LL_LLPANELPLACES_H

#include "llpanel.h"

class LLInventoryItem;
class LLFilterEditor;
class LLLandmark;

class LLPanelLandmarkInfo;
class LLPanelPlaceProfile;

class LLPanelPickEdit;
class LLPanelPlaceInfo;
class LLPanelPlacesTab;
class LLParcelSelection;
class LLPlacesInventoryObserver;
class LLPlacesParcelObserver;
class LLTabContainer;
class LLToggleableMenu;

typedef std::pair<LLUUID, std::string>	folder_pair_t;

class LLPanelPlaces : public LLPanel
{
public:
	LLPanelPlaces();
	virtual ~LLPanelPlaces();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);

	// Called on parcel selection change to update place information.
	void changedParcelSelection();
	// Called on agent inventory change to find out when inventory gets usable.
	void changedInventory(U32 mask);

	void setItem(LLInventoryItem* item);

private:
	void onLandmarkLoaded(LLLandmark* landmark);
	void onFilterEdit(const std::string& search_string, bool force_filter);
	void onTabSelected();

	//void onShareButtonClicked();
	void onTeleportButtonClicked();
	void onShowOnMapButtonClicked();
	void onEditButtonClicked();
	void onSaveButtonClicked();
	void onCancelButtonClicked();
	void onOverflowButtonClicked();
	void onOverflowMenuItemClicked(const LLSD& param);
	bool onOverflowMenuItemEnable(const LLSD& param);
	void onCreateLandmarkButtonClicked(const LLUUID& folder_id);
	void onBackButtonClicked();

	void toggleMediaPanel();
	void togglePickPanel(BOOL visible);
	void togglePlaceInfoPanel(BOOL visible);

	void updateVerbs();

	LLPanelPlaceInfo* getCurrentInfoPanel();

	LLFilterEditor*				mFilterEditor;
	LLPanelPlacesTab*			mActivePanel;
	LLTabContainer*				mTabContainer;
	LLPanelPlaceProfile*		mPlaceProfile;
	LLPanelLandmarkInfo*		mLandmarkInfo;

	LLPanelPickEdit*			mPickPanel;
	LLToggleableMenu*			mPlaceMenu;
	LLToggleableMenu*			mLandmarkMenu;

	LLButton*					mTeleportBtn;
	LLButton*					mShowOnMapBtn;
	LLButton*					mShareBtn;
	LLButton*					mEditBtn;
	LLButton*					mSaveBtn;
	LLButton*					mCancelBtn;
	LLButton*					mCloseBtn;
	LLButton*					mOverflowBtn;

	LLPlacesInventoryObserver*	mInventoryObserver;
	LLPlacesParcelObserver*		mParcelObserver;

	// Pointer to a landmark item or to a linked landmark
	LLPointer<LLInventoryItem>	mItem;

	// Absolute position of the location for teleport, may not
	// be available (hence zero)
	LLVector3d					mPosGlobal;

	// Search string for filtering landmarks and teleport
	// history locations
	std::string					mFilterSubString;

	// Information type currently shown in Place Information panel
	std::string					mPlaceInfoType;

	bool						isLandmarkEditModeOn;

	LLSafeHandle<LLParcelSelection>	mParcel;
};

#endif //LL_LLPANELPLACES_H
