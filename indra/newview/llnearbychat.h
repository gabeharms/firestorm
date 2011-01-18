/** 
 * @file llnearbychat.h
 * @brief nearby chat history scrolling panel implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#ifndef LL_LLNEARBYCHAT_H_
#define LL_LLNEARBYCHAT_H_

#include "lldockablefloater.h"
#include "llscrollbar.h"
#include "llviewerchat.h"

class LLResizeBar;
class LLChatHistory;
class LLLineEditor;

class LLNearbyChat: public LLDockableFloater
{
public:
	LLNearbyChat(const LLSD& key);
	~LLNearbyChat();

	BOOL	postBuild			();

	/** @param archive true - to save a message to the chat history log */
	void	addMessage			(const LLChat& message,bool archive = true, const LLSD &args = LLSD());	
	void	onNearbyChatContextMenuItemClicked(const LLSD& userdata);
	bool	onNearbyChatCheckContextMenuItem(const LLSD& userdata);

	virtual BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	virtual void	draw();

	// focus overrides
	/*virtual*/ void	onFocusLost();
	/*virtual*/ void	onFocusReceived();
	
	/*virtual*/ void	onOpen	(const LLSD& key);

	/*virtual*/ void	setVisible(BOOL visible);
	void	openFloater(const LLSD& key);

	virtual void setRect		(const LLRect &rect);

	virtual void updateChatHistoryStyle();

	static void processChatHistoryStyleUpdate(const LLSD& newvalue);

	void loadHistory();

	static LLNearbyChat* getInstance();
	
	static bool isChatMultiTab();
	
	void setDocked(bool docked, bool pop_on_undock = true);
	
	BOOL getVisible();
	static void onSendMsg( LLUICtrl*, void*);
	void sendMsg();

private:
	virtual void    applySavedVariables();

	void	getAllowedRect		(LLRect& rect);

	void	onNearbySpeakers	();
	
	void	setTyping(bool typing);
	static void		onInputEditorFocusReceived( LLFocusableElement* caller, void* userdata );
	static void		onInputEditorFocusLost(LLFocusableElement* caller, void* userdata);
	static void		onInputEditorKeystroke(LLLineEditor* caller, void* userdata);
	static BOOL		matchChatTypeTrigger(const std::string& in_str, std::string* out_str);

private:
	LLHandle<LLView>	mPopupMenuHandle;
	LLChatHistory*		mChatHistory;
	
	std::vector<LLChat> mMessageArchive;
	LLLineEditor* mInputEditor;
};

#endif


