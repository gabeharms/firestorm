/** 
 * @file fsradarmenu.cpp
 * @brief Menu used by Firestorm radar
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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

// libs
#include "llmenugl.h"
#include "lluictrlfactory.h"

#include "fsradarmenu.h"

// newview
#include "fsradar.h"
#include "llagent.h"
#include "llagentdata.h"			// for gAgentID
#include "llavataractions.h"
#include "llcallingcard.h"			// for LLAvatarTracker
#include "llviewermenu.h"			// for gMenuHolder
#include "rlvhandler.h"

namespace FSFloaterRadarMenu
{

FSRadarMenu gFSRadarMenu;

//== NearbyMenu ===============================================================

LLContextMenu* FSRadarMenu::createMenu()
{
	// set up the callbacks for all of the avatar menu items
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	if ( mUUIDs.size() == 1 )
	{
		// Set up for one person selected menu

		const LLUUID& id = mUUIDs.front();
		registrar.add("Avatar.Profile",							boost::bind(&LLAvatarActions::showProfile,					id));
		registrar.add("Avatar.AddFriend",						boost::bind(&LLAvatarActions::requestFriendshipDialog,		id));
		registrar.add("Avatar.RemoveFriend",					boost::bind(&LLAvatarActions::removeFriendDialog, 			id));
		registrar.add("Avatar.IM",								boost::bind(&LLAvatarActions::startIM,						id));
		registrar.add("Avatar.Call",							boost::bind(&LLAvatarActions::startCall,					id));
		registrar.add("Avatar.OfferTeleport",					boost::bind(&FSRadarMenu::offerTeleport,					this));
		registrar.add("Avatar.GroupInvite",						boost::bind(&LLAvatarActions::inviteToGroup,				id));
		registrar.add("Avatar.getScriptInfo",					boost::bind(&LLAvatarActions::getScriptInfo,				id));
		registrar.add("Avatar.ShowOnMap",						boost::bind(&LLAvatarActions::showOnMap,					id));
		registrar.add("Avatar.Share",							boost::bind(&LLAvatarActions::share,						id));
		registrar.add("Avatar.Pay",								boost::bind(&LLAvatarActions::pay,							id));
		registrar.add("Avatar.BlockUnblock",					boost::bind(&LLAvatarActions::toggleBlock,					id));
		registrar.add("Avatar.ZoomIn",							boost::bind(&LLAvatarActions::zoomIn,						id));
		registrar.add("Avatar.Report",							boost::bind(&LLAvatarActions::report,						id));
		registrar.add("Avatar.Eject",							boost::bind(&LLAvatarActions::landEject,					id));
		registrar.add("Avatar.Freeze",							boost::bind(&LLAvatarActions::landFreeze,					id));
		registrar.add("Avatar.Kick",							boost::bind(&LLAvatarActions::estateKick,					id));
		registrar.add("Avatar.TeleportHome",					boost::bind(&LLAvatarActions::estateTeleportHome,			id));
		registrar.add("Avatar.EstateBan",						boost::bind(&LLAvatarActions::estateBan,					id));
		registrar.add("Avatar.Derender",						boost::bind(&LLAvatarActions::derender,						id, false));
		registrar.add("Avatar.DerenderPermanent",				boost::bind(&LLAvatarActions::derender,						id, true));
		registrar.add("Nearby.People.TeleportToAvatar",			boost::bind(&FSRadarMenu::teleportToAvatar,					this));
		registrar.add("Nearby.People.TrackAvatar",				boost::bind(&FSRadarMenu::onTrackAvatarMenuItemClick,		this));

		enable_registrar.add("Avatar.EnableItem",				boost::bind(&FSRadarMenu::enableContextMenuItem,			this, _2));
		enable_registrar.add("Avatar.CheckItem",				boost::bind(&FSRadarMenu::checkContextMenuItem,				this, _2));
		enable_registrar.add("Avatar.VisibleZoomIn",			boost::bind(&LLAvatarActions::canZoomIn,					id));
		enable_registrar.add("Avatar.VisibleFreezeEject",		boost::bind(&LLAvatarActions::canLandFreezeOrEject,			id));
		enable_registrar.add("Avatar.VisibleKickTeleportHome",	boost::bind(&LLAvatarActions::canEstateKickOrTeleportHome,	id));

		// create the context menu from the XUI
		return createFromFile("menu_fs_radar.xml");
	}
	else
	{
		// Set up for multi-selected People

		// registrar.add("Avatar.AddFriend",					boost::bind(&LLAvatarActions::requestFriendshipDialog,				mUUIDs)); // *TODO: unimplemented
		registrar.add("Avatar.IM",								boost::bind(&LLAvatarActions::startConference,						mUUIDs));
		registrar.add("Avatar.Call",							boost::bind(&LLAvatarActions::startAdhocCall,						mUUIDs));
		registrar.add("Avatar.OfferTeleport",					boost::bind(&FSRadarMenu::offerTeleport,							this));
		registrar.add("Avatar.RemoveFriend",					boost::bind(&LLAvatarActions::removeFriendsDialog,					mUUIDs));
		// registrar.add("Avatar.Share",						boost::bind(&LLAvatarActions::startIM,								mUUIDs)); // *TODO: unimplemented
		// registrar.add("Avatar.Pay",							boost::bind(&LLAvatarActions::pay,									mUUIDs)); // *TODO: unimplemented
		registrar.add("Avatar.Eject",							boost::bind(&LLAvatarActions::landEjectMultiple,					mUUIDs));
		registrar.add("Avatar.Freeze",							boost::bind(&LLAvatarActions::landFreezeMultiple,					mUUIDs));
		registrar.add("Avatar.Kick",							boost::bind(&LLAvatarActions::estateKickMultiple,					mUUIDs));
		registrar.add("Avatar.TeleportHome",					boost::bind(&LLAvatarActions::estateTeleportHomeMultiple,			mUUIDs));
		registrar.add("Avatar.EstateBan",						boost::bind(&LLAvatarActions::estateBanMultiple,					mUUIDs));
		registrar.add("Avatar.Derender",						boost::bind(&LLAvatarActions::derenderMultiple,						mUUIDs, false));
		registrar.add("Avatar.DerenderPermanent",				boost::bind(&LLAvatarActions::derenderMultiple,						mUUIDs, true));

		enable_registrar.add("Avatar.EnableItem",				boost::bind(&FSRadarMenu::enableContextMenuItem,					this, _2));
		enable_registrar.add("Avatar.VisibleFreezeEject",		boost::bind(&LLAvatarActions::canLandFreezeOrEjectMultiple,			mUUIDs, false));
		enable_registrar.add("Avatar.VisibleKickTeleportHome",	boost::bind(&LLAvatarActions::canEstateKickOrTeleportHomeMultiple,	mUUIDs, false));
		
		// create the context menu from the XUI
		return createFromFile("menu_fs_radar_multiselect.xml");
	}
}

bool FSRadarMenu::enableContextMenuItem(const LLSD& userdata)
{
	std::string item = userdata.asString();

	// Note: can_block and can_delete is used only for one person selected menu
	// so we don't need to go over all uuids.

	if (item == std::string("can_block"))
	{
		const LLUUID& id = mUUIDs.front();
		return LLAvatarActions::canBlock(id);
	}
	else if (item == std::string("can_add"))
	{
		// We can add friends if:
		// - there are selected people
		// - and there are no friends among selection yet.

		//EXT-7389 - disable for more than 1
		if(mUUIDs.size() > 1)
		{
			return false;
		}

		bool result = (mUUIDs.size() > 0);

		uuid_vec_t::const_iterator
			id = mUUIDs.begin(),
			uuids_end = mUUIDs.end();

		for (;id != uuids_end; ++id)
		{
			if ( LLAvatarActions::isFriend(*id) )
			{
				result = false;
				break;
			}
		}

		return result;
	}
	else if (item == std::string("can_delete"))
	{
		// We can remove friends if:
		// - there are selected people
		// - and there are only friends among selection.

		bool result = (mUUIDs.size() > 0);

		uuid_vec_t::const_iterator
			id = mUUIDs.begin(),
			uuids_end = mUUIDs.end();

		for (;id != uuids_end; ++id)
		{
			if ( !LLAvatarActions::isFriend(*id) )
			{
				result = false;
				break;
			}
		}

		return result;
	}
	else if (item == std::string("can_call"))
	{
		return LLAvatarActions::canCall();
	}
	else if (item == std::string("can_show_on_map"))
	{
		const LLUUID& id = mUUIDs.front();

		return (LLAvatarTracker::instance().isBuddyOnline(id) && is_agent_mappable(id))
					|| gAgent.isGodlike();
	}
	// <FS> Prevent teleport button from being disabled when someone on your
	//      friends list logs out but is still in the region and you have
	//      multiple people selected.
	//else if(item == std::string("can_offer_teleport"))
	//{
	//	return LLAvatarActions::canOfferTeleport(mUUIDs);
	//}
	// </FS>
	else if (item == std::string("can_open_inventory"))
	{
		return (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV));
	}
	return false;
}

bool FSRadarMenu::checkContextMenuItem(const LLSD& userdata)
{
	std::string item = userdata.asString();
	const LLUUID& id = mUUIDs.front();

	if (item == std::string("is_blocked"))
	{
		return LLAvatarActions::isBlocked(id);
	}

	return false;
}

void FSRadarMenu::offerTeleport()
{
	// boost::bind cannot recognize overloaded method LLAvatarActions::offerTeleport(),
	// so we have to use a wrapper.
	LLAvatarActions::offerTeleport(mUUIDs);
}
	
void FSRadarMenu::teleportToAvatar()
// AO: wrapper for functionality managed by LLPanelPeople, because it manages the nearby avatar list.
// Will only work for avatars within radar range.
{
	FSRadar* radar = FSRadar::getInstance();
	if (radar)
	{
		radar->teleportToAvatar(mUUIDs.front());
	}
}

// Ansariel: Avatar tracking feature
void FSRadarMenu::onTrackAvatarMenuItemClick()
{
	FSRadar* radar = FSRadar::getInstance();
	if (radar)
	{
		radar->startTracking(mUUIDs.front());
	}
}

} // namespace FSFloaterRadarMenu
