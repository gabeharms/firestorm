/** 
 * @file lggbeammaps.h
 * @brief Manager for Beam Shapes
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This code is free. It comes
 * WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#ifndef LGG_BEAMMAPS_H
#define LGG_BEAMMAPS_H

#include "llhudeffecttrail.h"
#include "lggbeamscolors.h"

class lggBeamData
{
public:
	LLVector3d p;
	LLColor4U c;
};

class lggBeamMaps
{
public:
	lggBeamMaps() :
		mLastFileName(""),
		mScale(0.0f),
		mDuration(0.25f),
		mPartsNow(false),
		mBeamLastAt(LLVector3d::zero)
		{}
	~lggBeamMaps() {}

	F32							setUpAndGetDuration();
	void						fireCurrentBeams(LLPointer<LLHUDEffectSpiral>, const LLColor4U& rgb);
	void						forceUpdate();
	void						stopBeamChat();
	void						updateBeamChat(const LLVector3d& currentPos);
	static LLColor4U			beamColorFromData(const lggBeamsColors& data);
	LLColor4U					getCurrentColor(const LLColor4U& agentColor);
	std::vector<std::string>	getFileNames();
	std::vector<std::string>	getColorsFileNames();

private:
	LLSD			getPic(const std::string& filename);

	std::string		mLastFileName;
	std::string		mLastColorFileName;
	lggBeamsColors	mLastColorsData;
	F32				mDuration;
	F32				mScale;
	bool			mPartsNow;
	LLVector3d		mBeamLastAt;

	std::vector<lggBeamData> mDots;
};

extern lggBeamMaps gLggBeamMaps;

#endif // LGG_BEAMMAPS_H
