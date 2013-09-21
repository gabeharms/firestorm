/** 
 * @file quickprefs.cpp
 * @brief Quick preferences access panel for bottomtray
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2011, WoLf Loonie @ Second Life
 * Copyright (C) 2013, Zi Ree @ Second Life
 * Copyright (C) 2013, Ansariel Hiller @ Second Life
 * Copyright (C) 2013, Cinder Biscuits @ Me too
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

#include "llviewerprecompiledheaders.h"

#include "quickprefs.h"
#include "llboost.h"
#include "llcombobox.h"
#include "lldaycyclemanager.h"
#include "llwlparamset.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llfloatereditsky.h"
#include "llmultisliderctrl.h"
#include "lltimectrl.h"
#include "llenvmanager.h"
#include "llviewercontrol.h"
#include "lldrawpoolbump.h"
#include "llviewertexturelist.h"
#include "llfloaterreg.h"
#include "llfeaturemanager.h"
#include "rlvhandler.h"
#include "llcheckboxctrl.h"
#include "llcubemap.h"

// <FS:Zi> Dynamic quick prefs
#include "llappviewer.h"
#include "llcolorswatch.h"
#include "llf32uictrl.h"
#include "lllayoutstack.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llnotificationsutil.h" // <FS:CR> For restore defaults confirmation

#include <boost/foreach.hpp>
#include <llui.h>

static F32 sun_pos_to_time24(F32 sun_pos)
{
	return fmodf(sun_pos * 24.0f + 6, 24.0f);
}

static F32 time24_to_sun_pos(F32 time)
{
	F32 ret = time - 6.f;
	if (ret < 0)
	{
		ret += 24.f;
	}

	return (ret / 24.f);
}


FloaterQuickPrefs::QuickPrefsXML::QuickPrefsXML()
:	entries("entries")
{}

FloaterQuickPrefs::QuickPrefsXMLEntry::QuickPrefsXMLEntry()
:	control_name("control_name"),
	label("label"),
	control_type("control_type"),
	integer("integer"),
	min_value("min"),		// "min" is frowned upon by a braindead windows include
	max_value("max"),		// "max" see "min"
	increment("increment")
{}
// </FS:Zi>

FloaterQuickPrefs::FloaterQuickPrefs(const LLSD& key)
:	LLTransientDockableFloater(NULL, true, key)
{
	// For Phototools
	mCommitCallbackRegistrar.add("Quickprefs.ShaderChanged", boost::bind(&handleSetShaderChanged, LLSD()));
}

FloaterQuickPrefs::~FloaterQuickPrefs()
{
}

void FloaterQuickPrefs::onOpen(const LLSD& key)
{
	// <FS:Zi> Dynamic Quickprefs

	// bail out here if this is a reused Phototools floater
	if(getIsPhototools())
	{
		return;
	}

	gSavedSettings.setBOOL("QuickPrefsEditMode",FALSE);

	// Scan widgets and reapply control variables because some control types
	// (LLSliderCtrl for example) don't update their GUI when hidden
	control_list_t::iterator it;
	for(it=mControlsList.begin();it!=mControlsList.end();++it)
	{
		const ControlEntry& entry=it->second;

		LLUICtrl* current_widget=entry.widget;
		if(!current_widget)
		{
			llwarns << "missing widget for control " << it->first << llendl;
			continue;
		}

		LLControlVariable* var=current_widget->getControlVariable();
		if(var)
		{
			current_widget->setValue(var->getValue());
		}
	}
	// </FS:Zi>
}


void FloaterQuickPrefs::initCallbacks()
{
	getChild<LLUICtrl>("UseRegionWindlight")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeUseRegionWindlight, this));
	getChild<LLUICtrl>("WaterPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeWaterPreset, this));
	getChild<LLUICtrl>("WLPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeSkyPreset, this));
	getChild<LLUICtrl>("DCPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeDayCyclePreset, this));
	getChild<LLUICtrl>("WLPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickSkyPrev, this));
	getChild<LLUICtrl>("WLNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickSkyNext, this));
	getChild<LLUICtrl>("WWPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickWaterPrev, this));
	getChild<LLUICtrl>("WWNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickWaterNext, this));
	getChild<LLUICtrl>("DCPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickDayCyclePrev, this));
	getChild<LLUICtrl>("DCNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickDayCycleNext, this));
	getChild<LLUICtrl>("ResetToRegionDefault")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetToRegionDefault, this));
	getChild<LLUICtrl>("WLSunPos")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onSunMoved, this));

	// Phototools additions
	if (getIsPhototools())
	{
		gSavedSettings.getControl("VertexShaderEnable")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("WindLightUseAtmosShaders")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("RenderDeferred")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("RenderAvatarVP")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
// <FS:CR> FIRE-9630 - Vignette UI controls
		getChild<LLSpinCtrl>("VignetteSpinnerX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerX, this));
		getChild<LLSlider>("VignetteSliderX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteX, this));
		getChild<LLButton>("Reset_VignetteX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteX, this));
		getChild<LLSpinCtrl>("VignetteSpinnerY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerY, this));
		getChild<LLSlider>("VignetteSliderY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteY, this));
		getChild<LLButton>("Reset_VignetteY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteY, this));
		getChild<LLSpinCtrl>("VignetteSpinnerZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerZ, this));
		getChild<LLSlider>("VignetteSliderZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteZ, this));
		getChild<LLButton>("Reset_VignetteZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteZ, this));
// </FS:CR>
	}
	else
	{
		getChild<LLButton>("Restore_Btn")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickRestoreDefaults, this));
		gSavedSettings.getControl("QuickPrefsEditMode")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::onEditModeChanged, this));	// <FS:Zi> Dynamic Quickprefs
	}

	gRlvHandler.setBehaviourCallback(boost::bind(&FloaterQuickPrefs::updateRlvRestrictions, this, _1, _2));
}

void FloaterQuickPrefs::loadPresets()
{
	// WL Water combo box
	if (mWaterPresetsCombo != NULL)
	{
		std::list<std::string> user_presets, system_presets;
		LLWaterParamManager::instance().getPresetNames(user_presets, system_presets);
		
		mWaterPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT));
		mWaterPresetsCombo->addSeparator();

		// Add user presets first.
		for (std::list<std::string>::const_iterator mIt = user_presets.begin(); mIt != user_presets.end(); ++mIt)
		{
			std::string preset_name = *mIt;
			if (!preset_name.empty())
			{
				mWaterPresetsCombo->add(preset_name, LLSD(preset_name));
			}
		}

		if (user_presets.size() > 0)
		{
			mWaterPresetsCombo->addSeparator();
		}

		// Add system presets.
		for (std::list<std::string>::const_iterator mIt = system_presets.begin(); mIt != system_presets.end(); ++mIt)
		{
			std::string preset_name = *mIt;
			if (!preset_name.empty())
			{
				mWaterPresetsCombo->add(preset_name, LLSD(preset_name));
			}
		}
	}

	// WL Sky combo box
	if (mWLPresetsCombo != NULL)
	{
		LLWLParamManager::preset_name_list_t user_presets, sys_presets, region_presets;
		LLWLParamManager::instance().getPresetNames(region_presets, user_presets, sys_presets);

		mWLPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT));
		mWLPresetsCombo->add(LLTrans::getString("QP_WL_Day_Cycle_Based"), LLSD(PRESET_NAME_SKY_DAY_CYCLE));
		mWLPresetsCombo->addSeparator();

		// Add user presets.
		for (LLWLParamManager::preset_name_list_t::const_iterator it = user_presets.begin(); it != user_presets.end(); ++it)
		{
			std::string preset_name = *it;
			if (!preset_name.empty())
			{
				mWLPresetsCombo->add(preset_name, LLSD(preset_name));
			}
		}

		if (!user_presets.empty())
		{
			mWLPresetsCombo->addSeparator();
		}

		// Add system presets.
		for (LLWLParamManager::preset_name_list_t::const_iterator it = sys_presets.begin(); it != sys_presets.end(); ++it)
		{
			std::string preset_name = *it;
			if (!preset_name.empty())
			{
				mWLPresetsCombo->add(preset_name, LLSD(preset_name));
			}
		}
	}

	// WL Day Cycle combo box
	if (mWLPresetsCombo != NULL)
	{
		LLDayCycleManager::preset_name_list_t user_presets, sys_presets;
		LLDayCycleManager::instance().getPresetNames(user_presets, sys_presets);

		mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT));
		mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_None"), LLSD(PRESET_NAME_NONE));
		mDayCyclePresetsCombo->addSeparator();

		// Add user presets.
		for (LLDayCycleManager::preset_name_list_t::const_iterator it = user_presets.begin(); it != user_presets.end(); ++it)
		{
			std::string preset_name = *it;
			if (!preset_name.empty())
			{
				mDayCyclePresetsCombo->add(preset_name, LLSD(preset_name));
			}
		}

		if (!user_presets.empty())
		{
			mDayCyclePresetsCombo->addSeparator();
		}

		// Add system presets.
		for (LLDayCycleManager::preset_name_list_t::const_iterator it = sys_presets.begin(); it != sys_presets.end(); ++it)
		{
			std::string preset_name = *it;
			if (!preset_name.empty())
			{
				mDayCyclePresetsCombo->add(preset_name, LLSD(preset_name));
			}
		}
	}
}

BOOL FloaterQuickPrefs::postBuild()
{
	// Phototools additions
	if (getIsPhototools())
	{
		mCtrlShaderEnable = getChild<LLCheckBoxCtrl>("BasicShaders");
		mCtrlWindLight = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
		mCtrlDeferred = getChild<LLCheckBoxCtrl>("RenderDeferred");
		mCtrlUseSSAO = getChild<LLCheckBoxCtrl>("UseSSAO");
		mCtrlUseDoF = getChild<LLCheckBoxCtrl>("UseDepthofField");
		mCtrlShadowDetail = getChild<LLComboBox>("ShadowDetail");
		mCtrlReflectionDetail = getChild<LLComboBox>("Reflections");
// <FS:CR> FIRE-9630 - Vignette UI controls
		mSpinnerVignetteX = getChild<LLSpinCtrl>("VignetteSpinnerX");
		mSpinnerVignetteY = getChild<LLSpinCtrl>("VignetteSpinnerY");
		mSpinnerVignetteZ = getChild<LLSpinCtrl>("VignetteSpinnerZ");
		mSliderVignetteX = getChild<LLSlider>("VignetteSliderX");
		mSliderVignetteY = getChild<LLSlider>("VignetteSliderY");
		mSliderVignetteZ = getChild<LLSlider>("VignetteSliderZ");
// </FS:CR>
		refreshSettings();
	}
	else
	{
		mBtnResetDefaults = getChild<LLButton>("Restore_Btn");
	}

	mWaterPresetsCombo = getChild<LLComboBox>("WaterPresetsCombo");
	mWLPresetsCombo = getChild<LLComboBox>("WLPresetsCombo");
	mDayCyclePresetsCombo = getChild<LLComboBox>("DCPresetsCombo");
	mWLSunPos = getChild<LLMultiSliderCtrl>("WLSunPos");
	mUseRegionWindlight = getChild<LLCheckBoxCtrl>("UseRegionWindlight");
	mWLSunPos->addSlider(12.f);

	initCallbacks();
	loadPresets();

	if (gRlvHandler.isEnabled())
	{
		enableWindlightButtons(!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
	}

	// <FS:Zi> Dynamic quick prefs

	// bail out here if this is a reused Phototools floater
	if(getIsPhototools())
	{
		return LLDockableFloater::postBuild();
	}

	// find the layout_stack to insert the controls into
	mOptionsStack=getChild<LLLayoutStack>("options_stack");

	// get the path to the user defined or default quick preferences settings
	loadSavedSettingsFromFile(getSettingsPath(FALSE));
	
	// get edit widget pointers
	mControlLabelEdit=getChild<LLLineEditor>("label_edit");
	mControlNameCombo=getChild<LLComboBox>("control_name_combo");
	mControlTypeCombo=getChild<LLComboBox>("control_type_combo_box");
	mControlIntegerCheckbox=getChild<LLCheckBoxCtrl>("control_integer_checkbox");
	mControlMinSpinner=getChild<LLSpinCtrl>("control_min_edit");
	mControlMaxSpinner=getChild<LLSpinCtrl>("control_max_edit");
	mControlIncrementSpinner=getChild<LLSpinCtrl>("control_increment_edit");

	// wire up callbacks for changed values
	mControlLabelEdit->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged,this));
	mControlNameCombo->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged,this));
	mControlTypeCombo->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged,this));
	mControlIntegerCheckbox->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged,this));
	mControlMinSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged,this));
	mControlMaxSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged,this));
	mControlIncrementSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged,this));

	// wire up ordering and adding buttons
	getChild<LLButton>("move_up_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onMoveUpClicked,this));
	getChild<LLButton>("move_down_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onMoveDownClicked,this));
	getChild<LLButton>("add_new_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAddNewClicked,this));

	// functor to add debug settings to the editor dropdown
	struct f : public LLControlGroup::ApplyFunctor
	{
		LLComboBox* combo;
		f(LLComboBox* c) : combo(c) {}
		virtual void apply(const std::string& name, LLControlVariable* control)
		{
			// do not add things that are hidden in the debug settings floater
			if (!control->isHiddenFromSettingsEditor())
			{
				// don't add floater positions, sizes or visibility values
				if(name.find("floater_")!=0)
				{
					(*combo).addSimpleElement(name);
				}
			}
		}
	} func(mControlNameCombo);

	// add global and per account settings to the dropdown
	gSavedSettings.applyToAll(&func);
	gSavedPerAccountSettings.applyToAll(&func);
	mControlNameCombo->sortByName();
	// </FS:Zi>

	return LLDockableFloater::postBuild();
}

void FloaterQuickPrefs::loadSavedSettingsFromFile(const std::string& settings_path)
{
	QuickPrefsXML xml;
	LLXMLNodePtr root;
	
	if(!LLXMLNode::parseFile(settings_path,root,NULL))
	{
		llwarns << "Unable to load quick preferences from file: " << settings_path << llendl;
	}
	else if(!root->hasName("quickprefs"))
	{
		llwarns << settings_path << " is not a valid quick preferences definition file" << llendl;
	}
	else
	{
		// Parse the quick preferences settings
		LLXUIParser parser;
		parser.readXUI(root,xml,settings_path);
		
		if(!xml.validateBlock())
		{
			llwarns << "Unable to validate quick preferences from file: " << settings_path << llendl;
		}
		else
		{
			// add the elements from the XML file to the internal list of controls
			BOOST_FOREACH(const QuickPrefsXMLEntry& xml_entry,xml.entries)
			{
				// get the label
				std::string label=xml_entry.label;
				// get the same as translated label
				std::string translated_label=xml_entry.label;
				// replace translated label with translated version, if available
				LLTrans::findString(translated_label,"QP "+label);
				
				U32 type=xml_entry.control_type;
				addControl(
						   xml_entry.control_name,
						   translated_label,
						   NULL,
						   (ControlType) type,
						   xml_entry.integer,
						   xml_entry.min_value,
						   xml_entry.max_value,
						   xml_entry.increment
						   );
				// put it at the bottom of the ordering stack
				mControlsOrder.push_back(xml_entry.control_name);
			}
		}
	}
}


bool FloaterQuickPrefs::isValidPresetName(const std::string& preset_name)
{
	return (!preset_name.empty() &&
		preset_name != PRESET_NAME_REGION_DEFAULT &&
		preset_name != PRESET_NAME_SKY_DAY_CYCLE &&
		preset_name != PRESET_NAME_NONE);
}

std::string FloaterQuickPrefs::stepComboBox(LLComboBox* ctrl, bool forward)
{
	S32 increment = (forward ? 1 : -1);
	S32 lastitem = ctrl->getItemCount() - 1;
	S32 curid = ctrl->getCurrentIndex();
	std::string preset_name;
	std::string start_preset_name;

	start_preset_name = ctrl->getSelectedValue().asString();
	do
	{
		curid += increment;
		if (curid < 0)
		{
			curid = lastitem;
		}
		else if (curid > lastitem)
		{
			curid = 0;
		}
		ctrl->setCurrentByIndex(curid);
		preset_name = ctrl->getSelectedValue().asString();
	}
	while (!isValidPresetName(preset_name) && preset_name != start_preset_name);

	return preset_name;
}

void FloaterQuickPrefs::selectSkyPreset(const std::string& preset_name)
{
	if (isValidPresetName(preset_name))
	{	
		deactivateAnimator();
		LLEnvManagerNew::instance().setUseSkyPreset(preset_name, (bool)gSavedSettings.getBOOL("FSInterpolateSky"));
	}
}

void FloaterQuickPrefs::selectWaterPreset(const std::string& preset_name)
{
	if (isValidPresetName(preset_name))
	{
		deactivateAnimator();
		LLEnvManagerNew::instance().setUseWaterPreset(preset_name, (bool)gSavedSettings.getBOOL("FSInterpolateWater"));
	}
}

void FloaterQuickPrefs::selectDayCyclePreset(const std::string& preset_name)
{
	if (isValidPresetName(preset_name))
	{
		deactivateAnimator();
		LLEnvManagerNew::instance().setUseDayCycle(preset_name, (bool)gSavedSettings.getBOOL("FSInterpolateSky"));
	}
}

void FloaterQuickPrefs::onChangeUseRegionWindlight()
{
	LLEnvManagerNew::instance().setUseRegionSettings(mUseRegionWindlight->get(), (gSavedSettings.getBOOL("FSInterpolateSky") || gSavedSettings.getBOOL("FSInterpolateWater")));
}

void FloaterQuickPrefs::onChangeWaterPreset()
{
	std::string preset_name = mWaterPresetsCombo->getSelectedValue().asString();
	if (!isValidPresetName(preset_name))
	{
		preset_name = stepComboBox(mWaterPresetsCombo, true);
	}
	selectWaterPreset(preset_name);
}

void FloaterQuickPrefs::onChangeSkyPreset()
{
	std::string preset_name = mWLPresetsCombo->getSelectedValue().asString();
	if (!isValidPresetName(preset_name))
	{
		preset_name = stepComboBox(mWLPresetsCombo, true);
	}
	selectSkyPreset(preset_name);
}

void FloaterQuickPrefs::onChangeDayCyclePreset()
{
	std::string preset_name = mDayCyclePresetsCombo->getSelectedValue().asString();
	if (!isValidPresetName(preset_name))
	{
		preset_name = stepComboBox(mDayCyclePresetsCombo, true);
	}
	selectDayCyclePreset(preset_name);
}

void FloaterQuickPrefs::deactivateAnimator()
{
	LLWLParamManager::instance().mAnimator.deactivate();
}

void FloaterQuickPrefs::onClickWaterPrev()
{
	std::string preset_name = stepComboBox(mWaterPresetsCombo, false);
	selectWaterPreset(preset_name);
}

void FloaterQuickPrefs::onClickWaterNext()
{
	std::string preset_name = stepComboBox(mWaterPresetsCombo, true);
	selectWaterPreset(preset_name);
}

void FloaterQuickPrefs::onClickSkyPrev()
{
	std::string preset_name = stepComboBox(mWLPresetsCombo, false);
	selectSkyPreset(preset_name);
}

void FloaterQuickPrefs::onClickSkyNext()
{
	std::string preset_name = stepComboBox(mWLPresetsCombo, true);
	selectSkyPreset(preset_name);
}

void FloaterQuickPrefs::onClickDayCyclePrev()
{
	std::string preset_name = stepComboBox(mDayCyclePresetsCombo, false);
	selectDayCyclePreset(preset_name);
}

void FloaterQuickPrefs::onClickDayCycleNext()
{
	std::string preset_name = stepComboBox(mDayCyclePresetsCombo, true);
	selectDayCyclePreset(preset_name);
}


void FloaterQuickPrefs::draw()
{
	F32 val;

	if (LLEnvManagerNew::instance().getUseRegionSettings() ||
		LLEnvManagerNew::instance().getUseDayCycle())
	{
		val = (F32)(LLWLParamManager::instance().mAnimator.getDayTime() * 24.f);
	}
	else
	{
		val = sun_pos_to_time24(LLWLParamManager::instance().mCurParams.getSunAngle() / F_TWO_PI);
	}

	mWLSunPos->setCurSliderValue(val);

	LLFloater::draw();
}

void FloaterQuickPrefs::onSunMoved()
{
	if (LLEnvManagerNew::instance().getUseRegionSettings() ||
		LLEnvManagerNew::instance().getUseDayCycle())
	{
		F32 val = mWLSunPos->getCurSliderValue() / 24.0f;

		LLWLParamManager& mgr = LLWLParamManager::instance();
		mgr.mAnimator.setDayTime((F64)val);
		mgr.mAnimator.deactivate();
		mgr.mAnimator.update(mgr.mCurParams);
	}
	else
	{
		deactivateAnimator();
		F32 val = time24_to_sun_pos(mWLSunPos->getCurSliderValue()) * F_TWO_PI;
		LLWLParamManager::instance().mCurParams.setSunAngle(val);
	}
}

void FloaterQuickPrefs::onClickResetToRegionDefault()
{
	deactivateAnimator();
	LLWLParamManager::instance().mAnimator.stopInterpolation();
	LLEnvManagerNew::instance().useRegionSettings();
}

// This method is invoked by LLEnvManagerNew when a particular preset is applied
// static
void FloaterQuickPrefs::updateParam(EQuickPrefUpdateParam param, const LLSD& value)
{
	FloaterQuickPrefs* qp_floater = LLFloaterReg::getTypedInstance<FloaterQuickPrefs>("quickprefs");
	FloaterQuickPrefs* pt_floater = LLFloaterReg::getTypedInstance<FloaterQuickPrefs>(PHOTOTOOLS_FLOATER);

	switch (param)
	{
		case QP_PARAM_SKY:
			qp_floater->setSelectedSky(value.asString());
			pt_floater->setSelectedSky(value.asString());
			break;
		case QP_PARAM_WATER:
			qp_floater->setSelectedWater(value.asString());
			pt_floater->setSelectedWater(value.asString());
			break;
		case QP_PARAM_DAYCYCLE:
			qp_floater->setSelectedDayCycle(value.asString());
			pt_floater->setSelectedDayCycle(value.asString());
			break;
		default:
			break;
	}
}


void FloaterQuickPrefs::setSelectedSky(const std::string& preset_name)
{
	mWLPresetsCombo->setValue(LLSD(preset_name));
	mDayCyclePresetsCombo->setValue(LLSD(PRESET_NAME_NONE));
}

void FloaterQuickPrefs::setSelectedWater(const std::string& preset_name)
{
	mWaterPresetsCombo->setValue(LLSD(preset_name));
}

void FloaterQuickPrefs::setSelectedDayCycle(const std::string& preset_name)
{
	mDayCyclePresetsCombo->setValue(LLSD(preset_name));
	mWLPresetsCombo->setValue(LLSD(PRESET_NAME_SKY_DAY_CYCLE));
}

// Phototools additions
void FloaterQuickPrefs::refreshSettings()
{
	BOOL reflections = gSavedSettings.getBOOL("VertexShaderEnable") 
		&& gGLManager.mHasCubeMap
		&& LLCubeMap::sUseCubeMaps;
	mCtrlReflectionDetail->setEnabled(reflections);

	bool fCtrlShaderEnable = LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable");
	mCtrlShaderEnable->setEnabled(
		fCtrlShaderEnable && ((!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)) || (!gSavedSettings.getBOOL("VertexShaderEnable"))) );
	BOOL shaders = mCtrlShaderEnable->get();

	bool fCtrlWindLightEnable = fCtrlShaderEnable && shaders;
	mCtrlWindLight->setEnabled(
		fCtrlWindLightEnable && ((!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)) || (!gSavedSettings.getBOOL("WindLightUseAtmosShaders"))) );

	BOOL enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") && 
						shaders && 
						gGLManager.mHasFramebufferObject &&
						gSavedSettings.getBOOL("RenderAvatarVP") &&
						(mCtrlWindLight->get()) ? TRUE : FALSE;

	mCtrlDeferred->setEnabled(enabled);

	enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO") && (mCtrlDeferred->get() ? TRUE : FALSE);
		
	mCtrlUseSSAO->setEnabled(enabled);
	mCtrlUseDoF->setEnabled(enabled);

	enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail");

	mCtrlShadowDetail->setEnabled(enabled);


	// if vertex shaders off, disable all shader related products
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"))
	{
		mCtrlShaderEnable->setEnabled(FALSE);
		mCtrlShaderEnable->setValue(FALSE);
		
		mCtrlWindLight->setEnabled(FALSE);
		mCtrlWindLight->setValue(FALSE);
		
		mCtrlReflectionDetail->setEnabled(FALSE);
		mCtrlReflectionDetail->setValue(0);

		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);
		
		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);

		mCtrlUseDoF->setEnabled(FALSE);
		mCtrlUseDoF->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
	}
	
	// disabled windlight
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders"))
	{
		mCtrlWindLight->setEnabled(FALSE);
		mCtrlWindLight->setValue(FALSE);

		//deferred needs windlight, disable deferred
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);
		
		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);

		mCtrlUseDoF->setEnabled(FALSE);
		mCtrlUseDoF->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
	}

	// disabled deferred
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") ||
		!gGLManager.mHasFramebufferObject)
	{
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);
		
		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);

		mCtrlUseDoF->setEnabled(FALSE);
		mCtrlUseDoF->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
	}
	
	// disabled deferred SSAO
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO"))
	{
		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);
	}
	
	// disabled deferred shadows
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail"))
	{
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);
	}

	// disabled reflections
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderReflectionDetail"))
	{
		mCtrlReflectionDetail->setEnabled(FALSE);
		mCtrlReflectionDetail->setValue(FALSE);
	}

	// disabled av
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP"))
	{
		//deferred needs AvatarVP, disable deferred
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);
		
		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);

		mCtrlUseDoF->setEnabled(FALSE);
		mCtrlUseDoF->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
	}
	
	// <FS:CR> FIRE-9630 - Vignette UI controls
	if (getIsPhototools())
	{
		LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
		mSpinnerVignetteX->setValue(vignette.mV[VX]);
		mSpinnerVignetteY->setValue(vignette.mV[VY]);
		mSpinnerVignetteZ->setValue(vignette.mV[VZ]);
		mSliderVignetteX->setValue(vignette.mV[VX]);
		mSliderVignetteY->setValue(vignette.mV[VY]);
		mSliderVignetteZ->setValue(vignette.mV[VZ]);
	}
	// </FS:CR>
}

void FloaterQuickPrefs::updateRlvRestrictions(ERlvBehaviour behavior, ERlvParamType type)
{
	if (behavior == RLV_BHVR_SETENV)
	{
		if (type == RLV_TYPE_ADD)
		{
			enableWindlightButtons(FALSE);
		}
		else
		{
			enableWindlightButtons(TRUE);
		}
	}
}

void FloaterQuickPrefs::enableWindlightButtons(BOOL enable)
{
	childSetEnabled("WLPresetsCombo", enable);
	childSetEnabled("WLPrevPreset", enable);
	childSetEnabled("WLNextPreset", enable);
	childSetEnabled("WaterPresetsCombo", enable);
	childSetEnabled("WWPrevPreset", enable);
	childSetEnabled("WWNextPreset", enable);
	childSetEnabled("ResetToRegionDefault", enable);
	childSetEnabled("UseRegionWindlight", enable);
	childSetEnabled("DCPresetsCombo", enable);
	childSetEnabled("DCPrevPreset", enable);
	childSetEnabled("DCNextPreset", enable);

	if (getIsPhototools())
	{
		childSetEnabled("Sunrise", enable);
		childSetEnabled("Noon", enable);
		childSetEnabled("Sunset", enable);
		childSetEnabled("Midnight", enable);
		childSetEnabled("Revert to Region Default", enable);
		childSetEnabled("new_sky_preset", enable);
		childSetEnabled("edit_sky_preset", enable);
		childSetEnabled("new_water_preset", enable);
		childSetEnabled("edit_water_preset", enable);
	}
}

// <FS:Zi> Dynamic quick prefs
std::string FloaterQuickPrefs::getSettingsPath(BOOL save_mode)
{
	// get the settings file name
	std::string settings_file=LLAppViewer::instance()->getSettingsFilename("Default","QuickPreferences");
	// expand to user defined path
	std::string settings_path=gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,settings_file);

	// if not in save mode, and the file was not found, use the default path
	if(!save_mode && !LLFile::isfile(settings_path))
	{
		settings_path=gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,settings_file);
	}
	return settings_path;
}

void FloaterQuickPrefs::updateControl(const std::string& controlName,ControlEntry& entry)
{
	// rename the panel to contain the control's name, for identification later
	entry.panel->setName(controlName);

	// build a list of all possible control widget types
	std::map<ControlType,std::string> typeMap;
	std::map<ControlType,std::string>::iterator it;

	typeMap[ControlTypeCheckbox]="option_checkbox_control";
	typeMap[ControlTypeText]="option_text_control";
	typeMap[ControlTypeSpinner]="option_spinner_control";
	typeMap[ControlTypeSlider]="option_slider_control";
	typeMap[ControlTypeRadio]="option_radio_control";
	typeMap[ControlTypeColor3]="option_color3_control";
	typeMap[ControlTypeColor4]="option_color4_control";

	// hide all widget types except for the one the user wants
	LLUICtrl* widget;
	for(it=typeMap.begin();it!=typeMap.end();++it)
	{
		if(entry.type!=it->first)
		{
			widget=entry.panel->getChild<LLUICtrl>(it->second);

			if (widget)
			{
				// dummy to disable old control
				widget->setControlName("QuickPrefsEditMode");
				widget->setVisible(FALSE);
				widget->setEnabled(FALSE);
			}
		}
	}

	// get the widget type the user wanted from the panel
	widget=entry.panel->getChild<LLUICtrl>(typeMap[entry.type]);

	// use 3 decimal places by default
	S32 decimals=3;

	// save pointer to the widget in our internal list
	entry.widget=widget;

	// add the settings control to the widget and enable/show it
	widget->setControlName(controlName);
	widget->setVisible(TRUE);
	widget->setEnabled(TRUE);

	// if no increment is given, try to guess a good number
	if(entry.increment==0.0f)
	{
		// finer grained for sliders
		if(entry.type==ControlTypeSlider)
		{
			entry.increment=(entry.max_value-entry.min_value)/100.0f;
		}
		// a little less for spinners
		else if(entry.type==ControlTypeSpinner)
		{
			entry.increment=(entry.max_value-entry.min_value)/20.0f;
		}
	}

	// if it's an integer entry, round the numbers
	if(entry.integer)
	{
		entry.min_value=llround(entry.min_value);
		entry.max_value=llround(entry.max_value);

		// recalculate increment
		entry.increment=llround(entry.increment);
		if(entry.increment==0)
		{
			entry.increment=1;
		}

		// no decimal places for integers
		decimals=0;
	}

	// set up values for special case control widget types
	LLUICtrl* alpha_widget=entry.panel->getChild<LLUICtrl>("option_color_alpha_control");
	alpha_widget->setVisible(FALSE);

	// sadly, using LLF32UICtrl does not work properly, so we have to use a branch
	// for each floating point type
	if(entry.type==ControlTypeSpinner)
	{
		LLSpinCtrl* spinner=(LLSpinCtrl*) widget;
		spinner->setPrecision(decimals);
		spinner->setMinValue(entry.min_value);
		spinner->setMaxValue(entry.max_value);
		spinner->setIncrement(entry.increment);
	}
	else if(entry.type==ControlTypeSlider)
	{
		LLSliderCtrl* slider=(LLSliderCtrl*) widget;
		slider->setPrecision(decimals);
		slider->setMinValue(entry.min_value);
		slider->setMaxValue(entry.max_value);
		slider->setIncrement(entry.increment);
	}
	else if(entry.type==ControlTypeColor4)
	{
		LLColorSwatchCtrl* color_widget=(LLColorSwatchCtrl*) widget;
		alpha_widget->setVisible(TRUE);
		alpha_widget->setValue(color_widget->get().mV[VALPHA]);
	}

	// reuse a previously created text label if possible
	LLTextBox* label_textbox=entry.label_textbox;
	// if the text label is not known yet, this is a brand new control panel
	if(!label_textbox)
	{
		// otherwise, get the pointer to the new label
		label_textbox=entry.panel->getChild<LLTextBox>("option_label");

		// add double click and single click callbacks on the text label
		label_textbox->setDoubleClickCallback(boost::bind(&FloaterQuickPrefs::onDoubleClickLabel,this,_1,entry.panel));
		label_textbox->setMouseUpCallback(boost::bind(&FloaterQuickPrefs::onClickLabel,this,_1,entry.panel));

		// since this is a new control, wire up the remove button signal, too
		LLButton* remove_button=entry.panel->getChild<LLButton>("remove_button");
		remove_button->setCommitCallback(boost::bind(&FloaterQuickPrefs::onRemoveClicked,this,_1,entry.panel));

		// and the commit signal for the alpha value in a color4 control
		alpha_widget->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAlphaChanged,this,_1,widget));

		// save the text label pointer in the internal list
		entry.label_textbox=label_textbox;
	}
	// set the value(visible text) for the text label
	label_textbox->setValue(entry.label+":");

	// get the named control variable from global or per account settings
	LLControlVariable* var=gSavedSettings.getControl(controlName);
	if(!var)
	{
		var=gSavedPerAccountSettings.getControl(controlName);
	}

	// if we found the control, set up the chosen widget to use it
	if(var)
	{
		widget->setValue(var->getValue());
		widget->setToolTip(var->getComment());
		label_textbox->setToolTip(var->getComment());
	}
	else
	{
		llwarns << "Could not find control variable " << controlName << llendl;
	}
}

LLUICtrl* FloaterQuickPrefs::addControl(const std::string& controlName,const std::string& controlLabel,LLView* slot,ControlType type,BOOL integer,F32 min_value,F32 max_value,F32 increment)
{
	// create a new controls panel
	LLLayoutPanel* panel=LLUICtrlFactory::createFromFile<LLLayoutPanel>("panel_quickprefs_item.xml",NULL,LLLayoutStack::child_registry_t::instance());
	if(!panel)
	{
		llwarns << "could not add panel" << llendl;
		return NULL;
	}

	// sanity checks
	if(max_value<min_value)	max_value=min_value;
	// 0.0 will make updateControl calculate the increment itself
	if(increment<0.0f)		increment=0.0f;

	// create a new internal entry for this control
	ControlEntry newControl;
	newControl.panel=panel->getChild<LLPanel>("option_ordering_panel");
	newControl.widget=NULL;
	newControl.label_textbox=NULL;
	newControl.label=controlLabel;
	newControl.type=type;
	newControl.integer=integer;
	newControl.min_value=min_value;
	newControl.max_value=max_value;
	newControl.increment=increment;

	// update the new control
	updateControl(controlName,newControl);

	// add the control to the internal list
	mControlsList[controlName]=newControl;

	// if we have a slot already, reparent our new ordering panel and delete the old layout_panel
	if(slot)
	{
		// add the ordering panel to the slot
		slot->addChild(newControl.panel);
		// make sure the panel moves to the top left corner
		newControl.panel->setOrigin(0,0);
		// resize it to make it fill the slot
		newControl.panel->reshape(slot->getRect().getWidth(),slot->getRect().getHeight());
		// remove the old layout panel from memory
		delete panel;
	}
	// otherwise create a new slot
	else
	{
		// add a new layout_panel to the stack
		mOptionsStack->addPanel(panel,LLLayoutStack::NO_ANIMATE);
		// add the panel to the list of ordering slots
		mOrderingSlots.push_back(panel);
		// make the floater fit the newly added control panel
		reshape(getRect().getWidth(),getRect().getHeight()+panel->getRect().getHeight());
		// show the panel
		panel->setVisible(TRUE);
	}

	// hide the border
	newControl.panel->setBorderVisible(FALSE);

	return newControl.widget;
}

void FloaterQuickPrefs::removeControl(const std::string& controlName,BOOL remove_slot)
{
	// find the control panel to remove
	const control_list_t::iterator it=mControlsList.find(controlName);
	if(it==mControlsList.end())
	{
		llwarns << "Couldn't find control entry " << controlName << llendl;
		return;
	}

	// get a pointer to the panel to remove
	LLPanel* panel=it->second.panel;
	// remember the panel's height because it will be deleted by removeChild() later
	S32 height=panel->getRect().getHeight();

	// remove the panel from the internal list
	mControlsList.erase(it);

	// get a pointer to the layout slot used
	LLLayoutPanel* slot=(LLLayoutPanel*) panel->getParent();
	// remove the panel from the slot
	slot->removeChild(panel);
	// clear the panel from memory
	delete panel;

	// remove the layout_panel if desired
	if(remove_slot)
	{
		// remove the slot from our list
		mOrderingSlots.remove(slot);
		// and delete it from the user interface stack
		mOptionsStack->removeChild(slot);

		// make the floater shrink to its new size
		reshape(getRect().getWidth(),getRect().getHeight()-height);
	}
}

void FloaterQuickPrefs::selectControl(std::string controlName)
{
	// remove previously selected marker, if any
	if(!mSelectedControl.empty() && hasControl( mSelectedControl ) )
	{
		mControlsList[mSelectedControl].panel->setBorderVisible(FALSE);
	}

	// save the currently selected name in a volatile settings control to
	// enable/disable the editor widgets
	mSelectedControl=controlName;
	gSavedSettings.setString("QuickPrefsSelectedControl",controlName);

	if( mSelectedControl.size() && !hasControl( mSelectedControl ) )
	{
		mSelectedControl = "";
		return;
	}

	// if we are not in edit mode, we can stop here
	if(!gSavedSettings.getBOOL("QuickPrefsEditMode"))
	{
		return;
	}

	// select the topmost entry in the name dropdown, in case we don't find the name
	mControlNameCombo->selectNthItem(0);

	// assume we don't need the min/max/increment/integer widgets by default
	BOOL enable_floating_point=FALSE;

	// if actually a selection is present, set up the editor widgets
	if(!mSelectedControl.empty())
	{
		// draw the new selection border
		mControlsList[mSelectedControl].panel->setBorderVisible(TRUE);

		// set up editor values
		mControlLabelEdit->setValue(LLSD(mControlsList[mSelectedControl].label));
		mControlNameCombo->setValue(LLSD(mSelectedControl));
		mControlTypeCombo->setValue(mControlsList[mSelectedControl].type);
		mControlIntegerCheckbox->setValue(LLSD(mControlsList[mSelectedControl].integer));
		mControlMinSpinner->setValue(LLSD(mControlsList[mSelectedControl].min_value));
		mControlMaxSpinner->setValue(LLSD(mControlsList[mSelectedControl].max_value));
		mControlIncrementSpinner->setValue(LLSD(mControlsList[mSelectedControl].increment));

		// special handling to enable min/max/integer/increment widgets
		switch(mControlsList[mSelectedControl].type)
		{
			// enable floating point widgets for these types
			case ControlTypeSpinner:	// fall through
			case ControlTypeSlider:		// fall through
			{
				enable_floating_point=TRUE;

				// assume we have floating point widgets
				mControlIncrementSpinner->setIncrement(0.1f);
				// use 3 decimal places by default
				S32 decimals=3;
				// unless we have an integer control
				if(mControlsList[mSelectedControl].integer)
				{
					decimals=0;
					mControlIncrementSpinner->setIncrement(1.0);
				}
				// set up floating point widgets
				mControlMinSpinner->setPrecision(decimals);
				mControlMaxSpinner->setPrecision(decimals);
				mControlIncrementSpinner->setPrecision(decimals);
				break;
			}
			// the rest will not need them
			default:
			{
			}
		}
	}

	// enable/disable floating point widgets
	mControlMinSpinner->setEnabled(enable_floating_point);
	mControlMaxSpinner->setEnabled(enable_floating_point);
	mControlIntegerCheckbox->setEnabled(enable_floating_point);
	mControlIncrementSpinner->setEnabled(enable_floating_point);
}

void FloaterQuickPrefs::onClickLabel(LLUICtrl* ctrl,void* userdata)
{
	// don't do anything when we are not in edit mode
	if(!gSavedSettings.getBOOL("QuickPrefsEditMode"))
	{
		return;
	}
	// get the associated panel from the submitted userdata
	LLUICtrl* panel=(LLUICtrl*) userdata;
	// select the clicked control, identified by its name
	selectControl(panel->getName());
}

void FloaterQuickPrefs::onDoubleClickLabel(LLUICtrl* ctrl,void* userdata)
{
	// toggle edit mode
	BOOL edit_mode=!gSavedSettings.getBOOL("QuickPrefsEditMode");
	gSavedSettings.setBOOL("QuickPrefsEditMode",edit_mode);

	// select the double clicked control if we toggled edit on
	if(edit_mode)
	{
		// get the associated widget from the submitted userdata
		LLUICtrl* panel=(LLUICtrl*) userdata;
		selectControl(panel->getName());
	}
}

void FloaterQuickPrefs::onEditModeChanged()
{
	// if edit mode was enabled, stop here
	if(gSavedSettings.getBOOL("QuickPrefsEditMode"))
	{
		return;
	}

	// deselect the current control
	selectControl("");

	QuickPrefsXML xml;
	std::string settings_path=getSettingsPath(TRUE);

	// loop through the list of controls, in the displayed order
	std::list<std::string>::iterator it;
	for(it=mControlsOrder.begin();it!=mControlsOrder.end();++it)
	{
		const ControlEntry& entry=mControlsList[*it];
		QuickPrefsXMLEntry xml_entry;

		// add control values to the XML entry
		xml_entry.control_name=*it;
		xml_entry.label=entry.label;
		xml_entry.control_type=(U32) entry.type;
		xml_entry.integer=entry.integer;
		xml_entry.min_value=entry.min_value;
		xml_entry.max_value=entry.max_value;
		xml_entry.increment=entry.increment;

		// add the XML entry to the overall XML container
		xml.entries.add(xml_entry);
	}

	// Serialize the parameter tree
	LLXMLNodePtr output_node=new LLXMLNode("quickprefs",false);
	LLXUIParser parser;
	parser.writeXUI(output_node,xml);

	// Write the resulting XML to file
	if(!output_node->isNull())
	{
		LLFILE *fp=LLFile::fopen(settings_path,"w");
		if(fp!=NULL)
		{
			LLXMLNode::writeHeaderToFile(fp);
			output_node->writeToFile(fp);
			fclose(fp);
		}
	}
}

void FloaterQuickPrefs::onValuesChanged()
{
	// safety, do nothing if we are not in edit mode
	if(!gSavedSettings.getBOOL("QuickPrefsEditMode"))
	{
		return;
	}

	// remember the current and possibly new control names
	std::string old_control_name=mSelectedControl;
	std::string new_control_name=mControlNameCombo->getValue();

	// if we changed the control's variable, rebuild the user interface
	if(!new_control_name.empty() && old_control_name!=new_control_name)
	{
		// remember the old control parameters so we can restore them later
		ControlEntry old_parameters=mControlsList[mSelectedControl];
		// disable selection so the border doesn't cause a crash
		selectControl("");
		// rename the old ordering entry
		std::list<std::string>::iterator it;
		for(it=mControlsOrder.begin();it!=mControlsOrder.end();++it)
		{
			if(*it==old_control_name)
			{
				*it=new_control_name;
				break;
			}
		}

		// remember the old slot
		LLView* slot=old_parameters.panel->getParent();
		// remove the old control name from the internal list but keep the slot available
		removeControl(old_control_name,FALSE);
		// add new control with the old slot
		addControl(new_control_name,new_control_name,slot);
		// grab the new values and make the selection border go to the right panel
		selectControl(new_control_name);
		// restore the old UI settings
		mControlsList[mSelectedControl].label=old_parameters.label;
		// find the control variable in global or per account settings
		LLControlVariable* var=gSavedSettings.getControl(mSelectedControl);
		if(!var)
		{
			var=gSavedPerAccountSettings.getControl(mSelectedControl);
		}

		if(var && hasControl(mSelectedControl) )
		{
			// choose sane defaults for floating point controls, so the control value won't be destroyed
			// start with these
			F32 min_value=0.0;
			F32 max_value=1.0;
			F32 value=var->getValue().asReal();

			// if the value was negative and smaller than the current minimum
			if(value<0.0f)
			{
				// make the minimum even smaller
				min_value=value*2.0f;
			}
			// if the value is above zero, set max to double of the current value
			else if(value>0.0f)
			{
				max_value=value*2.0f;
			}

			// do a best guess on variable types and control widgets
			ControlType type;
			BOOL integer;
			switch(var->type())
			{
				// Boolean gets the full set
				case TYPE_BOOLEAN:
				{
					// increment will be calculated below
					min_value=0.0;
					max_value=1.0;
					integer=TRUE;
					type=ControlTypeRadio;
					break;
				}
				// LLColor3/4 are just colors
				case TYPE_COL3:
				{
					type=ControlTypeColor3;
					break;
				}
				case TYPE_COL4:
				{
					type=ControlTypeColor4;
					break;
				}
				// U32 can never be negative
				case TYPE_U32:
				{
					min_value=0.0;
				}
				// Fallthrough, S32 and U32 are integer values
				case TYPE_S32:
				{
					integer=TRUE;
				}
				// Fallthrough, S32, U32 and F32 should use sliders
				case TYPE_F32:
				{
					type=ControlTypeSlider;
					break;
				}
				// Everything else gets a text widget for now
				default:
				{
					type=ControlTypeText;
					integer=FALSE;
				}
			}

			// choose a sane increment
			F32 increment=0.1f;
			if( mControlsList[mSelectedControl].type==ControlTypeSlider )
			{
				// fine grained control for sliders
				increment=(max_value-min_value)/100.0f;
			}
			else if( mControlsList[mSelectedControl].type==ControlTypeSpinner)
			{
				// not as fine grained for spinners
				increment=(max_value-min_value)/20.0f;
			}

			// don't let values go too small
			if(increment<0.1)
			{
				increment=0.1f;
			}

			// save calculated values to the edit widgets
			mControlsList[mSelectedControl].min_value=min_value;
			mControlsList[mSelectedControl].max_value=max_value;
			mControlsList[mSelectedControl].increment=increment;
			mControlsList[mSelectedControl].type=type; // old_parameters.type;
			mControlsList[mSelectedControl].widget->setValue(var->getValue());
		}
		// rebuild controls UI (probably not needed)
		// updateControls();
		// update our new control
		updateControl(mSelectedControl,mControlsList[mSelectedControl]);
	}
	// the control's setting variable is still the same, so just update the values
	else if( hasControl(mSelectedControl) )
	{
		mControlsList[mSelectedControl].label=mControlLabelEdit->getValue().asString();
		mControlsList[mSelectedControl].type=(ControlType) mControlTypeCombo->getValue().asInteger();
		mControlsList[mSelectedControl].integer=mControlIntegerCheckbox->getValue().asBoolean();
		mControlsList[mSelectedControl].min_value=mControlMinSpinner->getValue().asReal();
		mControlsList[mSelectedControl].max_value=mControlMaxSpinner->getValue().asReal();
		mControlsList[mSelectedControl].increment=mControlIncrementSpinner->getValue().asReal();
		// and update the user interface
		updateControl(mSelectedControl,mControlsList[mSelectedControl]);
	}
	// select the control
	selectControl(mSelectedControl);

	// <FS:ND>
	// setting focus can lead to an endless loop of two floaters fighting for focus. See FIRE-9634
	// so we rather deal with focus loss sometimes (how often?) than a nasty hang

	// // sometimes we seem to lose focus, so make sure we keep it
	// setFocus(TRUE);

	// </FS:ND>
}

void FloaterQuickPrefs::onAddNewClicked()
{
	// count a number to keep control names unique
	static S32 sCount=0;
	std::string new_control_name="NewControl"+LLSD(sCount).asString();
	// add the new control to the internal list and user interface
	addControl(new_control_name,new_control_name);
	// put it at the bottom of the ordering stack
	mControlsOrder.push_back(new_control_name);
	sCount++;
	// select the newly created control
	selectControl(new_control_name);
}

void FloaterQuickPrefs::onRemoveClicked(LLUICtrl* ctrl,void* userdata)
{
	// get the associated panel from the submitted userdata
	LLUICtrl* panel=(LLUICtrl*) userdata;
	// deselect the current entry
	selectControl("");
	// first remove the control from the ordering list
	mControlsOrder.remove(panel->getName());
	// then remove it from the internal list and from memory
	removeControl(panel->getName());
	// reinstate focus in case we lost it
	setFocus(TRUE);
}

void FloaterQuickPrefs::onAlphaChanged(LLUICtrl* ctrl,void* userdata)
{
	// get the associated color swatch from the submitted userdata
	LLColorSwatchCtrl* color_swatch=(LLColorSwatchCtrl*) userdata;
	// get the current color
	LLColor4 color=color_swatch->get();
	// replace the alpha value of the color with the value in the alpha spinner
	color.setAlpha(ctrl->getValue().asReal());
	// save the color back into the color swatch
	color_swatch->getControlVariable()->setValue(color.getValue());
}

void FloaterQuickPrefs::swapControls(const std::string& control1, const std::string& control2)
{
	// get the control entries of both controls
	ControlEntry temp_entry_1=mControlsList[control1];
	ControlEntry temp_entry_2=mControlsList[control2];

	// find the respective ordering slots
	LLView* temp_slot_1=temp_entry_1.panel->getParent();
	LLView* temp_slot_2=temp_entry_2.panel->getParent();

	// swap the controls around
	temp_slot_1->addChild(temp_entry_2.panel);
	temp_slot_2->addChild(temp_entry_1.panel);
}

void FloaterQuickPrefs::onMoveUpClicked()
{
	// find the control in the ordering list
	std::list<std::string>::iterator it;
	for(it=mControlsOrder.begin();it!=mControlsOrder.end();++it)
	{
		if(*it==mSelectedControl)
		{
			// if it's already on top of the list, do nothing
			if(it==mControlsOrder.begin())
				return;

			// get the iterator of the previous item
			std::list<std::string>::iterator previous=it;
			--previous;

			// copy the previous item to the one we want to move
			*it=*previous;
			// copy the moving item to previous
			*previous=mSelectedControl;
			// update the user interface
			swapControls(mSelectedControl,*it);
			return;
		}
	}
	return;
}

void FloaterQuickPrefs::onMoveDownClicked()
{
	// find the control in the ordering list
	std::list<std::string>::iterator it;
	for(it=mControlsOrder.begin();it!=mControlsOrder.end();++it)
	{
		if(*it==mSelectedControl)
		{
			// if it's already at the end of the list, do nothing
			if(*it==mControlsOrder.back())
				return;

			// get the iterator of the next item
			std::list<std::string>::iterator next=it;
			++next;

			// copy the next item to the one we want to move
			*it=*next;
			// copy the moving item to next
			*next=mSelectedControl;
			// update the user interface
			swapControls(mSelectedControl,*it);
			return;
		}
	}
	return;
}

void FloaterQuickPrefs::onClose(bool app_quitting)
{
	// bail out here if this is a reused Phototools floater
	if(getIsPhototools())
	{
		return;
	}

	// close edit mode and save settings
	gSavedSettings.setBOOL("QuickPrefsEditMode",FALSE);
}
// </FS:Zi>

// <FS:CR> FIRE-9630 - Vignette UI callbacks
void FloaterQuickPrefs::onChangeVignetteX()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VX] = mSliderVignetteX->getValueF32();
	mSpinnerVignetteX->setValue(vignette.mV[VX]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteY()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VY] = mSliderVignetteY->getValueF32();
	mSpinnerVignetteY->setValue(vignette.mV[VY]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteZ()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VZ] = mSliderVignetteZ->getValueF32();
	mSpinnerVignetteZ->setValue(vignette.mV[VZ]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerX()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VX] = mSpinnerVignetteX->getValueF32();
	mSliderVignetteX->setValue(vignette.mV[VX]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerY()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VY] = mSpinnerVignetteY->getValueF32();
	mSliderVignetteY->setValue(vignette.mV[VY]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerZ()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VZ] = mSpinnerVignetteZ->getValueF32();
	mSliderVignetteZ->setValue(vignette.mV[VZ]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteX()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	// FIXME: Don't use a hardcoded default value for resetting to default.
	vignette.mV[VX] = 0.0;
	mSliderVignetteX->setValue(0);
	mSpinnerVignetteX->setValue(0);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteY()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	// FIXME: Don't use a hardcoded default value for resetting to default.
	vignette.mV[VY] = 1.0;
	mSliderVignetteY->setValue(1);
	mSpinnerVignetteY->setValue(1);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteZ()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	// FIXME: Don't use a hardcoded default value for resetting to default.
	vignette.mV[VZ] = 1.0;
	mSliderVignetteZ->setValue(1);
	mSpinnerVignetteZ->setValue(1);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}
// </FS:CR> FIRE-9630 - Vignette UI callbacks

// <FS:CR> FIRE-9407 - Restore Quickprefs Defaults
void FloaterQuickPrefs::callbackRestoreDefaults(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if ( option == 0 ) // YES
	{
		selectControl("");
		BOOST_FOREACH(const std::string& control, mControlsOrder)
		{
			removeControl(control);
		}
		mControlsOrder.clear();
		std::string settings_file = LLAppViewer::instance()->getSettingsFilename("Default", "QuickPreferences");
		LLFile::remove(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, settings_file));
		loadSavedSettingsFromFile(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, settings_file));
		gSavedSettings.setBOOL("QuickPrefsEditMode", FALSE);
	}
	else
	{
		llinfos << "User cancelled the reset." << llendl;
	}
}

void FloaterQuickPrefs::onClickRestoreDefaults()
{
	LLNotificationsUtil::add("ConfirmRestoreQuickPrefsDefaults", LLSD(), LLSD(), boost::bind(&FloaterQuickPrefs::callbackRestoreDefaults, this, _1, _2));
}
// </FS:CR>
