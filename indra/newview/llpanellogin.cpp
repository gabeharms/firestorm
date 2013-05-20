/** 
 * @file llpanellogin.cpp
 * @brief Login dialog and logo display
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

#include "llpanellogin.h"
#include "lllayoutstack.h"

#include "indra_constants.h"		// for key and mask constants
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llmd5.h"
#include "llsecondlifeurls.h"
#include "v4color.h"

#include "llappviewer.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcommandhandler.h"		// for secondlife:///app/login/
#include "llcombobox.h"
#include "llcurl.h"
#include "llviewercontrol.h"
#include "llfloaterpreference.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llsecapi.h"
#include "llstartup.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llslurl.h"
#include "llversioninfo.h"
#include "llviewerhelp.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"			// for handle_preferences()
#include "llviewernetwork.h"
#include "llviewerwindow.h"			// to link into child list
#include "lluictrlfactory.h"
#include "llhttpclient.h"
#include "llweb.h"
#include "llmediactrl.h"
#include "llrootview.h"

#include "llfloatertos.h"
#include "lltrans.h"
#include "llglheaders.h"
#include "llpanelloginlistener.h"

#include "fsdata.h"

#if LL_WINDOWS
#pragma warning(disable: 4355)      // 'this' used in initializer list
#endif  // LL_WINDOWS

#include "llsdserialize.h"

const S32 BLACK_BORDER_HEIGHT = 160;
const S32 MAX_PASSWORD = 16;

LLPanelLogin *LLPanelLogin::sInstance = NULL;
BOOL LLPanelLogin::sCapslockDidNotification = FALSE;

// Helper for converting a user name into the canonical "Firstname Lastname" form.
// For new accounts without a last name "Resident" is added as a last name.
static std::string canonicalize_username(const std::string& name);

class LLLoginRefreshHandler : public LLCommandHandler
{
public:
	// don't allow from external browsers
	LLLoginRefreshHandler() : LLCommandHandler("login_refresh", UNTRUSTED_BLOCK) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLMediaCtrl* web)
	{	
		if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
		{
			LLPanelLogin::loadLoginPage();
		}	
		return true;
	}
};

//---------------------------------------------------------------------------
// Public methods
//---------------------------------------------------------------------------
LLPanelLogin::LLPanelLogin(const LLRect &rect,
						 void (*callback)(S32 option, void* user_data),
						 void *cb_data)
:	LLPanel(),
	mLogoImage(),
	mCallback(callback),
	mCallbackData(cb_data),
	mListener(new LLPanelLoginListener(this))
{
	setBackgroundVisible(FALSE);
	setBackgroundOpaque(TRUE);

	// instance management
	if (LLPanelLogin::sInstance)
	{
		LL_WARNS("AppInit") << "Duplicate instance of login view deleted" << LL_ENDL;
		// Don't leave bad pointer in gFocusMgr
		gFocusMgr.setDefaultKeyboardFocus(NULL);

		delete LLPanelLogin::sInstance;
	}

	mPasswordModified = FALSE;
	LLPanelLogin::sInstance = this;

	LLView* login_holder = gViewerWindow->getLoginPanelHolder();
	if (login_holder)
	{
		login_holder->addChild(this);
	}

	// Logo
	mLogoImage = LLUI::getUIImage("startup_logo");

	buildFromFile( "panel_login.xml");

	reshape(rect.getWidth(), rect.getHeight());
	
	// <FS:CR> Mode Selector
	LLUICtrl& mode_combo = getChildRef<LLUICtrl>("mode_combo");
	mode_combo.setValue(gSavedSettings.getString("SessionSettingsFile"));
	mode_combo.setCommitCallback(boost::bind(&LLPanelLogin::onModeChange, this, getChild<LLUICtrl>("mode_combo")->getValue(), _2));
	// </FS:CR>

	LLLineEditor* password_edit(getChild<LLLineEditor>("password_edit"));
	password_edit->setKeystrokeCallback(onPassKey, this);
	// STEAM-14: When user presses Enter with this field in focus, initiate login
	//password_edit->setCommitCallback(boost::bind(&LLPanelLogin::onClickConnect, this)); // <FS:LO> Not needed because of the global fix below

	// change z sort of clickable text to be behind buttons
	sendChildToBack(getChildView("forgot_password_text"));

	LLComboBox* location_combo = getChild<LLComboBox>("start_location_combo");
	updateLocationSelectorsVisibility(); // separate so that it can be called from preferences
	location_combo->setFocusLostCallback(boost::bind(&LLPanelLogin::onLocationSLURL, this));
	
	LLComboBox* server_choice_combo = getChild<LLComboBox>("server_combo");
	server_choice_combo->setCommitCallback(boost::bind(&LLPanelLogin::onSelectServer, this));

// <FS:CR>
	// Load all of the grids, sorted, and then add a bar and the current grid at the top
	//server_choice_combo->removeall();

	//std::string current_grid = LLGridManager::getInstance()->getGrid();
	//std::map<std::string, std::string> known_grids = LLGridManager::getInstance()->getKnownGrids();
	//for (std::map<std::string, std::string>::iterator grid_choice = known_grids.begin();
	//	 grid_choice != known_grids.end();
	//	 grid_choice++)
	//{
	//	if (!grid_choice->first.empty() && current_grid != grid_choice->first)
	//	{
	//		LL_DEBUGS("AppInit")<<"adding "<<grid_choice->first<<LL_ENDL;
	//		server_choice_combo->add(grid_choice->second, grid_choice->first);
	//	}
	//}
	//server_choice_combo->sortByName();
	//server_choice_combo->addSeparator(ADD_TOP);
	//LL_DEBUGS("AppInit")<<"adding current "<<current_grid<<LL_ENDL;
	//server_choice_combo->add(LLGridManager::getInstance()->getGridLabel(), 
	//						 current_grid,
	//						 ADD_TOP);	
	//server_choice_combo->selectFirstItem();
	updateServer();
	if(LLStartUp::getStartSLURL().getType() != LLSLURL::LOCATION)
	{
		LLSLURL slurl(gSavedSettings.getString("LoginLocation"));
		LLStartUp::setStartSLURL(slurl);
	}
// </FS:CR>
	
// <FS:CR> Moved this down further
	//LLSLURL start_slurl(LLStartUp::getStartSLURL());
	//if ( !start_slurl.isSpatial() ) // has a start been established by the command line or NextLoginLocation ?
	//{
		// no, so get the preference setting
	//	std::string defaultStartLocation = gSavedSettings.getString("LoginLocation");
	//	LL_INFOS("AppInit") << "default LoginLocation '" << defaultStartLocation << "'" << LL_ENDL;
	//	LLSLURL defaultStart(defaultStartLocation);
	//	if ( defaultStart.isSpatial() )
	//	{
	//		LLStartUp::setStartSLURL(defaultStart);
	//	}
	//	else
	//	{
	//		LL_INFOS("AppInit")<<"no valid LoginLocation, using home"<<LL_ENDL;
	//		LLSLURL homeStart(LLSLURL::SIM_LOCATION_HOME);
	//		LLStartUp::setStartSLURL(homeStart);
	//	}
	//}
	//else
	//{
	//	LLPanelLogin::onUpdateStartSLURL(start_slurl); // updates grid if needed
	//}
// </FS:CR>

	childSetAction("remove_user_btn", onClickRemove, this); // <FS:CR> Remove credentials
	childSetAction("connect_btn", onClickConnect, this);
	
	getChild<LLPanel>("login")->setDefaultBtn(findChild<LLButton>("connect_btn")); // <FS:LO> manualy find the button with findChild() as setDefaultButton() uses getChild(), which cant be used in a ctor as it makes a dummy instead
	getChild<LLPanel>("start_location_panel")->setDefaultBtn(findChild<LLButton>("connect_btn")); // <FS:CR> Yeah, do that here too.

	std::string channel = LLVersionInfo::getChannel();
	std::string version = llformat("%s (%d)",
								   LLVersionInfo::getShortVersion().c_str(),
								   LLVersionInfo::getBuild());
	
	LLTextBox* forgot_password_text = getChild<LLTextBox>("forgot_password_text");
	forgot_password_text->setClickedCallback(onClickForgotPassword, NULL);

	LLTextBox* create_new_account_text = getChild<LLTextBox>("create_new_account_text");
	create_new_account_text->setClickedCallback(onClickNewAccount, NULL);

	// <FS:Ansariel> We don't have the help link
	//LLTextBox* need_help_text = getChild<LLTextBox>("login_help");
	//need_help_text->setClickedCallback(onClickHelp, NULL);
	// </FS:Ansariel>
	
// <FS:CR> Grid Manager Help link
	LLTextBox* grid_mgr_help_text = getChild<LLTextBox>("grid_login_text");
	grid_mgr_help_text->setClickedCallback(onClickGridMgrHelp, NULL);
// </FS:CR>
	
	// get the web browser control
	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>("login_html");
	web_browser->addObserver(this);

	reshapeBrowser();

	// </FS:CR> Moved below
	//loadLoginPage();

	// Show last logged in user favorites in "Start at" combo.
/// <FS:CR> We don't use addUsersWithFavoritesToUsername() in Firestorm. We use addUsersToCombo() when setting
/// visibility.
	//addUsersWithFavoritesToUsername();
// </FS:CR>
	LLComboBox* username_combo(getChild<LLComboBox>("username_combo"));
	//username_combo->setTextChangedCallback(boost::bind(&LLPanelLogin::addFavoritesToStartLocation, this));
// <FS:CR> Don't automatically connect on selection!
	//username_combo->setCommitCallback(boost::bind(&LLPanelLogin::onClickConnect, this));
	username_combo->setCommitCallback(boost::bind(&LLPanelLogin::onSelectUser, this));

	LLSLURL start_slurl(LLStartUp::getStartSLURL());
	if ( !start_slurl.isSpatial() ) // has a start been established by the command line or NextLoginLocation ?
	{
	// no, so get the preference setting
		std::string defaultStartLocation = gSavedSettings.getString("LoginLocation");
		LL_INFOS("AppInit") << "default LoginLocation '" << defaultStartLocation << "'" << LL_ENDL;
		LLSLURL defaultStart(defaultStartLocation);
		if ( defaultStart.isSpatial() )
		{
			LLStartUp::setStartSLURL(defaultStart);
		}
		else
		{
			LL_INFOS("AppInit")<<"no valid LoginLocation, using home"<<LL_ENDL;
			LLSLURL homeStart(LLSLURL::SIM_LOCATION_HOME);
			LLStartUp::setStartSLURL(homeStart);
		}
	}
	else
	{
		LLPanelLogin::onUpdateStartSLURL(start_slurl); // updates grid if needed
	}
	
	loadLoginPage();
// </FS:CR>
}

// <FS:CR> We don't use addUsersWithFavoritesToUsername() in Firestorm. We use addUsersToCombo().
#if 0
void LLPanelLogin::addUsersWithFavoritesToUsername()
{
	LLComboBox* combo = getChild<LLComboBox>("username_combo");
	if (!combo) return;
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "stored_favorites.xml");
	LLSD fav_llsd;
	llifstream file;
	file.open(filename);
	if (!file.is_open()) return;
	LLSDSerialize::fromXML(fav_llsd, file);
	for (LLSD::map_const_iterator iter = fav_llsd.beginMap();
		iter != fav_llsd.endMap(); ++iter)
	{
		combo->add(iter->first);
	}
}
#endif
// </FS:CR>

void LLPanelLogin::addFavoritesToStartLocation()
{
	// Clear the combo.
	LLComboBox* combo = getChild<LLComboBox>("start_location_combo");
	if (!combo) return;
	int num_items = combo->getItemCount();
	for (int i = num_items - 1; i > 2; i--)
	{
		combo->remove(i);
	}

	// Load favorites into the combo.
	std::string user_defined_name = getChild<LLComboBox>("username_combo")->getSimple();
// <FS:CR> FIRE-10122 - User@grid stored_favorites.xml
	//std::string canonical_user_name = canonicalize_username(user_defined_name);
	std::string current_grid = getChild<LLComboBox>("server_combo")->getSimple();
	std::string current_user = canonicalize_username(user_defined_name) + " @ " + current_grid;
// </FS:CR>
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "stored_favorites.xml");
	LLSD fav_llsd;
	llifstream file;
	file.open(filename);
	if (!file.is_open()) return;
	LLSDSerialize::fromXML(fav_llsd, file);
	for (LLSD::map_const_iterator iter = fav_llsd.beginMap();
		iter != fav_llsd.endMap(); ++iter)
	{
		// The account name in stored_favorites.xml has Resident last name even if user has
		// a single word account name, so it can be compared case-insensitive with the
		// user defined "firstname lastname".
// <FS:CR> FIRE-10122 - User@grid stored_favorites.xml
		//S32 res = LLStringUtil::compareInsensitive(canonical_user_name, iter->first);
		S32 res = LLStringUtil::compareInsensitive(current_user, iter->first);
// </FS:CR>
		if (res != 0)
		{
			lldebugs << "Skipping favorites for " << iter->first << llendl;
			continue;
		}

		combo->addSeparator();
		lldebugs << "Loading favorites for " << iter->first << llendl;
		LLSD user_llsd = iter->second;
		for (LLSD::array_const_iterator iter1 = user_llsd.beginArray();
			iter1 != user_llsd.endArray(); ++iter1)
		{
			std::string label = (*iter1)["name"].asString();
			std::string value = (*iter1)["slurl"].asString();
			if(label != "" && value != "")
			{
				combo->add(label, value);
			}
		}
		break;
	}
}

// force the size to be correct (XML doesn't seem to be sufficient to do this)
// (with some padding so the other login screen doesn't show through)
void LLPanelLogin::reshapeBrowser()
{
	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>("login_html");
	LLRect rect = gViewerWindow->getWindowRectScaled();
	LLRect html_rect;
	html_rect.setCenterAndSize(
		rect.getCenterX() - 2, rect.getCenterY() + 40,
		rect.getWidth() + 6, rect.getHeight() - 78 );
	web_browser->setRect( html_rect );
	web_browser->reshape( html_rect.getWidth(), html_rect.getHeight(), TRUE );
	reshape( rect.getWidth(), rect.getHeight(), 1 );
}

LLPanelLogin::~LLPanelLogin()
{
	LLPanelLogin::sInstance = NULL;

	// Controls having keyboard focus by default
	// must reset it on destroy. (EXT-2748)
	gFocusMgr.setDefaultKeyboardFocus(NULL);
}

// virtual
void LLPanelLogin::draw()
{
	gGL.pushMatrix();
	{
		F32 image_aspect = 1.333333f;
		F32 view_aspect = (F32)getRect().getWidth() / (F32)getRect().getHeight();
		// stretch image to maintain aspect ratio
		if (image_aspect > view_aspect)
		{
			gGL.translatef(-0.5f * (image_aspect / view_aspect - 1.f) * getRect().getWidth(), 0.f, 0.f);
			gGL.scalef(image_aspect / view_aspect, 1.f, 1.f);
		}

		S32 width = getRect().getWidth();
		S32 height = getRect().getHeight();

		if (getChild<LLView>("login_widgets")->getVisible())
		{
			// draw a background box in black
			gl_rect_2d( 0, height - 264, width, 264, LLColor4::black );
			// draw the bottom part of the background image
			// just the blue background to the native client UI
			mLogoImage->draw(0, -264, width + 8, mLogoImage->getHeight());
		};
	}
	gGL.popMatrix();

	LLPanel::draw();
}

// virtual
BOOL LLPanelLogin::handleKeyHere(KEY key, MASK mask)
{
	if ( KEY_F1 == key )
	{
		LLViewerHelp* vhelp = LLViewerHelp::getInstance();
		vhelp->showTopic(vhelp->f1HelpTopic());
		return TRUE;
	}

	return LLPanel::handleKeyHere(key, mask);
}

// virtual 
void LLPanelLogin::setFocus(BOOL b)
{
	if(b != hasFocus())
	{
		if(b)
		{
			LLPanelLogin::giveFocus();
		}
		else
		{
			LLPanel::setFocus(b);
		}
	}
}

// static
void LLPanelLogin::giveFocus()
{
	if( sInstance )
	{
		// Grab focus and move cursor to first blank input field
		std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
		std::string pass = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();

		BOOL have_username = !username.empty();
		BOOL have_pass = !pass.empty();

		LLLineEditor* edit = NULL;
		LLComboBox* combo = NULL;
		if (have_username && !have_pass)
		{
			// User saved his name but not his password.  Move
			// focus to password field.
			edit = sInstance->getChild<LLLineEditor>("password_edit");
		}
		else
		{
			// User doesn't have a name, so start there.
			combo = sInstance->getChild<LLComboBox>("username_combo");
		}

		if (edit)
		{
			edit->setFocus(TRUE);
			edit->selectAll();
		}
		else if (combo)
		{
			combo->setFocus(TRUE);
		}
	}
}

// static
void LLPanelLogin::showLoginWidgets()
{
	if (sInstance)
	{
		// *NOTE: Mani - This may or may not be obselete code.
		// It seems to be part of the defunct? reg-in-client project.
		sInstance->getChildView("login_widgets")->setVisible( true);
		LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
		sInstance->reshapeBrowser();
		// *TODO: Append all the usual login parameters, like first_login=Y etc.
		std::string splash_screen_url = LLGridManager::getInstance()->getLoginPage();
		web_browser->navigateTo( splash_screen_url, "text/html" );
		LLUICtrl* username_combo = sInstance->getChild<LLUICtrl>("username_combo");
		username_combo->setFocus(TRUE);
	}
}

// static
void LLPanelLogin::show(const LLRect &rect,
						void (*callback)(S32 option, void* user_data),
						void* callback_data)
{
	new LLPanelLogin(rect, callback, callback_data);

	if( !gFocusMgr.getKeyboardFocus() )
	{
		// Grab focus and move cursor to first enabled control
		sInstance->setFocus(TRUE);
	}

	// Make sure that focus always goes here (and use the latest sInstance that was just created)
	gFocusMgr.setDefaultKeyboardFocus(sInstance);
}

// static
// <FS:CR>
//void LLPanelLogin::setFields(LLPointer<LLCredential> credential,
//							 BOOL remember)
void LLPanelLogin::setFields(LLPointer<LLCredential> credential)
// </FS:CR>
{
	if (!sInstance)
	{
		llwarns << "Attempted fillFields with no login view shown" << llendl;
		return;
	}
	LL_INFOS("Credentials") << "Setting login fields to " << *credential << LL_ENDL;

	LLSD identifier = credential->getIdentifier();
	if((std::string)identifier["type"] == "agent") 
	{
		std::string firstname = identifier["first_name"].asString();
		std::string lastname = identifier["last_name"].asString();
	    std::string login_id = firstname;
	    if (!lastname.empty() && lastname != "Resident")
	    {
		    // support traditional First Last name SLURLs
		    login_id += " ";
		    login_id += lastname;
	    }
// <FS:CR>
		//sInstance->getChild<LLComboBox>("username_combo")->setLabel(login_id);
	}
	//else if((std::string)identifier["type"] == "account")
	//{
	//	sInstance->getChild<LLComboBox>("username_combo")->setLabel((std::string)identifier["account_name"]);
	//}
	//else
	//{
	//  sInstance->getChild<LLComboBox>("username_combo")->setLabel(std::string());
	//}
	std::string credName = credential->getCredentialName();
	sInstance->getChild<LLComboBox>("username_combo")->selectByValue(credName);
// </FS:CR>
	sInstance->addFavoritesToStartLocation();
	// if the password exists in the credential, set the password field with
	// a filler to get some stars
	LLSD authenticator = credential->getAuthenticator();
	LL_INFOS("Credentials") << "Setting authenticator field " << authenticator["type"].asString() << LL_ENDL;
	bool remember; // <FS:CR>
	if(authenticator.isMap() && 
	   authenticator.has("secret") && 
	   (authenticator["secret"].asString().size() > 0))
	{
		
		// This is a MD5 hex digest of a password.
		// We don't actually use the password input field, 
		// fill it with MAX_PASSWORD characters so we get a 
		// nice row of asterixes.
		const std::string filler("123456789!123456");
		sInstance->getChild<LLUICtrl>("password_edit")->setValue(std::string("123456789!123456"));
		remember = true; // <FS:CR>
	}
	else
	{
		sInstance->getChild<LLUICtrl>("password_edit")->setValue(std::string());		
		remember = false; // <FS:CR>
	}
	sInstance->getChild<LLUICtrl>("remember_check")->setValue(remember);
}


// static
void LLPanelLogin::getFields(LLPointer<LLCredential>& credential,
							 BOOL& remember)
{
	if (!sInstance)
	{
		llwarns << "Attempted getFields with no login view shown" << llendl;
		return;
	}
	
	// load the credential so we can pass back the stored password or hash if the user did
	// not modify the password field.
	
// <FS:CR>
	//credential = gSecAPIHandler->loadCredential(LLGridManager::getInstance()->getGrid());
	credential = gSecAPIHandler->loadCredential(credentialName());
// </FS:CR>

	LLSD identifier = LLSD::emptyMap();
	LLSD authenticator = LLSD::emptyMap();
	
	if(credential.notNull())
	{
		authenticator = credential->getAuthenticator();
	}

	std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
	LLStringUtil::trim(username);
// <FS:CR>
	U32 arobase = username.find("@");
	if(arobase != std::string::npos)
		username = username.substr(0, arobase);
// </FS:CR>
	std::string password = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();

	LL_INFOS2("Credentials", "Authentication") << "retrieving username:" << username << LL_ENDL;
	// determine if the username is a first/last form or not.
	size_t separator_index = username.find_first_of(' ');
	if (separator_index == username.npos
		&& !LLGridManager::getInstance()->isSystemGrid())
	{
		LL_INFOS2("Credentials", "Authentication") << "account: " << username << LL_ENDL;
		// single username, so this is a 'clear' identifier
		identifier["type"] = CRED_IDENTIFIER_TYPE_ACCOUNT;
		identifier["account_name"] = username;
		
		if (LLPanelLogin::sInstance->mPasswordModified)
		{
			authenticator = LLSD::emptyMap();
			// password is plaintext
			authenticator["type"] = CRED_AUTHENTICATOR_TYPE_CLEAR;
			authenticator["secret"] = password;
		}
	}
	else
	{
		// Be lenient in terms of what separators we allow for two-word names
		// and allow legacy users to login with firstname.lastname
		separator_index = username.find_first_of(" ._");
		std::string first = username.substr(0, separator_index);
		std::string last;
		if (separator_index != username.npos)
		{
			last = username.substr(separator_index + 1, username.npos);
		LLStringUtil::trim(last);
		}
		else
		{
			// ...on Linden grids, single username users as considered to have
			// last name "Resident"
			// *TODO: Make login.cgi support "account_name" like above
			last = "Resident";
		}
		
		if (last.find_first_of(' ') == last.npos)
		{
			LL_INFOS2("Credentials", "Authentication") << "agent: " << username << LL_ENDL;
			// traditional firstname / lastname
			identifier["type"] = CRED_IDENTIFIER_TYPE_AGENT;
			identifier["first_name"] = first;
			identifier["last_name"] = last;
		
			if (LLPanelLogin::sInstance->mPasswordModified)
			{
				authenticator = LLSD::emptyMap();
				authenticator["type"] = CRED_AUTHENTICATOR_TYPE_HASH;
				authenticator["algorithm"] = "md5";
				LLMD5 pass((const U8 *)password.c_str());
				char md5pass[33];               /* Flawfinder: ignore */
				pass.hex_digest(md5pass);
				authenticator["secret"] = md5pass;
			}
		}
	}
