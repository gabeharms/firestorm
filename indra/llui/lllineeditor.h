/** 
 * @file lllineeditor.h
 * @brief Text editor widget to let users enter/edit a single line.
 *
 * Features: 
 *		Text entry of a single line (text, delete, left and right arrow, insert, return).
 *		Callbacks either on every keystroke or just on the return key.
 *		Focus (allow multiple text entry widgets)
 *		Clipboard (cut, copy, and paste)
 *		Horizontal scrolling to allow strings longer than widget size allows 
 *		Pre-validation (limit which keys can be used)
 *		Optional line history so previous entries can be recalled by CTRL UP/DOWN
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

#ifndef LL_LLLINEEDITOR_H
#define LL_LLLINEEDITOR_H

#include "v4color.h"
#include "llframetimer.h"

#include "lleditmenuhandler.h"
#include "lluictrl.h"
#include "lluistring.h"
#include "llviewborder.h"

#include "llpreeditor.h"
#include <boost/function.hpp>

class LLFontGL;
class LLLineEditorRollback;
class LLButton;

typedef boost::function<BOOL (const LLWString &wstr)> LLLinePrevalidateFunc;

class LLLineEditor
: public LLUICtrl, public LLEditMenuHandler, protected LLPreeditor
{
public:

	struct PrevalidateNamedFuncs
	:	public LLInitParam::TypeValuesHelper<LLLinePrevalidateFunc, PrevalidateNamedFuncs>

	{
		static void declareValues();
	};
	
	typedef boost::function<void (LLLineEditor* caller)> keystroke_callback_t;
	
	struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		Optional<std::string>			default_text;
		Optional<S32>					max_length_bytes;

		Optional<keystroke_callback_t>	keystroke_callback;

		Optional<LLLinePrevalidateFunc, PrevalidateNamedFuncs>	prevalidate_callback;
		
		Optional<LLViewBorder::Params>	border;

		Optional<LLUIImage*>			background_image,
										background_image_disabled,
										background_image_focused;

		Optional<bool>					select_on_focus,
										handle_edit_keys_directly,
										revert_on_esc,
										commit_on_focus_lost,
										ignore_tab;

		// colors
		Optional<LLUIColor>				cursor_color,
										text_color,
										text_readonly_color,
										text_tentative_color,
										highlight_color,
										preedit_bg_color;
		
		Optional<S32>					text_pad_left,
										text_pad_right;

		Ignored							bg_visible;
		
		Params();
	};
protected:
	LLLineEditor(const Params&);
	friend class LLUICtrlFactory;
	friend class LLFloaterEditUI;
public:
	virtual ~LLLineEditor();

	// mousehandler overrides
	/*virtual*/ BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleMouseUp(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleHover(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleDoubleClick(S32 x,S32 y,MASK mask);
	/*virtual*/ BOOL	handleMiddleMouseDown(S32 x,S32 y,MASK mask);
	/*virtual*/ BOOL	handleKeyHere(KEY key, MASK mask );
	/*virtual*/ BOOL	handleUnicodeCharHere(llwchar uni_char);
	/*virtual*/ void	onMouseCaptureLost();

	// LLEditMenuHandler overrides
	virtual void	cut();
	virtual BOOL	canCut() const;
	virtual void	copy();
	virtual BOOL	canCopy() const;
	virtual void	paste();
	virtual BOOL	canPaste() const;

	virtual void	updatePrimary();
	virtual void	copyPrimary();
 	virtual void	pastePrimary();
	virtual BOOL	canPastePrimary() const;

	virtual void	doDelete();
	virtual BOOL	canDoDelete() const;

	virtual void	selectAll();
	virtual BOOL	canSelectAll() const;

	virtual void	deselect();
	virtual BOOL	canDeselect() const;

	// view overrides
	virtual void	draw();
	virtual void	reshape(S32 width,S32 height,BOOL called_from_parent=TRUE);
	virtual void	onFocusReceived();
	virtual void	onFocusLost();
	virtual void	setEnabled(BOOL enabled);

	// UI control overrides
	virtual void	clear();
	virtual void	onTabInto();
	virtual void	setFocus( BOOL b );
	virtual void 	setRect(const LLRect& rect);
	virtual BOOL	acceptsTextInput() const;
	virtual void	onCommit();
	virtual BOOL	isDirty() const;	// Returns TRUE if user changed value at all
	virtual void	resetDirty();		// Clear dirty state

	// assumes UTF8 text
	virtual void	setValue(const LLSD& value );
	virtual LLSD	getValue() const;
	virtual BOOL	setTextArg( const std::string& key, const LLStringExplicit& text );
	virtual BOOL	setLabelArg( const std::string& key, const LLStringExplicit& text );

	void			setLabel(const LLStringExplicit &new_label) { mLabel = new_label; }
	const std::string& 	getLabel()	{ return mLabel.getString(); }

	void			setText(const LLStringExplicit &new_text);

	const std::string& getText() const		{ return mText.getString(); }
	LLWString       getWText() const	{ return mText.getWString(); }
	LLWString getConvertedText() const; // trimmed text with paragraphs converted to newlines

	S32				getLength() const	{ return mText.length(); }

	S32				getCursor()	const	{ return mCursorPos; }
	void			setCursor( S32 pos );
	void			setCursorToEnd();

	// Selects characters 'start' to 'end'.
	void			setSelection(S32 start, S32 end);
	virtual void	getSelectionRange(S32 *position, S32 *length) const;
	
	void			setCommitOnFocusLost( BOOL b )	{ mCommitOnFocusLost = b; }
	void			setRevertOnEsc( BOOL b )		{ mRevertOnEsc = b; }

	void setCursorColor(const LLColor4& c)			{ mCursorColor = c; }
	const LLColor4& getCursorColor() const			{ return mCursorColor.get(); }

	void setFgColor( const LLColor4& c )			{ mFgColor = c; }
	void setReadOnlyFgColor( const LLColor4& c )	{ mReadOnlyFgColor = c; }
	void setTentativeFgColor(const LLColor4& c)		{ mTentativeFgColor = c; }

	const LLColor4& getFgColor() const			{ return mFgColor.get(); }
	const LLColor4& getReadOnlyFgColor() const	{ return mReadOnlyFgColor.get(); }
	const LLColor4& getTentativeFgColor() const { return mTentativeFgColor.get(); }

	void			setIgnoreArrowKeys(BOOL b)		{ mIgnoreArrowKeys = b; }
	void			setIgnoreTab(BOOL b)			{ mIgnoreTab = b; }
	void			setPassDelete(BOOL b)			{ mPassDelete = b; }
	void			setDrawAsterixes(BOOL b);

	// get the cursor position of the beginning/end of the prev/next word in the text
	S32				prevWordPos(S32 cursorPos) const;
	S32				nextWordPos(S32 cursorPos) const;

	BOOL			hasSelection() const { return (mSelectionStart != mSelectionEnd); }
	void			startSelection();
	void			endSelection();
	void			extendSelection(S32 new_cursor_pos);
	void			deleteSelection();

	void			setHandleEditKeysDirectly( BOOL b ) { mHandleEditKeysDirectly = b; }
	void			setSelectAllonFocusReceived(BOOL b);
	
	typedef boost::function<void (LLLineEditor* caller, void* user_data)> callback_t;
	void			setKeystrokeCallback(callback_t callback, void* user_data);

	void			setMaxTextLength(S32 max_text_length);
	// Manipulate left and right padding for text
	void getTextPadding(S32 *left, S32 *right);
	void setTextPadding(S32 left, S32 right);

	// Prevalidation controls which keystrokes can affect the editor
	void			setPrevalidate( LLLinePrevalidateFunc func );
	static BOOL		prevalidateFloat(const LLWString &str );
	static BOOL		prevalidateInt(const LLWString &str );
	static BOOL		prevalidatePositiveS32(const LLWString &str);
	static BOOL		prevalidateNonNegativeS32(const LLWString &str);
	static BOOL		prevalidateAlphaNum(const LLWString &str );
	static BOOL		prevalidateAlphaNumSpace(const LLWString &str );
	static BOOL		prevalidateASCIIPrintableNoPipe(const LLWString &str); 
	static BOOL		prevalidateASCIIPrintableNoSpace(const LLWString &str);
	static BOOL		prevalidateASCII(const LLWString &str);

	static BOOL		postvalidateFloat(const std::string &str);

	// line history support:
	void			setEnableLineHistory( BOOL enabled ) { mHaveHistory = enabled; } // switches line history on or off 
	void			updateHistory(); // stores current line in history

	void			setReplaceNewlinesWithSpaces(BOOL replace);
	
private:
	// private helper methods

	void                    pasteHelper(bool is_primary);

	void			removeChar();
	void			addChar(const llwchar c);
	void			setCursorAtLocalPos(S32 local_mouse_x);
	S32				findPixelNearestPos(S32 cursor_offset = 0) const;
	void			reportBadKeystroke();
	BOOL			handleSpecialKey(KEY key, MASK mask);
	BOOL			handleSelectionKey(KEY key, MASK mask);
	BOOL			handleControlKey(KEY key, MASK mask);
	S32				handleCommitKey(KEY key, MASK mask);
	void			updateTextPadding();
	
	// Draw the background image depending on enabled/focused state.
	void			drawBackground();

	//
	// private data members
	//
	void			updateAllowingLanguageInput();
	BOOL			hasPreeditString() const;
	// Implementation (overrides) of LLPreeditor
	virtual void	resetPreedit();
	virtual void	updatePreedit(const LLWString &preedit_string,
						const segment_lengths_t &preedit_segment_lengths, const standouts_t &preedit_standouts, S32 caret_position);
	virtual void	markAsPreedit(S32 position, S32 length);
	virtual void	getPreeditRange(S32 *position, S32 *length) const;
	virtual BOOL	getPreeditLocation(S32 query_position, LLCoordGL *coord, LLRect *bounds, LLRect *control) const;
	virtual S32		getPreeditFontSize() const;
	virtual LLWString getPreeditString() const { return getWText(); }

protected:
	LLUIString		mText;					// The string being edited.
	std::string		mPrevText;				// Saved string for 'ESC' revert
	LLUIString		mLabel;					// text label that is visible when no user text provided

	// line history support:
	BOOL		mHaveHistory;				// flag for enabled line history
	typedef std::vector<std::string>	line_history_t;
	line_history_t	mLineHistory;			// line history storage
	line_history_t::iterator	mCurrentHistoryLine;	// currently browsed history line

	LLViewBorder* mBorder;
	const LLFontGL*	mGLFont;
	S32			mMaxLengthBytes;			// Max length of the UTF8 string in bytes
	S32			mCursorPos;					// I-beam is just after the mCursorPos-th character.
	S32			mScrollHPos;				// Horizontal offset from the start of mText.  Used for scrolling.
	LLFrameTimer mScrollTimer;
	S32			mTextPadLeft;				// Used to reserve space before the beginning of the text for children.
	S32			mTextPadRight;				// Used to reserve space after the end of the text for children.
	S32			mMinHPixels;
	S32			mMaxHPixels;

	BOOL		mCommitOnFocusLost;
	BOOL		mRevertOnEsc;

	keystroke_callback_t mKeystrokeCallback;

	BOOL		mIsSelecting;				// Selection for clipboard operations
	S32			mSelectionStart;
	S32			mSelectionEnd;
	S32			mLastSelectionX;
	S32			mLastSelectionY;
	S32			mLastSelectionStart;
	S32			mLastSelectionEnd;

	LLLinePrevalidateFunc mPrevalidateFunc;

	LLFrameTimer mKeystrokeTimer;
	LLTimer		mTripleClickTimer;

	LLUIColor	mCursorColor;
	LLUIColor	mFgColor;
	LLUIColor	mReadOnlyFgColor;
	LLUIColor	mTentativeFgColor;
	LLUIColor	mHighlightColor;		// background for selected text
	LLUIColor	mPreeditBgColor;		// preedit marker background color

	S32			mBorderThickness;

	BOOL		mIgnoreArrowKeys;
	BOOL		mIgnoreTab;
	BOOL		mDrawAsterixes;

	BOOL		mHandleEditKeysDirectly;  // If true, the standard edit keys (Ctrl-X, Delete, etc,) are handled here instead of routed by the menu system
	BOOL		mSelectAllonFocusReceived;
	BOOL		mPassDelete;

	BOOL		mReadOnly;

	LLWString	mPreeditWString;
	LLWString	mPreeditOverwrittenWString;
	std::vector<S32> mPreeditPositions;
	LLPreeditor::standouts_t mPreeditStandouts;

private:
	// Instances that by default point to the statics but can be overidden in XML.
	LLPointer<LLUIImage> mBgImage;
	LLPointer<LLUIImage> mBgImageDisabled;
	LLPointer<LLUIImage> mBgImageFocused;

	BOOL        mReplaceNewlinesWithSpaces; // if false, will replace pasted newlines with paragraph symbol.

	// private helper class
	class LLLineEditorRollback
	{
	public:
		LLLineEditorRollback( LLLineEditor* ed )
			:
			mCursorPos( ed->mCursorPos ),
			mScrollHPos( ed->mScrollHPos ),
			mIsSelecting( ed->mIsSelecting ),
			mSelectionStart( ed->mSelectionStart ),
			mSelectionEnd( ed->mSelectionEnd )
		{
			mText = ed->getText();
		}

		void doRollback( LLLineEditor* ed )
		{
			ed->mCursorPos = mCursorPos;
			ed->mScrollHPos = mScrollHPos;
			ed->mIsSelecting = mIsSelecting;
			ed->mSelectionStart = mSelectionStart;
			ed->mSelectionEnd = mSelectionEnd;
			ed->mText = mText;
			ed->mPrevText = mText;
		}

		std::string getText()   { return mText; }

	private:
		std::string mText;
		S32		mCursorPos;
		S32		mScrollHPos;
		BOOL	mIsSelecting;
		S32		mSelectionStart;
		S32		mSelectionEnd;
	}; // end class LLLineEditorRollback

}; // end class LLLineEditor


#endif  // LL_LINEEDITOR_
