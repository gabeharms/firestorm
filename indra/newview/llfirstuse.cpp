/** 
 * @file llfirstuse.cpp
 * @brief Methods that spawn "first-use" dialogs
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llfirstuse.h"

// library includes
#include "indra_constants.h"
#include "llnotificationsutil.h"

// viewer includes
#include "llagent.h"	// for gAgent.inPrelude()
#include "llviewercontrol.h"
#include "llui.h"
#include "llappviewer.h"
#include "lltracker.h"


// static
std::set<std::string> LLFirstUse::sConfigVariables;

// static
void LLFirstUse::addConfigVariable(const std::string& var)
{
	sConfigVariables.insert(var);
}

// static
void LLFirstUse::disableFirstUse()
{
	// Set all first-use warnings to disabled
	for (std::set<std::string>::iterator iter = sConfigVariables.begin();
		 iter != sConfigVariables.end(); ++iter)
	{
		gWarningSettings.setBOOL(*iter, FALSE);
	}
}

// static
void LLFirstUse::resetFirstUse()
{
	// Set all first-use warnings to disabled
	for (std::set<std::string>::iterator iter = sConfigVariables.begin();
		 iter != sConfigVariables.end(); ++iter)
	{
		gWarningSettings.setBOOL(*iter, TRUE);
	}
}

// static
void LLFirstUse::useOverrideKeys()
{
	// Our orientation island uses key overrides to teach vehicle driving
	// so don't show this message until you get off OI. JC
	if (!gAgent.inPrelude())
	{
		if (gWarningSettings.getBOOL("FirstOverrideKeys"))
		{
			gWarningSettings.setBOOL("FirstOverrideKeys", FALSE);

			LLNotificationsUtil::add("FirstOverrideKeys");
		}
	}
}

// static
void LLFirstUse::otherAvatarChat()
{
	if (gWarningSettings.getBOOL("FirstOtherChatBeforeUser"))
	{
		gWarningSettings.setBOOL("FirstOtherChatBeforeUser", FALSE);

		LLNotificationsUtil::add("HintChat", LLSD(), LLSD().with("target", "nearby_chat_bar").with("direction", "top"));
	}
}

// static
void LLFirstUse::sit()
{
	if (gWarningSettings.getBOOL("FirstSit"))
	{
		gWarningSettings.setBOOL("FirstSit", FALSE);

		LLNotificationsUtil::add("HintSit", LLSD(), LLSD().with("target", "stand_btn").with("direction", "top"));
	}
}

// static
void LLFirstUse::inventoryOffer()
{
	if (gWarningSettings.getBOOL("FirstInventoryOffer"))
	{
		gWarningSettings.setBOOL("FirstInventoryOffer", FALSE);

		LLNotificationsUtil::add("HintInventory", LLSD(), LLSD().with("target", "inventory_btn").with("direction", "left"));
	}
}

// static
void LLFirstUse::useSandbox()
{
	if (gWarningSettings.getBOOL("FirstSandbox"))
	{
		gWarningSettings.setBOOL("FirstSandbox", FALSE);

		LLSD args;
		args["HOURS"] = llformat("%d",SANDBOX_CLEAN_FREQ);
		args["TIME"] = llformat("%d",SANDBOX_FIRST_CLEAN_HOUR);
		LLNotificationsUtil::add("FirstSandbox", args);
	}
}
