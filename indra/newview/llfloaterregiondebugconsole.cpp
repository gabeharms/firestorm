/** 
 * @file llfloaterregiondebugconsole.h
 * @author Brad Kittenbrink <brad@lindenlab.com>
 * @brief Quick and dirty console for region debug settings
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 * 
 * Copyright (c) 2010-2010, Linden Research, Inc.
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

#include "llfloaterregiondebugconsole.h"

#include "llagent.h"
#include "llhttpclient.h"
#include "lllineeditor.h"
#include "lltexteditor.h"
#include "llviewerregion.h"

class Responder : public LLHTTPClient::Responder {
public:
    Responder(LLTextEditor *output) : mOutput(output)
    {
    }

    /*virtual*/
    void error(U32 status, const std::string& reason)
    {
    }

    /*virtual*/
    void result(const LLSD& content)
    {
        std::string text = mOutput->getText();
        text += '\n';
        text += content.asString();
        text += '\n';
        mOutput->setText(text);
    };

    LLTextEditor * mOutput;
};

LLFloaterRegionDebugConsole::LLFloaterRegionDebugConsole(LLSD const & key)
: LLFloater(key), mOutput(NULL)
{
}

BOOL LLFloaterRegionDebugConsole::postBuild()
{
	getChild<LLLineEditor>("region_debug_console_input")->setCommitCallback(boost::bind(&LLFloaterRegionDebugConsole::onInput, this, _1, _2));
	mOutput = getChild<LLTextEditor>("region_debug_console_output");
	return TRUE;
}

void LLFloaterRegionDebugConsole::onInput(LLUICtrl* ctrl, const LLSD& param)
{
	LLLineEditor * input = static_cast<LLLineEditor*>(ctrl);
	std::string text = mOutput->getText();
    text += "\n\POST: ";
	text += input->getText();
	mOutput->setText(text);

    std::string url = gAgent.getRegion()->getCapability("SimConsole");
    LLHTTPClient::post(url, LLSD(input->getText()), new ::Responder(mOutput));

	input->setText(std::string(""));
}

