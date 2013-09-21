/**
 * @file fsslurlcommand.cpp
 * @brief SLurl command handler for Firestorm commands
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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
#include "fsslurlcommand.h"

#include "fscommon.h"
#include "fsradar.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llcommandhandler.h"
#include "llnotificationsutil.h"


class FSSlurlCommandHandler : public LLCommandHandler
{
public:
	// not allowed from outside the app
	FSSlurlCommandHandler() : LLCommandHandler("firestorm", UNTRUSTED_BLOCK) { }

    // Your code here
	bool handle(const LLSD& params, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		if (params.size() < 2)
		{
			return false;
		}

		LLUUID target_id;
		if (!target_id.set(params[0], FALSE))
		{
			return false;
		}

		const std::string verb = params[1].asString();

		if (verb == "zoom")
		{
			if (LLAvatarActions::canZoomIn(target_id))
			{
				LLAvatarActions::zoomIn(target_id);
				return true;
			}

			LLNotificationsUtil::add("ZoomToAvatarNotPossible");
			return true;
		}

		if (verb == "offerteleport")
		{
			if (gAgentID != target_id)
			{
				LLAvatarActions::offerTeleport(target_id);
			}

			return true;
		}

		if (verb == "teleportto")
		{
			if (gAgentID != target_id)
			{
				FSRadar* radar = FSRadar::getInstance();
				if (radar)
				{
					FSRadarEntry* entry = radar->getEntry(target_id);
					if (entry)
					{
						LLVector3d pos = entry->getGlobalPos();
						pos.mdV[VZ] += 2.0;
						gAgent.teleportViaLocation(pos);
						return true;
					}
				}

				LLNotificationsUtil::add("TeleportToAvatarNotPossible");
			}

			return true;
		}

		if (verb == "track")
		{
			if (gAgentID != target_id)
			{
				FSRadar* radar = FSRadar::getInstance();
				if (radar)
				{
					FSRadarEntry* entry = radar->getEntry(target_id);
					if (entry)
					{
						radar->startTracking(target_id);
						return true;
					}
				}


				LLNotificationsUtil::add("TrackAvatarNotPossible");
			}
	
			return true;
		}

		return false;
	}
};

// Creating the object registers with the dispatcher.
FSSlurlCommandHandler gFSSlurlHandler;
