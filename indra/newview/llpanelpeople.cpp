/** 
 * @file llpanelpeople.cpp
 * @brief Side tray "People" panel
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

// libs
#include "llavatarname.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llmenubutton.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "lleventtimer.h"
#include "llfiltereditor.h"
#include "lltabcontainer.h"
#include "lltoggleablemenu.h"
#include "lluictrlfactory.h"

#include "llpanelpeople.h"

// newview
#include "llaccordionctrl.h"
#include "llaccordionctrltab.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llavatarlist.h"
#include "llavatarlistitem.h"
#include "llcallingcard.h"			// for LLAvatarTracker
#include "llfloateravatarpicker.h"
//#include "llfloaterminiinspector.h"
#include "llfriendcard.h"
#include "llgroupactions.h"
#include "llgrouplist.h"
#include "llinventoryobserver.h"
#include "llnetmap.h"
#include "llpanelpeoplemenus.h"
#include "llsidetraypanelcontainer.h"
#include "llrecentpeople.h"
#include "llviewercontrol.h"		// for gSavedSettings
#include "llviewermenu.h"			// for gMenuHolder
#include "llvoiceclient.h"
#include "llworld.h"
#include "llspeakers.h"

// Firestorm includes
#include "fscontactsfloater.h"
#include "fsradar.h"
#include "fsradarlistctrl.h"
#include "fsradarmenu.h"
#include "llavatarconstants.h" // for range constants
#include "lllayoutstack.h"
#include "llnotificationmanager.h"
#include "rlvhandler.h"

#define FRIEND_LIST_UPDATE_TIMEOUT	0.5
#define NEARBY_LIST_UPDATE_INTERVAL 1

static const std::string NEARBY_TAB_NAME	= "nearby_panel";
static const std::string FRIENDS_TAB_NAME	= "friends_panel";
static const std::string GROUP_TAB_NAME		= "groups_panel";
static const std::string RECENT_TAB_NAME	= "recent_panel";

static const std::string COLLAPSED_BY_USER  = "collapsed_by_user";

/** Comparator for comparing avatar items by last interaction date */
class LLAvatarItemRecentComparator : public LLAvatarItemComparator
{
public:
	LLAvatarItemRecentComparator() {};
	virtual ~LLAvatarItemRecentComparator() {};

protected:
	virtual bool doCompare(const LLAvatarListItem* avatar_item1, const LLAvatarListItem* avatar_item2) const
	{
		LLRecentPeople& people = LLRecentPeople::instance();
		const LLDate& date1 = people.getDate(avatar_item1->getAvatarId());
		const LLDate& date2 = people.getDate(avatar_item2->getAvatarId());

		//older comes first
		return date1 > date2;
	}
};

/** Compares avatar items by online status, then by name */
class LLAvatarItemStatusComparator : public LLAvatarItemComparator
{
public:
	LLAvatarItemStatusComparator() {};

protected:
	/**
	 * @return true if item1 < item2, false otherwise
	 */
	virtual bool doCompare(const LLAvatarListItem* item1, const LLAvatarListItem* item2) const
	{
		LLAvatarTracker& at = LLAvatarTracker::instance();
		bool online1 = at.isBuddyOnline(item1->getAvatarId());
		bool online2 = at.isBuddyOnline(item2->getAvatarId());

		if (online1 == online2)
		{
			std::string name1 = item1->getAvatarName();
			std::string name2 = item2->getAvatarName();

			LLStringUtil::toUpper(name1);
			LLStringUtil::toUpper(name2);

			return name1 < name2;
		}
		
		return online1 > online2; 
	}
};

/** Compares avatar items by distance between you and them */
class LLAvatarItemDistanceComparator : public LLAvatarItemComparator
{
public:
	typedef std::map < LLUUID, LLVector3d > id_to_pos_map_t;
	LLAvatarItemDistanceComparator() {};

	void updateAvatarsPositions(std::vector<LLVector3d>& positions, uuid_vec_t& uuids)
	{
		std::vector<LLVector3d>::const_iterator
			pos_it = positions.begin(),
			pos_end = positions.end();

		uuid_vec_t::const_iterator
			id_it = uuids.begin(),
			id_end = uuids.end();

		LLAvatarItemDistanceComparator::id_to_pos_map_t pos_map;

		mAvatarsPositions.clear();

		for (;pos_it != pos_end && id_it != id_end; ++pos_it, ++id_it )
		{
			mAvatarsPositions[*id_it] = *pos_it;
		}
	};
	
	// Used for Range Display, originally from KB/Catznip
	const id_to_pos_map_t& getAvatarsPositions() { return mAvatarsPositions; }

protected:
	virtual bool doCompare(const LLAvatarListItem* item1, const LLAvatarListItem* item2) const
	{
		const LLVector3d& me_pos = gAgent.getPositionGlobal();
		const LLVector3d& item1_pos = mAvatarsPositions.find(item1->getAvatarId())->second;
		const LLVector3d& item2_pos = mAvatarsPositions.find(item2->getAvatarId())->second;
		
		return dist_vec_squared(item1_pos, me_pos) < dist_vec_squared(item2_pos, me_pos);
	}
private:
	id_to_pos_map_t mAvatarsPositions;
};

/** Comparator for comparing nearby avatar items by last spoken time */
class LLAvatarItemRecentSpeakerComparator : public  LLAvatarItemNameComparator
{
public:
	LLAvatarItemRecentSpeakerComparator() {};
	virtual ~LLAvatarItemRecentSpeakerComparator() {};

protected:
	virtual bool doCompare(const LLAvatarListItem* item1, const LLAvatarListItem* item2) const
	{
		LLPointer<LLSpeaker> lhs = LLActiveSpeakerMgr::instance().findSpeaker(item1->getAvatarId());
		LLPointer<LLSpeaker> rhs = LLActiveSpeakerMgr::instance().findSpeaker(item2->getAvatarId());
		if ( lhs.notNull() && rhs.notNull() )
		{
			// Compare by last speaking time
			if( lhs->mLastSpokeTime != rhs->mLastSpokeTime )
				return ( lhs->mLastSpokeTime > rhs->mLastSpokeTime );
		}
		else if ( lhs.notNull() )
		{
			// True if only item1 speaker info available
			return true;
		}
		else if ( rhs.notNull() )
		{
			// False if only item2 speaker info available
			return false;
		}
		// By default compare by name.
		return LLAvatarItemNameComparator::doCompare(item1, item2);
	}
};

static const LLAvatarItemRecentComparator RECENT_COMPARATOR;
static const LLAvatarItemStatusComparator STATUS_COMPARATOR;
static LLAvatarItemDistanceComparator DISTANCE_COMPARATOR;
static const LLAvatarItemRecentSpeakerComparator RECENT_SPEAKER_COMPARATOR;

static LLRegisterPanelClassWrapper<LLPanelPeople> t_people("panel_people");

//=============================================================================

/**
 * Updates given list either on regular basis or on external events (up to implementation). 
 */
class LLPanelPeople::Updater
{
public:
	typedef boost::function<void()> callback_t;
	Updater(callback_t cb)
	: mCallback(cb)
	{
	}

	virtual ~Updater()
	{
	}

	/**
	 * Activate/deactivate updater.
	 *
	 * This may start/stop regular updates.
	 */
	virtual void setActive(bool) {}

protected:
	void update()
	{
		mCallback();
	}

	callback_t		mCallback;
};

/**
 * Update buttons on changes in our friend relations (STORM-557).
 */
class LLButtonsUpdater : public LLPanelPeople::Updater, public LLFriendObserver
{
public:
	LLButtonsUpdater(callback_t cb)
	:	LLPanelPeople::Updater(cb)
	{
		LLAvatarTracker::instance().addObserver(this);
	}

	~LLButtonsUpdater()
	{
		LLAvatarTracker::instance().removeObserver(this);
	}

	/*virtual*/ void changed(U32 mask)
	{
		(void) mask;
		update();
	}
};

class LLAvatarListUpdater : public LLPanelPeople::Updater, public LLEventTimer
{
public:
	LLAvatarListUpdater(callback_t cb, F32 period)
	:	LLEventTimer(period),
		LLPanelPeople::Updater(cb)
	{
		mEventTimer.stop();
	}

	virtual BOOL tick() // from LLEventTimer
	{
		return FALSE;
	}
};

/**
 * Updates the friends list.
 * 
 * Updates the list on external events which trigger the changed() method. 
 */
