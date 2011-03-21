/** 
* @file llstatusbar.cpp
* @brief LLStatusBar class implementation
*
* $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "llstatusbar.h"

// viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llbutton.h"
#include "llcommandhandler.h"
#include "llfirstuse.h"
#include "llviewercontrol.h"
#include "llfloaterbuycurrency.h"
#include "llbuycurrencyhtml.h"
#include "llfloaterlagmeter.h"
#include "llpanelnearbymedia.h"
#include "llpanelvolumepulldown.h"
#include "llfloaterregioninfo.h"
#include "llfloaterscriptdebug.h"
#include "llhints.h"
#include "llhudicon.h"
#include "llnavigationbar.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llrootview.h"
#include "llsd.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llframetimer.h"
#include "llvoavatarself.h"
#include "llresmgr.h"
#include "llworld.h"
#include "llstatgraph.h"
#include "llviewermedia.h"
#include "llviewermenu.h"	// for gMenuBarView
#include "llviewerparcelmgr.h"
#include "llviewerthrottle.h"
#include "lluictrlfactory.h"

#include "llagentui.h"
#include "llclipboard.h"
#include "lllandmarkactions.h"
#include "lllocationinputctrl.h"
#include "llparcel.h"
#include "llsidetray.h"
#include "llslurl.h"
#include "llviewerinventory.h"

#include "lltoolmgr.h"
#include "llfocusmgr.h"
#include "llappviewer.h"
#include "lltrans.h"


#include "kcwlinterface.h"

// library includes
#include "imageids.h"
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llerror.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llstring.h"
#include "message.h"

// system includes
#include <iomanip>

//
// Globals
//
LLStatusBar *gStatusBar = NULL;
S32 STATUS_BAR_HEIGHT = 26;
extern S32 MENU_BAR_HEIGHT;


// TODO: these values ought to be in the XML too
const S32 MENU_PARCEL_SPACING = 1;	// Distance from right of menu item to parcel information
const S32 SIM_STAT_WIDTH = 8;
const F32 SIM_WARN_FRACTION = 0.75f;
const F32 SIM_FULL_FRACTION = 0.98f;
const LLColor4 SIM_OK_COLOR(0.f, 1.f, 0.f, 1.f);
const LLColor4 SIM_WARN_COLOR(1.f, 1.f, 0.f, 1.f);
const LLColor4 SIM_FULL_COLOR(1.f, 0.f, 0.f, 1.f);
const F32 ICON_TIMER_EXPIRY		= 3.f; // How long the balance and health icons should flash after a change.
const F32 ICON_FLASH_FREQUENCY	= 2.f;
const S32 TEXT_HEIGHT = 18;

static void onClickVolume(void*);

class LLStatusBar::LLParcelChangeObserver : public LLParcelObserver
{
public:
	LLParcelChangeObserver(LLStatusBar* topInfoBar) : mTopInfoBar(topInfoBar) {}

private:
	/*virtual*/ void changed()
	{
		if (mTopInfoBar)
		{
			mTopInfoBar->updateParcelIcons();
		}
	}

	LLStatusBar* mTopInfoBar;
};

LLStatusBar::LLStatusBar(const LLRect& rect)
:	LLPanel(),
	mTextTime(NULL),
	mSGBandwidth(NULL),
	mSGPacketLoss(NULL),
	mBtnVolume(NULL),
	mBalance(0),
	mHealth(100),
	mSquareMetersCredit(0),
	mSquareMetersCommitted(0)
{
	setRect(rect);
	
	// status bar can possible overlay menus?
	setMouseOpaque(FALSE);

	mBalanceTimer = new LLFrameTimer();
	mHealthTimer = new LLFrameTimer();
	
	LLUICtrl::CommitCallbackRegistry::currentRegistrar()
			.add("TopInfoBar.Action", boost::bind(&LLStatusBar::onContextMenuItemClicked, this, _2));

	buildFromFile("panel_status_bar.xml");
}

