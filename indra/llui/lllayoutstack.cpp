/** 
 * @file lllayoutstack.cpp
 * @brief LLLayout class - dynamic stacking of UI elements
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

// Opaque view with a background and a border.  Can contain LLUICtrls.

#include "linden_common.h"

#include "lllayoutstack.h"

#include "lllocalcliprect.h"
#include "llpanel.h"
#include "llresizebar.h"
#include "llcriticaldamp.h"
#include "boost/foreach.hpp"

static LLDefaultChildRegistry::Register<LLLayoutStack> register_layout_stack("layout_stack");
static LLLayoutStack::LayoutStackRegistry::Register<LLLayoutPanel> register_layout_panel("layout_panel");

void LLLayoutStack::OrientationNames::declareValues()
{
	declare("horizontal", HORIZONTAL);
	declare("vertical", VERTICAL);
}

//
// LLLayoutPanel
//
LLLayoutPanel::Params::Params()	
:	expanded_min_dim("expanded_min_dim", 0),
	min_dim("min_dim", -1),
	user_resize("user_resize", false),
	auto_resize("auto_resize", true)
{
	addSynonym(min_dim, "min_width");
	addSynonym(min_dim, "min_height");
}

LLLayoutPanel::LLLayoutPanel(const Params& p)	
:	LLPanel(p),
	mExpandedMinDimSpecified(false),
	mExpandedMinDim(p.min_dim),
 	mMinDim(p.min_dim), 
 	mAutoResize(p.auto_resize),
 	mUserResize(p.user_resize),
	mCollapsed(FALSE),
	mCollapseAmt(0.f),
	mVisibleAmt(1.f), // default to fully visible
	mResizeBar(NULL),
	mFractionalSize(0.f),
	mTargetDim(0),
	mIgnoreReshape(false),
	mOrientation(LLLayoutStack::HORIZONTAL)
{
	// Set the expanded min dim if it is provided, otherwise it gets the p.min_dim value
	if (p.expanded_min_dim.isProvided())
	{
		mExpandedMinDimSpecified = true;
		mExpandedMinDim = p.expanded_min_dim();
	}
	
	// panels initialized as hidden should not start out partially visible
	if (!getVisible())
	{
		mVisibleAmt = 0.f;
	}
}

void LLLayoutPanel::initFromParams(const Params& p)
{
	LLPanel::initFromParams(p);
	setFollowsNone();
}


LLLayoutPanel::~LLLayoutPanel()
{
	// probably not necessary, but...
	delete mResizeBar;
	mResizeBar = NULL;
}

F32 LLLayoutPanel::getAutoResizeFactor() const
{
	return mVisibleAmt * (1.f - mCollapseAmt);
}
 
F32 LLLayoutPanel::getVisibleAmount() const
{
	return mVisibleAmt;
}

S32 LLLayoutPanel::getLayoutDim() const
{
	return llround((F32)((mOrientation == LLLayoutStack::HORIZONTAL)
					? getRect().getWidth()
					: getRect().getHeight()));
}
 
S32 LLLayoutPanel::getVisibleDim() const
{
	F32 min_dim = getRelevantMinDim();
	return llround(mVisibleAmt
					* (min_dim
						+ (((F32)mTargetDim - min_dim) * (1.f - mCollapseAmt))));
}
 
void LLLayoutPanel::setOrientation( LLLayoutStack::ELayoutOrientation orientation )
{
	mOrientation = orientation;
	S32 layout_dim = llround((F32)((mOrientation == LLLayoutStack::HORIZONTAL)
		? getRect().getWidth()
		: getRect().getHeight()));

	if (mMinDim == -1)
	{
		if (!mAutoResize)
		{
			setMinDim(layout_dim);
		}
		else
		{
			setMinDim(0);
		}
	}

	mTargetDim = llmax(layout_dim, getMinDim());
}
 
void LLLayoutPanel::setVisible( BOOL visible )
{
	if (visible != getVisible())
	{
		LLLayoutStack* stackp = dynamic_cast<LLLayoutStack*>(getParent());
		if (stackp)
		{
			stackp->mNeedsLayout = true;
		}
	}
	LLPanel::setVisible(visible);
}

void LLLayoutPanel::reshape( S32 width, S32 height, BOOL called_from_parent /*= TRUE*/ )
{
	if (!mIgnoreReshape && !mAutoResize)
	{
		mTargetDim = (mOrientation == LLLayoutStack::HORIZONTAL) ? width : height;
	}
	LLPanel::reshape(width, height, called_from_parent);
}