class LLFriendListUpdater : public LLAvatarListUpdater, public LLFriendObserver
{
	LOG_CLASS(LLFriendListUpdater);
	class LLInventoryFriendCardObserver;

public: 
	friend class LLInventoryFriendCardObserver;
	LLFriendListUpdater(callback_t cb)
	:	LLAvatarListUpdater(cb, FRIEND_LIST_UPDATE_TIMEOUT)
	,	mIsActive(false)
	{
		LLAvatarTracker::instance().addObserver(this);

		// For notification when SIP online status changes.
		LLVoiceClient::getInstance()->addObserver(this);
		mInvObserver = new LLInventoryFriendCardObserver(this);
	}

	~LLFriendListUpdater()
	{
		// will be deleted by ~LLInventoryModel
		//delete mInvObserver;
		LLVoiceClient::getInstance()->removeObserver(this);
		LLAvatarTracker::instance().removeObserver(this);
	}

	/*virtual*/ void changed(U32 mask)
	{
		if (mIsActive)
		{
			// events can arrive quickly in bulk - we need not process EVERY one of them -
			// so we wait a short while to let others pile-in, and process them in aggregate.
			mEventTimer.start();
		}

		// save-up all the mask-bits which have come-in
		mMask |= mask;
	}


	/*virtual*/ BOOL tick()
	{
		if (!mIsActive) return FALSE;

		if (mMask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE))
		{
			update();
		}

		// Stop updates.
		mEventTimer.stop();
		mMask = 0;

		return FALSE;
	}

	// virtual
	void setActive(bool active)
	{
		mIsActive = active;
		if (active)
		{
			tick();
		}
	}

private:
	U32 mMask;
	LLInventoryFriendCardObserver* mInvObserver;
	bool mIsActive;

	/**
	 *	This class is intended for updating Friend List when Inventory Friend Card is added/removed.
	 * 
	 *	The main usage is when Inventory Friends/All content is added while synchronizing with 
	 *		friends list on startup is performed. In this case Friend Panel should be updated when 
	 *		missing Inventory Friend Card is created.
	 *	*NOTE: updating is fired when Inventory item is added into CallingCards/Friends subfolder.
	 *		Otherwise LLFriendObserver functionality is enough to keep Friends Panel synchronized.
	 */
	class LLInventoryFriendCardObserver : public LLInventoryObserver
	{
		LOG_CLASS(LLFriendListUpdater::LLInventoryFriendCardObserver);

		friend class LLFriendListUpdater;

	private:
		LLInventoryFriendCardObserver(LLFriendListUpdater* updater) : mUpdater(updater)
		{
			gInventory.addObserver(this);
		}
		~LLInventoryFriendCardObserver()
		{
			gInventory.removeObserver(this);
		}
		/*virtual*/ void changed(U32 mask)
		{
			lldebugs << "Inventory changed: " << mask << llendl;

			static bool synchronize_friends_folders = true;
			if (synchronize_friends_folders)
			{
				// Checks whether "Friends" and "Friends/All" folders exist in "Calling Cards" folder,
				// fetches their contents if needed and synchronizes it with buddies list.
				// If the folders are not found they are created.
				LLFriendCardsManager::instance().syncFriendCardsFolders();
				synchronize_friends_folders = false;
			}

			// *NOTE: deleting of InventoryItem is performed via moving to Trash. 
			// That means LLInventoryObserver::STRUCTURE is present in MASK instead of LLInventoryObserver::REMOVE
			if ((CALLINGCARD_ADDED & mask) == CALLINGCARD_ADDED)
			{
				lldebugs << "Calling card added: count: " << gInventory.getChangedIDs().size() 
					<< ", first Inventory ID: "<< (*gInventory.getChangedIDs().begin())
					<< llendl;

				bool friendFound = false;
				std::set<LLUUID> changedIDs = gInventory.getChangedIDs();
				for (std::set<LLUUID>::const_iterator it = changedIDs.begin(); it != changedIDs.end(); ++it)
				{
					if (isDescendentOfInventoryFriends(*it))
					{
						friendFound = true;
						break;
					}
				}

				if (friendFound)
				{
					lldebugs << "friend found, panel should be updated" << llendl;
					mUpdater->changed(LLFriendObserver::ADD);
				}
			}
		}

		bool isDescendentOfInventoryFriends(const LLUUID& invItemID)
		{
			LLViewerInventoryItem * item = gInventory.getItem(invItemID);
			if (NULL == item)
				return false;

			return LLFriendCardsManager::instance().isItemInAnyFriendsList(item);
		}
		LLFriendListUpdater* mUpdater;

		static const U32 CALLINGCARD_ADDED = LLInventoryObserver::ADD | LLInventoryObserver::CALLING_CARD;
	};
};

/**
 * Periodically updates the nearby people list while the Nearby tab is active.
 * 
 * The period is defined by NEARBY_LIST_UPDATE_INTERVAL constant.
 */
class LLNearbyListUpdater : public LLAvatarListUpdater
{
	LOG_CLASS(LLNearbyListUpdater);

public:
	LLNearbyListUpdater(callback_t cb)
	:	LLAvatarListUpdater(cb, NEARBY_LIST_UPDATE_INTERVAL)
	{
		setActive(false);
	}

	/*virtual*/ void setActive(bool val)
	{
		if (val)
		{
			// update immediately and start regular updates
			update();
			mEventTimer.start(); 
		}
		else
		{
			// stop regular updates
			mEventTimer.stop();
		}
	}

	/*virtual*/ BOOL tick()
	{
		update();
		return FALSE;
	}
private:
};

/**
 * Updates the recent people list (those the agent has recently interacted with).
 */
class LLRecentListUpdater : public LLAvatarListUpdater, public boost::signals2::trackable
{
	LOG_CLASS(LLRecentListUpdater);

public:
	LLRecentListUpdater(callback_t cb)
	:	LLAvatarListUpdater(cb, 0)
	{
		LLRecentPeople::instance().setChangedCallback(boost::bind(&LLRecentListUpdater::update, this));
	}
};

//=============================================================================

LLPanelPeople::LLPanelPeople()
	:	LLPanel(),
		mFilterSubString(LLStringUtil::null),
		mFilterSubStringOrig(LLStringUtil::null),
		mFilterEditor(NULL),
		mTabContainer(NULL),
		mOnlineFriendList(NULL),
		mAllFriendList(NULL),
		mNearbyList(NULL),
		mRecentList(NULL),
		mGroupList(NULL),
		mNearbyGearButton(NULL),
		mFriendsGearButton(NULL),
		mGroupsGearButton(NULL),
		mRecentGearButton(NULL),
		// <FS:Ansariel> Firestorm radar
		//mMiniMap(NULL)
		mMiniMap(NULL),
		mRadarList(NULL)
		// </FS:Ansariel> Firestorm radar
{
	mFriendListUpdater = new LLFriendListUpdater(boost::bind(&LLPanelPeople::updateFriendList,	this));
	// <FS:Ansariel> Firestorm radar
	//mNearbyListUpdater = new LLNearbyListUpdater(boost::bind(&LLPanelPeople::updateNearbyList,	this));
	mRecentListUpdater = new LLRecentListUpdater(boost::bind(&LLPanelPeople::updateRecentList,	this));
	mButtonsUpdater = new LLButtonsUpdater(boost::bind(&LLPanelPeople::updateButtons, this));
	mCommitCallbackRegistrar.add("People.addFriend", boost::bind(&LLPanelPeople::onAddFriendButtonClicked, this));
}

LLPanelPeople::~LLPanelPeople()
{
	// <FS:Ansariel> Firestorm radar
	mNearbyUpdateSignalConnection.disconnect();

	delete mButtonsUpdater;
	// <FS:Ansariel> Firestorm radar
	//delete mNearbyListUpdater;
	delete mFriendListUpdater;
	delete mRecentListUpdater;

	if(LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver(this);
	}

	if (mGroupPlusMenuHandle.get()) mGroupPlusMenuHandle.get()->die();
	if (mNearbyViewSortMenuHandle.get()) mNearbyViewSortMenuHandle.get()->die();
	if (mNearbyViewSortMenuHandle.get()) mNearbyViewSortMenuHandle.get()->die();
	if (mGroupsViewSortMenuHandle.get()) mGroupsViewSortMenuHandle.get()->die();
	if (mRecentViewSortMenuHandle.get()) mRecentViewSortMenuHandle.get()->die();

}

void LLPanelPeople::onFriendsAccordionExpandedCollapsed(LLUICtrl* ctrl, const LLSD& param, LLAvatarList* avatar_list)
{
	if(!avatar_list)
	{
		llerrs << "Bad parameter" << llendl;
		return;
	}

	bool expanded = param.asBoolean();

	setAccordionCollapsedByUser(ctrl, !expanded);
	if(!expanded)
	{
		avatar_list->resetSelection();
	}
}

