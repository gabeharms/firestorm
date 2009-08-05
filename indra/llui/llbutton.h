/** 
 * @file llbutton.h
 * @brief Header for buttons
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLBUTTON_H
#define LL_LLBUTTON_H

#include "lluuid.h"
#include "llcontrol.h"
#include "lluictrl.h"
#include "v4color.h"
#include "llframetimer.h"
#include "llfontgl.h"
#include "lluiimage.h"
#include "lluistring.h"

//
// Constants
//

// PLEASE please use these "constants" when building your own buttons.
// They are loaded from settings.xml at run time.
extern S32	LLBUTTON_H_PAD;
extern S32	LLBUTTON_V_PAD;
extern S32	BTN_HEIGHT_SMALL;
extern S32	BTN_HEIGHT;

//
// Helpful functions
//
S32 round_up(S32 grid, S32 value);


class LLUICtrlFactory;

//
// Classes
//

class LLButton
: public LLUICtrl
{
public:
	struct Params 
	:	public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		// text label
		Optional<std::string>	label_selected;
		Optional<bool>			label_shadow;
		Optional<bool>			auto_resize;

		// images
		Optional<LLUIImage*>	image_unselected,
								image_selected,
								image_hover_selected,
								image_hover_unselected,
								image_disabled_selected,
								image_disabled,
								image_overlay;

		Optional<std::string>	image_overlay_alignment;
		
		// colors
		Optional<LLUIColor>		label_color,
								label_color_selected,
								label_color_disabled,
								label_color_disabled_selected,
								highlight_color,
								image_color,
								image_color_disabled,
								image_overlay_color,
								flash_color;

		// layout
		Optional<S32>			pad_right;
		Optional<S32>			pad_left;
		
		// callbacks
		Optional<CommitCallbackParam>	click_callback, // alias -> commit_callback
										mouse_down_callback,
										mouse_up_callback,
										mouse_held_callback;
		
		// misc
		Optional<bool>			is_toggle,
								scale_image,
								commit_on_return,
								picture_style;      //if true, don't display label
		
		Optional<std::string>		help_url;
		Optional<F32>				hover_glow_amount;
		Optional<TimeIntervalParam>	held_down_delay;

		Params();
	};
	
protected:
	friend class LLUICtrlFactory;
	LLButton(const Params&);

public:
	// For backward compatability only
	typedef boost::function<void(void*)> button_callback_t;

	void			addImageAttributeToXML(LLXMLNodePtr node, const std::string& imageName,
										const LLUUID&	imageID,const std::string&	xmlTagName) const;
	virtual BOOL	handleUnicodeCharHere(llwchar uni_char);
	virtual BOOL	handleKeyHere(KEY key, MASK mask);
	virtual BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL	handleMouseUp(S32 x, S32 y, MASK mask);
	virtual BOOL	handleHover(S32 x, S32 y, MASK mask);
	virtual BOOL	handleRightMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL	handleRightMouseUp(S32 x, S32 y, MASK mask);	
	virtual void	draw();
	/*virtual*/ BOOL postBuild();

	virtual void	onMouseEnter(S32 x, S32 y, MASK mask);
	virtual void	onMouseLeave(S32 x, S32 y, MASK mask);
	virtual void	onMouseCaptureLost();

	virtual void	onCommit();

	void			setUnselectedLabelColor( const LLColor4& c )		{ mUnselectedLabelColor = c; }
	void			setSelectedLabelColor( const LLColor4& c )			{ mSelectedLabelColor = c; }

	boost::signals2::connection setClickedCallback( const commit_signal_t::slot_type& cb ); // mouse down and up within button
	boost::signals2::connection setMouseDownCallback( const commit_signal_t::slot_type& cb );
	boost::signals2::connection setMouseUpCallback( const commit_signal_t::slot_type& cb ); // mouse up, EVEN IF NOT IN BUTTON
	// Passes a 'count' parameter in the commit param payload, i.e. param["count"])
	boost::signals2::connection setHeldDownCallback( const commit_signal_t::slot_type& cb ); // Mouse button held down and in button
	boost::signals2::connection setRightClickedCallback( const commit_signal_t::slot_type& cb ); // right mouse down and up within button


	
	// *TODO: Deprecate (for backwards compatability only)
	boost::signals2::connection setClickedCallback( button_callback_t cb, void* data );
	boost::signals2::connection setMouseDownCallback( button_callback_t cb, void* data );
	boost::signals2::connection setMouseUpCallback( button_callback_t cb, void* data );
	boost::signals2::connection setHeldDownCallback( button_callback_t cb, void* data );
		
	void			setHeldDownDelay( F32 seconds, S32 frames = 0)		{ mHeldDownDelay = seconds; mHeldDownFrameDelay = frames; }
	
	F32				getHeldDownTime() const								{ return mMouseDownTimer.getElapsedTimeF32(); }

	BOOL			toggleState();
	BOOL			getToggleState() const;
	void			setToggleState(BOOL b);

	void			setFlashing( BOOL b );
	BOOL			getFlashing() const		{ return mFlashing; }

	void			setHAlign( LLFontGL::HAlign align )		{ mHAlign = align; }
	LLFontGL::HAlign getHAlign() const						{ return mHAlign; }
	void			setLeftHPad( S32 pad )					{ mLeftHPad = pad; }
	void			setRightHPad( S32 pad )					{ mRightHPad = pad; }

	const std::string	getLabelUnselected() const { return wstring_to_utf8str(mUnselectedLabel); }
	const std::string	getLabelSelected() const { return wstring_to_utf8str(mSelectedLabel); }

	void			setImageColor(const std::string& color_control);
	void			setImageColor(const LLColor4& c);
	virtual void	setColor(const LLColor4& c);

	void			setImages(const std::string &image_name, const std::string &selected_name);
	void			setDisabledImages(const std::string &image_name, const std::string &selected_name);
	void			setDisabledImages(const std::string &image_name, const std::string &selected_name, const LLColor4& c);
	
	void			setHoverImages(const std::string &image_name, const std::string &selected_name);

	void			setDisabledImageColor(const LLColor4& c)		{ mDisabledImageColor = c; }

	void			setDisabledSelectedLabelColor( const LLColor4& c )	{ mDisabledSelectedLabelColor = c; }

	void			setImageOverlay(const std::string& image_name, LLFontGL::HAlign alignment = LLFontGL::HCENTER, const LLColor4& color = LLColor4::white);
	LLPointer<LLUIImage> getImageOverlay() { return mImageOverlay; }

	void            autoResize();	// resize with label of current btn state 
	void            resize(LLUIString label); // resize with label input
	void			setLabel( const LLStringExplicit& label);
	virtual BOOL	setLabelArg( const std::string& key, const LLStringExplicit& text );
	void			setLabelUnselected(const LLStringExplicit& label);
	void			setLabelSelected(const LLStringExplicit& label);
	void			setDisabledLabel(const LLStringExplicit& disabled_label);
	void			setDisabledSelectedLabel(const LLStringExplicit& disabled_label);
	void			setDisabledLabelColor( const LLColor4& c )		{ mDisabledLabelColor = c; }
	
	void			setFont(const LLFontGL *font)		
		{ mGLFont = ( font ? font : LLFontGL::getFontSansSerif()); }
	void			setScaleImage(BOOL scale)			{ mScaleImage = scale; }
	BOOL			getScaleImage() const				{ return mScaleImage; }

	void			setDropShadowedText(BOOL b)			{ mDropShadowedText = b; }

	void			setBorderEnabled(BOOL b)					{ mBorderEnabled = b; }

	void			setHoverGlowStrength(F32 strength) { mHoverGlowStrength = strength; }

	void			setImageUnselected(const std::string &image_name);
	const std::string& getImageUnselectedName() const { return mImageUnselectedName; }
	void			setImageSelected(const std::string &image_name);
	const std::string& getImageSelectedName() const { return mImageSelectedName; }
	void			setImageHoverSelected(const std::string &image_name);
	void			setImageHoverUnselected(const std::string &image_name);
	void			setImageDisabled(const std::string &image_name);
	void			setImageDisabledSelected(const std::string &image_name);

	void			setImageUnselected(LLPointer<LLUIImage> image);
	void			setImageSelected(LLPointer<LLUIImage> image);
	void			setImageHoverSelected(LLPointer<LLUIImage> image);
	void			setImageHoverUnselected(LLPointer<LLUIImage> image);
	void			setImageDisabled(LLPointer<LLUIImage> image);
	void			setImageDisabledSelected(LLPointer<LLUIImage> image);

	void			setCommitOnReturn(BOOL commit) { mCommitOnReturn = commit; }
	BOOL			getCommitOnReturn() const { return mCommitOnReturn; }

	void			setHelpURLCallback(const std::string &help_url);
	const std::string&	getHelpURL() const { return mHelpURL; }

	static void		onHeldDown(void *userdata);  // to be called by gIdleCallbacks
	static void		toggleFloaterAndSetToggleState(LLUICtrl* ctrl, const LLSD& sdname);
	static void		setFloaterToggle(LLUICtrl* ctrl, const LLSD& sdname);
	