void LLLayoutPanel::handleReshape(const LLRect& new_rect, bool by_user)
{
	LLLayoutStack* stackp = dynamic_cast<LLLayoutStack*>(getParent());
	if (stackp)
	{
		stackp->mNeedsLayout = true;
		if (by_user)
		{
			// tell layout stack to account for new shape
			stackp->updatePanelRect(this, new_rect);
		}
	}
	LLPanel::handleReshape(new_rect, by_user);
}

//
// LLLayoutStack
//

LLLayoutStack::Params::Params()
:	orientation("orientation"),
	animate("animate", true),
	clip("clip", true),
	open_time_constant("open_time_constant", 0.02f),
	close_time_constant("close_time_constant", 0.03f),
	border_size("border_size", LLCachedControl<S32>(*LLUI::sSettingGroups["config"], "UIResizeBarHeight", 0))
{}

LLLayoutStack::LLLayoutStack(const LLLayoutStack::Params& p) 
:	LLView(p),
	mPanelSpacing(p.border_size),
	mOrientation(p.orientation),
	mAnimate(p.animate),
	mAnimatedThisFrame(false),
	mNeedsLayout(true),
	mClip(p.clip),
	mOpenTimeConstant(p.open_time_constant),
	mCloseTimeConstant(p.close_time_constant)
{}

LLLayoutStack::~LLLayoutStack()
{
	e_panel_list_t panels = mPanels; // copy list of panel pointers
	mPanels.clear(); // clear so that removeChild() calls don't cause trouble
	std::for_each(panels.begin(), panels.end(), DeletePointer());
}

void LLLayoutStack::draw()
{
	updateLayout();

	// always clip to stack itself
	LLLocalClipRect clip(getLocalRect());
	BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
	{
		// clip to layout rectangle, not bounding rectangle
		LLRect clip_rect = panelp->getRect();
		// scale clipping rectangle by visible amount
		if (mOrientation == HORIZONTAL)
		{
			clip_rect.mRight = clip_rect.mLeft + panelp->getVisibleDim();
		}
		else
		{
			clip_rect.mBottom = clip_rect.mTop - panelp->getVisibleDim();
		}

		{LLLocalClipRect clip(clip_rect, mClip);
			// only force drawing invisible children if visible amount is non-zero
			drawChild(panelp, 0, 0, !clip_rect.isEmpty());
		}
	}
	mAnimatedThisFrame = false;
}

void LLLayoutStack::removeChild(LLView* view)
{
	LLLayoutPanel* embedded_panelp = findEmbeddedPanel(dynamic_cast<LLPanel*>(view));

	if (embedded_panelp)
	{
		mPanels.erase(std::find(mPanels.begin(), mPanels.end(), embedded_panelp));
		delete embedded_panelp;
		updateFractionalSizes();
		mNeedsLayout = true;
	}

	LLView::removeChild(view);
}

BOOL LLLayoutStack::postBuild()
{
	updateLayout();
	return TRUE;
}

bool LLLayoutStack::addChild(LLView* child, S32 tab_group)
{
	LLLayoutPanel* panelp = dynamic_cast<LLLayoutPanel*>(child);
	if (panelp)
	{
		panelp->setOrientation(mOrientation);
		mPanels.push_back(panelp);
		createResizeBar(panelp);
		mNeedsLayout = true;
	}
	BOOL result = LLView::addChild(child, tab_group);

	updateFractionalSizes();
	return result;
}

void LLLayoutStack::addPanel(LLLayoutPanel* panel, EAnimate animate)
{
	addChild(panel);

	// panel starts off invisible (collapsed)
	if (animate == ANIMATE)
	{
		panel->mVisibleAmt = 0.f;
		panel->setVisible(TRUE);
	}
}