BOOL LLPanelPeople::postBuild()
{
	mFilterEditor = getChild<LLFilterEditor>("filter_input");
	mFilterEditor->setCommitCallback(boost::bind(&LLPanelPeople::onFilterEdit, this, _2));

	mTabContainer = getChild<LLTabContainer>("tabs");
	mTabContainer->setCommitCallback(boost::bind(&LLPanelPeople::onTabSelected, this, _2));

	LLPanel* friends_tab = getChild<LLPanel>(FRIENDS_TAB_NAME);
	// updater is active only if panel is visible to user.
	friends_tab->setVisibleCallback(boost::bind(&Updater::setActive, mFriendListUpdater, _2));
	mOnlineFriendList = friends_tab->getChild<LLAvatarList>("avatars_online");
	mAllFriendList = friends_tab->getChild<LLAvatarList>("avatars_all");
	mOnlineFriendList->setNoItemsCommentText(getString("no_friends_online"));
	mOnlineFriendList->setShowIcons("FriendsListShowIcons");
	// <FS:Ansariel> Firestorm radar
	//mOnlineFriendList->showPermissions("FriendsListShowPermissions");
	mOnlineFriendList->showPermissions(true);
	// </FS:Ansariel> Firestorm radar
	mAllFriendList->setNoItemsCommentText(getString("no_friends"));
	mAllFriendList->setShowIcons("FriendsListShowIcons");
	// <FS:Ansariel> Firestorm radar
	//mAllFriendList->showPermissions("FriendsListShowPermissions");
	mAllFriendList->showPermissions(true);
	// </FS:Ansariel> Firestorm radar

	LLPanel* nearby_tab = getChild<LLPanel>(NEARBY_TAB_NAME);
	// <FS:Ansariel> Firestorm radar
	//nearby_tab->setVisibleCallback(boost::bind(&Updater::setActive, mNearbyListUpdater, _2));
	
	// <FS:AO> Radarlist takes over for nearbylist for presentation.
	mRadarList = nearby_tab->getChild<FSRadarListCtrl>("radar_list");
	mRadarList->sortByColumn("range", TRUE); // sort by range
	mRadarList->setFilterColumn(0);
	// </FS:AO>

	// <FS:Ansariel> Firestorm radar
	//mNearbyList = nearby_tab->getChild<LLAvatarList>("avatar_list");
	//mNearbyList->setNoItemsCommentText(getString("no_one_near"));
	//mNearbyList->setNoItemsMsg(getString("no_one_near"));
	//mNearbyList->setNoFilteredItemsMsg(getString("no_one_filtered_near"));
	//mNearbyList->setShowIcons("NearbyListShowIcons");
	// </FS:Ansariel> Firestorm radar
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
	// Externalized to FSRadar
	//mNearbyList->setRlvCheckShowNames(true);
// [/RLVa:KB]
	
	mMiniMap = (LLNetMap*)getChildView("Net Map",true);
	// <FS:Ansariel> Synchronize tooltips throughout instances
	//mMiniMap->setToolTipMsg(gSavedSettings.getBOOL("DoubleClickTeleport") ? 
	//	getString("AltMiniMapToolTipMsg") :	getString("MiniMapToolTipMsg"));
	// <//FS:Ansariel> Synchronize tooltips throughout instances

	mRecentList = getChild<LLPanel>(RECENT_TAB_NAME)->getChild<LLAvatarList>("avatar_list");
	mRecentList->setNoItemsCommentText(getString("no_recent_people"));
	mRecentList->setNoItemsMsg(getString("no_recent_people"));
	mRecentList->setNoFilteredItemsMsg(getString("no_filtered_recent_people"));
	mRecentList->setShowIcons("RecentListShowIcons");

	mGroupList = getChild<LLGroupList>("group_list");
	mGroupList->setNoItemsMsg(getString("no_groups_msg"));
	mGroupList->setNoFilteredItemsMsg(getString("no_filtered_groups_msg"));

	// <FS:Ansariel> Use Firestorm radar menu handler
	//mNearbyList->setContextMenu(&LLPanelPeopleMenus::gNearbyMenu);
	mRadarList->setContextMenu(&FSFloaterRadarMenu::gFSRadarMenu);
	// </FS:Ansariel>
	mRecentList->setContextMenu(&LLPanelPeopleMenus::gNearbyMenu);
	mAllFriendList->setContextMenu(&LLPanelPeopleMenus::gNearbyMenu);
	mOnlineFriendList->setContextMenu(&LLPanelPeopleMenus::gNearbyMenu);

	setSortOrder(mRecentList,		(ESortOrder)gSavedSettings.getU32("RecentPeopleSortOrder"),	false);
	setSortOrder(mAllFriendList,	(ESortOrder)gSavedSettings.getU32("FriendsSortOrder"),		false);
	// <FS:Ansariel> Firestorm radar
	//setSortOrder(mNearbyList,		(ESortOrder)gSavedSettings.getU32("NearbyPeopleSortOrder"),	false);

	LLPanel* groups_panel = getChild<LLPanel>(GROUP_TAB_NAME);
	groups_panel->childSetAction("activate_btn", boost::bind(&LLPanelPeople::onActivateButtonClicked,	this));
	groups_panel->childSetAction("plus_btn",	boost::bind(&LLPanelPeople::onGroupPlusButtonClicked,	this));

	LLPanel* friends_panel = getChild<LLPanel>(FRIENDS_TAB_NAME);
	friends_panel->childSetAction("add_btn",	boost::bind(&LLPanelPeople::onAddFriendWizButtonClicked,	this));
	friends_panel->childSetAction("del_btn",	boost::bind(&LLPanelPeople::onDeleteFriendButtonClicked,	this));
	// <FS:Ansariel> Firestorm radar
	friends_panel->childSetAction("GlobalOnlineStatusToggle", boost::bind(&LLPanelPeople::onGlobalVisToggleButtonClicked,	this));

	mOnlineFriendList->setItemDoubleClickCallback(boost::bind(&LLPanelPeople::onAvatarListDoubleClicked, this, _1));
	mAllFriendList->setItemDoubleClickCallback(boost::bind(&LLPanelPeople::onAvatarListDoubleClicked, this, _1));
	// <FS:Ansariel> Firestorm radar
	//mNearbyList->setItemDoubleClickCallback(boost::bind(&LLPanelPeople::onAvatarListDoubleClicked, this, _1));
	mRadarList->setDoubleClickCallback(boost::bind(&LLPanelPeople::onRadarListDoubleClicked, this));
	// </FS:Ansariel> Firestorm radar
	mRecentList->setItemDoubleClickCallback(boost::bind(&LLPanelPeople::onAvatarListDoubleClicked, this, _1));

	mOnlineFriendList->setCommitCallback(boost::bind(&LLPanelPeople::onAvatarListCommitted, this, mOnlineFriendList));
	mAllFriendList->setCommitCallback(boost::bind(&LLPanelPeople::onAvatarListCommitted, this, mAllFriendList));
	// <FS:Ansariel> We only use mRadarList here
	//mNearbyList->setCommitCallback(boost::bind(&LLPanelPeople::onAvatarListCommitted, this, mNearbyList));
	mRadarList->setCommitCallback(boost::bind(&LLPanelPeople::onAvatarListCommitted, this, mNearbyList));
	mRecentList->setCommitCallback(boost::bind(&LLPanelPeople::onAvatarListCommitted, this, mRecentList));

	// Set openning IM as default on return action for avatar lists
	mOnlineFriendList->setReturnCallback(boost::bind(&LLPanelPeople::onImButtonClicked, this));
	mAllFriendList->setReturnCallback(boost::bind(&LLPanelPeople::onImButtonClicked, this));
	// <FS:Ansariel> Firestorm radar
	//mNearbyList->setReturnCallback(boost::bind(&LLPanelPeople::onImButtonClicked, this));
	mRecentList->setReturnCallback(boost::bind(&LLPanelPeople::onImButtonClicked, this));

	mGroupList->setDoubleClickCallback(boost::bind(&LLPanelPeople::onChatButtonClicked, this));
	mGroupList->setCommitCallback(boost::bind(&LLPanelPeople::updateButtons, this));
	mGroupList->setReturnCallback(boost::bind(&LLPanelPeople::onChatButtonClicked, this));

	LLAccordionCtrlTab* accordion_tab = getChild<LLAccordionCtrlTab>("tab_all");
	accordion_tab->setDropDownStateChangedCallback(
		boost::bind(&LLPanelPeople::onFriendsAccordionExpandedCollapsed, this, _1, _2, mAllFriendList));

	accordion_tab = getChild<LLAccordionCtrlTab>("tab_online");
	accordion_tab->setDropDownStateChangedCallback(
		boost::bind(&LLPanelPeople::onFriendsAccordionExpandedCollapsed, this, _1, _2, mOnlineFriendList));

	buttonSetAction("view_profile_btn",	boost::bind(&LLPanelPeople::onViewProfileButtonClicked,	this));
	buttonSetAction("group_info_btn",	boost::bind(&LLPanelPeople::onGroupInfoButtonClicked,	this));
	buttonSetAction("chat_btn",			boost::bind(&LLPanelPeople::onChatButtonClicked,		this));
	buttonSetAction("im_btn",			boost::bind(&LLPanelPeople::onImButtonClicked,			this));
	buttonSetAction("call_btn",			boost::bind(&LLPanelPeople::onCallButtonClicked,		this));
	buttonSetAction("group_call_btn",	boost::bind(&LLPanelPeople::onGroupCallButtonClicked,	this));
	buttonSetAction("teleport_btn",		boost::bind(&LLPanelPeople::onTeleportButtonClicked,	this));
	buttonSetAction("share_btn",		boost::bind(&LLPanelPeople::onShareButtonClicked,		this));

	// Must go after setting commit callback and initializing all pointers to children.
	mTabContainer->selectTabByName(NEARBY_TAB_NAME);

	// Create menus.
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	
	registrar.add("People.Group.Plus.Action",  boost::bind(&LLPanelPeople::onGroupPlusMenuItemClicked,  this, _2));
	registrar.add("People.Group.Minus.Action", boost::bind(&LLPanelPeople::onGroupMinusButtonClicked,  this));
	registrar.add("People.Friends.ViewSort.Action",  boost::bind(&LLPanelPeople::onFriendsViewSortMenuItemClicked,  this, _2));
	registrar.add("People.Nearby.ViewSort.Action",  boost::bind(&LLPanelPeople::onNearbyViewSortMenuItemClicked,  this, _2));
	registrar.add("People.Groups.ViewSort.Action",  boost::bind(&LLPanelPeople::onGroupsViewSortMenuItemClicked,  this, _2));
	registrar.add("People.Recent.ViewSort.Action",  boost::bind(&LLPanelPeople::onRecentViewSortMenuItemClicked,  this, _2));
	// <FS:Ansariel> Firestorm radar
	registrar.add("Radar.Gear.Action",				boost::bind(&LLPanelPeople::onNearbyViewSortMenuItemClicked,  this, _2));

	enable_registrar.add("People.Group.Minus.Enable",	boost::bind(&LLPanelPeople::isRealGroup,	this));
	enable_registrar.add("People.Friends.ViewSort.CheckItem",	boost::bind(&LLPanelPeople::onFriendsViewSortMenuItemCheck,	this, _2));
	enable_registrar.add("People.Recent.ViewSort.CheckItem",	boost::bind(&LLPanelPeople::onRecentViewSortMenuItemCheck,	this, _2));
	enable_registrar.add("People.Nearby.ViewSort.CheckItem",	boost::bind(&LLPanelPeople::onNearbyViewSortMenuItemCheck,	this, _2));

	// <FS:Ansariel> Firestorm radar options
	registrar.add("Radar.NameFmt",								boost::bind(&FSRadar::onRadarNameFmtClicked, _2));
	registrar.add("Radar.ReportTo",								boost::bind(&FSRadar::onRadarReportToClicked, _2));	// <FS:CR> Milkshake-style Radar Alerts
	enable_registrar.add("Radar.NameFmtCheck",					boost::bind(&FSRadar::radarNameFmtCheck, _2));
	enable_registrar.add("Radar.ReportToCheck",					boost::bind(&FSRadar::radarReportToCheck, _2));	// <FS:CR> Milkshake-style Radar Alerts
	// </FS:Ansariel>

	mNearbyGearButton = getChild<LLMenuButton>("nearby_view_sort_btn");
	mFriendsGearButton = getChild<LLMenuButton>("friends_viewsort_btn");
	mGroupsGearButton = getChild<LLMenuButton>("groups_viewsort_btn");
	mRecentGearButton = getChild<LLMenuButton>("recent_viewsort_btn");

	LLMenuGL* plus_menu  = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_group_plus.xml",  gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	mGroupPlusMenuHandle  = plus_menu->getHandle();

	// <FS:Ansariel> Use Firestorm gear menu
	//LLToggleableMenu* nearby_view_sort  = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_people_nearby_view_sort.xml",  gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	LLToggleableMenu* nearby_view_sort  = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_fs_radar_gear.xml",  gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	// </FS:Ansariel>
	if(nearby_view_sort)
	{
		mNearbyViewSortMenuHandle  = nearby_view_sort->getHandle();
		mNearbyGearButton->setMenu(nearby_view_sort);
	}

	LLToggleableMenu* friend_view_sort  = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_people_friends_view_sort.xml",  gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if(friend_view_sort)
	{
		mFriendsViewSortMenuHandle  = friend_view_sort->getHandle();
		mFriendsGearButton->setMenu(friend_view_sort);
	}

	LLToggleableMenu* group_view_sort  = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_people_groups_view_sort.xml",  gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if(group_view_sort)
	{
		mGroupsViewSortMenuHandle  = group_view_sort->getHandle();
		mGroupsGearButton->setMenu(group_view_sort);
	}

	LLToggleableMenu* recent_view_sort  = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_people_recent_view_sort.xml",  gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if(recent_view_sort)
	{
		mRecentViewSortMenuHandle  = recent_view_sort->getHandle();
		mRecentGearButton->setMenu(recent_view_sort);
	}

	LLVoiceClient::getInstance()->addObserver(this);

	// call this method in case some list is empty and buttons can be in inconsistent state
	updateButtons();

	mOnlineFriendList->setRefreshCompleteCallback(boost::bind(&LLPanelPeople::onFriendListRefreshComplete, this, _1, _2));
	mAllFriendList->setRefreshCompleteCallback(boost::bind(&LLPanelPeople::onFriendListRefreshComplete, this, _1, _2));

	// <FS:Ansariel> Register for radar updates
	mNearbyUpdateSignalConnection = FSRadar::getInstance()->setUpdateCallback(boost::bind(&LLPanelPeople::updateNearby, this, _1, _2));

	return TRUE;
}

