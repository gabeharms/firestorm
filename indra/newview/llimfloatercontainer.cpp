/** 
 * @file llimfloatercontainer.cpp
 * @brief Multifloater containing active IM sessions in separate tab container tabs
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llimfloater.h"
#include "llimfloatercontainer.h"

#include "llfloaterreg.h"
#include "lllayoutstack.h"
#include "llnearbychat.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llavatariconctrl.h"
#include "llavatarnamecache.h"
#include "llgroupiconctrl.h"
#include "llfloateravatarpicker.h"
#include "llimview.h"
#include "lltransientfloatermgr.h"
#include "llviewercontrol.h"

//
// LLIMFloaterContainer
//
LLIMFloaterContainer::LLIMFloaterContainer(const LLSD& seed)
:	LLMultiFloater(seed),
	mExpandCollapseBtn(NULL),
	mConversationsRoot(NULL)
{
	// Firstly add our self to IMSession observers, so we catch session events
    LLIMMgr::getInstance()->addSessionObserver(this);

	mAutoResize = FALSE;
	LLTransientFloaterMgr::getInstance()->addControlView(LLTransientFloaterMgr::IM, this);
}

LLIMFloaterContainer::~LLIMFloaterContainer()
{
	mNewMessageConnection.disconnect();
	LLTransientFloaterMgr::getInstance()->removeControlView(LLTransientFloaterMgr::IM, this);

	gSavedPerAccountSettings.setBOOL("ConversationsListPaneCollapsed", mConversationsPane->isCollapsed());
	gSavedPerAccountSettings.setBOOL("ConversationsMessagePaneCollapsed", mMessagesPane->isCollapsed());

	if (!LLSingleton<LLIMMgr>::destroyed())
	{
		LLIMMgr::getInstance()->removeSessionObserver(this);
	}
}

void LLIMFloaterContainer::sessionVoiceOrIMStarted(const LLUUID& session_id)
{
	LLIMFloater::show(session_id);
}

void LLIMFloaterContainer::sessionRemoved(const LLUUID& session_id)
{
	LLIMFloater* floaterp = LLIMFloater::findInstance(session_id);
	LLFloater::onClickClose(floaterp);
	removeConversationListItem(floaterp);
}

BOOL LLIMFloaterContainer::postBuild()
{
	mNewMessageConnection = LLIMModel::instance().mNewMsgSignal.connect(boost::bind(&LLIMFloaterContainer::onNewMessageReceived, this, _1));
	// Do not call base postBuild to not connect to mCloseSignal to not close all floaters via Close button
	// mTabContainer will be initialized in LLMultiFloater::addChild()
	
	setTabContainer(getChild<LLTabContainer>("im_box_tab_container"));

	mConversationsStack = getChild<LLLayoutStack>("conversations_stack");
	mConversationsPane = getChild<LLLayoutPanel>("conversations_layout_panel");
	mMessagesPane = getChild<LLLayoutPanel>("messages_layout_panel");
	
	mConversationsListPanel = getChild<LLPanel>("conversations_list_panel");

	// CHUI-98 : View Model for conversations
	LLConversationItem* base_item = new LLConversationItem();
	LLFolderView::Params p;
	p.view_model = &mConversationViewModel;
	p.parent_panel = mConversationsListPanel;
	p.rect = mConversationsListPanel->getLocalRect();
	p.follows.flags = FOLLOWS_ALL;
	p.listener = base_item;

	mConversationsRoot = LLUICtrlFactory::create<LLFolderView>(p);
	mConversationsListPanel->addChild(mConversationsRoot);

	mExpandCollapseBtn = getChild<LLButton>("expand_collapse_btn");
	mExpandCollapseBtn->setClickedCallback(boost::bind(&LLIMFloaterContainer::onExpandCollapseButtonClicked, this));

	childSetAction("add_btn", boost::bind(&LLIMFloaterContainer::onAddButtonClicked, this));

	collapseMessagesPane(gSavedPerAccountSettings.getBOOL("ConversationsMessagePaneCollapsed"));
	collapseConversationsPane(gSavedPerAccountSettings.getBOOL("ConversationsListPaneCollapsed"));
	LLAvatarNameCache::addUseDisplayNamesCallback(
			boost::bind(&LLIMConversation::processChatHistoryStyleUpdate));

	return TRUE;
}

void LLIMFloaterContainer::onOpen(const LLSD& key)
{
	LLMultiFloater::onOpen(key);
}

// virtual
void LLIMFloaterContainer::addFloater(LLFloater* floaterp, 
									BOOL select_added_floater, 
									LLTabContainer::eInsertionPoint insertion_point)
{
	if(!floaterp) return;

	// already here
	if (floaterp->getHost() == this)
	{
		openFloater(floaterp->getKey());
		return;
	}
	
	// Make sure the message panel is open when adding a floater or it stays mysteriously hidden
	collapseMessagesPane(false);

	// Add the floater
	LLMultiFloater::addFloater(floaterp, select_added_floater, insertion_point);

	LLUUID session_id = floaterp->getKey();
	
	LLIconCtrl* icon = 0;

	if(gAgent.isInGroup(session_id, TRUE))
	{
		LLGroupIconCtrl::Params icon_params;
		icon_params.group_id = session_id;
		icon = LLUICtrlFactory::instance().create<LLGroupIconCtrl>(icon_params);

		mSessions[session_id] = floaterp;
		floaterp->mCloseSignal.connect(boost::bind(&LLIMFloaterContainer::onCloseFloater, this, session_id));
	}
	else
	{
		LLUUID avatar_id = LLIMModel::getInstance()->getOtherParticipantID(session_id);

		LLAvatarIconCtrl::Params icon_params;
		icon_params.avatar_id = avatar_id;
		icon = LLUICtrlFactory::instance().create<LLAvatarIconCtrl>(icon_params);

		mSessions[session_id] = floaterp;
		floaterp->mCloseSignal.connect(boost::bind(&LLIMFloaterContainer::onCloseFloater, this, session_id));
	}
	mTabContainer->setTabImage(floaterp, icon);
}


void LLIMFloaterContainer::onCloseFloater(LLUUID& id)
{
	mSessions.erase(id);
}

// virtual
void LLIMFloaterContainer::computeResizeLimits(S32& new_min_width, S32& new_min_height)
{
	bool is_left_pane_expanded = !mConversationsPane->isCollapsed();
	bool is_right_pane_expanded = !mMessagesPane->isCollapsed();

	S32 conversations_pane_min_dim = mConversationsPane->getMinDim();

	if (is_right_pane_expanded)
	{
		S32 conversations_pane_width =
				(is_left_pane_expanded ? gSavedPerAccountSettings.getS32("ConversationsListPaneWidth") : conversations_pane_min_dim);

		// possibly increase minimum size constraint due to children's minimums.
		for (S32 tab_idx = 0; tab_idx < mTabContainer->getTabCount(); ++tab_idx)
		{
			LLFloater* floaterp = dynamic_cast<LLFloater*>(mTabContainer->getPanelByIndex(tab_idx));
			if (floaterp)
			{
				new_min_width = llmax(new_min_width,
						floaterp->getMinWidth() + conversations_pane_width + LLPANEL_BORDER_WIDTH * 2);
				new_min_height = llmax(new_min_height, floaterp->getMinHeight());
			}
		}
	}
	else
	{
		new_min_width = conversations_pane_min_dim;
	}
}

void LLIMFloaterContainer::onNewMessageReceived(const LLSD& data)
{
	LLUUID session_id = data["session_id"].asUUID();
	LLFloater* floaterp = get_ptr_in_map(mSessions, session_id);
	LLFloater* current_floater = LLMultiFloater::getActiveFloater();

	if(floaterp && current_floater && floaterp != current_floater)
	{
		if(LLMultiFloater::isFloaterFlashing(floaterp))
			LLMultiFloater::setFloaterFlashing(floaterp, FALSE);
		LLMultiFloater::setFloaterFlashing(floaterp, TRUE);
	}
}

void LLIMFloaterContainer::onExpandCollapseButtonClicked()
{
	if (mConversationsPane->isCollapsed() && mMessagesPane->isCollapsed()
			&& gSavedPerAccountSettings.getBOOL("ConversationsExpandMessagePaneFirst"))
	{
		// Expand the messages pane from ultra minimized state
		// if it was collapsed last in order.
		collapseMessagesPane(false);
	}
	else
	{
		collapseConversationsPane(!mConversationsPane->isCollapsed());
	}
}

LLIMFloaterContainer* LLIMFloaterContainer::findInstance()
{
	return LLFloaterReg::findTypedInstance<LLIMFloaterContainer>("im_container");
}

LLIMFloaterContainer* LLIMFloaterContainer::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLIMFloaterContainer>("im_container");
}

void LLIMFloaterContainer::setMinimized(BOOL b)
{
	if (isMinimized() == b) return;
	
	LLMultiFloater::setMinimized(b);

	if (isMinimized()) return;

	if (getActiveFloater())
	{
		getActiveFloater()->setVisible(TRUE);
	}
}

void LLIMFloaterContainer::draw()
{
	if (mTabContainer->getTabCount() == 0)
	{
		// Do not close the container when every conversation is torn off because the user
		// still needs the conversation list. Simply collapse the message pane in that case.
		collapseMessagesPane(true);
	}
	LLFloater::draw();

	repositioningWidgets();
}

void LLIMFloaterContainer::tabClose()
{
	if (mTabContainer->getTabCount() == 0)
	{
		// Do not close the container when every conversation is torn off because the user
		// still needs the conversation list. Simply collapse the message pane in that case.
		collapseMessagesPane(true);
	}
}

void LLIMFloaterContainer::setVisible(BOOL visible)
{
	if (visible)
	{
		// Make sure we have the Nearby Chat present when showing the conversation container
		LLFloater* nearby_chat = LLFloaterReg::findInstance("chat_bar");
		if (nearby_chat == NULL)
		{
			// If not found, force the creation of the nearby chat conversation panel
			// *TODO: find a way to move this to XML as a default panel or something like that
			LLSD name("chat_bar");
			LLFloaterReg::toggleInstanceOrBringToFront(name);
		}
	}

	// We need to show/hide all the associated conversations that have been torn off
	// (and therefore, are not longer managed by the multifloater),
	// so that they show/hide with the conversations manager.
	conversations_items_map::iterator item_it = mConversationsItems.begin();
	for (;item_it != mConversationsItems.end(); ++item_it)
	{
		LLConversationItem* item = item_it->second;
		item->setVisibleIfDetached(visible);
	}
	
	// Now, do the normal multifloater show/hide
	LLMultiFloater::setVisible(visible);
	
}

void LLIMFloaterContainer::collapseMessagesPane(bool collapse)
{
	if (mMessagesPane->isCollapsed() == collapse)
	{
		return;
	}

	if (collapse)
	{
		// Save the messages pane width before collapsing it.
		gSavedPerAccountSettings.setS32("ConversationsMessagePaneWidth", mMessagesPane->getRect().getWidth());

		// Save the order in which the panels are closed to reverse user's last action.
		gSavedPerAccountSettings.setBOOL("ConversationsExpandMessagePaneFirst", mConversationsPane->isCollapsed());
	}

	// Show/hide the messages pane.
	mConversationsStack->collapsePanel(mMessagesPane, collapse);

	updateState(collapse, gSavedPerAccountSettings.getS32("ConversationsMessagePaneWidth"));
}

void LLIMFloaterContainer::collapseConversationsPane(bool collapse)
{
	if (mConversationsPane->isCollapsed() == collapse)
	{
		return;
	}

	LLView* button_panel = getChild<LLView>("conversations_pane_buttons_expanded");
	button_panel->setVisible(!collapse);
	mExpandCollapseBtn->setImageOverlay(getString(collapse ? "expand_icon" : "collapse_icon"));

	if (collapse)
	{
		// Save the conversations pane width before collapsing it.
		gSavedPerAccountSettings.setS32("ConversationsListPaneWidth", mConversationsPane->getRect().getWidth());

		// Save the order in which the panels are closed to reverse user's last action.
		gSavedPerAccountSettings.setBOOL("ConversationsExpandMessagePaneFirst", !mMessagesPane->isCollapsed());
	}

	mConversationsStack->collapsePanel(mConversationsPane, collapse);

	S32 collapsed_width = mConversationsPane->getMinDim();
	updateState(collapse, gSavedPerAccountSettings.getS32("ConversationsListPaneWidth") - collapsed_width);
}

void LLIMFloaterContainer::updateState(bool collapse, S32 delta_width)
{
	LLRect floater_rect = getRect();
	floater_rect.mRight += ((collapse ? -1 : 1) * delta_width);

	// Set by_user = true so that reshaped rect is saved in user_settings.
	setShape(floater_rect, true);

	updateResizeLimits();

	bool is_left_pane_expanded = !mConversationsPane->isCollapsed();
	bool is_right_pane_expanded = !mMessagesPane->isCollapsed();

	setCanResize(is_left_pane_expanded || is_right_pane_expanded);
	setCanMinimize(is_left_pane_expanded || is_right_pane_expanded);
}

void LLIMFloaterContainer::onAddButtonClicked()
{
    LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&LLIMFloaterContainer::onAvatarPicked, this, _1), TRUE, TRUE);
    LLFloater* root_floater = gFloaterView->getParentFloater(this);
    if (picker && root_floater)
    {
        root_floater->addDependentFloater(picker);
    }
}

void LLIMFloaterContainer::onAvatarPicked(const uuid_vec_t& ids)
{
    if (ids.size() == 1)
    {
        LLAvatarActions::startIM(ids.back());
    }
    else
    {
        LLAvatarActions::startConference(ids);
    }
}

void LLIMFloaterContainer::repositioningWidgets()
{
	LLRect panel_rect = mConversationsListPanel->getRect();
	S32 item_height = 16;
	int index = 0;
	for (conversations_widgets_map::iterator widget_it = mConversationsWidgets.begin();
		 widget_it != mConversationsWidgets.end();
		 widget_it++, ++index)
	{
		LLFolderViewItem* widget = widget_it->second;
		widget->setRect(LLRect(0,
							   panel_rect.getHeight() - item_height*index,
							   panel_rect.getWidth(),
							   panel_rect.getHeight() - item_height*(index+1)));
	}
}

// CHUI-137 : Temporary implementation of conversations list
void LLIMFloaterContainer::addConversationListItem(std::string name, const LLUUID& uuid, LLFloater* floaterp)
{
	// Check if the item is not already in the list, exit if it is and has the same name and uuid (nothing to do)
	// Note: this happens often, when reattaching a torn off conversation for instance
	conversations_items_map::iterator item_it = mConversationsItems.find(floaterp);
	if (item_it != mConversationsItems.end())
	{
		LLConversationItem* item = item_it->second;
		// Check if the item has changed
		if (item->hasSameValues(name,uuid))
		{
			// If it hasn't changed, nothing to do -> exit
			return;
		}
	}
	
	// Remove the conversation item that might exist already: it'll be recreated anew further down anyway
	// and nothing wrong will happen removing it if it doesn't exist
	removeConversationListItem(floaterp,false);

	// Create a conversation item
	LLConversationItem* item = new LLConversationItem(name, uuid, floaterp, this);
	mConversationsItems[floaterp] = item;

	// Create a widget from it
	LLFolderViewItem* widget = createConversationItemWidget(item);
	mConversationsWidgets[floaterp] = widget;

	// Add a new conversation widget to the root folder of a folder view.
	mConversationsRoot->addItem(widget);

	// Add it to the UI
	widget->setVisible(TRUE);

	repositioningWidgets();

	mConversationsListPanel->addChild(widget);

	return;
}

void LLIMFloaterContainer::removeConversationListItem(LLFloater* floaterp, bool change_focus)
{
	// Delete the widget and the associated conversation item
	// Note : since the mConversationsItems is also the listener to the widget, deleting 
	// the widget will also delete its listener
	conversations_widgets_map::iterator widget_it = mConversationsWidgets.find(floaterp);
	if (widget_it != mConversationsWidgets.end())
	{
		LLFolderViewItem* widget = widget_it->second;
		widget->destroyView();
	}
	
	// Suppress the conversation items and widgets from their respective maps
	mConversationsItems.erase(floaterp);
	mConversationsWidgets.erase(floaterp);

	repositioningWidgets();
	
	// Don't let the focus fall IW, select and refocus on the first conversation in the list
	if (change_focus)
	{
		setFocus(TRUE);
		conversations_items_map::iterator item_it = mConversationsItems.begin();
		if (item_it != mConversationsItems.end())
		{
			LLConversationItem* item = item_it->second;
			item->selectItem();
		}
	}
	return;
}

LLFloater* LLIMFloaterContainer::findConversationItem(LLUUID& uuid)
{
	LLFloater* floaterp = NULL;
	for (conversations_items_map::iterator item_it = mConversationsItems.begin(); item_it != mConversationsItems.end(); ++item_it)
	{
		LLConversationItem* item = item_it->second;
		if (item->hasSameValue(uuid))
		{
			floaterp = item_it->first;
			break;
		}
	}
	return floaterp;
}

LLFolderViewItem* LLIMFloaterContainer::createConversationItemWidget(LLConversationItem* item)
{
	LLFolderViewItem::Params params;
	
	params.name = item->getDisplayName();
	//params.icon = bridge->getIcon();
	//params.icon_open = bridge->getOpenIcon();
	//params.creation_date = bridge->getCreationDate();
	params.root = mConversationsRoot;
	params.listener = item;
	params.rect = LLRect (0, 0, 0, 0);
	params.tool_tip = params.name;
	
	return LLUICtrlFactory::create<LLFolderViewItem>(params);
}

// Conversation items
LLConversationItem::LLConversationItem(std::string name, const LLUUID& uuid, LLFloater* floaterp, LLIMFloaterContainer* containerp) :
	mName(name),
	mUUID(uuid),
    mFloater(floaterp),
    mContainer(containerp)
{
}

LLConversationItem::LLConversationItem() :
	mName(""),
	mUUID(),
	mFloater(NULL),
	mContainer(NULL)
{
}


// Virtual action callbacks
void LLConversationItem::selectItem(void)
{
	LLMultiFloater* host_floater = mFloater->getHost();
	if (host_floater == mContainer)
	{
		// Always expand the message pane if the panel is hosted by the container
		mContainer->collapseMessagesPane(false);
		// Switch to the conversation floater that is being selected
		mContainer->selectFloater(mFloater);
	}
	// Set the focus on the selected floater
	mFloater->setFocus(TRUE);    
}

void LLConversationItem::setVisibleIfDetached(BOOL visible)
{
	// Do this only if the conversation floater has been torn off (i.e. no multi floater host) and is not minimized
	// Note: minimized dockable floaters are brought to front hence unminimized when made visible and we don't want that here
	if (!mFloater->getHost() && !mFloater->isMinimized())
	{
		mFloater->setVisible(visible);    
	}
}

void LLConversationItem::performAction(LLInventoryModel* model, std::string action)
{
}

void LLConversationItem::openItem( void )
{
}

void LLConversationItem::closeItem( void )
{
}

void LLConversationItem::previewItem( void )
{
}

void LLConversationItem::showProperties(void)
{
}

bool LLConversationSort::operator()(const LLConversationItem* const& a, const LLConversationItem* const& b) const
{
	// We compare only by name for the moment
	// *TODO : Implement the sorting by date
	S32 compare = LLStringUtil::compareDict(a->getDisplayName(), b->getDisplayName());
	return (compare < 0);
}

// EOF