void LLLayoutStack::collapsePanel(LLPanel* panel, BOOL collapsed)
{
	LLLayoutPanel* panel_container = findEmbeddedPanel(panel);
	if (!panel_container) return;

	panel_container->mCollapsed = collapsed;
	mNeedsLayout = true;
}

void LLLayoutStack::updatePanelAutoResize(const std::string& panel_name, BOOL auto_resize)
{
	LLLayoutPanel* panel = findEmbeddedPanelByName(panel_name);
	
	if (panel)
	{
		panel->mAutoResize = auto_resize;
	}

	mNeedsLayout = true;
}

void LLLayoutStack::setPanelUserResize(const std::string& panel_name, BOOL user_resize)
{
	LLLayoutPanel* panel = findEmbeddedPanelByName(panel_name);

	if (panel)
	{
		panel->mUserResize = user_resize;
	}

	mNeedsLayout = true;
	updateFractionalSizes();
}


static LLFastTimer::DeclareTimer FTM_UPDATE_LAYOUT("Update LayoutStacks");

void LLLayoutStack::updateLayout()
{	
	LLFastTimer ft(FTM_UPDATE_LAYOUT);

	if (!mNeedsLayout) return;

	bool animation_in_progress = animatePanels();
	F32 total_visible_fraction = 0.f;
	S32 space_to_distribute = (mOrientation == HORIZONTAL)
							? getRect().getWidth()
							: getRect().getHeight();

	// first, assign minimum dimensions
	LLLayoutPanel* panelp = NULL;
	BOOST_FOREACH(panelp, mPanels)
	{
		if (panelp->mAutoResize)
		{
			panelp->mTargetDim = panelp->getRelevantMinDim();
		}
		space_to_distribute -= panelp->getVisibleDim() + llround((F32)mPanelSpacing * panelp->getVisibleAmount());
		total_visible_fraction += panelp->mFractionalSize * panelp->getAutoResizeFactor();
	}

	llassert(total_visible_fraction < 1.01f);

	// don't need spacing after last panel
	space_to_distribute += panelp ? llround((F32)mPanelSpacing * panelp->getVisibleAmount()) : 0;

	// scale up space to distribute, since some of might will go to an invisible fraction of the auto-resize space
	space_to_distribute = (total_visible_fraction > 0.f)
						? llround((F32)space_to_distribute / total_visible_fraction)
						: space_to_distribute;

	if (space_to_distribute > 0)
	{	// give space proportionally to auto resize panels, even invisible ones
		BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
		{
			if (panelp->mAutoResize == TRUE)
			{
				S32 delta = llround((F32)space_to_distribute * panelp->mFractionalSize/* * panelp->getAutoResizeFactor()*/);
				panelp->mTargetDim += delta;
			}
		}
	}

	F32 cur_pos = (mOrientation == HORIZONTAL) ? 0.f : (F32)getRect().getHeight();

	BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
	{
		F32 panel_dim = panelp->mTargetDim;
		F32 panel_visible_dim = panelp->getVisibleDim();

		LLRect panel_rect;
		if (mOrientation == HORIZONTAL)
		{
			panel_rect.setLeftTopAndSize(llround(cur_pos),
										getRect().getHeight(),
										llround(panel_dim),
										getRect().getHeight());
		}
		else
		{
			panel_rect.setLeftTopAndSize(0,
				llround(cur_pos),
				getRect().getWidth(),
				llround(panel_dim));
		}
		panelp->setIgnoreReshape(true);
		panelp->setShape(panel_rect);
		panelp->setIgnoreReshape(false);

		static LLUICachedControl<S32> resize_bar_overlap ("UIResizeBarOverlap", 0);
		LLRect resize_bar_rect(panel_rect);

		F32 panel_spacing = (F32)mPanelSpacing * panelp->getVisibleAmount();
		if (mOrientation == HORIZONTAL)
		{
			resize_bar_rect.mLeft = panel_rect.mRight - resize_bar_overlap;
			resize_bar_rect.mRight = panel_rect.mRight + panel_spacing + resize_bar_overlap;

			cur_pos += panel_visible_dim + panel_spacing;
		}
		else //VERTICAL
		{
			resize_bar_rect.mTop = panel_rect.mBottom + resize_bar_overlap;
			resize_bar_rect.mBottom = panel_rect.mBottom - panel_spacing - resize_bar_overlap;

			cur_pos -= panel_visible_dim + panel_spacing;
		}
		panelp->mResizeBar->setShape(resize_bar_rect);
	}

	updateResizeBarLimits();

	// clear animation flag at end, since panel resizes will set it
	// and leave it set if there is any animation in progress
	mNeedsLayout = animation_in_progress;
} // end LLLayoutStack::updateLayout