LLStatusBar::~LLStatusBar()
{
	delete mBalanceTimer;
	mBalanceTimer = NULL;

	delete mHealthTimer;
	mHealthTimer = NULL;
	
	if (mParcelChangedObserver)
	{
		LLViewerParcelMgr::getInstance()->removeObserver(mParcelChangedObserver);
		delete mParcelChangedObserver;
	}

	if (mParcelPropsCtrlConnection.connected())
	{
		mParcelPropsCtrlConnection.disconnect();
	}

	if (mParcelMgrConnection.connected())
	{
		mParcelMgrConnection.disconnect();
	}

	if (mShowCoordsCtrlConnection.connected())
	{
		mShowCoordsCtrlConnection.disconnect();
	}

	// LLView destructor cleans up children
}

//-----------------------------------------------------------------------
// Overrides
//-----------------------------------------------------------------------

// virtual
void LLStatusBar::draw()
{
	refresh();
	updateParcelInfoText();
	updateHealth();
	LLPanel::draw();
}

BOOL LLStatusBar::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	show_navbar_context_menu(this,x,y);
	return TRUE;
}

BOOL LLStatusBar::postBuild()
{
	gMenuBarView->setRightMouseDownCallback(boost::bind(&show_navbar_context_menu, _1, _2, _3));

	mTextTime = getChild<LLTextBox>("TimeText" );
	
	getChild<LLUICtrl>("buyL")->setCommitCallback(
		boost::bind(&LLStatusBar::onClickBuyCurrency, this));

	mBtnVolume = getChild<LLButton>( "volume_btn" );
	mBtnVolume->setClickedCallback( onClickVolume, this );
	mBtnVolume->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterVolume, this));

	mMediaToggle = getChild<LLButton>("media_toggle_btn");
	mMediaToggle->setClickedCallback( &LLStatusBar::onClickMediaToggle, this );
	mMediaToggle->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterNearbyMedia, this));

	LLHints::registerHintTarget("linden_balance", getChild<LLView>("balance_bg")->getHandle());

	gSavedSettings.getControl("MuteAudio")->getSignal()->connect(boost::bind(&LLStatusBar::onVolumeChanged, this, _2));

	// Adding Net Stat Graph
	S32 x = getRect().getWidth() - 2;
	S32 y = 0;
	LLRect r;
	r.set( x-SIM_STAT_WIDTH, y+MENU_BAR_HEIGHT-1, x, y+1);
	LLStatGraph::Params sgp;
	sgp.name("BandwidthGraph");
	sgp.rect(r);
	sgp.follows.flags(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	sgp.mouse_opaque(false);
	mSGBandwidth = LLUICtrlFactory::create<LLStatGraph>(sgp);
	mSGBandwidth->setStat(&LLViewerStats::getInstance()->mKBitStat);
	mSGBandwidth->setUnits("Kbps");
	mSGBandwidth->setPrecision(0);
	addChild(mSGBandwidth);
	x -= SIM_STAT_WIDTH + 2;

	r.set( x-SIM_STAT_WIDTH, y+MENU_BAR_HEIGHT-1, x, y+1);
	//these don't seem to like being reused
	LLStatGraph::Params pgp;
	pgp.name("PacketLossPercent");
	pgp.rect(r);
	pgp.follows.flags(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	pgp.mouse_opaque(false);

	mSGPacketLoss = LLUICtrlFactory::create<LLStatGraph>(pgp);
	mSGPacketLoss->setStat(&LLViewerStats::getInstance()->mPacketsLostPercentStat);
	mSGPacketLoss->setUnits("%");
	mSGPacketLoss->setMin(0.f);
	mSGPacketLoss->setMax(5.f);
	mSGPacketLoss->setThreshold(0, 0.5f);
	mSGPacketLoss->setThreshold(1, 1.f);
	mSGPacketLoss->setThreshold(2, 3.f);
	mSGPacketLoss->setPrecision(1);
	mSGPacketLoss->mPerSec = FALSE;
	addChild(mSGPacketLoss);

	mPanelVolumePulldown = new LLPanelVolumePulldown();
	addChild(mPanelVolumePulldown);
	mPanelVolumePulldown->setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
	mPanelVolumePulldown->setVisible(FALSE);

	mPanelNearByMedia = new LLPanelNearByMedia();
	addChild(mPanelNearByMedia);
	mPanelNearByMedia->setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
	mPanelNearByMedia->setVisible(FALSE);

	mScriptOut = getChildView("scriptout");
	
	mInfoBtn = getChild<LLButton>("place_info_btn");
	mInfoBtn->setClickedCallback(boost::bind(&LLStatusBar::onInfoButtonClicked, this));

	mParcelInfoPanel = getChild<LLPanel>("parcel_info_panel");
	mParcelInfoText = getChild<LLTextBox>("parcel_info_text");
	mDamageText = getChild<LLTextBox>("damage_text");

	mPWLBtn = getChild<LLButton>("status_wl_btn");
	mPWLBtn->setClickedCallback(boost::bind(&LLStatusBar::onParcelWLClicked, this));

	initParcelIcons();

	mParcelChangedObserver = new LLParcelChangeObserver(this);
	LLViewerParcelMgr::getInstance()->addObserver(mParcelChangedObserver);

	// Connecting signal for updating parcel icons on "Show Parcel Properties" setting change.
	LLControlVariable* ctrl = gSavedSettings.getControl("NavBarShowParcelProperties").get();
	if (ctrl)
	{
		mParcelPropsCtrlConnection = ctrl->getSignal()->connect(boost::bind(&LLStatusBar::updateParcelIcons, this));
	}

	// Connecting signal for updating parcel text on "Show Coordinates" setting change.
	ctrl = gSavedSettings.getControl("NavBarShowCoordinates").get();
	if (ctrl)
	{
		mShowCoordsCtrlConnection = ctrl->getSignal()->connect(boost::bind(&LLStatusBar::onNavBarShowParcelPropertiesCtrlChanged, this));
	}

	mParcelMgrConnection = LLViewerParcelMgr::getInstance()->addAgentParcelChangedCallback(
			boost::bind(&LLStatusBar::onAgentParcelChange, this));

	return TRUE;
}

// Per-frame updates of visibility
void LLStatusBar::refresh()
{
	static LLCachedControl<bool> show_net_stats(gSavedSettings, "ShowNetStats", false);
	bool net_stats_visible = show_net_stats;

	if (net_stats_visible)
	{
		// Adding Net Stat Meter back in
		F32 bwtotal = gViewerThrottle.getMaxBandwidth() / 1000.f;
		mSGBandwidth->setMin(0.f);
		mSGBandwidth->setMax(bwtotal*1.25f);
		mSGBandwidth->setThreshold(0, bwtotal*0.75f);
		mSGBandwidth->setThreshold(1, bwtotal);
		mSGBandwidth->setThreshold(2, bwtotal);
	}
	
	// update clock every 10 seconds
	if(mClockUpdateTimer.getElapsedTimeF32() > 10.f)
	{
		mClockUpdateTimer.reset();

		// Get current UTC time, adjusted for the user's clock
		// being off.
		time_t utc_time;
		utc_time = time_corrected();

		std::string timeStr = getString("time");
		LLSD substitution;
		substitution["datetime"] = (S32) utc_time;
		LLStringUtil::format (timeStr, substitution);
		mTextTime->setText(timeStr);

		// set the tooltip to have the date
		std::string dtStr = getString("timeTooltip");
		LLStringUtil::format (dtStr, substitution);
		mTextTime->setToolTip (dtStr);
	}

	LLRect r;
	const S32 MENU_RIGHT = gMenuBarView->getRightmostMenuEdge();

	// reshape menu bar to its content's width
	if (MENU_RIGHT != gMenuBarView->getRect().getWidth())
	{
		gMenuBarView->reshape(MENU_RIGHT, gMenuBarView->getRect().getHeight());
	}
	// also update the parcel info panel pos -KC
	if ((MENU_RIGHT + MENU_PARCEL_SPACING) != mParcelInfoPanel->getRect().mLeft)
	{
		updateParcelPanel();
	}

	mSGBandwidth->setVisible(net_stats_visible);
	mSGPacketLoss->setVisible(net_stats_visible);
	getChildView("stat_btn")->setEnabled(net_stats_visible);

	// update the master volume button state
	bool mute_audio = LLAppViewer::instance()->getMasterSystemAudioMute();
	mBtnVolume->setToggleState(mute_audio);
	
	// Disable media toggle if there's no media, parcel media, and no parcel audio
	// (or if media is disabled)
	bool button_enabled = (gSavedSettings.getBOOL("AudioStreamingMusic")||gSavedSettings.getBOOL("AudioStreamingMedia")) && 
						  (LLViewerMedia::hasInWorldMedia() || LLViewerMedia::hasParcelMedia() || LLViewerMedia::hasParcelAudio());
	mMediaToggle->setEnabled(button_enabled);
	// Note the "sense" of the toggle is opposite whether media is playing or not
	bool any_media_playing = (LLViewerMedia::isAnyMediaShowing() || 
							  LLViewerMedia::isParcelMediaPlaying() ||
							  LLViewerMedia::isParcelAudioPlaying());
	mMediaToggle->setValue(!any_media_playing);
}

void LLStatusBar::setVisibleForMouselook(bool visible)
{
	mTextTime->setVisible(visible);
	getChild<LLUICtrl>("balance_bg")->setVisible(visible);
	mBtnVolume->setVisible(visible);
	mMediaToggle->setVisible(visible);
	mSGBandwidth->setVisible(visible);
	mSGPacketLoss->setVisible(visible);
	setBackgroundVisible(visible);
}

void LLStatusBar::debitBalance(S32 debit)
{
	setBalance(getBalance() - debit);
}

void LLStatusBar::creditBalance(S32 credit)
{
	setBalance(getBalance() + credit);
}

void LLStatusBar::setBalance(S32 balance)
{
	if (balance > getBalance() && getBalance() != 0)
	{
		LLFirstUse::receiveLindens();
	}

	std::string money_str = LLResMgr::getInstance()->getMonetaryString( balance );

	LLTextBox* balance_box = getChild<LLTextBox>("balance");
	LLStringUtil::format_map_t string_args;
	string_args["[AMT]"] = llformat("%s", money_str.c_str());
	std::string label_str = getString("buycurrencylabel", string_args);
	balance_box->setValue(label_str);

	// Resize the L$ balance background to be wide enough for your balance plus the buy button
	{
		const S32 HPAD = 24;
		LLRect balance_rect = balance_box->getTextBoundingRect();
		LLRect buy_rect = getChildView("buyL")->getRect();
		LLView* balance_bg_view = getChildView("balance_bg");
		LLRect balance_bg_rect = balance_bg_view->getRect();
		balance_bg_rect.mLeft = balance_bg_rect.mRight - (buy_rect.getWidth() + balance_rect.getWidth() + HPAD);
		balance_bg_view->setShape(balance_bg_rect);
	}

	if (mBalance && (fabs((F32)(mBalance - balance)) > gSavedSettings.getF32("UISndMoneyChangeThreshold")))
	{
		if (mBalance > balance)
			make_ui_sound("UISndMoneyChangeDown");
		else
			make_ui_sound("UISndMoneyChangeUp");
	}

	if( balance != mBalance )
	{
		mBalanceTimer->reset();
		mBalanceTimer->setTimerExpirySec( ICON_TIMER_EXPIRY );
		mBalance = balance;
	}
}


// static
void LLStatusBar::sendMoneyBalanceRequest()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_MoneyBalanceRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MoneyData);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null );
	gAgent.sendReliableMessage();
}


