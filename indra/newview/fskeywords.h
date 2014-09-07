/** 
 * @file fskeywords.h
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Firestorm Project, Inc.
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

#ifndef FS_KEYWORDS_H
#define FS_KEYWORDS_H

#include "llsingleton.h"

class LLChat;

class FSKeywords : public LLSingleton<FSKeywords>
{
public:
	FSKeywords();
	virtual ~FSKeywords();

	void updateKeywords();
	bool chatContainsKeyword(const LLChat& chat, bool is_local);
	void static notify(const LLChat& chat); // <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground

private:
	std::vector<std::string> mWordList;
};

#endif // FS_KEYWORDS_H
