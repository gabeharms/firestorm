/** 
 * @file llocationhistory.h
 * @brief Typed locations history
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009, Linden Research, Inc.
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

#ifndef LL_LLLOCATIONHISTORY_H
#define LL_LLLOCATIONHISTORY_H

#include "llsingleton.h" // for LLSingleton

#include <vector>
#include <string>
#include <boost/function.hpp>

class LLLocationHistory: public LLSingleton<LLLocationHistory>
{
	LOG_CLASS(LLLocationHistory);

public:
	typedef std::vector<std::string>	location_list_t;
	typedef boost::function<void()>		loaded_callback_t;
	typedef boost::signals2::signal<void()> loaded_signal_t;
	
	LLLocationHistory();
	
	void					addItem(std::string item);
	void                    removeItems();
	size_t					getItemCount() const	{ return mItems.size(); }
	const location_list_t&	getItems() const		{ return mItems; }
	bool					getMatchingItems(std::string substring, location_list_t& result) const;
	boost::signals2::connection	setLoadedCallback(loaded_callback_t cb) { return mLoadedSignal.connect(cb); }
	
	void					save() const;
	void					load();
	void					dump() const;

private:
	std::vector<std::string>	mItems;
	std::string					mFilename; /// File to store the history to.
	loaded_signal_t				mLoadedSignal;
};

#endif