// <FS:CR>
	//credential = gSecAPIHandler->createCredential(LLGridManager::getInstance()->getGrid(), identifier, authenticator);
	credential = gSecAPIHandler->createCredential(credentialName(), identifier, authenticator);
// </FS:CR>
	remember = sInstance->getChild<LLUICtrl>("remember_check")->getValue();
}


// static
BOOL LLPanelLogin::areCredentialFieldsDirty()
{
	if (!sInstance)
	{
		llwarns << "Attempted getServer with no login view shown" << llendl;
	}
	else
	{
		std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
		LLStringUtil::trim(username);
		std::string password = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();
		LLComboBox* combo = sInstance->getChild<LLComboBox>("username_combo");
		if(combo && combo->isDirty())
		{
			return true;
		}
		LLLineEditor* ctrl = sInstance->getChild<LLLineEditor>("password_edit");
		if(ctrl && ctrl->isDirty()) 
		{
			return true;
		}
	}
	return false;	
}


// static
void LLPanelLogin::updateLocationSelectorsVisibility()
{
	if (sInstance) 
	{
		BOOL show_start = gSavedSettings.getBOOL("ShowStartLocation");
		sInstance->getChild<LLLayoutPanel>("start_location_panel")->setVisible(show_start);

		BOOL show_server = gSavedSettings.getBOOL("ForceShowGrid");
		sInstance->getChild<LLLayoutPanel>("grid_panel")->setVisible(show_server);
// <FS:CR> Refresh the username combo
		sInstance->addUsersToCombo(show_server);
// </FS:CR>
	}	
}

