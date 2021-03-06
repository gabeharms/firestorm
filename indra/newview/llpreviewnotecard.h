/** 
 * @file llpreviewnotecard.h
 * @brief LLPreviewNotecard class header file
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

#ifndef LL_LLPREVIEWNOTECARD_H
#define LL_LLPREVIEWNOTECARD_H

#include "llpreview.h"
#include "llassetstorage.h"
#include "lliconctrl.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPreviewNotecard
//
// This class allows us to edit notecards
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLViewerTextEditor;
class LLButton;
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-11-05 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
class LLTextEditor;
// [/SL:KB]

class LLPreviewNotecard : public LLPreview
{
public:
	LLPreviewNotecard(const LLSD& key);
	virtual ~LLPreviewNotecard();
	
	bool saveItem();

	// llview
	virtual void draw();
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-11-05 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
	virtual bool hasAccelerators() const;
// [/SL:KB]
	virtual BOOL handleKeyHere(KEY key, MASK mask);
	virtual void setEnabled( BOOL enabled );

	// llfloater
	virtual BOOL canClose();

	// llpanel
	virtual BOOL postBuild();

	// reach into the text editor, and grab the drag item
	const LLInventoryItem* getDragItem();

// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-11-05 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
	LLTextEditor* getEditor();
// [/SL:KB]

	// return true if there is any embedded inventory.
	bool hasEmbeddedInventory();

	// After saving a notecard, the tcp based upload system will
	// change the asset, therefore, we need to re-fetch it from the
	// asset system. :(
	void refreshFromInventory(const LLUUID& item_id = LLUUID::null);

	// <FS:Ansariel> FIRE-9039: Close notecard after choosing "Save" in close confirmation
	void checkCloseAfterSave();

protected:

	void updateTitleButtons();
	virtual void loadAsset();
	bool saveIfNeeded(LLInventoryItem* copyitem = NULL);

	void deleteNotecard();

	static void onLoadComplete(LLVFS *vfs,
							   const LLUUID& asset_uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);

	static void onClickSave(void* data);

	static void onClickDelete(void* data);

	static void onSaveComplete(const LLUUID& asset_uuid,
							   void* user_data,
							   S32 status, LLExtStat ext_status);

	bool handleSaveChangesDialog(const LLSD& notification, const LLSD& response);
	
	// [FS:CR] FIRE-2700
	bool handleDeleteChangesDialog(const LLSD& notification, const LLSD& response);
	static void onConfirmDelete(void* user_data);
	// [FS:CR]

	// <FS:Ansariel> FIRE-13969: Search button
	void onSearchButtonClicked();

protected:
	LLViewerTextEditor* mEditor;
	LLButton* mSaveBtn;

	LLUUID mAssetID;

	LLUUID mObjectID;
};


#endif // LL_LLPREVIEWNOTECARD_H