void LLStatusBar::setHealth(S32 health)
{
	//llinfos << "Setting health to: " << buffer << llendl;
	if( mHealth > health )
	{
		if (mHealth > (health + gSavedSettings.getF32("UISndHealthReductionThreshold")))
		{
			if (isAgentAvatarValid())
			{
				if (gAgentAvatarp->getSex() == SEX_FEMALE)
				{
					make_ui_sound("UISndHealthReductionF");
				}
				else
				{
					make_ui_sound("UISndHealthReductionM");
				}
			}
		}

		mHealthTimer->reset();
		mHealthTimer->setTimerExpirySec( ICON_TIMER_EXPIRY );
	}

	mHealth = health;
}

S32 LLStatusBar::getBalance() const
{
	return mBalance;
}


S32 LLStatusBar::getHealth() const
{
	return mHealth;
}

void LLStatusBar::setLandCredit(S32 credit)
{
	mSquareMetersCredit = credit;
}
void LLStatusBar::setLandCommitted(S32 committed)
{
	mSquareMetersCommitted = committed;
}

BOOL LLStatusBar::isUserTiered() const
{
	return (mSquareMetersCredit > 0);
}

S32 LLStatusBar::getSquareMetersCredit() const
{
	return mSquareMetersCredit;
}

S32 LLStatusBar::getSquareMetersCommitted() const
{
	return mSquareMetersCommitted;
}