// static - called from LLStartUp::setStartSLURL
void LLPanelLogin::onUpdateStartSLURL(const LLSLURL& new_start_slurl)
{
	if (!sInstance) return;

	LL_DEBUGS("AppInit")<<new_start_slurl.asString()<<LL_ENDL;

	LLComboBox* location_combo = sInstance->getChild<LLComboBox>("start_location_combo");
	/*
	 * Determine whether or not the new_start_slurl modifies the grid.
	 *
	 * Note that some forms that could be in the slurl are grid-agnostic.,
	 * such as "home".  Other forms, such as
	 * https://grid.example.com/region/Party%20Town/20/30/5 
	 * specify a particular grid; in those cases we want to change the grid
	 * and the grid selector to match the new value.
	 */
	enum LLSLURL::SLURL_TYPE new_slurl_type = new_start_slurl.getType();
	switch ( new_slurl_type )
	{
	case LLSLURL::LOCATION:
	  {
		std::string slurl_grid = LLGridManager::getInstance()->getGrid(new_start_slurl.getGrid());
		if ( ! slurl_grid.empty() ) // is that a valid grid?
		{
			if ( slurl_grid != LLGridManager::getInstance()->getGrid() ) // new grid?
			{
				// the slurl changes the grid, so update everything to match
				LLGridManager::getInstance()->setGridChoice(slurl_grid);

				// update the grid selector to match the slurl
				LLComboBox* server_combo = sInstance->getChild<LLComboBox>("server_combo");
				std::string server_label(LLGridManager::getInstance()->getGridLabel(slurl_grid));
				server_combo->setSimple(server_label);

				updateServer(); // to change the links and splash screen
			}
			location_combo->setTextEntry(new_start_slurl.getLocationString());
		}
		else
		{
			// the grid specified by the slurl is not known
			LLNotificationsUtil::add("InvalidLocationSLURL");
			LL_WARNS("AppInit")<<"invalid LoginLocation:"<<new_start_slurl.asString()<<LL_ENDL;
			location_combo->setTextEntry(LLStringUtil::null);
		}
	  }
 	break;

	case LLSLURL::HOME_LOCATION:
		location_combo->setCurrentByIndex(1); // home location
		break;
		
	case LLSLURL::LAST_LOCATION:
		location_combo->setCurrentByIndex(0); // last location
		break;

	default:
		LL_WARNS("AppInit")<<"invalid login slurl, using home"<<LL_ENDL;
		location_combo->setCurrentByIndex(1); // home location
		break;
	}
}