// virtual
void LLPanelPeople::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
	if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
	{
		return;
	}
	
	updateButtons();
}

void LLPanelPeople::updateFriendListHelpText()
{
	// show special help text for just created account to help finding friends. EXT-4836
	static LLTextBox* no_friends_text = getChild<LLTextBox>("no_friends_help_text");

	// Seems sometimes all_friends can be empty because of issue with Inventory loading (clear cache, slow connection...)
	// So, lets check all lists to avoid overlapping the text with online list. See EXT-6448.
	bool any_friend_exists = mAllFriendList->filterHasMatches() || mOnlineFriendList->filterHasMatches();
	no_friends_text->setVisible(!any_friend_exists);
	if (no_friends_text->getVisible())
	{
		//update help text for empty lists
		std::string message_name = mFilterSubString.empty() ? "no_friends_msg" : "no_filtered_friends_msg";
		LLStringUtil::format_map_t args;
		args["[SEARCH_TERM]"] = LLURI::escape(mFilterSubStringOrig);
		no_friends_text->setText(getString(message_name, args));
	}
}

void LLPanelPeople::updateFriendList()
{
	if (!mOnlineFriendList || !mAllFriendList)
		return;

	// get all buddies we know about
	const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	LLAvatarTracker::buddy_map_t all_buddies;
	av_tracker.copyBuddyList(all_buddies);

	// save them to the online and all friends vectors
	uuid_vec_t& online_friendsp = mOnlineFriendList->getIDs();
	uuid_vec_t& all_friendsp = mAllFriendList->getIDs();

	all_friendsp.clear();
	online_friendsp.clear();

	uuid_vec_t buddies_uuids;
	LLAvatarTracker::buddy_map_t::const_iterator buddies_iter;

	// Fill the avatar list with friends UUIDs
	for (buddies_iter = all_buddies.begin(); buddies_iter != all_buddies.end(); ++buddies_iter)
	{
		buddies_uuids.push_back(buddies_iter->first);
	}

	if (buddies_uuids.size() > 0)
	{
		lldebugs << "Friends added to the list: " << buddies_uuids.size() << llendl;
		all_friendsp = buddies_uuids;
	}
	else
	{
		lldebugs << "No friends found" << llendl;
	}

	LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();
	for (; buddy_it != all_buddies.end(); ++buddy_it)
	{
		LLUUID buddy_id = buddy_it->first;
		if (av_tracker.isBuddyOnline(buddy_id))
			online_friendsp.push_back(buddy_id);
	}

	/*
	 * Avatarlists  will be hidden by showFriendsAccordionsIfNeeded(), if they do not have items.
	 * But avatarlist can be updated only if it is visible @see LLAvatarList::draw();   
	 * So we need to do force update of lists to avoid inconsistency of data and view of avatarlist. 
	 */
	mOnlineFriendList->setDirty(true, !mOnlineFriendList->filterHasMatches());// do force update if list do NOT have items
	mAllFriendList->setDirty(true, !mAllFriendList->filterHasMatches());
	//update trash and other buttons according to a selected item
	updateButtons();
	showFriendsAccordionsIfNeeded();
}