S32 LLStatusBar::getSquareMetersLeft() const
{
	return mSquareMetersCredit - mSquareMetersCommitted;
}

void LLStatusBar::onClickBuyCurrency()
{
	// open a currency floater - actual one open depends on 
	// value specified in settings.xml
	LLBuyCurrencyHTML::openCurrencyFloater();
	LLFirstUse::receiveLindens(false);
}

void LLStatusBar::onMouseEnterVolume()
{
	LLButton* volbtn =  getChild<LLButton>( "volume_btn" );
	LLRect vol_btn_rect = volbtn->getRect();
	LLRect volume_pulldown_rect = mPanelVolumePulldown->getRect();
	volume_pulldown_rect.setLeftTopAndSize(vol_btn_rect.mLeft -
	     (volume_pulldown_rect.getWidth() - vol_btn_rect.getWidth())/2,
			       vol_btn_rect.mBottom,
			       volume_pulldown_rect.getWidth(),
			       volume_pulldown_rect.getHeight());

	mPanelVolumePulldown->setShape(volume_pulldown_rect);


	// show the master volume pull-down
	LLUI::clearPopups();
	LLUI::addPopup(mPanelVolumePulldown);
	mPanelNearByMedia->setVisible(FALSE);
	mPanelVolumePulldown->setVisible(TRUE);
}

