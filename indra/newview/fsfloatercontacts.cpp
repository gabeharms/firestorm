/** 
 * @file fsfloatercontacts.cpp
 * @brief Legacy contacts floater implementation
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2013, The Phoenix Firestorm Project, Inc.
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloatercontacts.h"

#include "fscommon.h"
#include "fscontactsfriendsctrl.h"
#include "fscontactsfriendsmenu.h"
#include "fsfloaterimcontainer.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llcallingcard.h"			// for LLAvatarTracker
#include "llfloateravatarpicker.h"
#include "llfloatergroupinvite.h"
#include "llfloaterreg.h"
#include "llgroupactions.h"
#include "llgrouplist.h"
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"
#include "llstartup.h"
#include "lltabcontainer.h"
#include "llviewermenu.h"
#include "llvoiceclient.h"

//Maximum number of people you can select to do an operation on at once.
const U32 MAX_FRIEND_SELECT = 20;
const F32 DEFAULT_PERIOD = 5.f;
const F32 RIGHTS_CHANGE_TIMEOUT = 5.f;
const F32 OBSERVER_TIMEOUT = 0.5f;

static const std::string FRIENDS_TAB_NAME	= "friends_panel";
static const std::string GROUP_TAB_NAME		= "groups_panel";


// simple class to observe the calling cards.
class LLLocalFriendsObserver : public LLFriendObserver
{
public: 
	LLLocalFriendsObserver(FSFloaterContacts* floater) : mFloater(floater) {}
	virtual ~LLLocalFriendsObserver()
	{
		mFloater = NULL;
	}
	virtual void changed(U32 mask)
	{
		mFloater->onFriendListUpdate(mask);
	}
protected:
	FSFloaterContacts* mFloater;
};

//
// FSFloaterContacts
//

FSFloaterContacts::FSFloaterContacts(const LLSD& seed)
	: LLFloater(seed),
	mTabContainer(NULL),
	mObserver(NULL),
	mFriendsList(NULL),
	mGroupList(NULL),
	mAllowRightsChange(TRUE),
	mNumRightsChanged(0),
	mRlvBehaviorCallbackConnection(),
	mResetLastColumnDisplayModeChanged(false),
	mDirtyNames(false)
{
	mObserver = new LLLocalFriendsObserver(this);
	LLAvatarTracker::instance().addObserver(mObserver);
	// For notification when SIP online status changes.
	LLVoiceClient::getInstance()->addObserver(mObserver);
}

FSFloaterContacts::~FSFloaterContacts()
{
	// For notification when SIP online status changes.
	LLVoiceClient::getInstance()->removeObserver(mObserver);
	LLAvatarTracker::instance().removeObserver(mObserver);
	delete mObserver;

	if (mRlvBehaviorCallbackConnection.connected())
	{
		mRlvBehaviorCallbackConnection.disconnect();
	}
}

BOOL FSFloaterContacts::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("friends_and_groups");
	mFriendsTab = getChild<LLPanel>(FRIENDS_TAB_NAME);
	mFriendListFontName = mFriendsTab->getString("FontName");

	mFriendsList = mFriendsTab->getChild<FSContactsFriendsCtrl>("friend_list");
	mFriendsList->setMaxSelectable(MAX_FRIEND_SELECT);
	mFriendsList->setCommitOnSelectionChange(TRUE);
	mFriendsList->setCommitCallback(boost::bind(&FSFloaterContacts::onSelectName, this));
	mFriendsList->setDoubleClickCallback(boost::bind(&FSFloaterContacts::onImButtonClicked, this));
	mFriendsList->setContextMenu(&gFSContactsFriendsMenu);
	
	mFriendsTab->childSetAction("im_btn",				boost::bind(&FSFloaterContacts::onImButtonClicked,				this));
	mFriendsTab->childSetAction("profile_btn",			boost::bind(&FSFloaterContacts::onViewProfileButtonClicked,		this));
	mFriendsTab->childSetAction("offer_teleport_btn",	boost::bind(&FSFloaterContacts::onTeleportButtonClicked,		this));
	mFriendsTab->childSetAction("map_btn", 				boost::bind(&FSFloaterContacts::onMapButtonClicked,				this));
	mFriendsTab->childSetAction("pay_btn",				boost::bind(&FSFloaterContacts::onPayButtonClicked,				this));
	mFriendsTab->childSetAction("remove_btn",			boost::bind(&FSFloaterContacts::onDeleteFriendButtonClicked,	this));
	mFriendsTab->childSetAction("add_btn",				boost::bind(&FSFloaterContacts::onAddFriendWizButtonClicked,	this));
	mFriendsTab->setDefaultBtn("im_btn");

	mGroupsTab = getChild<LLPanel>(GROUP_TAB_NAME);
	mGroupList = mGroupsTab->getChild<LLGroupList>("group_list");
	mGroupList->setNoItemsMsg(getString("no_groups_msg"));
	mGroupList->setNoFilteredItemsMsg(getString("no_filtered_groups_msg"));
	
	mGroupList->setDoubleClickCallback(boost::bind(&FSFloaterContacts::onGroupChatButtonClicked, this));
	mGroupList->setCommitCallback(boost::bind(&FSFloaterContacts::updateGroupButtons, this));
	mGroupList->setReturnCallback(boost::bind(&FSFloaterContacts::onGroupChatButtonClicked, this));
	
	mGroupsTab->childSetAction("chat_btn",		boost::bind(&FSFloaterContacts::onGroupChatButtonClicked,		this));
	mGroupsTab->childSetAction("info_btn",		boost::bind(&FSFloaterContacts::onGroupInfoButtonClicked,		this));
	mGroupsTab->childSetAction("activate_btn",	boost::bind(&FSFloaterContacts::onGroupActivateButtonClicked,	this));
	mGroupsTab->childSetAction("leave_btn",		boost::bind(&FSFloaterContacts::onGroupLeaveButtonClicked,		this));
	mGroupsTab->childSetAction("create_btn",	boost::bind(&FSFloaterContacts::onGroupCreateButtonClicked,		this));
	mGroupsTab->childSetAction("search_btn",	boost::bind(&FSFloaterContacts::onGroupSearchButtonClicked,		this));
	mGroupsTab->childSetAction("titles_btn",	boost::bind(&FSFloaterContacts::onGroupTitlesButtonClicked,		this));
	mGroupsTab->childSetAction("invite_btn",	boost::bind(&FSFloaterContacts::onGroupInviteButtonClicked,		this));
	mGroupsTab->setDefaultBtn("chat_btn");
	
	mRlvBehaviorCallbackConnection = gRlvHandler.setBehaviourCallback(boost::bind(&FSFloaterContacts::updateRlvRestrictions, this, _1));

	gSavedSettings.getControl("FSFriendListFullNameFormat")->getSignal()->connect(boost::bind(&FSFloaterContacts::onDisplayNameChanged, this));
	gSavedSettings.getControl("FSFriendListSortOrder")->getSignal()->connect(boost::bind(&FSFloaterContacts::sortFriendList, this));
	gSavedSettings.getControl("FSFriendListColumnShowUserName")->getSignal()->connect(boost::bind(&FSFloaterContacts::onColumnDisplayModeChanged, this, "FSFriendListColumnShowUserName"));
	gSavedSettings.getControl("FSFriendListColumnShowDisplayName")->getSignal()->connect(boost::bind(&FSFloaterContacts::onColumnDisplayModeChanged, this, "FSFriendListColumnShowDisplayName"));
	gSavedSettings.getControl("FSFriendListColumnShowFullName")->getSignal()->connect(boost::bind(&FSFloaterContacts::onColumnDisplayModeChanged, this, "FSFriendListColumnShowFullName"));
	onColumnDisplayModeChanged();

	LLAvatarNameCache::addUseDisplayNamesCallback(boost::bind(&FSFloaterContacts::onDisplayNameChanged, this));

	return TRUE;
}

void FSFloaterContacts::draw()
{
	if (mResetLastColumnDisplayModeChanged)
	{
		gSavedSettings.setBOOL(mLastColumnDisplayModeChanged, TRUE);
		mResetLastColumnDisplayModeChanged = false;
	}

	if (mDirtyNames)
	{
		onDisplayNameChanged();
		mFriendsList->setNeedsSort();
		mDirtyNames = false;
	}

	LLFloater::draw();
}

void FSFloaterContacts::updateGroupButtons()
{
	LLUUID groupId = getCurrentItemID();
	bool isGroup = groupId.notNull();

	mGroupsTab->getChild<LLUICtrl>("groupcount")->setValue(FSCommon::populateGroupCount());
	
	getChildView("chat_btn")->setEnabled(isGroup && gAgent.hasPowerInGroup(groupId, GP_SESSION_JOIN));
	getChildView("info_btn")->setEnabled(isGroup);
	getChildView("activate_btn")->setEnabled(groupId != gAgent.getGroupID());
	getChildView("leave_btn")->setEnabled(isGroup);
	getChildView("create_btn")->setEnabled((!gMaxAgentGroups) || (gAgent.mGroups.size() < gMaxAgentGroups));
	getChildView("invite_btn")->setEnabled(isGroup && gAgent.hasPowerInGroup(groupId, GP_MEMBER_INVITE));
}

void FSFloaterContacts::onOpen(const LLSD& key)
{
	FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();
	if (gSavedSettings.getBOOL("ContactsTornOff"))
	{
		// first set the tear-off host to the conversations container
		setHost(floater_container);
		// clear the tear-off host right after, the "last host used" will still stick
		setHost(NULL);
		// reparent to floater view
		gFloaterView->addChild(this);
	}
	else
	{
		floater_container->addFloater(this, TRUE);
	}

	openTab(key.asString());

	LLFloater::onOpen(key);
}

void FSFloaterContacts::openTab(const std::string& name)
{
	if (name == "friends")
	{
		childShowTab("friends_and_groups", "friends_panel");
	}
	else if (name == "groups")
	{
		childShowTab("friends_and_groups", "groups_panel");
		updateGroupButtons();
	}
	else if (name == "contact_sets")
	{
		childShowTab("friends_and_groups", "contact_sets_panel");
	}
	else
	{
		return;
	}

	FSFloaterIMContainer* floater_container = dynamic_cast<FSFloaterIMContainer*>(getHost());
	if (floater_container)
	{
		floater_container->setVisible(TRUE);
		floater_container->showFloater(this);
	}
	else
	{
		setVisible(TRUE);
	}
}

//static
FSFloaterContacts* FSFloaterContacts::findInstance()
{
	return LLFloaterReg::findTypedInstance<FSFloaterContacts>("imcontacts");
}

//static
FSFloaterContacts* FSFloaterContacts::getInstance()
{
	return LLFloaterReg::getTypedInstance<FSFloaterContacts>("imcontacts");
}


//
// Friend actions
//

void FSFloaterContacts::onImButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	if (selected_uuids.size() == 1)
	{
		// if selected only one person then start up IM
		LLAvatarActions::startIM(selected_uuids.front());
	}
	else if (selected_uuids.size() > 1)
	{
		// for multiple selection start up friends conference
		LLAvatarActions::startConference(selected_uuids);
	}
}

void FSFloaterContacts::onViewProfileButtonClicked()
{
	LLUUID id = getCurrentItemID();
	LLAvatarActions::showProfile(id);
}

void FSFloaterContacts::onTeleportButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	LLAvatarActions::offerTeleport(selected_uuids);
}

void FSFloaterContacts::onPayButtonClicked()
{
	LLUUID id = getCurrentItemID();
	if (id.notNull())
	{
		LLAvatarActions::pay(id);
	}
}

void FSFloaterContacts::onDeleteFriendButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);

	if (selected_uuids.size() == 1)
	{
		LLAvatarActions::removeFriendDialog(selected_uuids.front());
	}
	else if (selected_uuids.size() > 1)
	{
		LLAvatarActions::removeFriendsDialog(selected_uuids);
	}
}

bool FSFloaterContacts::isItemsFreeOfFriends(const uuid_vec_t& uuids)
{
	const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	for ( uuid_vec_t::const_iterator
			  id = uuids.begin(),
			  id_end = uuids.end();
		  id != id_end; ++id )
	{
		if (av_tracker.isBuddy (*id))
		{
			return false;
		}
	}
	return true;
}

// static
void FSFloaterContacts::onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names)
{
	if (!names.empty() && !ids.empty())
	{
		LLAvatarActions::requestFriendshipDialog(ids[0], names[0].getCompleteName());
	}
}

void FSFloaterContacts::onAddFriendWizButtonClicked()
{
	// Show add friend wizard.
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&FSFloaterContacts::onAvatarPicked, _1, _2), FALSE, TRUE);
	// Need to disable 'ok' button when friend occurs in selection
	if (picker)
	{
		picker->setOkBtnEnableCb(boost::bind(&FSFloaterContacts::isItemsFreeOfFriends, this, _1));
	}

	LLFloater* root_floater = gFloaterView->getParentFloater(this);
	if (root_floater)
	{
		root_floater->addDependentFloater(picker);
	}
}

//
// Group actions
//

void FSFloaterContacts::onGroupChatButtonClicked()
{
	LLUUID group_id = getCurrentItemID();
	if (group_id.notNull())
	{
		LLGroupActions::startIM(group_id);
	}
}

void FSFloaterContacts::onGroupInfoButtonClicked()
{
	LLGroupActions::show(getCurrentItemID());
}

void FSFloaterContacts::onGroupActivateButtonClicked()
{
	LLGroupActions::activate(mGroupList->getSelectedUUID());
}

void FSFloaterContacts::onGroupLeaveButtonClicked()
{
	LLUUID group_id = getCurrentItemID();
	if (group_id.notNull())
	{
		LLGroupActions::leave(group_id);
	}
}

void FSFloaterContacts::onGroupCreateButtonClicked()
{
	LLGroupActions::createGroup();
}

void FSFloaterContacts::onGroupSearchButtonClicked()
{
	LLGroupActions::search();
}

void FSFloaterContacts::onGroupTitlesButtonClicked()
{
	LLFloaterReg::toggleInstance("fs_group_titles");
}

void FSFloaterContacts::onGroupInviteButtonClicked()
{
	LLUUID group_id = getCurrentItemID();
	if (group_id.notNull())
	{
		LLFloaterGroupInvite::showForGroup(group_id);
	}
}

//
// Tab and list functions
//

std::string FSFloaterContacts::getActiveTabName() const
{
	return mTabContainer->getCurrentPanel()->getName();
}

LLPanel* FSFloaterContacts::getPanelByName(const std::string& panel_name)
{
	return mTabContainer->getPanelByName(panel_name);
}

LLUUID FSFloaterContacts::getCurrentItemID() const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME)
	{
		LLScrollListItem* selected = mFriendsList->getFirstSelected();
		if (selected)
		{
			return selected->getUUID();
		}
		else
		{
			return LLUUID::null;
		}
	}
	else if (cur_tab == GROUP_TAB_NAME)
	{
		return mGroupList->getSelectedUUID();
	}

	return LLUUID::null;
}

void FSFloaterContacts::getCurrentItemIDs(uuid_vec_t& selected_uuids) const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME)
	{
		getCurrentFriendItemIDs(selected_uuids);
	}
	else if (cur_tab == GROUP_TAB_NAME)
	{
		mGroupList->getSelectedUUIDs(selected_uuids);
	}
}

void FSFloaterContacts::getCurrentFriendItemIDs(uuid_vec_t& selected_uuids) const
{
	listitem_vec_t selected = mFriendsList->getAllSelected();
	for (listitem_vec_t::iterator itr = selected.begin(); itr != selected.end(); ++itr)
	{
		selected_uuids.push_back((*itr)->getUUID());
	}
}

void FSFloaterContacts::sortFriendList()
{
	mFriendsList->updateLineHeight();
	mFriendsList->updateLayout();
	mFriendsList->clearSortOrder();

	if (gSavedSettings.getS32("FSFriendListSortOrder"))
	{
		mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mName;
		mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mName;
		mFriendsList->getColumn(LIST_FRIEND_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mName;
		mFriendsList->sortByColumn(std::string("display_name"), TRUE);
	}
	else
	{
		mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mName;
		mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mName;
		mFriendsList->getColumn(LIST_FRIEND_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mName;
		mFriendsList->sortByColumn(std::string("user_name"), TRUE);
	}
	mFriendsList->sortByColumn(std::string("icon_online_status"), FALSE);
}


//
// Friends list
//

void FSFloaterContacts::onFriendListUpdate(U32 changed_mask)
{
	LLAvatarTracker& at = LLAvatarTracker::instance();

	switch (changed_mask)
	{
		case LLFriendObserver::ADD:
			{
				const std::set<LLUUID>& changed_items = at.getChangedIDs();
				std::set<LLUUID>::const_iterator id_it = changed_items.begin();
				std::set<LLUUID>::const_iterator id_end = changed_items.end();
				for (;id_it != id_end; ++id_it)
				{
					addFriend(*id_it);
				}
			}
			break;
		case LLFriendObserver::REMOVE:
			{
				const std::set<LLUUID>& changed_items = at.getChangedIDs();
				std::set<LLUUID>::const_iterator id_it = changed_items.begin();
				std::set<LLUUID>::const_iterator id_end = changed_items.end();
				for (;id_it != id_end; ++id_it)
				{
					mFriendsList->deleteSingleItem(mFriendsList->getItemIndex(*id_it));
				}
			}
			break;
		case LLFriendObserver::POWERS:
			{
				--mNumRightsChanged;
				if (mNumRightsChanged > 0)
				{
					mAllowRightsChange = FALSE;
				}
				else
				{
					mAllowRightsChange = TRUE;
				}
			
				const std::set<LLUUID>& changed_items = at.getChangedIDs();
				std::set<LLUUID>::const_iterator id_it = changed_items.begin();
				std::set<LLUUID>::const_iterator id_end = changed_items.end();
				for (;id_it != id_end; ++id_it)
				{
					const LLRelationship* info = at.getBuddyInfo(*id_it);
					updateFriendItem(*id_it, info);
				}
			}
			break;
		case LLFriendObserver::ONLINE:
			{
				const std::set<LLUUID>& changed_items = at.getChangedIDs();
				std::set<LLUUID>::const_iterator id_it = changed_items.begin();
				std::set<LLUUID>::const_iterator id_end = changed_items.end();
				for (;id_it != id_end; ++id_it)
				{
					const LLRelationship* info = at.getBuddyInfo(*id_it);
					updateFriendItem(*id_it, info);
				}
			}
		default:;
	}
	
	refreshUI();
}

void FSFloaterContacts::addFriend(const LLUUID& agent_id)
{
	LLAvatarTracker& at = LLAvatarTracker::instance();
	const LLRelationship* relationInfo = at.getBuddyInfo(agent_id);
	
	if (!relationInfo)
	{
		return;
	}

	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(agent_id);
	bool isOnline = relationInfo->isOnline();

	LLAvatarName av_name;
	if (!LLAvatarNameCache::get(agent_id, &av_name))
	{
		const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(agent_id);
		LLAvatarNameCache::get(agent_id, boost::bind(&FSFloaterContacts::updateFriendItem, this, agent_id, info));
	}

	LLSD element;
	element["id"] = agent_id;
	LLSD& username_column				= element["columns"][LIST_FRIEND_USER_NAME];
	username_column["column"]			= "user_name";
	username_column["value"]			= av_name.getUserNameForDisplay();
	username_column["font"]["name"]		= mFriendListFontName;
	username_column["font"]["style"]	= "NORMAL";
	
	LLSD& display_name_column			= element["columns"][LIST_FRIEND_DISPLAY_NAME];
	display_name_column["column"]		= "display_name";
	display_name_column["value"]		= av_name.getDisplayName();
	display_name_column["font"]["name"]	= mFriendListFontName;
	display_name_column["font"]["style"]= "NORMAL";

	LLSD& friend_column					= element["columns"][LIST_FRIEND_NAME];
	friend_column["column"]				= "full_name";
	friend_column["value"]				= getFullName(av_name);
	friend_column["font"]["name"]		= mFriendListFontName;
	friend_column["font"]["style"]		= "NORMAL";

	LLSD& online_status_column			= element["columns"][LIST_ONLINE_STATUS];
	online_status_column["column"]		= "icon_online_status";
	online_status_column["type"]		= "icon";
	online_status_column["halign"]		= "center";
	
	if (isOnline)
	{	
		username_column["font"]["style"]	= "BOLD";
		display_name_column["font"]["style"]= "BOLD";
		friend_column["font"]["style"]		= "BOLD";
		online_status_column["value"]		= "icon_avatar_online";
	}
	else if (isOnlineSIP)
	{	
		username_column["font"]["style"]	= "BOLD";
		display_name_column["font"]["style"]= "BOLD";
		friend_column["font"]["style"]		= "BOLD";
		online_status_column["value"]		= "slim_icon_16_viewer";
	}

	LLSD& online_column						= element["columns"][LIST_VISIBLE_ONLINE];
	online_column["column"]					= "icon_visible_online";
	online_column["type"]					= "checkbox";
	online_column["value"]					= relationInfo->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS);

	LLSD& visible_map_column				= element["columns"][LIST_VISIBLE_MAP];
	visible_map_column["column"]			= "icon_visible_map";
	visible_map_column["type"]				= "checkbox";
	visible_map_column["value"]				= relationInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION);

	LLSD& edit_my_object_column				= element["columns"][LIST_EDIT_MINE];
	edit_my_object_column["column"]			= "icon_edit_mine";
	edit_my_object_column["type"]			= "checkbox";
	edit_my_object_column["value"]			= relationInfo->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& visible_their_map_column			= element["columns"][LIST_VISIBLE_MAP_THEIRS];
	visible_their_map_column["column"]		= "icon_visible_map_theirs";
	visible_their_map_column["type"]		= "checkbox";
	visible_their_map_column["enabled"]		= "";
	visible_their_map_column["value"]		= relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION);
	
	LLSD& edit_their_object_column			= element["columns"][LIST_EDIT_THEIRS];
	edit_their_object_column["column"]		= "icon_edit_theirs";
	edit_their_object_column["type"]		= "checkbox";
	edit_their_object_column["enabled"]		= "";
	edit_their_object_column["value"]		= relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& update_gen_column					= element["columns"][LIST_FRIEND_UPDATE_GEN];
	update_gen_column["column"]				= "friend_last_update_generation";
	update_gen_column["value"]				= relationInfo->getChangeSerialNum();

	mFriendsList->addElement(element, ADD_BOTTOM);
}

void FSFloaterContacts::onMapButtonClicked()
{
	LLUUID current_id = getCurrentItemID();
	if (current_id.notNull() && is_agent_mappable(current_id))
	{
		LLAvatarActions::showOnMap(current_id);
	}
}

// propagate actual relationship to UI.
// Does not resort the UI list because it can be called frequently. JC
void FSFloaterContacts::updateFriendItem(const LLUUID& agent_id, const LLRelationship* info)
{
	if (!info)
	{
		return;
	}

	LLScrollListItem* itemp = mFriendsList->getItem(agent_id);
	if (!itemp)
	{
		return;
	}
	
	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(itemp->getUUID());
	bool isOnline = info->isOnline();

	LLAvatarName av_name;
	if (!LLAvatarNameCache::get(agent_id, &av_name))
	{
		LLAvatarNameCache::get(agent_id, boost::bind(&FSFloaterContacts::updateFriendItem, this, agent_id, info));
	}

	// Name of the status icon to use
	std::string statusIcon;
	if (isOnline)
	{
		statusIcon = "icon_avatar_online";
	}
	else if(isOnlineSIP)
	{
		statusIcon = "slim_icon_16_viewer";
	}

	itemp->getColumn(LIST_ONLINE_STATUS)->setValue(statusIcon);

	itemp->getColumn(LIST_FRIEND_USER_NAME)->setValue(av_name.getUserNameForDisplay());
	itemp->getColumn(LIST_FRIEND_DISPLAY_NAME)->setValue(av_name.getDisplayName());
	itemp->getColumn(LIST_FRIEND_NAME)->setValue(getFullName(av_name));

	// render name of online friends in bold text
	LLFontGL::StyleFlags font_style = ((isOnline || isOnlineSIP) ? LLFontGL::BOLD : LLFontGL::NORMAL);
	((LLScrollListText*)itemp->getColumn(LIST_FRIEND_USER_NAME))->setFontStyle(font_style);
	((LLScrollListText*)itemp->getColumn(LIST_FRIEND_DISPLAY_NAME))->setFontStyle(font_style);
	((LLScrollListText*)itemp->getColumn(LIST_FRIEND_NAME))->setFontStyle(font_style);

	itemp->getColumn(LIST_VISIBLE_ONLINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS));
	itemp->getColumn(LIST_VISIBLE_MAP)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_MINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS));
	itemp->getColumn(LIST_VISIBLE_MAP_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS));
	S32 change_generation = info->getChangeSerialNum();
	itemp->getColumn(LIST_FRIEND_UPDATE_GEN)->setValue(change_generation);

	// enable this item, in case it was disabled after user input
	itemp->setEnabled(TRUE);
}

void FSFloaterContacts::refreshRightsChangeList()
{
	uuid_vec_t friends;
	getCurrentFriendItemIDs(friends);

	size_t num_selected = friends.size();

	bool can_offer_teleport = num_selected >= 1;
	bool selected_friends_online = true;

	const LLRelationship* friend_status = NULL;
	for (uuid_vec_t::iterator itr = friends.begin(); itr != friends.end(); ++itr)
	{
		friend_status = LLAvatarTracker::instance().getBuddyInfo(*itr);
		if (friend_status)
		{
			if (!friend_status->isOnline())
			{
				can_offer_teleport = false;
				selected_friends_online = false;
			}
			else
			{
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC) &&
					!gRlvHandler.isException(RLV_BHVR_TPLURE, *itr, RLV_CHECK_PERMISSIVE) &&
					!friend_status->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION))
				{
					can_offer_teleport = false;
				}
			}
		}
		else // missing buddy info, don't allow any operations
		{
			can_offer_teleport = false;
		}
	}
	
	if (num_selected == 0) // nothing selected
	{
		childSetEnabled("im_btn", false);
		childSetEnabled("offer_teleport_btn", false);
	}
	else // we have at least one friend selected...
	{
		// only allow IMs to groups when everyone in the group is online
		// to be consistent with context menus in inventory and because otherwise
		// offline friends would be silently dropped from the session
		childSetEnabled("im_btn", selected_friends_online || num_selected == 1);
		childSetEnabled("offer_teleport_btn", can_offer_teleport);
	}
}

void FSFloaterContacts::refreshUI()
{
	sortFriendList();

	bool single_selected = false;
	bool multiple_selected = false;
	size_t num_selected = mFriendsList->getAllSelected().size();
	if (num_selected > 0)
	{
		single_selected = true;
		multiple_selected = (num_selected > 1);
	}

	//Options that can only be performed with one friend selected
	childSetEnabled("profile_btn", single_selected && !multiple_selected);
	childSetEnabled("pay_btn", single_selected && !multiple_selected);

	//Options that can be performed with up to MAX_FRIEND_SELECT friends selected
	//(single_selected will always be true in this situations)
	childSetEnabled("remove_btn", single_selected);
	childSetEnabled("im_btn", single_selected);

	LLScrollListItem* selected_item = mFriendsList->getFirstSelected();
	bool mappable = false;
	if (selected_item)
	{
		LLUUID av_id = selected_item->getUUID();
		mappable = (single_selected && !multiple_selected && av_id.notNull() && is_agent_mappable(av_id));
	}
	childSetEnabled("map_btn", mappable && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWWORLDMAP));

	// Set friend count
	LLStringUtil::format_map_t args;
	args["[COUNT]"] = llformat("%d", mFriendsList->getItemCount());
	mFriendsTab->childSetValue("friend_count", LLSD( mFriendsTab->getString("FriendCount", args) ) );

	refreshRightsChangeList();
}

void FSFloaterContacts::onSelectName()
{
	refreshUI();
	// check to see if rights have changed
	applyRightsToFriends();
}

void FSFloaterContacts::confirmModifyRights(rights_map_t& ids, EGrantRevoke command)
{
	if (ids.empty())
	{
		return;
	}
	
	LLSD args;
	if (ids.size() > 0)
	{
		rights_map_t* rights = new rights_map_t(ids);

		// for single friend, show their name
		if (ids.size() == 1)
		{
			LLUUID agent_id = ids.begin()->first;
			std::string name;
			if (gCacheName->getFullName(agent_id, name))
			{
				args["NAME"] = name;
			}
			if (command == GRANT)
			{
				LLNotificationsUtil::add("GrantModifyRights", 
					args, 
					LLSD(), 
					boost::bind(&FSFloaterContacts::modifyRightsConfirmation, this, _1, _2, rights));
			}
			else
			{
				LLNotificationsUtil::add("RevokeModifyRights", 
					args, 
					LLSD(), 
					boost::bind(&FSFloaterContacts::modifyRightsConfirmation, this, _1, _2, rights));
			}
		}
		else
		{
			if (command == GRANT)
			{
				LLNotificationsUtil::add("GrantModifyRightsMultiple", 
					args, 
					LLSD(), 
					boost::bind(&FSFloaterContacts::modifyRightsConfirmation, this, _1, _2, rights));
			}
			else
			{
				LLNotificationsUtil::add("RevokeModifyRightsMultiple", 
					args, 
					LLSD(), 
					boost::bind(&FSFloaterContacts::modifyRightsConfirmation, this, _1, _2, rights));
			}
		}
	}
}

bool FSFloaterContacts::modifyRightsConfirmation(const LLSD& notification, const LLSD& response, rights_map_t* rights)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == option)
	{
		sendRightsGrant(*rights);
	}
	else
	{
		// need to resync view with model, since user cancelled operation
		rights_map_t::iterator rights_it;
		for (rights_it = rights->begin(); rights_it != rights->end(); ++rights_it)
		{
			const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(rights_it->first);
			updateFriendItem(rights_it->first, info);
		}
	}
	refreshUI();

	delete rights;
	return false;
}

void FSFloaterContacts::applyRightsToFriends()
{
	bool rights_changed = false;

	// store modify rights separately for confirmation
	rights_map_t rights_updates;

	bool need_confirmation = false;
	EGrantRevoke confirmation_type = GRANT;

	// this assumes that changes only happened to selected items
	listitem_vec_t selected = mFriendsList->getAllSelected();
	for (listitem_vec_t::iterator itr = selected.begin(); itr != selected.end(); ++itr)
	{
		LLUUID id = (*itr)->getValue();
		const LLRelationship* buddy_relationship = LLAvatarTracker::instance().getBuddyInfo(id);
		if (buddy_relationship == NULL)
		{
			continue;
		}

		bool show_online_staus = (*itr)->getColumn(LIST_VISIBLE_ONLINE)->getValue().asBoolean();
		bool show_map_location = (*itr)->getColumn(LIST_VISIBLE_MAP)->getValue().asBoolean();
		bool allow_modify_objects = (*itr)->getColumn(LIST_EDIT_MINE)->getValue().asBoolean();

		S32 rights = buddy_relationship->getRightsGrantedTo();
		if (buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS) != show_online_staus)
		{
			rights_changed = true;
			if (show_online_staus) 
			{
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
			}
			else 
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights &= ~LLRelationship::GRANT_ONLINE_STATUS;
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
				// propagate rights constraint to UI
				(*itr)->getColumn(LIST_VISIBLE_MAP)->setValue(FALSE);
			}
		}
		if (buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION) != show_map_location)
		{
			rights_changed = true;
			if (show_map_location) 
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights |= LLRelationship::GRANT_MAP_LOCATION;
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
				(*itr)->getColumn(LIST_VISIBLE_ONLINE)->setValue(TRUE);
			}
			else 
			{
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
			}
		}
		
		// now check for change in modify object rights, which requires confirmation
		if (buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS) != allow_modify_objects)
		{
			rights_changed = true;
			need_confirmation = true;

			if (allow_modify_objects)
			{
				rights |= LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = GRANT;
			}
			else
			{
				rights &= ~LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = REVOKE;
			}
		}

		if (rights_changed)
		{
			rights_updates.insert(std::make_pair(id, rights));
			// disable these ui elements until response from server
			// to avoid race conditions
			(*itr)->setEnabled(FALSE);
		}
	}

	// separately confirm grant and revoke of modify rights
	if (need_confirmation)
	{
		confirmModifyRights(rights_updates, confirmation_type);
	}
	else
	{
		sendRightsGrant(rights_updates);
	}
}

void FSFloaterContacts::sendRightsGrant(rights_map_t& ids)
{
	if (ids.empty())
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystem;

	// setup message header
	msg->newMessageFast(_PREHASH_GrantUserRights);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgent.getID());
	msg->addUUID(_PREHASH_SessionID, gAgent.getSessionID());

	rights_map_t::iterator id_it;
	rights_map_t::iterator end_it = ids.end();
	for (id_it = ids.begin(); id_it != end_it; ++id_it)
	{
		msg->nextBlockFast(_PREHASH_Rights);
		msg->addUUID(_PREHASH_AgentRelated, id_it->first);
		msg->addS32(_PREHASH_RelatedRights, id_it->second);
	}

	mNumRightsChanged = ids.size();
	gAgent.sendReliableMessage();
}

void FSFloaterContacts::childShowTab(const std::string& id, const std::string& tabname)
{
	LLTabContainer* child = findChild<LLTabContainer>(id);
	if (child)
	{
		child->selectTabByName(tabname);
	}
}

void FSFloaterContacts::updateRlvRestrictions(ERlvBehaviour behavior)
{
	if (behavior == RLV_BHVR_SHOWLOC ||
		behavior == RLV_BHVR_SHOWWORLDMAP)
	{
		refreshUI();
	}
}

void FSFloaterContacts::onColumnDisplayModeChanged(const std::string& settings_name)
{
	mLastColumnDisplayModeChanged = settings_name;

	// Make sure at least one column is shown!
	if (!settings_name.empty() &&
		!gSavedSettings.getBOOL("FSFriendListColumnShowUserName") &&
		!gSavedSettings.getBOOL("FSFriendListColumnShowDisplayName") &&
		!gSavedSettings.getBOOL("FSFriendListColumnShowFullName"))
	{
		mResetLastColumnDisplayModeChanged = true;
		return;
	}

	std::vector<LLScrollListColumn::Params> column_params = mFriendsList->getColumnInitParams();

	mFriendsList->clearColumns();
	for (std::vector<LLScrollListColumn::Params>::iterator it = column_params.begin(); it != column_params.end(); ++it)
	{
		LLScrollListColumn::Params p = *it;

		if (p.name.getValue() == "user_name")
		{
			LLScrollListColumn::Params params;
			params.header = p.header;
			params.name = p.name;
			params.halign = p.halign;
			params.sort_direction = p.sort_direction;
			params.sort_column = p.sort_column;
			params.tool_tip = p.tool_tip;

			if (gSavedSettings.getBOOL("FSFriendListColumnShowUserName"))
			{
				params.width.dynamic_width.set(true, true);
			}
			else
			{
				params.width.pixel_width.set(-1, true);
			}
			mFriendsList->addColumn(params);
		}
		else if (p.name.getValue() == "display_name")
		{
			LLScrollListColumn::Params params;
			params.header = p.header;
			params.name = p.name;
			params.halign = p.halign;
			params.sort_direction = p.sort_direction;
			params.sort_column = p.sort_column;
			params.tool_tip = p.tool_tip;

			if (gSavedSettings.getBOOL("FSFriendListColumnShowDisplayName"))
			{
				params.width.dynamic_width.set(true, true);
			}
			else
			{
				params.width.pixel_width.set(-1, true);
			}
			mFriendsList->addColumn(params);
		}
		else if (p.name.getValue() == "full_name")
		{
			LLScrollListColumn::Params params;
			params.header = p.header;
			params.name = p.name;
			params.halign = p.halign;
			params.sort_direction = p.sort_direction;
			params.sort_column = p.sort_column;
			params.tool_tip = p.tool_tip;

			if (gSavedSettings.getBOOL("FSFriendListColumnShowFullName"))
			{
				params.width.dynamic_width.set(true, true);
			}
			else
			{
				params.width.pixel_width.set(-1, true);
			}
			mFriendsList->addColumn(params);
		}
		else
		{
			mFriendsList->addColumn(p);
		}
		
	}

	mFriendsList->dirtyColumns();

	// primary sort = online status, secondary sort = name
	if (gSavedSettings.getS32("FSFriendListSortOrder"))
	{
		mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mName;
		mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mName;
		mFriendsList->getColumn(LIST_FRIEND_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mName;
		mFriendsList->sortByColumn(std::string("display_name"), TRUE);
	}
	else
	{
		mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mName;
		mFriendsList->getColumn(LIST_FRIEND_DISPLAY_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mName;
		mFriendsList->getColumn(LIST_FRIEND_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mName;
		mFriendsList->sortByColumn(std::string("user_name"), TRUE);
	}
	mFriendsList->sortByColumn(std::string("icon_online_status"), FALSE);
	mFriendsList->setSearchColumn(mFriendsList->getColumn("full_name")->mIndex);
}

void FSFloaterContacts::onDisplayNameChanged()
{
	listitem_vec_t items = mFriendsList->getAllData();
	for (listitem_vec_t::iterator it = items.begin(); it != items.end(); ++it)
	{
		LLAvatarName av_name;
		if (LLAvatarNameCache::get((*it)->getUUID(), &av_name))
		{
			(*it)->getColumn(LIST_FRIEND_USER_NAME)->setValue(av_name.getUserNameForDisplay());
			(*it)->getColumn(LIST_FRIEND_DISPLAY_NAME)->setValue(av_name.getDisplayName());
			(*it)->getColumn(LIST_FRIEND_NAME)->setValue(getFullName(av_name));
		}
		else
		{
			LLAvatarNameCache::get((*it)->getUUID(), boost::bind(&FSFloaterContacts::setDirtyNames, this));
		}
	}
	mFriendsList->setNeedsSort();
}

std::string FSFloaterContacts::getFullName(const LLAvatarName& av_name)
{
	if (av_name.isDisplayNameDefault() || !gSavedSettings.getBOOL("UseDisplayNames"))
	{
		return av_name.getUserNameForDisplay();
	}

	if (gSavedSettings.getS32("FSFriendListFullNameFormat"))
	{
		// Display name (Username)
		return llformat("%s (%s)", av_name.getDisplayName().c_str(), av_name.getUserNameForDisplay().c_str());
	}
	else
	{
		// Username (Display name)
		return llformat("%s (%s)", av_name.getUserNameForDisplay().c_str(), av_name.getDisplayName().c_str());
	}
}
// EOF