void LLPanelPeople::updateNearbyList()
{
	if (!mNearbyList)
		return;

	std::vector<LLVector3d> positions;

	LLWorld::getInstance()->getAvatars(&mNearbyList->getIDs(), &positions, gAgent.getPositionGlobal(), gSavedSettings.getF32("NearMeRange"));
	mNearbyList->setDirty();

	DISTANCE_COMPARATOR.updateAvatarsPositions(positions, mNearbyList->getIDs());
	LLActiveSpeakerMgr::instance().update(TRUE);
}

void LLPanelPeople::updateRecentList()
{
	if (!mRecentList)
		return;

	LLRecentPeople::instance().get(mRecentList->getIDs());
	mRecentList->setDirty();
}

void LLPanelPeople::buttonSetVisible(std::string btn_name, BOOL visible)
{
	// To make sure we're referencing the right widget (a child of the button bar).
	LLButton* button = getChild<LLView>("button_bar")->getChild<LLButton>(btn_name);
	button->setVisible(visible);
}

void LLPanelPeople::buttonSetEnabled(const std::string& btn_name, bool enabled)
{
	// To make sure we're referencing the right widget (a child of the button bar).
	LLButton* button = getChild<LLView>("button_bar")->getChild<LLButton>(btn_name);
	button->setEnabled(enabled);
}

void LLPanelPeople::buttonSetAction(const std::string& btn_name, const commit_signal_t::slot_type& cb)
{
	// To make sure we're referencing the right widget (a child of the button bar).
	LLButton* button = getChild<LLView>("button_bar")->getChild<LLButton>(btn_name);
	button->setClickedCallback(cb);
}

void LLPanelPeople::updateButtons()
{
	std::string cur_tab		= getActiveTabName();
	bool nearby_tab_active	= (cur_tab == NEARBY_TAB_NAME);
	bool friends_tab_active = (cur_tab == FRIENDS_TAB_NAME);
	bool group_tab_active	= (cur_tab == GROUP_TAB_NAME);
	//bool recent_tab_active	= (cur_tab == RECENT_TAB_NAME);
	// <FS:Ansariel> Firestorm changes
	bool recent_tab_active	= (cur_tab == RECENT_TAB_NAME);
	// </FS:Ansariel> Firestorm changes
	LLUUID selected_id;

	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	bool item_selected = (selected_uuids.size() == 1);
	bool multiple_selected = (selected_uuids.size() >= 1);

	buttonSetVisible("group_info_btn",		group_tab_active);
	buttonSetVisible("chat_btn",			group_tab_active);
	buttonSetVisible("view_profile_btn",	!group_tab_active);
	buttonSetVisible("im_btn",				!group_tab_active);
	buttonSetVisible("call_btn",			!group_tab_active);
	buttonSetVisible("group_call_btn",		group_tab_active);
	// <FS:Ansariel> Firestorm changes
	//buttonSetVisible("teleport_btn",		friends_tab_active);
	buttonSetVisible("teleport_btn",		nearby_tab_active || friends_tab_active);
	// </FS:Ansariel> Firestorm changes
	buttonSetVisible("share_btn",			nearby_tab_active || friends_tab_active);

	if (group_tab_active)
	{
		bool cur_group_active = true;

		if (item_selected)
		{
			selected_id = mGroupList->getSelectedUUID();
			cur_group_active = (gAgent.getGroupID() == selected_id);
		}

		LLPanel* groups_panel = mTabContainer->getCurrentPanel();
//		groups_panel->getChildView("activate_btn")->setEnabled(item_selected && !cur_group_active); // "none" or a non-active group selected
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.4.1a) | Added: RLVa-1.3.0f
		groups_panel->getChildView("activate_btn")->setEnabled(
			item_selected && !cur_group_active && !gRlvHandler.hasBehaviour(RLV_BHVR_SETGROUP)); // "none" or a non-active group selected
// [/RLVa:KB]
		groups_panel->getChildView("minus_btn")->setEnabled(item_selected && selected_id.notNull());
	}
	else
	{
		bool is_friend = true;

		// Check whether selected avatar is our friend.
		if (item_selected)
		{
			selected_id = selected_uuids.front();
			is_friend = LLAvatarTracker::instance().getBuddyInfo(selected_id) != NULL;
		}

		LLPanel* cur_panel = mTabContainer->getCurrentPanel();
		if (cur_panel)
		{
//			cur_panel->getChildView("add_friend_btn")->setEnabled(!is_friend);
// [RLVa:KB] - Checked: 2010-07-20 (RLVa-1.2.2a) | Added: RLVa-1.2.0h
			if (nearby_tab_active)
			{
				cur_panel->getChildView("add_friend_btn_nearby")->setEnabled(!is_friend && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES));
			}
			else if (recent_tab_active)
			{
				cur_panel->getChildView("add_friend_btn_recent")->setEnabled(!is_friend);
			}

// [/RLBa:KB]
			if (friends_tab_active)
			{
				cur_panel->getChildView("del_btn")->setEnabled(multiple_selected);
			}
		}
	}

	bool enable_calls = LLVoiceClient::getInstance()->isVoiceWorking() && LLVoiceClient::getInstance()->voiceEnabled();

// [RLVa:KB] - Checked: 2010-06-04 (RLVa-1.2.2a) | Modified: RLVa-1.2.0d
	if ( (nearby_tab_active) && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) )
	{
		item_selected = multiple_selected = false;
	}
// [/RLBa:KB]

	buttonSetEnabled("view_profile_btn",item_selected);
	buttonSetEnabled("share_btn",		item_selected);
	buttonSetEnabled("im_btn",			multiple_selected); // allow starting the friends conference for multiple selection
	buttonSetEnabled("call_btn",		multiple_selected && enable_calls);
	buttonSetEnabled("teleport_btn",	multiple_selected /* && LLAvatarActions::canOfferTeleport(selected_uuids) */ ); // LO - Dont block the TP button at all.

	bool none_group_selected = item_selected && selected_id.isNull();
	buttonSetEnabled("group_info_btn", !none_group_selected);
	buttonSetEnabled("group_call_btn", !none_group_selected && enable_calls);
	buttonSetEnabled("chat_btn", !none_group_selected);
}

std::string LLPanelPeople::getActiveTabName() const
{
	return mTabContainer->getCurrentPanel()->getName();
}

LLUUID LLPanelPeople::getCurrentItemID() const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME) // this tab has two lists
	{
		LLUUID cur_online_friend;

		if ((cur_online_friend = mOnlineFriendList->getSelectedUUID()).notNull())
			return cur_online_friend;

		return mAllFriendList->getSelectedUUID();
	}

	if (cur_tab == NEARBY_TAB_NAME)
	// <FS:AO> Adapted for scrolllist
		//return mNearbyList->getSelectedUUID();
	{
		static S32 uuid_column_index = mRadarList->getColumn("uuid")->mIndex;

		LLScrollListItem* item = mRadarList->getFirstSelected();
		if (item)
		{
			return item->getColumn(uuid_column_index)->getValue().asUUID();
		}
		else 
		{
			return LLUUID::null;
		}
	}
	// </FS:AO>
	
	if (cur_tab == RECENT_TAB_NAME)
		return mRecentList->getSelectedUUID();

	if (cur_tab == GROUP_TAB_NAME)
		return mGroupList->getSelectedUUID();

	llassert(0 && "unknown tab selected");
	return LLUUID::null;
}

void LLPanelPeople::getCurrentItemIDs(uuid_vec_t& selected_uuids) const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME)
	{
		// friends tab has two lists
		mOnlineFriendList->getSelectedUUIDs(selected_uuids);
		mAllFriendList->getSelectedUUIDs(selected_uuids);
	}
	else if (cur_tab == NEARBY_TAB_NAME)
	// <FS:AO> Adapted for scrolllist
		//mNearbyList->getSelectedUUIDs(selected_uuids);
	{
		static S32 uuid_column_index = mRadarList->getColumn("uuid")->mIndex;

		for (size_t i = 0; i < mRadarList->getAllSelected().size(); ++i)
		{
			selected_uuids.push_back(mRadarList->getAllSelected().at(i)->getColumn(uuid_column_index)->getValue().asUUID());
		}
	}
	// </FS:AO>
	else if (cur_tab == RECENT_TAB_NAME)
		mRecentList->getSelectedUUIDs(selected_uuids);
	else if (cur_tab == GROUP_TAB_NAME)
		mGroupList->getSelectedUUIDs(selected_uuids);
	else
		llassert(0 && "unknown tab selected");

}

