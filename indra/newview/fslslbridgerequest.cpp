/** 
 * @file fslslbridgerequest.cpp
 * @brief FSLSLBridgerequest implementation
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

#include "llviewerprecompiledheaders.h"

#include "fslslbridgerequest.h"
#include "fsradar.h"

#include <boost/tokenizer.hpp>


FSLSLBridgeRequestResponder::FSLSLBridgeRequestResponder() 
{
}

FSLSLBridgeRequestResponder::~FSLSLBridgeRequestResponder()
{
}

//If we get back a normal response, handle it here
void FSLSLBridgeRequestResponder::result(const LLSD& content)
{
	std::string strContent = content.asString();
	LL_DEBUGS() << "Got info: " << strContent << LL_ENDL;

	//do not use - infinite loop, only here for testing.
	//FSLSLBridge::instance().viewerToLSL("Response_to_response|" + strContent);
}

//If we get back an error (not found, etc...), handle it here
void FSLSLBridgeRequestResponder::error(U32 status, const std::string& reason)
{
	LL_WARNS() << "FSLSLBridgeRequest::error(" << status << ": " << reason << ")" << LL_ENDL;
}

// AO: The below handler is used to parse return data from the bridge, requesting bulk ZOffset updates.
FSLSLBridgeRequestRadarPosResponder::FSLSLBridgeRequestRadarPosResponder()
{
}

FSLSLBridgeRequestRadarPosResponder::~FSLSLBridgeRequestRadarPosResponder()
{
}

void FSLSLBridgeRequestRadarPosResponder::result(const LLSD& content)
{
	FSRadar* radar = FSRadar::getInstance();
	if (radar)
	{
		std::string strContent = content.asString();
		//LL_INFOS() << "Got info: " << strContent << LL_ENDL;
		// AO: parse content into pairs of [agent UUID,agent zHeight] , update our peoplepanel radar for each one
		
		LLUUID targetAv;
		F32 targetZ;
		
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep(", "); 
		tokenizer tokens(strContent, sep);
		for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
		{
			targetAv = LLUUID(*(tok_iter++));
			targetZ = (F32)::atof((*tok_iter).c_str());
			
			FSRadarEntry* entry = radar->getEntry(targetAv);
			if (entry)
			{
				entry->setZOffset((F32)(targetZ));
				//LL_INFOS() << targetAv << " ::: " << targetZ << LL_ENDL;
			}
		}
	}
}