LLLayoutPanel* LLLayoutStack::findEmbeddedPanel(LLPanel* panelp) const
{
	if (!panelp) return NULL;

	e_panel_list_t::const_iterator panel_it;
	BOOST_FOREACH(LLLayoutPanel* p, mPanels)
	{
		if (p == panelp)
		{
			return p;
		}
	}
	return NULL;
}

LLLayoutPanel* LLLayoutStack::findEmbeddedPanelByName(const std::string& name) const
{
	LLLayoutPanel* result = NULL;

	BOOST_FOREACH(LLLayoutPanel* p, mPanels)
	{
		if (p->getName() == name)
		{
			result = p;
			break;
		}
	}

	return result;
}

void LLLayoutStack::createResizeBar(LLLayoutPanel* panelp)
{
	BOOST_FOREACH(LLLayoutPanel* lp, mPanels)
	{
		if (lp->mResizeBar == NULL)
		{
			LLResizeBar::Side side = (mOrientation == HORIZONTAL) ? LLResizeBar::RIGHT : LLResizeBar::BOTTOM;
			LLRect resize_bar_rect = getRect();

			LLResizeBar::Params resize_params;
			resize_params.name("resize");
			resize_params.resizing_view(lp);
			resize_params.min_size(lp->getRelevantMinDim());
			resize_params.side(side);
			resize_params.snapping_enabled(false);
			LLResizeBar* resize_bar = LLUICtrlFactory::create<LLResizeBar>(resize_params);
			lp->mResizeBar = resize_bar;
			LLView::addChild(resize_bar, 0);
		}
	}
	// bring all resize bars to the front so that they are clickable even over the panels
	// with a bit of overlap
	for (e_panel_list_t::iterator panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		LLResizeBar* resize_barp = (*panel_it)->mResizeBar;
		sendChildToFront(resize_barp);
	}
}

// update layout stack animations, etc. once per frame
// NOTE: we use this to size world view based on animating UI, *before* we draw the UI
// we might still need to call updateLayout during UI draw phase, in case UI elements
// are resizing themselves dynamically
//static 
void LLLayoutStack::updateClass()
{
	for (instance_iter it = beginInstances(); it != endInstances(); ++it)
	{
		it->updateLayout();
	}
}

void LLLayoutStack::updateFractionalSizes()
{
	F32 total_resizable_dim = 0;
	S32 num_auto_resize_panels = 0;

	BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
	{
		if (panelp->mAutoResize)
		{
			total_resizable_dim += llmax(0, panelp->getLayoutDim() - panelp->getRelevantMinDim());
			num_auto_resize_panels++;
		}
	}

	F32 total_fractional_size = 0.f;
	
	BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
	{
		if (panelp->mAutoResize)
		{
			F32 panel_resizable_dim = llmax(0.f, (F32)(panelp->getLayoutDim() - panelp->getRelevantMinDim()));
			panelp->mFractionalSize = llmin(1.f, (panel_resizable_dim == 0.f)
																	? 0.f
																	: panel_resizable_dim / total_resizable_dim);
			total_fractional_size += panelp->mFractionalSize;
			// check for NaNs
			llassert(panelp->mFractionalSize == panelp->mFractionalSize);
		}
	}

	if (total_fractional_size == 0.f)
	{ // equal distribution
		BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
		{
			if (panelp->mAutoResize)
			{
				panelp->mFractionalSize = 1.f / (F32)num_auto_resize_panels;
			}
		}
	}
	else
	{ // renormalize
		BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
		{
			if (panelp->mAutoResize)
			{
				panelp->mFractionalSize /= total_fractional_size;
			}
		}
	}
}