void LLPanelPeople::showGroupMenu(LLMenuGL* menu)
{
	// Shows the menu at the top of the button bar.

	// Calculate its coordinates.
	// (assumes that groups panel is the current tab)
	LLPanel* bottom_panel = mTabContainer->getCurrentPanel()->getChild<LLPanel>("bottom_panel");
	LLPanel* parent_panel = mTabContainer->getCurrentPanel();
	menu->arrangeAndClear();
	S32 menu_height = menu->getRect().getHeight();
	S32 menu_x = -2; // *HACK: compensates HPAD in showPopup()
	S32 menu_y = bottom_panel->getRect().mTop + menu_height;

	// Actually show the menu.
	menu->buildDrawLabels();
	menu->updateParent(LLMenuGL::sMenuContainer);
	LLMenuGL::showPopup(parent_panel, menu, menu_x, menu_y);
}

void LLPanelPeople::setSortOrder(LLAvatarList* list, ESortOrder order, bool save)
{
	switch (order)
	{
	case E_SORT_BY_NAME:
		list->sortByName();
		break;
	case E_SORT_BY_STATUS:
		list->setComparator(&STATUS_COMPARATOR);
		list->sort();
		break;
	case E_SORT_BY_MOST_RECENT:
		list->setComparator(&RECENT_COMPARATOR);
		list->sort();
		break;
	case E_SORT_BY_RECENT_SPEAKERS:
		list->setComparator(&RECENT_SPEAKER_COMPARATOR);
		list->sort();
		break;
	case E_SORT_BY_DISTANCE:
		list->setComparator(&DISTANCE_COMPARATOR);
		list->sort();
		break;
	default:
		llwarns << "Unrecognized people sort order for " << list->getName() << llendl;
		return;
	}

	if (save)
	{
		std::string setting;

		if (list == mAllFriendList || list == mOnlineFriendList)
			setting = "FriendsSortOrder";
		else if (list == mRecentList)
			setting = "RecentPeopleSortOrder";
		// <FS:Ansariel> Not used in Firestorm radar
		//else if (list == mNearbyList)
		//	setting = "NearbyPeopleSortOrder";
		// </FS:Ansariel>

		if (!setting.empty())
			gSavedSettings.setU32(setting, order);
	}
}

bool LLPanelPeople::isRealGroup()
{
	return getCurrentItemID() != LLUUID::null;
}

void LLPanelPeople::onFilterEdit(const std::string& search_string)
{
	mFilterSubStringOrig = search_string;
	LLStringUtil::trimHead(mFilterSubStringOrig);
	// Searches are case-insensitive
	std::string search_upper = mFilterSubStringOrig;
	LLStringUtil::toUpper(search_upper);

	if (mFilterSubString == search_upper)
		return;

	mFilterSubString = search_upper;

	//store accordion tabs state before any manipulation with accordion tabs
	if(!mFilterSubString.empty())
	{
		notifyChildren(LLSD().with("action","store_state"));
	}


	// Apply new filter.
	// <FS:Ansariel> Changed for FS specific radar
	// mNearbyList->setNameFilter(mFilterSubStringOrig);
	mRadarList->setFilterString(mFilterSubStringOrig);
	// </FS:Ansariel> Changed for FS specific radar
	mOnlineFriendList->setNameFilter(mFilterSubStringOrig);
	mAllFriendList->setNameFilter(mFilterSubStringOrig);
	mRecentList->setNameFilter(mFilterSubStringOrig);
	mGroupList->setNameFilter(mFilterSubStringOrig);

	setAccordionCollapsedByUser("tab_online", false);
	setAccordionCollapsedByUser("tab_all", false);

	showFriendsAccordionsIfNeeded();

	//restore accordion tabs state _after_ all manipulations...
	if(mFilterSubString.empty())
	{
		notifyChildren(LLSD().with("action","restore_state"));
	}
}

void LLPanelPeople::onTabSelected(const LLSD& param)
{
	std::string tab_name = getChild<LLPanel>(param.asString())->getName();
	updateButtons();

	showFriendsAccordionsIfNeeded();

	// <FS:AO> Layout panels will not initialize at a constant size, force it here.
	if (tab_name == NEARBY_TAB_NAME)
	{
		LLLayoutPanel* minilayout = (LLLayoutPanel*)getChildView("minimaplayout", true);
		if (minilayout->getVisible())
		{
			LLRect rec = minilayout->getRect();
			rec.mBottom = 635;
			rec.mTop = 775;
			minilayout->setShape(rec,true);
		}
	}
	// </FS:AO>
	
	if (GROUP_TAB_NAME == tab_name)
		mFilterEditor->setLabel(getString("groups_filter_label"));
	else
		mFilterEditor->setLabel(getString("people_filter_label"));
}

void LLPanelPeople::onAvatarListDoubleClicked(LLUICtrl* ctrl)
{
	LLAvatarListItem* item = dynamic_cast<LLAvatarListItem*>(ctrl);
	if(!item)
	{
		return;
	}

	LLUUID clicked_id = item->getAvatarId();
	
#if 0 // SJB: Useful for testing, but not currently functional or to spec
	LLAvatarActions::showProfile(clicked_id);
#else // spec says open IM window
	LLAvatarActions::startIM(clicked_id);
#endif
}


void LLPanelPeople::onAvatarListCommitted(LLAvatarList* list)
{
	if (getActiveTabName() == NEARBY_TAB_NAME)
	{
		uuid_vec_t selected_uuids;
		// <FS:Ansariel> Firestorm radar
		//getCurrentItemIDs(selected_uuids);
		//mMiniMap->setSelected(selected_uuids)
		LLUUID sVal = mRadarList->getSelectedValue().asUUID();
		if (sVal.notNull()) 
		{
			selected_uuids.push_back(sVal);
			mMiniMap->setSelected(selected_uuids);
		}
		// </FS:Ansariel> Firestorm radar
	} else
	// Make sure only one of the friends lists (online/all) has selection.
	if (getActiveTabName() == FRIENDS_TAB_NAME)
	{
		if (list == mOnlineFriendList)
			mAllFriendList->resetSelection(true);
		else if (list == mAllFriendList)
			mOnlineFriendList->resetSelection(true);
// possible side effect of sidebar work; should be no harm in ignoring this -KC
//		else
//			llassert(0 && "commit on unknown friends list");
	}

	updateButtons();
}

void LLPanelPeople::onViewProfileButtonClicked()
{
	LLUUID id = getCurrentItemID();
	LLAvatarActions::showProfile(id);
}

void LLPanelPeople::onAddFriendButtonClicked()
{
	LLUUID id = getCurrentItemID();
	if (id.notNull())
	{
		LLAvatarActions::requestFriendshipDialog(id);
	}
}

bool LLPanelPeople::isItemsFreeOfFriends(const uuid_vec_t& uuids)
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

void LLPanelPeople::onAddFriendWizButtonClicked()
{
	// Show add friend wizard.
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&LLPanelPeople::onAvatarPicked, _1, _2), FALSE, TRUE);
	if (!picker)
	{
		return;
	}

	// Need to disable 'ok' button when friend occurs in selection
	picker->setOkBtnEnableCb(boost::bind(&LLPanelPeople::isItemsFreeOfFriends, this, _1));
	LLFloater* root_floater = gFloaterView->getParentFloater(this);
	if (root_floater)
	{
		root_floater->addDependentFloater(picker);
	}
}

void LLPanelPeople::onDeleteFriendButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);

	if (selected_uuids.size() == 1)
	{
		LLAvatarActions::removeFriendDialog( selected_uuids.front() );
	}
	else if (selected_uuids.size() > 1)
	{
		LLAvatarActions::removeFriendsDialog( selected_uuids );
	}
}

void LLPanelPeople::onGroupInfoButtonClicked()
{
	LLGroupActions::show(getCurrentItemID());
}

void LLPanelPeople::onChatButtonClicked()
{
	LLUUID group_id = getCurrentItemID();
	if (group_id.notNull())
		LLGroupActions::startIM(group_id);
}

void LLPanelPeople::onImButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	if ( selected_uuids.size() == 1 )
	{
		// if selected only one person then start up IM
		LLAvatarActions::startIM(selected_uuids.at(0));
	}
	else if ( selected_uuids.size() > 1 )
	{
		// for multiple selection start up friends conference
		LLAvatarActions::startConference(selected_uuids);
	}
}

