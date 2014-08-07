/** 
 * @file lggbeammapps.cpp
 * @brief Manager for beam shapes
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This code is free. It comes
 * WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */


#include "llviewerprecompiledheaders.h"
#include "lggbeammaps.h"
#include "lggbeamscolors.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llfile.h"
#include "llframetimer.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "message.h"

lggBeamMaps gLggBeamMaps;
F32 hueToRgb ( F32 val1In, F32 val2In, F32 valHUeIn )
{
	while ( valHUeIn < 0.0f ) valHUeIn += 1.0f;
	while ( valHUeIn > 1.0f ) valHUeIn -= 1.0f;
	if ( ( 6.0f * valHUeIn ) < 1.0f ) return ( val1In + ( val2In - val1In ) * 6.0f * valHUeIn );
	if ( ( 2.0f * valHUeIn ) < 1.0f ) return ( val2In );
	if ( ( 3.0f * valHUeIn ) < 2.0f ) return ( val1In + ( val2In - val1In ) * ( ( 2.0f / 3.0f ) - valHUeIn ) * 6.0f );
	return ( val1In );
}

std::string unescape_name(const std::string& name)
{
	// bugfix for SL-46920: preventing filenames that break stuff.
	char * curl_str = curl_unescape(name.c_str(), name.size());
	std::string unescaped_name(curl_str);
	curl_free(curl_str);
	curl_str = NULL;

	return unescaped_name;
}

void hslToRgb ( F32 hValIn, F32 sValIn, F32 lValIn, F32& rValOut, F32& gValOut, F32& bValOut )
{
	if ( sValIn < 0.00001f )
	{
		rValOut = lValIn;
		gValOut = lValIn;
		bValOut = lValIn;
	}
	else
	{
		F32 interVal1;
		F32 interVal2;

		if ( lValIn < 0.5f )
		{
			interVal2 = lValIn * ( 1.0f + sValIn );
		}
		else
		{
			interVal2 = ( lValIn + sValIn ) - ( sValIn * lValIn );
		}

		interVal1 = 2.0f * lValIn - interVal2;

		rValOut = hueToRgb ( interVal1, interVal2, hValIn + ( 1.f / 3.f ) );
		gValOut = hueToRgb ( interVal1, interVal2, hValIn );
		bValOut = hueToRgb ( interVal1, interVal2, hValIn - ( 1.f / 3.f ) );
	}
}

LLSD lggBeamMaps::getPic(const std::string& filename)
{
	LLSD data;
	llifstream importer(filename);
	LLSDSerialize::fromXMLDocument(data, importer);

	return data;
}

LLColor4U lggBeamMaps::getCurrentColor(const LLColor4U& agentColor)
{
	static LLCachedControl<std::string> settingName(gSavedSettings, "FSBeamColorFile");
	std::string setName(settingName);

	if (setName.empty())
	{
		return agentColor;
	}

	if (setName != mLastColorFileName)
	{
		mLastColorFileName = settingName;
	
		std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beamsColors", ""));
		std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "beamsColors", ""));
		std::string filename = path_name + setName + ".xml";
		if (!gDirUtilp->fileExists(filename))
		{
			filename = path_name2 + setName + ".xml";
			if (!gDirUtilp->fileExists(filename))
			{
				return agentColor;
			}
		}

		mLastColorsData = lggBeamsColors::fromLLSD(getPic(filename));
	}
	
	return beamColorFromData(mLastColorsData);
}

static LLFrameTimer timer;
LLColor4U lggBeamMaps::beamColorFromData(const lggBeamsColors& data)
{
	F32 r, g, b;
	LLColor4 output;
	LLColor4U toReturn;
	F32 timeinc =  timer.getElapsedTimeF32() * 0.3f * ((data.mRotateSpeed + .01f)) * (360 / (data.mEndHue - data.mStartHue));

	S32 diference = llround(data.mEndHue  - data.mStartHue);
	if (diference == 360 || diference == 720)
	{
		//full rainbow
		//liner one
		hslToRgb(fmod(timeinc,1.0f), 1.0f, 0.5f, r, g, b);
	}
	else
	{
		F32 variance = ((data.mEndHue / 360.0f) - (data.mStartHue / 360.0f)) / 2.0f;
		hslToRgb((data.mStartHue / 360.0f) + variance + (sinf(timeinc) * variance), 1.0f, 0.5f, r, g, b);
	}
	output.set(r, g, b);

	toReturn.setVecScaleClamp(output);
	return toReturn;
}

void lggBeamMaps::fireCurrentBeams(LLPointer<LLHUDEffectSpiral> mBeam, const LLColor4U& rgb)
{
	if (mScale == 0.0f)
	{
		return;
	}

	static LLCachedControl<std::string> colorf(gSavedSettings, "FSBeamColorFile");
	bool colorsDisabled = (colorf().empty());
	
	for (std::vector<lggBeamData>::iterator it = mDots.begin(); it != mDots.end(); ++it)
	{
		LLColor4U myColor = rgb;
		if (colorsDisabled)
		{
			myColor = (*it).c;
		}

		F32 distanceAdjust = dist_vec(mBeam->getPositionGlobal(), gAgent.getPositionGlobal());
		F32 pulse = (F32)(.75f + sinf(gFrameTimeSeconds * 1.0f) * 0.25f);
		LLVector3d offset = (*it).p;
		offset.mdV[VY] *= -1.f;
		offset *= pulse * mScale * distanceAdjust * 0.1f;
		
		LLVector3 beamLine = LLVector3( mBeam->getPositionGlobal() - gAgent.getPositionGlobal());
		LLVector3 beamLineFlat = beamLine;
		beamLineFlat.mV[VZ]= 0.0f;

		LLVector3 newDirFlat = LLVector3::x_axis;
		beamLine.normalize();
		beamLineFlat.normalize();
		LLQuaternion change;
		change.shortestArc(newDirFlat, beamLineFlat);
		offset.rotVec(change);
		newDirFlat.rotVec(change);
		change.shortestArc(newDirFlat, beamLine);
		offset.rotVec(change);

		LLPointer<LLHUDEffectSpiral> myBeam = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_BEAM);
		myBeam->setPositionGlobal(mBeam->getPositionGlobal() + offset + (LLVector3d(beamLine) * sinf(gFrameTimeSeconds * 2.0f) * 0.2f));
		
		myBeam->setColor(myColor);
		myBeam->setTargetObject(mBeam->getTargetObject());
		myBeam->setSourceObject(mBeam->getSourceObject());
		myBeam->setNeedsSendToSim(mBeam->getNeedsSendToSim());
		myBeam->setDuration(mDuration * 1.2f);
	}
}