protected:

	virtual void	drawBorder(const LLColor4& color, S32 size);

	void			setImageUnselectedID(const LLUUID &image_id);
	const LLUUID&	getImageUnselectedID() const { return mImageUnselectedID; }
	void			setImageSelectedID(const LLUUID &image_id);
	const LLUUID&	getImageSelectedID() const { return mImageSelectedID; }
	void			setImageHoverSelectedID(const LLUUID &image_id);
	void			setImageHoverUnselectedID(const LLUUID &image_id);
	void			setImageDisabledID(const LLUUID &image_id);
	void			setImageDisabledSelectedID(const LLUUID &image_id);
	const LLPointer<LLUIImage>&	getImageUnselected() const	{ return mImageUnselected; }
	const LLPointer<LLUIImage>& getImageSelected() const	{ return mImageSelected; }
	void			resetMouseDownTimer();

	LLFrameTimer	mMouseDownTimer;

	// If the label is empty, set the picture_style attribute
	static void setupParamsForExport(Params& p, LLView* parent);

private:
	commit_signal_t mMouseDownSignal;
	commit_signal_t mMouseUpSignal;
	commit_signal_t mHeldDownSignal;
	
	const LLFontGL	*mGLFont;
	
	S32				mMouseDownFrame;
	S32 			mMouseHeldDownCount; 	// Counter for parameter passed to held-down callback
	F32				mHeldDownDelay;			// seconds, after which held-down callbacks get called
	S32				mHeldDownFrameDelay;	// frames, after which held-down callbacks get called

	LLPointer<LLUIImage>		mImageOverlay;
	LLFontGL::HAlign			mImageOverlayAlignment;
	LLUIColor	mImageOverlayColor;

	LLPointer<LLUIImage>		mImageUnselected;
	LLUIString					mUnselectedLabel;
	LLUIColor	mUnselectedLabelColor;

	LLPointer<LLUIImage>		mImageSelected;
	LLUIString					mSelectedLabel;
	LLUIColor	mSelectedLabelColor;

	LLPointer<LLUIImage>		mImageHoverSelected;

	LLPointer<LLUIImage>		mImageHoverUnselected;

	LLPointer<LLUIImage>		mImageDisabled;
	LLUIString					mDisabledLabel;
	LLUIColor	mDisabledLabelColor;

	LLPointer<LLUIImage>		mImageDisabledSelected;
	LLUIString					mDisabledSelectedLabel;
	LLUIColor	mDisabledSelectedLabelColor;

	LLUUID			mImageUnselectedID;
	LLUUID			mImageSelectedID;
	LLUUID			mImageHoverSelectedID;
	LLUUID			mImageHoverUnselectedID;
	LLUUID			mImageDisabledID;
	LLUUID			mImageDisabledSelectedID;
	std::string		mImageUnselectedName;
	std::string		mImageSelectedName;
	std::string		mImageHoverSelectedName;
	std::string		mImageHoverUnselectedName;
	std::string		mImageDisabledName;
	std::string		mImageDisabledSelectedName;

	LLUIColor	mHighlightColor;
	LLUIColor		mFlashBgColor;

	LLUIColor	mImageColor;
	LLUIColor	mDisabledImageColor;

	BOOL			mIsToggle;
	BOOL			mScaleImage;

	BOOL			mDropShadowedText;
	BOOL			mAutoResize;
	BOOL			mBorderEnabled;

	BOOL			mFlashing;

	LLFontGL::HAlign mHAlign;
	S32				mLeftHPad;
	S32				mRightHPad;

	F32				mHoverGlowStrength;
	F32				mCurGlowStrength;

	BOOL			mNeedsHighlight;
	BOOL			mCommitOnReturn;
	BOOL			mFadeWhenDisabled;

	std::string		mHelpURL;

	LLPointer<LLUIImage> mImagep;

	LLFrameTimer	mFlashingTimer;
};


#endif  // LL_LLBUTTON_H
