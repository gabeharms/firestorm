/** 
 * @file llviewerfloaterreg.cpp
 * @brief LLViewerFloaterReg class registers floaters used in the viewer
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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


#include "llviewerprecompiledheaders.h"

#include "llfloaterreg.h"

#include "llviewerfloaterreg.h"

#include "ao.h"				// ## Zi: Animation Overrider
#include "kvfloaterflickrauth.h"
#include "kvfloaterflickrupload.h"
#include "lggautocorrectfloater.h"
#include "lggbeamcolormapfloater.h"
#include "lggbeammapfloater.h"
#include "llcompilequeue.h"
#include "llcallfloater.h"
#include "llfloaterabout.h"
#include "llfloateranimpreview.h"
#include "llfloaterauction.h"
#include "llfloateravatarpicker.h"
#include "llfloateravatartextures.h"
#include "llfloaterbeacons.h"
#include "llfloaterbuildoptions.h"
#include "llfloaterbuy.h"
#include "llfloaterbuycontents.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterbuycurrencyhtml.h"
#include "llfloaterbuyland.h"
#include "llfloaterbulkpermission.h"
#include "llfloaterbump.h"
#include "llfloatercamera.h"
#include "llfloaterdeleteenvpreset.h"
#include "llfloaterdisplayname.h"
#include "llfloatereditdaycycle.h"
#include "llfloatereditsky.h"
#include "llfloatereditwater.h"
#include "llfloaterenvironmentsettings.h"
#include "llfloaterevent.h"
#include "llfloatersearch.h"
#include "llfloaterfonttest.h"
#include "llfloatergesture.h"
#include "llfloatergodtools.h"
#include "llfloatergroups.h"
#include "llfloaterhardwaresettings.h"
#include "llfloaterhelpbrowser.h"
#include "llfloatermediabrowser.h"
#include "llfloaterwebcontent.h"
#include "llfloatermediasettings.h"
#include "llfloaterhud.h"
#include "llfloaterimagepreview.h"
#include "llimfloater.h"
#include "llfloaterinspect.h"
#include "llfloaterinventory.h"
#include "llfloaterjoystick.h"
#include "llfloaterlagmeter.h"
#include "llfloaterland.h"
#include "llfloaterlandholdings.h"
#include "llfloaterlocalbitmap.h"
#include "llfloatermap.h"
#include "llfloatermemleak.h"
#include "llfloatermodelwizard.h"
#include "llfloaternamedesc.h"
#include "llfloaternotificationsconsole.h"
#include "llfloateropenobject.h"
#include "llfloaterpay.h"
#include "llfloaterperms.h"
#include "llfloaterpostcard.h"
#include "llfloaterpostprocess.h"
#include "llfloaterpreference.h"
#include "llfloaterproperties.h"
#include "llfloaterregiondebugconsole.h"
#include "llfloaterregioninfo.h"
#include "llfloaterreporter.h"
#include "llfloaterscriptdebug.h"
#include "llfloaterscriptlimits.h"
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-10-26 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
#include "llfloatersearchreplace.h"
// [/SL:KB]
#include "llfloatersellland.h"
#include "llfloatersettingsdebug.h"
#include "llfloatersidetraytab.h"
#include "llfloatersnapshot.h"
#include "llfloatersounddevices.h"
#include "llfloatertelehub.h"
#include "llfloatertestinspectors.h"
#include "llfloatertestlistview.h"
#include "llfloatertools.h"
#include "llfloatertos.h"
#include "llfloatertopobjects.h"
#include "llfloateruipreview.h"
#include "llfloatervoiceeffect.h"
#include "llfloaterwhitelistentry.h"
#include "llfloaterwindowsize.h"
#include "llfloaterworldmap.h"
#include "llimfloatercontainer.h"
#include "llinspectavatar.h"
#include "llinspectgroup.h"
#include "llinspectobject.h"
#include "llinspectremoteobject.h"
#include "llinspecttoast.h"
#include "llmoveview.h"
#include "llnearbychat.h"
#include "llpanelblockedlist.h"
#include "llpanelclassified.h"
//-TT - Patch : ShowGroupFloaters
#include "llpanelgroup.h"
//-TT
// [SL:KB] - Patch : UI-ProfileGroupFloater 
#include "llpanelprofileview.h"
// [/SL:KB]
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "llsyswellwindow.h"
#include "llscriptfloater.h"
#include "llfloatermodelpreview.h"
#include "llcommandhandler.h"

// *NOTE: Please add files in alphabetical order to keep merges easy.
// [RLVa:KB] - Checked: 2010-03-11
#include "rlvfloaters.h"
// [/RLVa:KB]
#include "fscontactsfloater.h"
#include "floatermedialists.h"
#include "fsareasearch.h"
#include "quickprefs.h"	// Quick Preferences panel	-WoLf
#include "lggcontactsetsfloater.h"

// handle secondlife:///app/openfloater/{NAME} URLs
class LLFloaterOpenHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLFloaterOpenHandler() : LLCommandHandler("openfloater", UNTRUSTED_THROTTLE) { }

	bool handle(const LLSD& params, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		if (params.size() != 1)
		{
			return false;
		}

		const std::string floater_name = LLURI::unescape(params[0].asString());
		LLFloaterReg::showInstance(floater_name);

		return true;
	}
};

LLFloaterOpenHandler gFloaterOpenHandler;

void LLViewerFloaterReg::registerFloaters()
{
	// *NOTE: Please keep these alphabetized for easier merges

	LLFloaterAboutUtil::registerFloater();
	LLFloaterReg::add("autocorrect", "floater_autocorrect.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LGGAutoCorrectFloater>);
	LLFloaterReg::add("about_land", "floater_about_land.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterLand>);
	LLFloaterReg::add("animation_overrider", "floater_ao.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<FloaterAO>);	// ## Zi: Animation Overrider
	LLFloaterReg::add("area_search", "floater_fs_area_search.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<FSAreaSearch>);
	LLFloaterReg::add("auction", "floater_auction.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterAuction>);
	LLFloaterReg::add("avatar_picker", "floater_avatar_picker.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterAvatarPicker>);
	LLFloaterReg::add("avatar_textures", "floater_avatar_textures.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterAvatarTextures>);

	LLFloaterReg::add("beacons", "floater_beacons.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterBeacons>);
	LLFloaterReg::add("bulk_perms", "floater_bulk_perms.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterBulkPermission>);
	LLFloaterReg::add("buy_currency", "floater_buy_currency.xml", &LLFloaterBuyCurrency::buildFloater);
	LLFloaterReg::add("buy_currency_html", "floater_buy_currency_html.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterBuyCurrencyHTML>);	
	LLFloaterReg::add("buy_land", "floater_buy_land.xml", &LLFloaterBuyLand::buildFloater);
	LLFloaterReg::add("buy_object", "floater_buy_object.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterBuy>);
	LLFloaterReg::add("buy_object_contents", "floater_buy_contents.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterBuyContents>);
	LLFloaterReg::add("build", "floater_tools.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterTools>);
	LLFloaterReg::add("build_options", "floater_build_options.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterBuildOptions>);
	LLFloaterReg::add("bumps", "floater_bumps.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterBump>);

	LLFloaterReg::add("camera", "floater_camera.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterCamera>);
	if (gSavedSettings.getBOOL("FSNearbyChatbar")) 	// xml toggle for chatbar in nearby chat -WoLf
	{
	LLFloaterReg::add("nearby_chat", "floater_nearby_chat.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLNearbyChat>);
	}
	else
	{
	LLFloaterReg::add("nearby_chat", "floater_nearby_chat-cb.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLNearbyChat>);
	}

	LLFloaterReg::add("compile_queue", "floater_script_queue.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterCompileQueue>);
	LLFloaterReg::add("contactsets", "floater_contactsets.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<lggContactSetsFloater>);
	LLFloaterReg::add("contactsetsettings", "floater_contactsets_settings.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<lggContactSetsFloaterSettings>);

	LLFloaterReg::add("env_post_process", "floater_post_process.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPostProcess>);
	LLFloaterReg::add("env_settings", "floater_environment_settings.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterEnvironmentSettings>);
	LLFloaterReg::add("env_delete_preset", "floater_delete_env_preset.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterDeleteEnvPreset>);
	LLFloaterReg::add("env_edit_sky", "floater_edit_sky_preset.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterEditSky>);
	LLFloaterReg::add("env_edit_water", "floater_edit_water_preset.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterEditWater>);
	LLFloaterReg::add("env_edit_day_cycle", "floater_edit_day_cycle.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterEditDayCycle>);

	LLFloaterReg::add("event", "floater_event.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterEvent>);
//-TT - Patch : ShowGroupFloaters
	LLFloaterReg::add("floater_group_view", "floater_group_view.xml",&LLFloaterReg::build<LLFloaterGroupView>);
//-TT
// [SL:KB] - Patch : UI-ProfileGroupFloater | Checked: 2010-09-08 (Catznip-2.1.2c) | Added: Catznip-2.1.2c
	LLFloaterReg::add("floater_profile_view", "floater_profile_view.xml",&LLFloaterReg::build<LLFloaterProfileView>);
// [/SL:KB]
	LLFloaterReg::add("flickr_auth", "floater_flickr_auth.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<KVFloaterFlickrAuth>);
	LLFloaterReg::add("flickr_upload", "floater_flickr_upload.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<KVFloaterFlickrUpload>);
	LLFloaterReg::add("font_test", "floater_font_test.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterFontTest>);

	LLFloaterReg::add("gestures", "floater_gesture.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterGesture>);
	LLFloaterReg::add("god_tools", "floater_god_tools.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterGodTools>);
	LLFloaterReg::add("group_picker", "floater_choose_group.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterGroupPicker>);

	LLFloaterReg::add("help_browser", "floater_help_browser.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterHelpBrowser>);	
	LLFloaterReg::add("hud", "floater_hud.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterHUD>);

	LLFloaterReg::add("imcontacts", "floater_fs_contacts.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<FSFloaterContacts>);
	LLFloaterReg::add("impanel", "floater_im_session.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLIMFloater>);
	LLFloaterReg::add("im_container", "floater_im_container.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLIMFloaterContainer>);
	LLFloaterReg::add("im_well_window", "floater_sys_well.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLIMWellWindow>);
	LLFloaterReg::add("incoming_call", "floater_incoming_call.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLIncomingCallDialog>);
	LLFloaterReg::add("inventory", "floater_inventory.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterInventory>);
	LLFloaterReg::add("inspect", "floater_inspect.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterInspect>);
	LLInspectAvatarUtil::registerFloater();
	LLInspectGroupUtil::registerFloater();
	LLInspectObjectUtil::registerFloater();
	LLInspectRemoteObjectUtil::registerFloater();
	LLNotificationsUI::registerFloater();
	LLFloaterDisplayNameUtil::registerFloater();
	
	LLFloaterReg::add("lagmeter", "floater_lagmeter.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterLagMeter>);
	LLFloaterReg::add("land_holdings", "floater_land_holdings.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterLandHoldings>);

	LLFloaterReg::add("local_bitmap_browser", "floater_local_bitmap.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterLocalBitmapBrowser>);

	LLFloaterReg::add("lgg_beamcolormap", "floater_beamcolor.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<lggBeamColorMapFloater>);
	LLFloaterReg::add("lgg_beamshape", "floater_beamshape.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<lggBeamMapFloater>);

	LLFloaterReg::add("mem_leaking", "floater_mem_leaking.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterMemLeak>);
	LLFloaterReg::add("media_browser", "floater_media_browser.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterMediaBrowser>);	
	LLFloaterReg::add("media_lists", "floater_media_lists.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<FloaterMediaLists>);	
	LLFloaterReg::add("media_settings", "floater_media_settings.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterMediaSettings>);	
	LLFloaterReg::add("message_critical", "floater_critical.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterTOS>);
	LLFloaterReg::add("message_tos", "floater_tos.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterTOS>);
	LLFloaterReg::add("moveview", "floater_moveview.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterMove>);
	LLFloaterReg::add("mute_object_by_name", "floater_mute_object.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterGetBlockedObjectName>);
	LLFloaterReg::add("mini_map", "floater_map.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterMap>);

	LLFloaterReg::add("notifications_console", "floater_notifications_console.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterNotificationConsole>);
	LLFloaterReg::add("notification_well_window", "floater_sys_well.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLNotificationWellWindow>);

	LLFloaterReg::add("openobject", "floater_openobject.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterOpenObject>);
	LLFloaterReg::add("outgoing_call", "floater_outgoing_call.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLOutgoingCallDialog>);
	LLFloaterPayUtil::registerFloater();

	LLFloaterReg::add("postcard", "floater_postcard.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPostcard>);
	LLFloaterReg::add("preferences", "floater_preferences.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPreference>);
	LLFloaterReg::add("prefs_proxy", "floater_preferences_proxy.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPreferenceProxy>);
	LLFloaterReg::add("prefs_hardware_settings", "floater_hardware_settings.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterHardwareSettings>);
	LLFloaterReg::add("perm_prefs", "floater_perm_prefs.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPerms>);
	LLFloaterReg::add("pref_joystick", "floater_joystick.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterJoystick>);
	LLFloaterReg::add("preview_anim", "floater_preview_animation.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLPreviewAnim>, "preview");
	LLFloaterReg::add("preview_gesture", "floater_preview_gesture.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLPreviewGesture>, "preview");
	LLFloaterReg::add("preview_notecard", "floater_preview_notecard.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLPreviewNotecard>, "preview");
	LLFloaterReg::add("preview_script", "floater_script_preview.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLPreviewLSL>, "preview");
	LLFloaterReg::add("preview_scriptedit", "floater_live_lsleditor.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLLiveLSLEditor>, "preview");
	LLFloaterReg::add("preview_sound", "floater_preview_sound.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLPreviewSound>, "preview");
	LLFloaterReg::add("preview_texture", "floater_preview_texture.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLPreviewTexture>, "preview");
	LLFloaterReg::add("properties", "floater_inventory_item_properties.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterProperties>);
	LLFloaterReg::add("publish_classified", "floater_publish_classified.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLPublishClassifiedFloater>);

	LLFloaterReg::add("quickprefs", "floater_quickprefs.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<FloaterQuickPrefs>);  // Quick Preferences panel -WoLf

	LLFloaterReg::add("telehubs", "floater_telehub.xml",&LLFloaterReg::build<LLFloaterTelehub>);
	LLFloaterReg::add("test_inspectors", "floater_test_inspectors.xml", &LLFloaterReg::build<LLFloaterTestInspectors>);
	//LLFloaterReg::add("test_list_view", "floater_test_list_view.xml",&LLFloaterReg::build<LLFloaterTestListView>);
	LLFloaterReg::add("test_textbox", "floater_test_textbox.xml", &LLFloaterReg::build<LLFloater>);
	LLFloaterReg::add("test_text_editor", "floater_test_text_editor.xml", &LLFloaterReg::build<LLFloater>);
	LLFloaterReg::add("test_widgets", "floater_test_widgets.xml", &LLFloaterReg::build<LLFloater>);
	LLFloaterReg::add("top_objects", "floater_top_objects.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterTopObjects>);
	
	LLFloaterReg::add("reporter", "floater_report_abuse.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterReporter>);
	LLFloaterReg::add("reset_queue", "floater_script_queue.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterResetQueue>);
	LLFloaterReg::add("region_debug_console", "floater_region_debug_console.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterRegionDebugConsole>);
	LLFloaterReg::add("region_info", "floater_region_info.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterRegionInfo>);
// [RLVa:KB] - Checked: 2010-03-11 (RLVa-1.2.0e) | Added: RLVa-1.2.0a
	LLFloaterReg::add("rlv_behaviours", "floater_rlv_behaviours.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<RlvFloaterBehaviours>);
	LLFloaterReg::add("rlv_locks", "floater_rlv_locks.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<RlvFloaterLocks>);
// [/RLVa:KB]
	
	LLFloaterReg::add("script_debug", "floater_script_debug.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterScriptDebug>);
	LLFloaterReg::add("script_debug_output", "floater_script_debug_panel.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterScriptDebugOutput>);
	LLFloaterReg::add("script_floater", "floater_script.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLScriptFloater>);
	LLFloaterReg::add("script_limits", "floater_script_limits.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterScriptLimits>);
	LLFloaterReg::add("sell_land", "floater_sell_land.xml", &LLFloaterSellLand::buildFloater);
	LLFloaterReg::add("settings_debug", "floater_settings_debug.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterSettingsDebug>);
	LLFloaterReg::add("side_bar_tab", "floater_side_bar_tab.xml", &LLFloaterReg::build<LLFloaterSideTrayTab>);
	LLFloaterReg::add("sound_devices", "floater_sound_devices.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterSoundDevices>);
	LLFloaterReg::add("stats", "floater_stats.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloater>);
	LLFloaterReg::add("start_queue", "floater_script_queue.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterRunQueue>);
	LLFloaterReg::add("stop_queue", "floater_script_queue.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterNotRunQueue>);
	LLFloaterReg::add("snapshot", "floater_snapshot.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterSnapshot>);
	LLFloaterReg::add("search", "floater_search.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterSearch>);
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-10-26 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
	LLFloaterReg::add("search_replace", "floater_search_replace.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterSearchReplace>);	
// [/SL:KB]
	
	LLFloaterUIPreviewUtil::registerFloater();
	LLFloaterReg::add("upload_anim", "floater_animation_preview.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterAnimPreview>, "upload");
	LLFloaterReg::add("upload_image", "floater_image_preview.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterImagePreview>, "upload");
	LLFloaterReg::add("upload_sound", "floater_sound_preview.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterSoundPreview>, "upload");
	LLFloaterReg::add("upload_model", "floater_model_preview.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterModelPreview>, "upload");
	LLFloaterReg::add("upload_model_wizard", "floater_model_wizard.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterModelWizard>);

	LLFloaterReg::add("voice_controls", "floater_voice_controls.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLCallFloater>);
	LLFloaterReg::add("voice_effect", "floater_voice_effect.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterVoiceEffect>);

	LLFloaterReg::add("web_content", "floater_web_content.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterWebContent>);	
	LLFloaterReg::add("whitelist_entry", "floater_whitelist_entry.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterWhiteListEntry>);	
	LLFloaterWindowSizeUtil::registerFloater();
	LLFloaterReg::add("world_map", "floater_world_map.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterWorldMap>);	

	// *NOTE: Please keep these alphabetized for easier merges
	
	LLFloaterReg::registerControlVariables(); // Make sure visibility and rect controls get preserved when saving
}
