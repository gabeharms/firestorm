/** 
 * @file lllogchat.h
 * @brief LLFloaterChat class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef LL_LLLOGCHAT_H
#define LL_LLLOGCHAT_H

class LLChat;

class LLLogChat
{
public:
	// status values for callback function
	enum ELogLineType {
		LOG_EMPTY,
		LOG_LINE,
		LOG_LLSD,
		LOG_END
	};
	static std::string timestamp(bool withdate = false);
	static std::string makeLogFileName(std::string(filename));

	// Log a single line item to the appropriate chat file
	static void saveHistory(const std::string& filename, const LLChat& chat);

	// Prefer the above version - it saves more metadata about the item
	static void saveHistory(const std::string& filename,
				const std::string& from,
				const LLUUID& from_id,
				const std::string& line);

	static void loadAllHistory(const std::string& file_name, std::list<LLSD>& messages);
private:
	static std::string cleanFileName(std::string filename);
};

/**
 * Parser for the plain text chat log files
 */
class LLChatLogParser
{
public:

	 /**
	 * Parse a line from the plain text chat log file
	 * General plain text log format is like: "[timestamp]  [name]: [message]"
	 * [timestamp] and [name] are optional
	 * Examples of plain text chat log lines:
	 * "[2009/11/20 2:53]  Igor ProductEngine: howdy"
	 * "Igor ProductEngine: howdy"
	 * "Dserduk ProductEngine is Online"
	 *
	 * @return false if failed to parse mandatory data - message text
	 */
	static bool parse(const std::string& raw, LLSD& im);

protected:
	LLChatLogParser();
	virtual ~LLChatLogParser() {};
};

// LLSD map lookup constants
extern const std::string IM_TIME; //("time");
extern const std::string IM_TEXT; //("message");
extern const std::string IM_FROM; //("from");
extern const std::string IM_FROM_ID; //("from_id");
extern const std::string IM_SOURCE_TYPE; //("source_type");

#endif