void LLStatusBar::onMouseEnterNearbyMedia()
{
	LLView* popup_holder = gViewerWindow->getRootView()->getChildView("popup_holder");
	LLRect nearby_media_rect = mPanelNearByMedia->getRect();
	LLButton* nearby_media_btn =  getChild<LLButton>( "media_toggle_btn" );
	LLRect nearby_media_btn_rect = nearby_media_btn->getRect();
	nearby_media_rect.setLeftTopAndSize(nearby_media_btn_rect.mLeft - 
										(nearby_media_rect.getWidth() - nearby_media_btn_rect.getWidth())/2,
										nearby_media_btn_rect.mBottom,
										nearby_media_rect.getWidth(),
										nearby_media_rect.getHeight());
	// force onscreen
	nearby_media_rect.translate(popup_holder->getRect().getWidth() - nearby_media_rect.mRight, 0);
	
	// show the master volume pull-down
	mPanelNearByMedia->setShape(nearby_media_rect);
	LLUI::clearPopups();
	LLUI::addPopup(mPanelNearByMedia);

	mPanelVolumePulldown->setVisible(FALSE);
	mPanelNearByMedia->setVisible(TRUE);
}


static void onClickVolume(void* data)
{
	// toggle the master mute setting
	bool mute_audio = LLAppViewer::instance()->getMasterSystemAudioMute();
	LLAppViewer::instance()->setMasterSystemAudioMute(!mute_audio);	
}