void lggBeamMaps::forceUpdate()
{
	mDots.clear();
	mScale = 0.0f;
	mLastFileName = "";
}

F32 lggBeamMaps::setUpAndGetDuration()
{
	static LLCachedControl<std::string> settingNameCached(gSavedSettings, "FSBeamShape");
	std::string settingName(settingNameCached);

	if (settingName != mLastFileName)
	{
		mLastFileName = settingName;
		if (!settingName.empty())
		{
			std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beams", ""));
			std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "beams", ""));
			std::string filename = path_name + settingName + ".xml";
			if (!gDirUtilp->fileExists(filename))
			{
				filename = path_name2 + settingName + ".xml";
			}

			LLSD mydata = getPic(filename);
			mScale = (F32)mydata["scale"].asReal() / 10.0f;
			LLSD myPicture = mydata["data"];
			mDots.clear();
			for (LLSD::array_iterator it = myPicture.beginArray(); it != myPicture.endArray(); ++it)
			{
				LLSD beamData = *it;
				lggBeamData dot;
				
				dot.p = LLVector3d(beamData["offset"]);
				static LLCachedControl<F32> FSBeamShapeScale(gSavedSettings, "FSBeamShapeScale");
				dot.p *= (FSBeamShapeScale * 2.0f);
				LLColor4 color = LLColor4(beamData["color"]);
				dot.c = LLColor4U(color);
				mDots.push_back(dot);
			}
			
			static LLCachedControl<F32> FSMaxBeamsPerSecond(gSavedSettings, "FSMaxBeamsPerSecond");
			F32 maxBPerQS = FSMaxBeamsPerSecond / 4.0f;
			mDuration = llceil((F32)(myPicture.size()) / maxBPerQS) * 0.25f;
			LL_INFOS("LGG_Beams") << "reading it all now size is " << myPicture.size() << " and duration is " << mDuration << LL_ENDL;
		}
		else
		{
			mDots.clear();
			mScale = 0.0f; //used as a flag too
			mDuration = 0.25f;
		}
	}

	return mDuration;
}

string_vec_t lggBeamMaps::getFileNames()
{
	string_vec_t names;
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beams", ""));
	bool found = true;

	while (found)
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name, "*.xml", name);
		if (found)
		{
			name = name.erase(name.length() - 4);
			names.push_back(unescape_name(name));
		}
	}

	std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beams", ""));
	found = true;
	while (found)
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name2, "*.xml", name);
		if (found)
		{
			name = name.erase(name.length() - 4);
			names.push_back(unescape_name(name));
		}
	}
	return names;
}

string_vec_t lggBeamMaps::getColorsFileNames()
{
	string_vec_t names;
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beamsColors", ""));
	bool found = true;

	while (found)
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name, "*.xml", name);
		if (found)
		{
			name = name.erase(name.length() - 4);
			names.push_back(unescape_name(name));
		}
	}

	std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beamsColors", ""));
	found = true;

	while (found)
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name2, "*.xml", name);
		if (found)
		{
			name = name.erase(name.length() - 4);
			names.push_back(unescape_name(name));
		}
	}
	return names;
}

void lggBeamMaps::stopBeamChat()
{
	static LLCachedControl<bool> FSParticleChat(gSavedSettings, "FSParticleChat");
	if (FSParticleChat)
	{
		if (mPartsNow)
		{
			mPartsNow = false;
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ChatFromViewer);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_ChatData);
			msg->addStringFast(_PREHASH_Message, "stop");
			msg->addU8Fast(_PREHASH_Type, 0);
			msg->addS32("Channel", 9000);

			gAgent.sendReliableMessage();
			mBeamLastAt = LLVector3d::zero;
		}
	}
}

void lggBeamMaps::updateBeamChat(const LLVector3d& currentPos)
{
	static LLCachedControl<bool> FSParticleChat(gSavedSettings, "FSParticleChat");
	if (FSParticleChat)
	{
		if (!mPartsNow)
		{
			mPartsNow = true;
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ChatFromViewer);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_ChatData);
			msg->addStringFast(_PREHASH_Message, "start");
			msg->addU8Fast(_PREHASH_Type, 0);
			msg->addS32("Channel", 9000);

			gAgent.sendReliableMessage();
		}

		if ((mBeamLastAt - currentPos).length() > .2f)
		{
			mBeamLastAt = currentPos;

			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ChatFromViewer);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_ChatData);
			msg->addStringFast(_PREHASH_Message, llformat("<%.6f, %.6f, %.6f>",(F32)(mBeamLastAt.mdV[VX]), (F32)(mBeamLastAt.mdV[VY]), (F32)(mBeamLastAt.mdV[VZ])));
			msg->addU8Fast(_PREHASH_Type, 0);
			msg->addS32("Channel", 9000); // *TODO: make configurable

			gAgent.sendReliableMessage();
		}
	}
}

