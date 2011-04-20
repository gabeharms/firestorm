/**
 * @file aoset.cpp
 * @brief Implementation of an Animation Overrider set of animations
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Zi Ree @ Second Life
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
 */

#include "llviewerprecompiledheaders.h"

#include "aoengine.h"
#include "aoset.h"
#include "llanimationstates.h"

AOSet::AOSet(const LLUUID inventoryID)
:	LLEventTimer(10000.0f),
	mInventoryID(inventoryID),
	mName("** New AO Set **"),
	mSitOverride(FALSE),
	mMouselookDisable(FALSE),
	mComplete(FALSE),
	mDirty(FALSE),
	mCurrentMotion(LLUUID())
{
	llwarns << "Creating new AO set: " << this << llendl;

	// keep number and order in sync with the enum in the declaration
	const std::string stateNames[AOSTATES_MAX]=
	{
		"Standing",
		"Walking",
		"Running",
		"Sitting",
		"Sitting On Ground",
		"Crouching",
		"Crouch Walking",
		"Landing",
		"Standing Up",
		"Falling",
		"Flying Down",
		"Flying Up",
		"Flying",
		"Flying Slow",
		"Hovering",
		"Jumping",
		"Pre Jumping",
		"Turning Right",
		"Turning Left",
		"Typing",
		"Floating",
		"Swimming Forward",
		"Swimming Up",
		"Swimming Down"
	};

	// keep number and order in sync with the enum in the declaration
	const LLUUID stateUUIDs[AOSTATES_MAX]=
	{
		ANIM_AGENT_STAND,
		ANIM_AGENT_WALK,
		ANIM_AGENT_RUN,
		ANIM_AGENT_SIT,
		ANIM_AGENT_SIT_GROUND,
		ANIM_AGENT_CROUCH,
		ANIM_AGENT_CROUCHWALK,
		ANIM_AGENT_LAND,
		ANIM_AGENT_STANDUP,
		ANIM_AGENT_FALLDOWN,
		ANIM_AGENT_HOVER_DOWN,
		ANIM_AGENT_HOVER_UP,
		ANIM_AGENT_FLY,
		ANIM_AGENT_FLYSLOW,
		ANIM_AGENT_HOVER,
		ANIM_AGENT_JUMP,
		ANIM_AGENT_PRE_JUMP,
		ANIM_AGENT_TURNRIGHT,
		ANIM_AGENT_TURNLEFT,
		ANIM_AGENT_TYPE,
		ANIM_AGENT_HOVER,		// needs special treatment
		ANIM_AGENT_FLY,			// needs special treatment
		ANIM_AGENT_HOVER_UP,	// needs special treatment
		ANIM_AGENT_HOVER_DOWN	// needs special treatment
	};

	for(S32 index=0;index<AOSTATES_MAX;index++)
	{
		mStates[index].mName=stateNames[index];
		mStates[index].mRemapID=stateUUIDs[index];
		mStates[index].mInventoryUUID=LLUUID::null;
		mStates[index].mCurrentAnimationID=LLUUID::null;
		mStates[index].mCycleTime=0.0f;
		mStates[index].mRandom=FALSE;
		mStates[index].mDirty=FALSE;
		mStateNames.push_back(stateNames[index]);
	}
	stopTimer();
}

AOSet::~AOSet()
{
	llwarns << "Set deleted: " << this << llendl;
}

AOSet::AOState* AOSet::getState(S32 eName)
{
	return &mStates[eName];
}

AOSet::AOState* AOSet::getStateByName(const std::string name)
{
	for(S32 index=0;index<AOSTATES_MAX;index++)
	{
		if(mStates[index].mName.compare(name)==0)
		{
			return &mStates[index];
		}
	}
	return NULL;
}

AOSet::AOState* AOSet::getStateByRemapID(const LLUUID id)
{
	for(S32 index=0;index<AOSTATES_MAX;index++)
	{
		if(mStates[index].mRemapID==id)
		{
			return &mStates[index];
		}
	}
	return NULL;
}

LLUUID AOSet::getAnimationForState(AOState* state)
{
	if(state)
	{
		S32 numOfAnimations=state->mAnimations.size();
		if(numOfAnimations)
		{
			if(state->mRandom)
			{
				state->mCurrentAnimation=ll_frand()*numOfAnimations;
				llwarns << "randomly chosen " << state->mCurrentAnimation << " of " << numOfAnimations << llendl;
			}
			else
			{
				state->mCurrentAnimation++;
				if(state->mCurrentAnimation>=state->mAnimations.size())
					state->mCurrentAnimation=0;
				llwarns << "cycle " << state->mCurrentAnimation << " of " << numOfAnimations << llendl;
			}
			return state->mAnimations[state->mCurrentAnimation].mAssetUUID;
		}
		else
			llwarns << "animation state has no animations assigned" << llendl;
	}
	return LLUUID();
}

void AOSet::startTimer(F32 timeout)
{
	mEventTimer.stop();
	mPeriod=timeout;
	mEventTimer.start();
	lldebugs << "Starting state timer for " << getName() << " at " << timeout << llendl;
}

void AOSet::stopTimer()
{
	lldebugs << "State timer for " << getName() << " stopped." << llendl;
	mEventTimer.stop();
}

BOOL AOSet::tick()
{
	AOEngine::instance().cycleTimeout(this);
	return FALSE;
}

const LLUUID AOSet::getInventoryUUID() const
{
	return mInventoryID;
}

void AOSet::setInventoryUUID(const LLUUID inventoryID)
{
	mInventoryID=inventoryID;
}

const std::string AOSet::getName() const
{
	return mName;
}

void AOSet::setName(std::string name)
{
	mName=name;
}

BOOL AOSet::getSitOverride() const
{
	return mSitOverride;
}

void AOSet::setSitOverride(BOOL yes)
{
	mSitOverride=yes;
}

BOOL AOSet::getMouselookDisable() const
{
	return mMouselookDisable;
}

void AOSet::setMouselookDisable(BOOL yes)
{
	mMouselookDisable=yes;
}

BOOL AOSet::getComplete() const
{
	return mComplete;
}

void AOSet::setComplete(BOOL yes)
{
	mComplete=yes;
}

BOOL AOSet::getDirty() const
{
	return mDirty;
}

void AOSet::setDirty(BOOL yes)
{
	mDirty=yes;
}

void AOSet::setMotion(const LLUUID motion)
{
	mCurrentMotion=motion;
}

LLUUID AOSet::getMotion() const
{
	return mCurrentMotion;
}