//static 
void LLStatusBar::onClickMediaToggle(void* data)
{
	LLStatusBar *status_bar = (LLStatusBar*)data;
	// "Selected" means it was showing the "play" icon (so media was playing), and now it shows "pause", so turn off media
	bool enable = ! status_bar->mMediaToggle->getValue();
	LLViewerMedia::setAllMediaEnabled(enable);
}

BOOL can_afford_transaction(S32 cost)
{
	return((cost <= 0)||((gStatusBar) && (gStatusBar->getBalance() >=cost)));
}

void LLStatusBar::onVolumeChanged(const LLSD& newvalue)
{
	refresh();
}

// Implements secondlife:///app/balance/request to request a L$ balance
// update via UDP message system. JC
class LLBalanceHandler : public LLCommandHandler
{
public:
	// Requires "trusted" browser/URL source
	LLBalanceHandler() : LLCommandHandler("balance", UNTRUSTED_BLOCK) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLMediaCtrl* web)
	{
		if (tokens.size() == 1
			&& tokens[0].asString() == "request")
		{
			LLStatusBar::sendMoneyBalanceRequest();
			return true;
		}
		return false;
	}
};
// register with command dispatch system
LLBalanceHandler gBalanceHandler;


void LLStatusBar::initParcelIcons()
{
	mParcelIcon[VOICE_ICON] = getChild<LLIconCtrl>("voice_icon");
	mParcelIcon[FLY_ICON] = getChild<LLIconCtrl>("fly_icon");
	mParcelIcon[PUSH_ICON] = getChild<LLIconCtrl>("push_icon");
	mParcelIcon[BUILD_ICON] = getChild<LLIconCtrl>("build_icon");
	mParcelIcon[SCRIPTS_ICON] = getChild<LLIconCtrl>("scripts_icon");
	mParcelIcon[DAMAGE_ICON] = getChild<LLIconCtrl>("damage_icon");

	mParcelIcon[VOICE_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, VOICE_ICON));
	mParcelIcon[FLY_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, FLY_ICON));
	mParcelIcon[PUSH_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, PUSH_ICON));
	mParcelIcon[BUILD_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, BUILD_ICON));
	mParcelIcon[SCRIPTS_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, SCRIPTS_ICON));
	mParcelIcon[DAMAGE_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, DAMAGE_ICON));

	mDamageText->setText(LLStringExplicit("100%"));
}

void LLStatusBar::handleLoginComplete()
{
	// An agent parcel update hasn't occurred yet, so
	// we have to manually set location and the icons.
	update();
}

void LLStatusBar::onNavBarShowParcelPropertiesCtrlChanged()
{
	std::string new_text;

	// don't need to have separate show_coords variable; if user requested the coords to be shown
	// they will be added during the next call to the draw() method.
	buildLocationString(new_text, false);
	setParcelInfoText(new_text);
}

void LLStatusBar::buildLocationString(std::string& loc_str, bool show_coords)
{
	LLAgentUI::ELocationFormat format =
		(show_coords ? LLAgentUI::LOCATION_FORMAT_FULL : LLAgentUI::LOCATION_FORMAT_NO_COORDS);

	if (!LLAgentUI::buildLocationString(loc_str, format))
	{
		loc_str = "???";
	}
}

void LLStatusBar::setParcelInfoText(const std::string& new_text)
{
	const LLFontGL* font = mParcelInfoText->getDefaultFont();
	S32 new_text_width = font->getWidth(new_text);

	mParcelInfoText->setText(new_text);

	LLRect rect = mParcelInfoText->getRect();
	rect.setOriginAndSize(rect.mLeft, rect.mBottom, new_text_width, rect.getHeight());

	mParcelInfoText->reshape(rect.getWidth(), rect.getHeight(), TRUE);
	mParcelInfoText->setRect(rect);
	layoutParcelIcons();
}

