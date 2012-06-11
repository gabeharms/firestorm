/** 
 * @file llfloaterspellchecksettings.h
 * @brief Spell checker settings floater
 *
* $LicenseInfo:firstyear=2011&license=viewerlgpl$
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

#ifndef LLFLOATERAUTOREPLACESETTINGS_H
#define LLFLOATERAUTOREPLACESETTINGS_H

#include "llfloater.h"

class LLFloaterSpellCheckerSettings : public LLFloater
{
public:
	LLFloaterSpellCheckerSettings(const LLSD& key);

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);

protected:
	void onBtnCancel();
	void onBtnImport();
	void onBtnMove(const std::string& from, const std::string& to);
	void onBtnOK();
	void onSpellCheckSettingsChange();
	void refreshDictionaries(bool from_settings);
};

class LLFloaterSpellCheckerImport : public LLFloater
{
public:
	LLFloaterSpellCheckerImport(const LLSD& key);

	/*virtual*/ BOOL postBuild();

protected:
	void onBtnBrowse();
	void onBtnCancel();
	void onBtnOK();

	std::string mDictionaryDir;
	std::string mDictionaryBasename;
};

#endif  // LLFLOATERAUTOREPLACESETTINGS_H
