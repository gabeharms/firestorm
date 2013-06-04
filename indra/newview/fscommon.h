/**
 * @file fscommon.h
 * @brief Central class for common used functions in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
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

#ifndef FS_COMMON_H
#define FS_COMMON_H

#include "llchat.h"
#include "llpanelpeople.h"
#include "llviewerobject.h"

const F32 AVATAR_UNKNOWN_Z_OFFSET = -1.f; // Const value for avatars at unknown height
const F32 AVATAR_UNKNOWN_RANGE = -1.f;

void reportToNearbyChat(const std::string& message);
std::string applyAutoCloseOoc(const std::string& message);
std::string applyMuPose(const std::string& message);
std::string formatString(std::string text, const LLStringUtil::format_map_t& args);
LLPanelPeople* getPeoplePanel();

namespace FSCommon
{
	/**
	* Convert a string of a specified date format into seconds since the Epoch.
	*
	* Many of the format flags are those used by strftime(...), but not all.
	* For the full list of supported time format specifiers
	* see http://www.boost.org/doc/libs/1_47_0/doc/html/date_time/date_time_io.html#date_time.format_flags
	* 
	* time support added by Techwolf Lupindo
	*
	* @param format Format characters string. Example: "%A %b %d, %Y"
	* @param str    Date string containing the time in specified format.
	*
	* @return Number of seconds since 01/01/1970 UTC.
	*/
	S32 secondsSinceEpochFromString(const std::string& format, const std::string& str);
	
	// apply default build preferences to the object
	void applyDefaultBuildPreferences(LLViewerObject* object);

	bool isLinden(const LLUUID& av_id);
	
	/**
	 * HACK
	 * 
	 * This is used to work around a LL design flaw of the similular returning the same object update packet
	 * for _PREHASH_ObjectAdd, _PREHASH_RezObject, and _PREHASH_RezObjectFromNotecard.
	 * 
	 * keep track of ObjectAdd messages sent to the similular.
	 */
	extern S32 sObjectAddMsg;
};

#endif // FS_COMMON_H