bool LLLayoutStack::animatePanels()
{
	bool animation_in_progress = false;
	
	//
	// animate visibility
	//
	BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
	{
		if (panelp->getVisible())
		{
			if (mAnimate && panelp->mVisibleAmt < 1.f)
			{
				if (!mAnimatedThisFrame)
				{
					panelp->mVisibleAmt = lerp(panelp->mVisibleAmt, 1.f, LLCriticalDamp::getInterpolant(mOpenTimeConstant));
					if (panelp->mVisibleAmt > 0.99f)
					{
						panelp->mVisibleAmt = 1.f;
					}
				}
				
				animation_in_progress = true;
			}
			else
			{
				if (panelp->mVisibleAmt != 1.f)
				{
					panelp->mVisibleAmt = 1.f;
					animation_in_progress = true;
				}
			}
		}
		else // not visible
		{
			if (mAnimate && panelp->mVisibleAmt > 0.f)
			{
				if (!mAnimatedThisFrame)
				{
					panelp->mVisibleAmt = lerp(panelp->mVisibleAmt, 0.f, LLCriticalDamp::getInterpolant(mCloseTimeConstant));
					if (panelp->mVisibleAmt < 0.001f)
					{
						panelp->mVisibleAmt = 0.f;
					}
				}

				animation_in_progress = true;
			}
			else
			{
				if (panelp->mVisibleAmt != 0.f)
				{
					panelp->mVisibleAmt = 0.f;
					animation_in_progress = true;
				}
			}
		}

		F32 collapse_state = panelp->mCollapsed ? 1.f : 0.f;
		if (panelp->mCollapseAmt != collapse_state)
		{
			if (!mAnimatedThisFrame)
			{
				panelp->mCollapseAmt = lerp(panelp->mCollapseAmt, collapse_state, LLCriticalDamp::getInterpolant(mCloseTimeConstant));
			}
			animation_in_progress = true;
			
			if (llabs(panelp->mCollapseAmt - collapse_state) < 0.001f)
			{
				panelp->mCollapseAmt = collapse_state;
			}
		}
	}

	mAnimatedThisFrame = true;

	return animation_in_progress;
}