void LLPanelPeople::onActivateButtonClicked()
{
	LLGroupActions::activate(mGroupList->getSelectedUUID());
}

// static
void LLPanelPeople::onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names)
{
	if (!names.empty() && !ids.empty())
		LLAvatarActions::requestFriendshipDialog(ids[0], names[0].getCompleteName());
}

void LLPanelPeople::onGroupPlusButtonClicked()
{
	if (!gAgent.canJoinGroups())
	{
		LLNotificationsUtil::add("JoinedTooManyGroups");
		return;
	}

	LLMenuGL* plus_menu = (LLMenuGL*)mGroupPlusMenuHandle.get();
	if (!plus_menu)
		return;

	showGroupMenu(plus_menu);
}

void LLPanelPeople::onGroupMinusButtonClicked()
{
	LLUUID group_id = getCurrentItemID();
	if (group_id.notNull())
		LLGroupActions::leave(group_id);
}

void LLPanelPeople::onGroupPlusMenuItemClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();

	if (chosen_item == "join_group")
		LLGroupActions::search();
	else if (chosen_item == "new_group")
		LLGroupActions::createGroup();
}

void LLPanelPeople::onFriendsViewSortMenuItemClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();

	if (chosen_item == "sort_name")
	{
		setSortOrder(mAllFriendList, E_SORT_BY_NAME);
	}
	else if (chosen_item == "sort_status")
	{
		setSortOrder(mAllFriendList, E_SORT_BY_STATUS);
	}
	else if (chosen_item == "view_icons")
	{
		mAllFriendList->toggleIcons();
		mOnlineFriendList->toggleIcons();
	}
	else if (chosen_item == "view_permissions")
	{
		bool show_permissions = !gSavedSettings.getBOOL("FriendsListShowPermissions");
		gSavedSettings.setBOOL("FriendsListShowPermissions", show_permissions);

		mAllFriendList->showPermissions(show_permissions);
		mOnlineFriendList->showPermissions(show_permissions);
	}
	else if (chosen_item == "panel_block_list_sidetray")
	{
		// <FS:Ansariel> Optional standalone blocklist floater
		//LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD());
		if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
		{
			LLFloaterReg::showInstance("fs_blocklist", LLSD());
		}
		else
		{
			LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD());
		}
		// </FS:Ansariel>
	}
}

void LLPanelPeople::onGroupsViewSortMenuItemClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();

	if (chosen_item == "show_icons")
	{
		mGroupList->toggleIcons();
	}
}

void LLPanelPeople::onNearbyViewSortMenuItemClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();

	// <FS:Ansariel> Firestorm radar
	//if (chosen_item == "sort_by_recent_speakers")
	//{
	//	setSortOrder(mNearbyList, E_SORT_BY_RECENT_SPEAKERS);
	//}
	//else if (chosen_item == "sort_name")
	//{
	//	setSortOrder(mNearbyList, E_SORT_BY_NAME);
	//}
	//else if (chosen_item == "view_icons")
	//{
	//	mNearbyList->toggleIcons();
	//}
	//else if (chosen_item == "sort_distance")
	//{
	//	setSortOrder(mNearbyList, E_SORT_BY_DISTANCE);
	//}
	//else if (chosen_item == "panel_block_list_sidetray")
	if (chosen_item == "panel_block_list_sidetray")
	// </FS:Ansariel> Firestorm radar
	{
		// <FS:Ansariel> Optional standalone blocklist floater
		//LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD());
		if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
		{
			LLFloaterReg::showInstance("fs_blocklist", LLSD());
		}
		else
		{
			LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD());
		}
		// </FS:Ansariel>
	}
}

bool LLPanelPeople::onNearbyViewSortMenuItemCheck(const LLSD& userdata)
{
	std::string item = userdata.asString();
	U32 sort_order = gSavedSettings.getU32("NearbyPeopleSortOrder");

	if (item == "sort_by_recent_speakers")
		return sort_order == E_SORT_BY_RECENT_SPEAKERS;
	if (item == "sort_name")
		return sort_order == E_SORT_BY_NAME;
	if (item == "sort_distance")
		return sort_order == E_SORT_BY_DISTANCE;

	return false;
}

void LLPanelPeople::onRecentViewSortMenuItemClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();

	if (chosen_item == "sort_recent")
	{
		setSortOrder(mRecentList, E_SORT_BY_MOST_RECENT);
	} 
	else if (chosen_item == "sort_name")
	{
		setSortOrder(mRecentList, E_SORT_BY_NAME);
	}
	else if (chosen_item == "view_icons")
	{
		mRecentList->toggleIcons();
	}
	else if (chosen_item == "panel_block_list_sidetray")
	{
		// <FS:Ansariel> Optional standalone blocklist floater
		//LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD());
		if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
		{
			LLFloaterReg::showInstance("fs_blocklist", LLSD());
		}
		else
		{
			LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD());
		}
		// </FS:Ansariel>
	}
}

bool LLPanelPeople::onFriendsViewSortMenuItemCheck(const LLSD& userdata) 
{
	std::string item = userdata.asString();
	U32 sort_order = gSavedSettings.getU32("FriendsSortOrder");

	if (item == "sort_name") 
		return sort_order == E_SORT_BY_NAME;
	if (item == "sort_status")
		return sort_order == E_SORT_BY_STATUS;

	return false;
}

bool LLPanelPeople::onRecentViewSortMenuItemCheck(const LLSD& userdata) 
{
	std::string item = userdata.asString();
	U32 sort_order = gSavedSettings.getU32("RecentPeopleSortOrder");

	if (item == "sort_recent")
		return sort_order == E_SORT_BY_MOST_RECENT;
	if (item == "sort_name") 
		return sort_order == E_SORT_BY_NAME;

	return false;
}

void LLPanelPeople::onCallButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);

	if (selected_uuids.size() == 1)
	{
		// initiate a P2P voice chat with the selected user
		LLAvatarActions::startCall(getCurrentItemID());
	}
	else if (selected_uuids.size() > 1)
	{
		// initiate an ad-hoc voice chat with multiple users
		LLAvatarActions::startAdhocCall(selected_uuids);
	}
}

void LLPanelPeople::onGroupCallButtonClicked()
{
	LLGroupActions::startCall(getCurrentItemID());
}

void LLPanelPeople::onTeleportButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	// <FS:Ansariel> Firestorm change
	//LLAvatarActions::offerTeleport(selected_uuids);
	LLAvatarActions::offerTeleport(LLAvatarActions::canOfferTeleport(selected_uuids));
	// </FS:Ansariel> Firestorm change
}

void LLPanelPeople::onShareButtonClicked()
{
	LLAvatarActions::share(getCurrentItemID());
}

void LLPanelPeople::onMoreButtonClicked()
{
	// *TODO: not implemented yet
}

void	LLPanelPeople::onOpen(const LLSD& key)
{
	std::string tab_name = key["people_panel_tab_name"];
	if (!tab_name.empty())
		mTabContainer->selectTabByName(tab_name);
}

bool LLPanelPeople::notifyChildren(const LLSD& info)
{
	if (info.has("task-panel-action") && info["task-panel-action"].asString() == "handle-tri-state")
	{
		LLSideTrayPanelContainer* container = dynamic_cast<LLSideTrayPanelContainer*>(getParent());
		if (!container)
		{
			llwarns << "Cannot find People panel container" << llendl;
			return true;
		}

		if (container->getCurrentPanelIndex() > 0) 
		{
			// if not on the default panel, switch to it
			container->onOpen(LLSD().with(LLSideTrayPanelContainer::PARAM_SUB_PANEL_NAME, getName()));
		}
		else
			LLFloaterReg::hideInstance("people");

		return true; // this notification is only supposed to be handled by task panels
	}

	return LLPanel::notifyChildren(info);
}

void LLPanelPeople::showAccordion(const std::string name, bool show)
{
	if(name.empty())
	{
		llwarns << "No name provided" << llendl;
		return;
	}

	LLAccordionCtrlTab* tab = getChild<LLAccordionCtrlTab>(name);
	tab->setVisible(show);
	if(show)
	{
		// don't expand accordion if it was collapsed by user
		if(!isAccordionCollapsedByUser(tab))
		{
			// expand accordion
			tab->changeOpenClose(false);
		}
	}
}