void LLPanelLogin::setLocation(const LLSLURL& slurl)
{
	LL_DEBUGS("AppInit")<<"setting Location "<<slurl.asString()<<LL_ENDL;
	LLStartUp::setStartSLURL(slurl); // calls onUpdateStartSLURL, above
}

// static
void LLPanelLogin::closePanel()
{
	if (sInstance)
	{
		LLPanelLogin::sInstance->getParent()->removeChild( LLPanelLogin::sInstance );

		delete sInstance;
		sInstance = NULL;
	}
}

// static
void LLPanelLogin::setAlwaysRefresh(bool refresh)
{
	if (sInstance && LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
	{
		LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");

		if (web_browser)
		{
			web_browser->setAlwaysRefresh(refresh);
		}
	}
}



void LLPanelLogin::loadLoginPage()
{
	if (!sInstance) return;

	LLURI login_page = LLURI(LLGridManager::getInstance()->getLoginPage());
	LLSD params(login_page.queryMap());

	LL_DEBUGS("AppInit") << "login_page: " << login_page << LL_ENDL;

	// Language
	params["lang"] = LLUI::getLanguage();

	// First Login?
	if (gSavedSettings.getBOOL("FirstLoginThisInstall"))
	{
		params["firstlogin"] = "TRUE"; // not bool: server expects string TRUE
	}

	// Channel and Version
	params["version"] = llformat("%s (%d)",
								 LLVersionInfo::getShortVersion().c_str(),
								 LLVersionInfo::getBuild());
	params["channel"] = LLVersionInfo::getChannel();

	// Grid
	params["grid"] = LLGridManager::getInstance()->getGridId();

	// add OS info
	params["os"] = LLAppViewer::instance()->getOSInfo().getOSStringSimple();

	// sourceid
	params["sourceid"] = gSavedSettings.getString("sourceid");

	// Make an LLURI with this augmented info
	LLURI login_uri(LLURI::buildHTTP(login_page.authority(),
									 login_page.path(),
									 params));

// <FS:CR>
	//gViewerWindow->setMenuBackgroundColor(false, !LLGridManager::getInstance()->isInProductionGrid());
// </FS:CR>

	LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
	if (web_browser->getCurrentNavUrl() != login_uri.asString())
	{
		LL_DEBUGS("AppInit") << "loading:    " << login_uri << LL_ENDL;
		web_browser->navigateTo( login_uri.asString(), "text/html" );
	}
}

void LLPanelLogin::handleMediaEvent(LLPluginClassMedia* /*self*/, EMediaEvent event)
{
}

//---------------------------------------------------------------------------
// Protected methods
//---------------------------------------------------------------------------

// static
void LLPanelLogin::onClickConnect(void *)
{
	if (sInstance && sInstance->mCallback)
	{
		// JC - Make sure the fields all get committed.
		sInstance->setFocus(FALSE);

		LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
		LLSD combo_val = combo->getSelectedValue();

		// the grid definitions may come from a user-supplied grids.xml, so they may not be good
		LL_DEBUGS("AppInit")<<"grid "<<combo_val.asString()<<LL_ENDL;
		try
		{
			LLGridManager::getInstance()->setGridChoice(combo_val.asString());
		}
		catch (LLInvalidGridName ex)
		{
			LLSD args;
			args["GRID"] = ex.name();
			LLNotificationsUtil::add("InvalidGrid", args);
			return;
		}

		// The start location SLURL has already been sent to LLStartUp::setStartSLURL

		std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
		gSavedSettings.setString("UserLoginInfo", credentialName()); // <FS:CR>

// <FS:CR> Block release
		LLSD blocked = FSData::getInstance()->allowed_login();
		if (!blocked.isMap()) //hack for testing for an empty LLSD
		{
// </FS:CR>
			if(username.empty())
			{
				// user must type in something into the username field
// <FS:CR>
				//LLNotificationsUtil::add("MustHaveAccountToLogIn");
				LLSD args;
				args["CURRENT_GRID"] = LLGridManager::getInstance()->getGridLabel();
				LLNotificationsUtil::add("MustHaveAccountToLogIn", args);
// </FS:CR>
			}
			else
			{
				LLPointer<LLCredential> cred;
				BOOL remember;
				getFields(cred, remember);
				std::string identifier_type;
				cred->identifierType(identifier_type);
				LLSD allowed_credential_types;
				LLGridManager::getInstance()->getLoginIdentifierTypes(allowed_credential_types);
			
				// check the typed in credential type against the credential types expected by the server.
				for(LLSD::array_iterator i = allowed_credential_types.beginArray();
					i != allowed_credential_types.endArray();
					i++)
				{
				
					if(i->asString() == identifier_type)
					{
						// yay correct credential type
						sInstance->mCallback(0, sInstance->mCallbackData);
						return;
					}
				}
			
				// Right now, maingrid is the only thing that is picky about
				// credential format, as it doesn't yet allow account (single username)
				// format creds.  - Rox.  James, we wanna fix the message when we change
				// this.
				LLNotificationsUtil::add("InvalidCredentialFormat");
			}
		}
// <FS:CR> Blocked Release
		else
		{
			LLNotificationsUtil::add("BlockLoginInfo", blocked);
		}
// </FS:CR>
	}
}

// static
void LLPanelLogin::onClickNewAccount(void*)
{
	if (sInstance)
	{
// <AW: opensim>
#ifdef OPENSIM
		LLSD grid_info;
		LLGridManager::getInstance()->getGridData(grid_info);

		if (LLGridManager::getInstance()->isInOpenSim() && grid_info.has(GRID_REGISTER_NEW_ACCOUNT))
			LLWeb::loadURLInternal(grid_info[GRID_REGISTER_NEW_ACCOUNT]);
		else
#endif // OPENSIM
// </AW: opensim>
			LLWeb::loadURLExternal(LLTrans::getString("create_account_url"));
	}
}


// static
void LLPanelLogin::onClickVersion(void*)
{
	LLFloaterReg::showInstance("sl_about"); 
}

//static
void LLPanelLogin::onClickForgotPassword(void*)
{
	if (sInstance)
	{
// <AW: opensim>
#ifdef OPENSIM
		LLSD grid_info;
		LLGridManager::getInstance()->getGridData(grid_info);

		if (LLGridManager::getInstance()->isInOpenSim() && grid_info.has(GRID_FORGOT_PASSWORD))
			LLWeb::loadURLInternal(grid_info[GRID_FORGOT_PASSWORD]);
		else
#endif // OPENSIM
// </AW: opensim>
		LLWeb::loadURLExternal(sInstance->getString( "forgot_password_url" ));
	}
}

//static
void LLPanelLogin::onClickHelp(void*)
{
	if (sInstance)
	{
		LLViewerHelp* vhelp = LLViewerHelp::getInstance();
		vhelp->showTopic(vhelp->preLoginTopic());
	}
}

// static
void LLPanelLogin::onPassKey(LLLineEditor* caller, void* user_data)
{
	LLPanelLogin *This = (LLPanelLogin *) user_data;
	This->mPasswordModified = TRUE;
	if (gKeyboard->getKeyDown(KEY_CAPSLOCK) && sCapslockDidNotification == FALSE)
	{
		// *TODO: use another way to notify user about enabled caps lock, see EXT-6858
		sCapslockDidNotification = TRUE;
	}
}


void LLPanelLogin::updateServer()
{
	if (!sInstance)
	{
		return;
	}
	try
	{
		// if they've selected another grid, we should load the credentials
		// for that grid and set them to the UI.
		if(!sInstance->areCredentialFieldsDirty())
		{
// <FS:CR>
			//LLPointer<LLCredential> credential = gSecAPIHandler->loadCredential(LLGridManager::getInstance()->getGrid());
			LLPointer<LLCredential> credential = gSecAPIHandler->loadCredential(credentialName());
			//bool remember = sInstance->getChild<LLUICtrl>("remember_check")->getValue();
			//sInstance->setFields(credential, remember);
			sInstance->setFields(credential);
// </FS:CR>
		}

		// update the login panel links
		// <FS:CR> Unused by Firestorm
		//bool system_grid = LLGridManager::getInstance()->isSystemGrid();
		// </FS:CR>
		
		// Want to vanish not only create_new_account_btn, but also the
		// title text over it, so turn on/off the whole layout_panel element.
		// <FS:CR> or not!
		//sInstance->getChild<LLLayoutPanel>("links")->setVisible(system_grid);
		//sInstance->getChildView("forgot_password_text")->setVisible(system_grid);
		// </FS:CR>

		// grid changed so show new splash screen (possibly)
		updateServerCombo();
		loadLoginPage();
	}
	catch (LLInvalidGridName ex)
	{
		LL_WARNS("AppInit")<<"server '"<<ex.name()<<"' selection failed"<<LL_ENDL;
		LLSD args;
		args["GRID"] = ex.name();
		LLNotificationsUtil::add("InvalidGrid", args);
		return;
	}
}

void LLPanelLogin::onSelectServer()
{
	// The user twiddled with the grid choice ui.
	// apply the selection to the grid setting.
	LLPointer<LLCredential> credential;
	
	LLComboBox* server_combo = getChild<LLComboBox>("server_combo");
	LLSD server_combo_val = server_combo->getSelectedValue();
	LL_INFOS("AppInit") << "grid "<<server_combo_val.asString()<< LL_ENDL;
	LLGridManager::getInstance()->setGridChoice(server_combo_val.asString());
	
	/*
	 * Determine whether or not the value in the start_location_combo makes sense
	 * with the new grid value.
	 *
	 * Note that some forms that could be in the location combo are grid-agnostic,
	 * such as "MyRegion/128/128/0".  There could be regions with that name on any
	 * number of grids, so leave them alone.  Other forms, such as
	 * https://grid.example.com/region/Party%20Town/20/30/5 specify a particular
	 * grid; in those cases we want to clear the location.
	 */
	LLComboBox* location_combo = getChild<LLComboBox>("start_location_combo");
	S32 index = location_combo->getCurrentIndex();
	switch (index)
	{
	case 0: // last location
	case 1: // home location
		// do nothing - these are grid-agnostic locations
		break;
		
	default:
		{
			std::string location = location_combo->getValue().asString();
			LLSLURL slurl(location); // generata a slurl from the location combo contents
			if (   slurl.getType() == LLSLURL::LOCATION
				&& slurl.getGrid() != LLGridManager::getInstance()->getGrid()
				)
			{
				// the grid specified by the location is not this one, so clear the combo
				location_combo->setCurrentByIndex(0); // last location on the new grid
				location_combo->setTextEntry(LLStringUtil::null);
			}
		}			
		break;
	}
	updateServer();
}

void LLPanelLogin::onLocationSLURL()
{
	LLComboBox* location_combo = getChild<LLComboBox>("start_location_combo");
	std::string location = location_combo->getValue().asString();
	LL_DEBUGS("AppInit") << location << LL_ENDL;

	LLStartUp::setStartSLURL(location); // calls onUpdateStartSLURL, above 
}

std::string canonicalize_username(const std::string& name)
{
	std::string cname = name;
	
// <FS:CR> Strip off any grid appendage
	U32 arobase = cname.find("@");
	if(arobase > 0)
		cname = cname.substr(0, arobase - 1);
// </FS:CR>
	
	// determine if the username is a first/last form or not.
	size_t separator_index = cname.find_first_of(" ._");
	std::string first = cname.substr(0, separator_index);
	std::string last;
	if (separator_index != cname.npos)
	{
		last = cname.substr(separator_index + 1, cname.npos);
		LLStringUtil::trim(last);
	}
	else
	{
		// ...on Linden grids, single username users as considered to have
		// last name "Resident"
		last = "Resident";
	}

	// Username in traditional "firstname lastname" form.
	return first + ' ' + last;
}

// <FS:CR>
void LLPanelLogin::addUsersToCombo(BOOL show_server)
{
	LLComboBox* combo = getChild<LLComboBox>("username_combo");
	if (!combo) return;
	
	combo->removeall();
	std::string current_creds=credentialName();
	if(current_creds.find("@") < 1)
	{
		current_creds = gSavedSettings.getString("UserLoginInfo");
	}
	
	std::vector<std::string> logins = gSecAPIHandler->listCredentials();
	LLUUID selectid;
	LLStringUtil::trim(current_creds);
	for (std::vector<std::string>::iterator login_choice = logins.begin();
		 login_choice != logins.end();
		 login_choice++)
	{
		std::string name = *login_choice;
		LLStringUtil::trim(name);
		
		std::string credname = name;
		std::string gridname = name;
		U32 arobase = gridname.find("@");
		if (arobase != std::string::npos && arobase + 1 < gridname.length() && arobase > 1)
		{
			gridname = gridname.substr(arobase + 1, gridname.length() - arobase - 1);
			name = name.substr(0,arobase);
			
			const std::string grid_label = LLGridManager::getInstance()->getGridLabel(gridname);
			
			bool add_grid = false;
			/// We only want to append a grid label when the user has enabled logging into other grids, or
			/// they are using the OpenSim build. That way users who only want Second Life Agni can remain
			/// blissfully ignorant. We will also not show them any saved credential that isn't Agni because
			/// they don't want them.
			if (SECOND_LIFE_MAIN_LABEL == grid_label)
			{
				if (show_server)
					name.append( " @ " + grid_label);
				add_grid = true;
			}
#ifdef OPENSIM
			else if (!grid_label.empty() && show_server)
			{
				name.append(" @ " + grid_label);
				add_grid = true;
			}
#else  // OPENSIM
			else if (SECOND_LIFE_BETA_LABEL == grid_label && show_server)
			{
				name.append(" @ " + grid_label);
				add_grid = true;
			}
#endif // OPENSIM
			if (add_grid)
			{
				combo->add(name,LLSD(credname));
			}
		}
	}
	combo->sortByName();
	combo->selectByValue(LLSD(current_creds));
}

// static
void LLPanelLogin::onClickRemove(void*)
{
	if (sInstance)
	{
		LLComboBox* combo = sInstance->getChild<LLComboBox>("username_combo");
		std::string credName = combo->getValue().asString();
		if ( credName == gSavedSettings.getString("UserLoginInfo") )
			gSavedSettings.getControl("UserLoginInfo")->resetToDefault();
		LLPointer<LLCredential> credential = gSecAPIHandler->loadCredential(credName);
		gSecAPIHandler->deleteCredential(credential);
		sInstance->addUsersToCombo(gSavedSettings.getBOOL("ForceShowGrid"));
		if(!combo->selectFirstItem()){
			sInstance->getChild<LLUICtrl>("username_combo")->clear();
			sInstance->getChild<LLUICtrl>("password_edit")->clear();
		}
	}
}

//static
void LLPanelLogin::onClickGridMgrHelp(void*)
{
	if (sInstance)
	{
		LLViewerHelp* vhelp = LLViewerHelp::getInstance();
		vhelp->showTopic(vhelp->gridMgrHelpTopic());
	}
}

void LLPanelLogin::onSelectUser()
{
	// *NOTE: The paramters for this method are ignored.
	LL_INFOS("AppInit") << "onSelectUser()" << LL_ENDL;
	
	LLComboBox* combo = sInstance->getChild<LLComboBox>("username_combo");
	LLSD combo_val = combo->getSelectedValue();
	if (combo_val.isUndefined())
	{
		combo_val = combo->getValue();
	}
	LLPointer<LLCredential> credential =  gSecAPIHandler->loadCredential(combo_val);
	
	//combo = sInstance->getChild<LLComboBox>("start_location_combo");
	//LLStartUp::setStartSLURL(LLSLURL(gSavedSettings.getString("LoginLocation")));
	
	std::string credName = combo_val.asString();
	
	// if they've selected another grid, we should load the credentials
	// for that grid and set them to the UI.
	if(sInstance && !sInstance->areCredentialFieldsDirty())
	{
		LLPointer<LLCredential> credential = gSecAPIHandler->loadCredential(credName);
		sInstance->setFields(credential);
	}
	U32 arobase = credName.find("@");
	if (arobase != -1 && arobase +1 < credName.length())
		credName = credName.substr(arobase + 1, credName.length() - arobase - 1);
	if(LLGridManager::getInstance()->getGrid() == credName)
	{
		// Even if we didn't change grids, this user might have favorites stored.
		addFavoritesToStartLocation();
		return;
	}
	
	try
	{
		LLGridManager::getInstance()->setGridChoice(credName);
	}
	catch (LLInvalidGridName ex)
	{
		// do nothing
	}
	updateServer();
	addFavoritesToStartLocation();
}

// static
void LLPanelLogin::updateServerCombo()
{
	if (!sInstance) return;
	
#ifdef OPENSIM
	LLGridManager::getInstance()->addGridListChangedCallback(&LLPanelLogin::gridListChanged);
#endif // OPENSIM
	// We add all of the possible values, sorted, and then add a bar and the current value at the top
	LLComboBox* server_choice_combo = sInstance->getChild<LLComboBox>("server_combo");
	server_choice_combo->removeall();

	std::string current_grid = LLGridManager::getInstance()->getGrid();
	std::map<std::string, std::string> known_grids = LLGridManager::getInstance()->getKnownGrids();
	
	for (std::map<std::string, std::string>::iterator grid_choice = known_grids.begin();
		 grid_choice != known_grids.end();
		 grid_choice++)
	{
		if (!grid_choice->first.empty() && current_grid != grid_choice->first)
		{
			LL_DEBUGS("AppInit") << "adding " << grid_choice->first << LL_ENDL;
			server_choice_combo->add(grid_choice->second, grid_choice->first);
		}
	}
	server_choice_combo->sortByName();
	server_choice_combo->addSeparator(ADD_TOP);
	
	LL_DEBUGS("AppInit") << "adding current " << current_grid << LL_ENDL;
	server_choice_combo->add(LLGridManager::getInstance()->getGridLabel(),
							 current_grid,
							 ADD_TOP);
	server_choice_combo->selectFirstItem();
	update_grid_help();
}

// static
std::string LLPanelLogin::credentialName()
{
	std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
	LLStringUtil::trim(username);
	
	U32 arobase = username.find("@");
	if (arobase != std::string::npos && arobase + 1 < username.length())
		username = username.substr(0,arobase);
	LLStringUtil::trim(username);
	
	return username + "@" + LLGridManager::getInstance()->getGrid();
}

// static
void LLPanelLogin::gridListChanged(bool success)
{
	updateServer();
}

/////////////////////////
//    Mode selector    //
/////////////////////////

void LLPanelLogin::onModeChange(const LLSD& original_value, const LLSD& new_value)
{
	// <FS:AO> make sure toolbar settings are reset on mode change
	llinfos << "Clearing toolbar settings." << llendl;
	gSavedSettings.setBOOL("ResetToolbarSettings",TRUE);
	
	if (original_value.asString() != new_value.asString())
	{
		LLNotificationsUtil::add("ModeChange", LLSD(), LLSD(), boost::bind(&LLPanelLogin::onModeChangeConfirm, this, original_value, new_value, _1, _2));
	}
}

void LLPanelLogin::onModeChangeConfirm(const LLSD& original_value, const LLSD& new_value, const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:
			gSavedSettings.getControl("SessionSettingsFile")->set(new_value);
			LLAppViewer::instance()->requestQuit();
			break;
		case 1:
			// revert to original value
			getChild<LLUICtrl>("mode_combo")->setValue(original_value);
			break;
		default:
			break;
	}
}
// </FS:CR>