void LLLayoutStack::updatePanelRect( LLLayoutPanel* resized_panel, const LLRect& new_rect )
{
	S32 new_dim = (mOrientation == HORIZONTAL)
					? new_rect.getWidth()
					: new_rect.getHeight();
	S32 delta_dim = new_dim - resized_panel->getVisibleDim();
	if (delta_dim == 0) return;

	F32 total_visible_fraction = 0.f;
	F32 delta_auto_resize_headroom = 0.f;
	F32 total_auto_resize_headroom = 0.f;

	LLLayoutPanel* other_resize_panel = NULL;
	LLLayoutPanel* following_panel = NULL;

	BOOST_REVERSE_FOREACH(LLLayoutPanel* panelp, mPanels)
	{
		if (panelp->mAutoResize)
		{
			total_auto_resize_headroom += (F32)(panelp->mTargetDim - panelp->getRelevantMinDim());
			total_visible_fraction += panelp->mFractionalSize * panelp->getAutoResizeFactor();
		}

		if (panelp == resized_panel)
		{
			other_resize_panel = following_panel;
		}

		if (panelp->getVisible() && !panelp->mCollapsed)
		{
			following_panel = panelp;
		}
	}

	if (resized_panel->mAutoResize == FALSE)
	{
		delta_auto_resize_headroom += -delta_dim;
	}
	if (other_resize_panel && other_resize_panel->mAutoResize == FALSE)
	{
		delta_auto_resize_headroom += delta_dim;
	}


	//delta_auto_resize_headroom = (total_visible_fraction > 0.f)
	//								? delta_auto_resize_headroom / total_visible_fraction
	//								: 0.f;

	F32 fraction_given_up = 0.f;
	F32 fraction_remaining = 1.f;
	F32 updated_auto_resize_headroom = total_auto_resize_headroom + delta_auto_resize_headroom;

	enum
	{
		BEFORE_RESIZED_PANEL,
		RESIZED_PANEL,
		NEXT_PANEL,
		AFTER_RESIZED_PANEL
	} which_panel = BEFORE_RESIZED_PANEL;

	BOOST_FOREACH(LLLayoutPanel* panelp, mPanels)
	{
		if (!panelp->getVisible() || panelp->mCollapsed) continue;

		if (panelp == resized_panel)
		{
			which_panel = RESIZED_PANEL;
		}

		switch(which_panel)
		{
		case BEFORE_RESIZED_PANEL:
			if (panelp->mAutoResize)
			{	// freeze current size as fraction of overall auto_resize space
				F32 fractional_adjustment_factor = total_auto_resize_headroom / updated_auto_resize_headroom;
				F32 new_fractional_size = llclamp(panelp->mFractionalSize * fractional_adjustment_factor,
													0.f,
													1.f);
				F32 fraction_delta = (new_fractional_size - panelp->mFractionalSize);
				fraction_given_up -= fraction_delta;
				fraction_remaining -= panelp->mFractionalSize;
				panelp->mFractionalSize += fraction_delta;
				llassert(!llisnan(panelp->mFractionalSize));
			}
			else
			{
				// leave non auto-resize panels alone
			}
			break;
		case RESIZED_PANEL:
			if (panelp->mAutoResize)
			{	// freeze new size as fraction
				F32 new_fractional_size = (updated_auto_resize_headroom == 0.f)
					? 1.f
					: llmin(1.f, ((F32)(new_dim - panelp->getRelevantMinDim()) / updated_auto_resize_headroom));
				fraction_given_up -= new_fractional_size - panelp->mFractionalSize;
				fraction_remaining -= panelp->mFractionalSize;
				panelp->mFractionalSize = new_fractional_size;
				llassert(!llisnan(panelp->mFractionalSize));
			}
			else
			{	// freeze new size as original size
				panelp->mTargetDim = new_dim;
				fraction_remaining -= fraction_given_up;
			}
			which_panel = NEXT_PANEL;
			break;
		case NEXT_PANEL:
			if (panelp->mAutoResize)
			{
				fraction_remaining -= panelp->mFractionalSize;
				if (fraction_given_up != 0.f)
				{
					panelp->mFractionalSize += fraction_given_up;
					fraction_given_up = 0.f;
				}
				else
				{
					F32 new_fractional_size = llmin(1.f, 
													(F32)(panelp->mTargetDim - panelp->getRelevantMinDim() + delta_auto_resize_headroom) 
														/ updated_auto_resize_headroom);
					fraction_given_up -= new_fractional_size - panelp->mFractionalSize;
					panelp->mFractionalSize = new_fractional_size;
				}
			}
			else
			{
				panelp->mTargetDim -= delta_dim;
			}
			which_panel = AFTER_RESIZED_PANEL;
			break;
		case AFTER_RESIZED_PANEL:
			if (panelp->mAutoResize)
			{
				panelp->mFractionalSize += (panelp->mFractionalSize / fraction_remaining) * fraction_given_up;
			}
		default:
			break;
		}
	}
}

void LLLayoutStack::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	mNeedsLayout = true;
	LLView::reshape(width, height, called_from_parent);
}

void LLLayoutStack::updateResizeBarLimits()
{
	LLLayoutPanel* previous_visible_panelp = NULL;
	BOOST_REVERSE_FOREACH(LLLayoutPanel* visible_panelp, mPanels)
	{
		if (!visible_panelp->getVisible() || visible_panelp->mCollapsed)
		{
			visible_panelp->mResizeBar->setVisible(FALSE);
			continue;
		}

		// toggle resize bars based on panel visibility, resizability, etc
		if (previous_visible_panelp 
			&& (visible_panelp->mUserResize 
				|| previous_visible_panelp->mUserResize))
		{
			visible_panelp->mResizeBar->setVisible(TRUE);
			visible_panelp->mResizeBar->setResizeLimits(visible_panelp->getRelevantMinDim(), 
														visible_panelp->getVisibleDim() 
															+ (previous_visible_panelp->getVisibleDim() 
																- previous_visible_panelp->getRelevantMinDim()));
		}
		else
		{
			visible_panelp->mResizeBar->setVisible(FALSE);
		}

		previous_visible_panelp = visible_panelp;
	}
}