void LLPanelPeople::showFriendsAccordionsIfNeeded()
{
	if(FRIENDS_TAB_NAME == getActiveTabName())
	{
		// Expand and show accordions if needed, else - hide them
		showAccordion("tab_online", mOnlineFriendList->filterHasMatches());
		showAccordion("tab_all", mAllFriendList->filterHasMatches());

		// Rearrange accordions
		LLAccordionCtrl* accordion = getChild<LLAccordionCtrl>("friends_accordion");
		accordion->arrange();

		// *TODO: new no_matched_tabs_text attribute was implemented in accordion (EXT-7368).
		// this code should be refactored to use it
		// keep help text in a synchronization with accordions visibility.
		updateFriendListHelpText();
	}
}

void LLPanelPeople::onFriendListRefreshComplete(LLUICtrl*ctrl, const LLSD& param)
{
	if(ctrl == mOnlineFriendList)
	{
		showAccordion("tab_online", param.asInteger());
	}
	else if(ctrl == mAllFriendList)
	{
		showAccordion("tab_all", param.asInteger());
	}
}

void LLPanelPeople::setAccordionCollapsedByUser(LLUICtrl* acc_tab, bool collapsed)
{
	if(!acc_tab)
	{
		llwarns << "Invalid parameter" << llendl;
		return;
	}

	LLSD param = acc_tab->getValue();
	param[COLLAPSED_BY_USER] = collapsed;
	acc_tab->setValue(param);
}

void LLPanelPeople::setAccordionCollapsedByUser(const std::string& name, bool collapsed)
{
	setAccordionCollapsedByUser(getChild<LLUICtrl>(name), collapsed);
}

bool LLPanelPeople::isAccordionCollapsedByUser(LLUICtrl* acc_tab)
{
	if(!acc_tab)
	{
		llwarns << "Invalid parameter" << llendl;
		return false;
	}

	LLSD param = acc_tab->getValue();
	if(!param.has(COLLAPSED_BY_USER))
	{
		return false;
	}
	return param[COLLAPSED_BY_USER].asBoolean();
}

bool LLPanelPeople::isAccordionCollapsedByUser(const std::string& name)
{
	return isAccordionCollapsedByUser(getChild<LLUICtrl>(name));
}

// <FS:Ansariel> Firestorm radar
void LLPanelPeople::onRadarListDoubleClicked()
{
	LLScrollListItem* item = mRadarList->getFirstSelected();
	if (!item)
	{
		return;
	}

	LLUUID clicked_id = item->getColumn(mRadarList->getColumn("uuid")->mIndex)->getValue().asUUID();
	std::string name = item->getColumn(mRadarList->getColumn("name")->mIndex)->getValue().asString();

	FSRadar* radar = FSRadar::getInstance();
	if (radar)
	{
		radar->zoomAvatar(clicked_id, name);
	}
}

void LLPanelPeople::updateNearby(const std::vector<LLSD>& entries, const LLSD& stats)
{
	if (!LLFloaterReg::instanceVisible("people") || getActiveTabName() != NEARBY_TAB_NAME)
	{
		return;
	}

	if (!mRadarList)
	{
		return;
	}

	static const std::string flagsColumnType = getString("FlagsColumnType");
	static const std::string flagsColumnValues [3] = { getString("FlagsColumnValue_0"), getString("FlagsColumnValue_1"), getString("FlagsColumnValue_2") };

	// Store current selection and scroll position
	static S32 uuidColumnIndex = mRadarList->getColumn("uuid")->mIndex;
	std::vector<LLScrollListItem*> selected_items = mRadarList->getAllSelected();
	uuid_vec_t selected_ids;
	for (size_t i = 0; i < selected_items.size(); i++)
	{
		selected_ids.push_back(selected_items.at(i)->getColumn(uuidColumnIndex)->getValue().asUUID());
	}
	S32 lastScroll = mRadarList->getScrollPos();

	// Update list
	mRadarList->clearRows();
	const std::vector<LLSD>::const_iterator it_end = entries.end();
	for (std::vector<LLSD>::const_iterator it = entries.begin(); it != it_end; ++it)
	{
		LLSD entry = (*it)["entry"];
		LLSD options = (*it)["options"];

		LLSD row_data;
		row_data["value"] = entry["id"];
		row_data["columns"][0]["column"] = "name";
		row_data["columns"][0]["value"] = entry["name"];
		row_data["columns"][1]["column"] = "voice_level";
		row_data["columns"][1]["type"] = "icon";
		row_data["columns"][1]["value"] = ""; // Need to set it after the row has been created because it's to big for the row
		row_data["columns"][2]["column"] = "in_region";
		row_data["columns"][2]["type"] = "icon";
		row_data["columns"][2]["value"] = (entry["in_region"].asBoolean() ? "avatar_in_region" : "");
		row_data["columns"][3]["column"] = "flags";
		row_data["columns"][3]["type"] = flagsColumnType;
		row_data["columns"][4]["column"] = "age";
		row_data["columns"][4]["value"] = entry["age"];
		row_data["columns"][5]["column"] = "seen";
		row_data["columns"][5]["value"] = entry["seen"];
		row_data["columns"][6]["column"] = "range";
		row_data["columns"][6]["value"] = entry["range"];
		row_data["columns"][7]["column"] = "uuid"; // invisible column for referencing av-key the row belongs to
		row_data["columns"][7]["value"] = entry["id"];

		LLScrollListItem* row = mRadarList->addElement(row_data);

		static S32 rangeColumnIndex = mRadarList->getColumn("range")->mIndex;
		static S32 nameColumnIndex = mRadarList->getColumn("name")->mIndex;
		static S32 voiceLevelColumnIndex = mRadarList->getColumn("voice_level")->mIndex;
		static S32 flagsColumnIndex = mRadarList->getColumn("flags")->mIndex;

		LLScrollListText* radarRangeCell = (LLScrollListText*)row->getColumn(rangeColumnIndex);
		radarRangeCell->setColor(LLColor4(options["range_color"]));
		radarRangeCell->setFontStyle(options["range_style"].asInteger());

		LLScrollListText* radarNameCell = (LLScrollListText*)row->getColumn(nameColumnIndex);
		radarNameCell->setFontStyle(options["name_style"].asInteger());
		if (options.has("name_color"))
		{
			radarNameCell->setColor(LLColor4(options["name_color"]));
		}

		LLScrollListText* voiceLevelCell = (LLScrollListText*)row->getColumn(voiceLevelColumnIndex);
		if (entry.has("voice_level_icon"))
		{
			voiceLevelCell->setValue(entry["voice_level_icon"].asString());
		}

		LLScrollListText* flagsCell = (LLScrollListText*)row->getColumn(flagsColumnIndex);
		if (entry.has("flags"))
		{
			flagsCell->setValue(flagsColumnValues[entry["flags"].asInteger()]);
		}
	}

	LLStringUtil::format_map_t name_count_args;
	name_count_args["[TOTAL]"] = stats["total"].asString();
	name_count_args["[IN_REGION]"] = stats["region"].asString();
	name_count_args["[IN_CHAT_RANGE]"] = stats["chatrange"].asString();
	LLScrollListColumn* column = mRadarList->getColumn("name");
	column->mHeader->setLabel(getString("avatar_name_count", name_count_args));
	column->mHeader->setToolTipArgs(name_count_args);

	// Restore scroll position
	mRadarList->setScrollPos(lastScroll);

	// Restore selection list
	if (!selected_ids.empty())
	{
		mRadarList->selectMultiple(selected_ids);
	}

	updateButtons();
}


void LLPanelPeople::onGlobalVisToggleButtonClicked()
// Iterate through friends lists, toggling status permission on or off 
{	
	bool vis = getChild<LLUICtrl>("GlobalOnlineStatusToggle")->getValue().asBoolean();
	gSavedSettings.setBOOL("GlobalOnlineStatusToggle", vis);
	
	const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	LLAvatarTracker::buddy_map_t all_buddies;
	av_tracker.copyBuddyList(all_buddies);
	LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();
	for (; buddy_it != all_buddies.end(); ++buddy_it)
	{
		LLUUID buddy_id = buddy_it->first;
		const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(buddy_id);
		if (relation == NULL)
		{
			// Lets have a warning log message instead of having a crash. EXT-4947.
			llwarns << "Trying to modify rights for non-friend avatar. Skipped." << llendl;
			return;
		}
		
		S32 cur_rights = relation->getRightsGrantedTo();
		S32 new_rights = 0;
		if (vis)
			new_rights = LLRelationship::GRANT_ONLINE_STATUS + (cur_rights &  LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);
		else
			new_rights = (cur_rights &  LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);

		LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(buddy_id,new_rights);
	}		
	
	mAllFriendList->showPermissions(true);
	mOnlineFriendList->showPermissions(true);
	
	LLSD args;
	args["MESSAGE"] = getString("high_server_load");
	LLNotificationsUtil::add("GenericAlert", args);
}
// </FS:Ansariel> Firestorm radar

// EOF
