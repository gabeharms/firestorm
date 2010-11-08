/**
 * @file   llupdaterservice_test.cpp
 * @brief  Tests of llupdaterservice.cpp.
 * 
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
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

// Precompiled header
#include "linden_common.h"
// associated header
#include "../llupdaterservice.h"
#include "../llupdatechecker.h"
#include "../llupdatedownloader.h"

#include "../../../test/lltut.h"
//#define DEBUG_ON
#include "../../../test/debug.h"

#include "llevents.h"

/*****************************************************************************
*   MOCK'd
*****************************************************************************/
LLUpdateChecker::LLUpdateChecker(LLUpdateChecker::Client & client)
{}
void LLUpdateChecker::check(std::string const & protocolVersion, std::string const & hostUrl, 
								  std::string const & servicePath, std::string channel, std::string version)
{}
LLUpdateDownloader::LLUpdateDownloader(Client & ) {}
void LLUpdateDownloader::download(LLURI const & , std::string const &){}

/*****************************************************************************
*   TUT
*****************************************************************************/
namespace tut
{
    struct llupdaterservice_data
    {
		llupdaterservice_data() :
            pumps(LLEventPumps::instance()),
			test_url("dummy_url"),
			test_channel("dummy_channel"),
			test_version("dummy_version")
		{}
		LLEventPumps& pumps;
		std::string test_url;
		std::string test_channel;
		std::string test_version;
	};

    typedef test_group<llupdaterservice_data> llupdaterservice_group;
    typedef llupdaterservice_group::object llupdaterservice_object;
    llupdaterservice_group llupdaterservicegrp("LLUpdaterService");

    template<> template<>
    void llupdaterservice_object::test<1>()
    {
        DEBUG;
		LLUpdaterService updater;
		bool got_usage_error = false;
		try
		{
			updater.startChecking();
		}
		catch(LLUpdaterService::UsageError)
		{
			got_usage_error = true;
		}
		ensure("Caught start before params", got_usage_error);
	}

    template<> template<>
    void llupdaterservice_object::test<2>()
    {
        DEBUG;
		LLUpdaterService updater;
		bool got_usage_error = false;
		try
		{
			updater.setParams("1.0",test_url, "update" ,test_channel, test_version);
			updater.startChecking();
			updater.setParams("1.0", "other_url", "update", test_channel, test_version);
		}
		catch(LLUpdaterService::UsageError)
		{
			got_usage_error = true;
		}
		ensure("Caught params while running", got_usage_error);
	}

    template<> template<>
    void llupdaterservice_object::test<3>()
    {
        DEBUG;
		LLUpdaterService updater;
		updater.setParams("1.0", test_url, "update", test_channel, test_version);
		updater.startChecking();
		ensure(updater.isChecking());
		updater.stopChecking();
		ensure(!updater.isChecking());
	}
}