void LLStatusBar::update()
{
	std::string new_text;

	// don't need to have separate show_coords variable; if user requested the coords to be shown
	// they will be added during the next call to the draw() method.
	buildLocationString(new_text, false);
	setParcelInfoText(new_text);

	updateParcelIcons();
	updateParcelPanel();
}

void LLStatusBar::updateParcelPanel()
{
	const S32 MENU_RIGHT = gMenuBarView->getRightmostMenuEdge();
	S32 left = MENU_RIGHT + MENU_PARCEL_SPACING;
	LLRect rect = mParcelInfoPanel->getRect();
	rect.mRight = left + rect.getWidth();
	rect.mLeft = left;
	
	mParcelInfoPanel->setRect(rect);
}

void LLStatusBar::updateParcelInfoText()
{
	static LLUICachedControl<bool> show_coords("NavBarShowCoordinates", false);

	if (show_coords)
	{
		std::string new_text;

		buildLocationString(new_text, show_coords);
		setParcelInfoText(new_text);
	}
}

void LLStatusBar::updateParcelIcons()
{
	LLViewerParcelMgr* vpm = LLViewerParcelMgr::getInstance();

	LLViewerRegion* agent_region = gAgent.getRegion();
	LLParcel* agent_parcel = vpm->getAgentParcel();
	if (!agent_region || !agent_parcel)
		return;

	if (gSavedSettings.getBOOL("NavBarShowParcelProperties"))
	{
		LLParcel* current_parcel;
		LLViewerRegion* selection_region = vpm->getSelectionRegion();
		LLParcel* selected_parcel = vpm->getParcelSelection()->getParcel();

		// If agent is in selected parcel we use its properties because
		// they are updated more often by LLViewerParcelMgr than agent parcel properties.
		// See LLViewerParcelMgr::processParcelProperties().
		// This is needed to reflect parcel restrictions changes without having to leave
		// the parcel and then enter it again. See EXT-2987
		if (selected_parcel && selected_parcel->getLocalID() == agent_parcel->getLocalID()
				&& selection_region == agent_region)
		{
			current_parcel = selected_parcel;
		}
		else
		{
			current_parcel = agent_parcel;
		}

		bool allow_voice	= vpm->allowAgentVoice(agent_region, current_parcel);
		bool allow_fly		= vpm->allowAgentFly(agent_region, current_parcel);
		bool allow_push		= vpm->allowAgentPush(agent_region, current_parcel);
		bool allow_build	= vpm->allowAgentBuild(current_parcel); // true when anyone is allowed to build. See EXT-4610.
		bool allow_scripts	= vpm->allowAgentScripts(agent_region, current_parcel);
		bool allow_damage	= vpm->allowAgentDamage(agent_region, current_parcel);
		bool has_pwl		= KCWindlightInterface::instance().WLset;

		// Most icons are "block this ability"
		mParcelIcon[VOICE_ICON]->setVisible(   !allow_voice );
		mParcelIcon[FLY_ICON]->setVisible(     !allow_fly );
		mParcelIcon[PUSH_ICON]->setVisible(    !allow_push );
		mParcelIcon[BUILD_ICON]->setVisible(   !allow_build );
		mParcelIcon[SCRIPTS_ICON]->setVisible( !allow_scripts );
		mParcelIcon[DAMAGE_ICON]->setVisible(  allow_damage );
		mDamageText->setVisible(allow_damage);
		mPWLBtn->setVisible(has_pwl);
		mPWLBtn->setEnabled(has_pwl);

		layoutParcelIcons();
	}
	else
	{
		for (S32 i = 0; i < ICON_COUNT; ++i)
		{
			mParcelIcon[i]->setVisible(false);
		}
		mDamageText->setVisible(false);
	}
}

void LLStatusBar::updateHealth()
{
	static LLUICachedControl<bool> show_icons("NavBarShowParcelProperties", false);

	// *FIXME: Status bar owns health information, should be in agent
	if (show_icons && gStatusBar)
	{
		static S32 last_health = -1;
		S32 health = gStatusBar->getHealth();
		if (health != last_health)
		{
			std::string text = llformat("%d%%", health);
			mDamageText->setText(text);
			last_health = health;
		}
	}
}

