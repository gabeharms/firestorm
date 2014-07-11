/** 
 * @file llconsole.h
 * @brief a simple console-style output device
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLCONSOLE_H
#define LL_LLCONSOLE_H

#include "llfixedbuffer.h"
#include "lluictrl.h"
#include "v4color.h"
#include <deque>

class LLSD;

class LLConsole : public LLFixedBuffer, public LLUICtrl, public LLInstanceTracker<LLConsole>
{
public:

	typedef enum e_font_size
	{
		MONOSPACE = -1,
		SMALL = 0,
		BIG = 1
	} EFontSize;

	struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		Optional<U32>	max_lines;
		Optional<F32>	persist_time;
		Optional<S32>	font_size_index;
		Optional<bool>	parse_urls; // <FS:Ansariel> If lines should be parsed for URLs
		Optional<std::string> background_image; // <FS:Ansariel> Configurable background for different console types

		Params()
		:	max_lines("max_lines", LLUI::sSettingGroups["config"]->getS32("ConsoleMaxLines")),
			persist_time("persist_time", 0.f), // forever
			font_size_index("font_size_index"),
			parse_urls("parse_urls", false), // <FS:Ansariel> If lines should be parsed for URLs
			background_image("background_image", "Console_Background") // <FS:Ansariel> Configurable background for different console types
		{
			changeDefault(mouse_opaque, false);
		}
	};
protected:
	LLConsole(const Params&);
	friend class LLUICtrlFactory;

public:
	// call once per frame to pull data out of LLFixedBuffer
	static void updateClass();

	//A paragraph color segment defines the color of text in a line 
	//of text that was received for console display.  It has no 
	//notion of line wraps, screen position, or the text it contains.
	//It is only the number of characters that are a color and the
	//color.
	struct ParagraphColorSegment
	{
		S32		mNumChars;
		LLColor4 mColor;
	};
	
	//A line color segment is a chunk of text, the color associated
	//with it, and the X Position it was calculated to begin at 
	//on the screen.  X Positions are re-calculated if the 
	//screen changes size.
	class LineColorSegment
	{
		public:
			LineColorSegment(LLWString text, LLColor4 color, F32 xpos) : mText(text), mColor(color), mXPosition(xpos) {}
		public:
			LLWString mText;
			LLColor4  mColor;
			F32		  mXPosition;
	};
	 	
	typedef std::list<LineColorSegment> line_color_segments_t;
	
	//A line is composed of one or more color segments.
	class Line
	{
		public:
			line_color_segments_t mLineColorSegments;
			// <FS:Ansariel> Added styleflags member for fontstyle customization
			LLFontGL::StyleFlags mStyleFlags;
	};
	
	typedef std::list<Line> lines_t;
	typedef std::list<ParagraphColorSegment> paragraph_color_segments_t;
	
	//A paragraph is a processed element containing the entire text of the
	//message (used for recalculating positions on screen resize)
	//The time this message was added to the console output
	//The visual screen width of the longest line in this block
	//And a list of one or more lines which are used to display this message.
	class Paragraph
	{
		public:
			// <FS:Ansariel> Added styleflags parameter for style customization
			//Paragraph (LLWString str, const LLColor4 &color, F32 add_time, const LLFontGL* font, F32 screen_width);
			Paragraph (LLWString str, const LLColor4 &color, F32 add_time, const LLFontGL* font, F32 screen_width, LLFontGL::StyleFlags styleflags);
			// </FS:Ansariel>
			void makeParagraphColorSegments ( const LLColor4 &color);
			// <FS:Ansariel> Added styleflags parameter for style customization
			//void updateLines ( F32 screen_width,  const LLFontGL* font, bool force_resize=false );
			void updateLines ( F32 screen_width,  const LLFontGL* font, LLFontGL::StyleFlags styleflags, bool force_resize=false );
			// </FS:Ansariel>
		public:
			LLWString mParagraphText;	//The entire text of the paragraph
			paragraph_color_segments_t	mParagraphColorSegments;
			F32 mAddTime;				//Time this paragraph was added to the display.
			F32 mMaxWidth;				//Width of the widest line of text in this paragraph.
			lines_t	mLines;
			
	};
		
	//The console contains a deque of paragraphs which represent the individual messages.
	typedef std::deque<Paragraph> paragraph_t;
	paragraph_t mParagraphs;

	~LLConsole(){};

	// each line lasts this long after being added
	void			setLinePersistTime(F32 seconds);

	void			reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);

	// -1 = monospace, 0 means small, font size = 1 means big
	void			setFontSize(S32 size_index);

	
	// Overrides
	/*virtual*/ void	draw();

// <FS:Ansariel> Chat console
	void addConsoleLine(const std::string& utf8line, const LLColor4 &color, LLFontGL::StyleFlags styleflags = LLFontGL::NORMAL);
	void addConsoleLine(const LLWString& wline, const LLColor4 &color, LLFontGL::StyleFlags styleflags = LLFontGL::NORMAL);
	void clear();
	
	std::deque<LLColor4> mLineColors;
	std::deque<LLFontGL::StyleFlags> mLineStyle;

protected:
	void removeExtraLines();
// </FS:Ansariel>

private:
	void		update();

	F32			mLinePersistTime; // Age at which to stop drawing.
	F32			mFadeTime; // Age at which to start fading
	const LLFontGL*	mFont;
	S32			mConsoleWidth;
	S32			mConsoleHeight;
	bool		mParseUrls; // <FS:Ansariel> If lines should be parsed for URLs
	LLUIImagePtr	mBackgroundImage; // <FS:Ansariel> Configurable background for different console types
};

extern LLConsole* gConsole;

#endif
