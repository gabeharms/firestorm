/**
 * @file   llviewerwindowlistener.cpp
 * @author Nat Goodspeed
 * @date   2009-06-30
 * @brief  Implementation for llviewerwindowlistener.
 * 
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * Copyright (c) 2009, Linden Research, Inc.
 * $/LicenseInfo$
 */

// Precompiled header
#include "llviewerprecompiledheaders.h"
// associated header
#include "llviewerwindowlistener.h"
// STL headers
#include <map>
// std headers
// external library headers
// other Linden headers
#include "llviewerwindow.h"

LLViewerWindowListener::LLViewerWindowListener(const std::string& pumpname, LLViewerWindow* llviewerwindow):
    LLDispatchListener(pumpname, "op"),
    mViewerWindow(llviewerwindow)
{
    // add() every method we want to be able to invoke via this event API.
    LLSD saveSnapshotArgs;
    saveSnapshotArgs["filename"] = LLSD::String();
    saveSnapshotArgs["reply"] = LLSD::String();
    // The following are optional, so don't build them into required prototype.
//  saveSnapshotArgs["width"] = LLSD::Integer();
//  saveSnapshotArgs["height"] = LLSD::Integer();
//  saveSnapshotArgs["showui"] = LLSD::Boolean();
//  saveSnapshotArgs["rebuild"] = LLSD::Boolean();
//  saveSnapshotArgs["type"] = LLSD::String();
    add("saveSnapshot", &LLViewerWindowListener::saveSnapshot, saveSnapshotArgs);
    add("requestReshape", &LLViewerWindowListener::requestReshape);
}

void LLViewerWindowListener::saveSnapshot(const LLSD& event) const
{
    LLReqID reqid(event);
    typedef std::map<LLSD::String, LLViewerWindow::ESnapshotType> TypeMap;
    TypeMap types;
#define tp(name) types[#name] = LLViewerWindow::SNAPSHOT_TYPE_##name
    tp(COLOR);
    tp(DEPTH);
    tp(OBJECT_ID);
#undef  tp
    // Our add() call should ensure that the incoming LLSD does in fact
    // contain our required arguments. Deal with the optional ones.
    S32 width (mViewerWindow->getWindowDisplayWidth());
    S32 height(mViewerWindow->getWindowDisplayHeight());
    if (event.has("width"))
        width = event["width"].asInteger();
    if (event.has("height"))
        height = event["height"].asInteger();
    // showui defaults to true, requiring special treatment
    bool showui = true;
    if (event.has("showui"))
        showui = event["showui"].asBoolean();
    bool rebuild(event["rebuild"]); // defaults to false
    LLViewerWindow::ESnapshotType type(LLViewerWindow::SNAPSHOT_TYPE_COLOR);
    if (event.has("type"))
    {
        TypeMap::const_iterator found = types.find(event["type"]);
        if (found == types.end())
        {
            LL_ERRS("LLViewerWindowListener") << "LLViewerWindowListener::saveSnapshot(): "
                                              << "unrecognized type " << event["type"] << LL_ENDL;
        }
        type = found->second;
    }
    bool ok = mViewerWindow->saveSnapshot(event["filename"], width, height, showui, rebuild, type);
    LLSD response(reqid.makeResponse());
    response["ok"] = ok;
    LLEventPumps::instance().obtain(event["reply"]).post(response);
}

void LLViewerWindowListener::requestReshape(LLSD const & event_data) const
{
	if(event_data.has("w") && event_data.has("h"))
	{
		mViewerWindow->reshape(event_data["w"].asInteger(), event_data["h"].asInteger());
	}
}