void LLStatusBar::layoutParcelIcons()
{
	// TODO: remove hard-coded values and read them as xml parameters
	static const int FIRST_ICON_HPAD = 16;
	// Kadah - not needed static const int LAST_ICON_HPAD = 11;

	S32 left = mParcelInfoText->getRect().mRight + FIRST_ICON_HPAD;

	left = layoutWidget(mDamageText, left);

	for (int i = ICON_COUNT - 1; i >= 0; --i)
	{
		left = layoutWidget(mParcelIcon[i], left);
	}

	layoutWidget(mPWLBtn, left);
}

S32 LLStatusBar::layoutWidget(LLUICtrl* ctrl, S32 left)
{
	// TODO: remove hard-coded values and read them as xml parameters
	static const int ICON_HPAD = 2;

	if (ctrl->getVisible())
	{
		LLRect rect = ctrl->getRect();
		rect.mRight = left + rect.getWidth();
		rect.mLeft = left;

		ctrl->setRect(rect);
		left += rect.getWidth() + ICON_HPAD;
	}

	return left;
}

void LLStatusBar::onParcelIconClick(EParcelIcon icon)
{
	switch (icon)
	{
	case VOICE_ICON:
		LLNotificationsUtil::add("NoVoice");
		break;
	case FLY_ICON:
		LLNotificationsUtil::add("NoFly");
		break;
	case PUSH_ICON:
		LLNotificationsUtil::add("PushRestricted");
		break;
	case BUILD_ICON:
		LLNotificationsUtil::add("NoBuild");
		break;
	case SCRIPTS_ICON:
	{
		LLViewerRegion* region = gAgent.getRegion();
		if(region && region->getRegionFlags() & REGION_FLAGS_ESTATE_SKIP_SCRIPTS)
		{
			LLNotificationsUtil::add("ScriptsStopped");
		}
		else if(region && region->getRegionFlags() & REGION_FLAGS_SKIP_SCRIPTS)
		{
			LLNotificationsUtil::add("ScriptsNotRunning");
		}
		else
		{
			LLNotificationsUtil::add("NoOutsideScripts");
		}
		break;
	}
	case DAMAGE_ICON:
		LLNotificationsUtil::add("NotSafe");
		break;
	case ICON_COUNT:
		break;
	// no default to get compiler warning when a new icon gets added
	}
}

void LLStatusBar::onAgentParcelChange()
{
	update();
}

void LLStatusBar::onContextMenuItemClicked(const LLSD::String& item)
{
	if (item == "landmark")
	{
		LLViewerInventoryItem* landmark = LLLandmarkActions::findLandmarkForAgentPos();

		if(landmark == NULL)
		{
			LLSideTray::getInstance()->showPanel("panel_places", LLSD().with("type", "create_landmark"));
		}
		else
		{
			LLSideTray::getInstance()->showPanel("panel_places",
					LLSD().with("type", "landmark").with("id",landmark->getUUID()));
		}
	}
	else if (item == "copy")
	{
		LLSLURL slurl;
		LLAgentUI::buildSLURL(slurl, false);
		LLUIString location_str(slurl.getSLURLString());

		gClipboard.copyFromString(location_str);
	}
}

void LLStatusBar::onInfoButtonClicked()
{
	//LLSideTray::getInstance()->showPanel("panel_places", LLSD().with("type", "agent"));
	LLFloaterReg::showInstance("about_land");
}

void LLStatusBar::onParcelWLClicked()
{
	KCWindlightInterface::instance().onClickWLStatusButton();
}

// hack -KC
void LLStatusBar::setBackgroundColor( const LLColor4& color )
{
	LLPanel::setBackgroundColor(color);
	getChild<LLPanel>("balance_bg")->setBackgroundColor(color);
	getChild<LLPanel>("time_and_media_bg")->setBackgroundColor(color);
}