/** 
 * @file llfavoritesbar.cpp
 * @brief LLFavoritesBarCtrl class implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llfavoritesbar.h"

#include "llbutton.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llinventory.h"
#include "lllandmarkactions.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llmenugl.h"
#include "lltooltip.h"

#include "llagent.h"
#include "llclipboard.h"
#include "llinventoryclipboard.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"
#include "llfloaterworldmap.h"
#include "lllandmarkactions.h"
#include "llsidetray.h"
#include "lltoggleablemenu.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"
#include "llviewermenu.h"
#include "lltooldraganddrop.h"

static LLDefaultChildRegistry::Register<LLFavoritesBarCtrl> r("favorites_bar");

const S32 DROP_DOWN_MENU_WIDTH = 250;

/**
 * Helper for LLFavoriteLandmarkButton and LLFavoriteLandmarkMenuItem.
 * Performing requests for SLURL for given Landmark ID
 */
class LLLandmarkInfoGetter
{
public:
	LLLandmarkInfoGetter()
	:	mLandmarkID(LLUUID::null),
		mName("(Loading...)"),
		mPosX(0),
		mPosY(0),
		mLoaded(false) 
	{}

	void setLandmarkID(const LLUUID& id) { mLandmarkID = id; }
	const LLUUID& getLandmarkId() const { return mLandmarkID; }

	const std::string& getName()
	{
		if(!mLoaded)
			requestNameAndPos();

		return mName;
	}

	S32 getPosX()
	{
		if (!mLoaded)
			requestNameAndPos();
		return mPosX;
	}

	S32 getPosY()
	{
		if (!mLoaded)
			requestNameAndPos();
		return mPosY;
	}
private:
	/**
	 * Requests landmark data from server.
	 */
	void requestNameAndPos()
	{
		if (mLandmarkID.isNull())
			return;

		LLVector3d g_pos;
		if(LLLandmarkActions::getLandmarkGlobalPos(mLandmarkID, g_pos))
		{
			LLLandmarkActions::getRegionNameAndCoordsFromPosGlobal(g_pos,
				boost::bind(&LLLandmarkInfoGetter::landmarkNameCallback, this, _1, _2, _3));
		}
	}

	void landmarkNameCallback(const std::string& name, S32 x, S32 y)
	{
		mPosX = x;
		mPosY = y;
		mName = name;
		mLoaded = true;
	}

	LLUUID mLandmarkID;
	std::string mName;
	S32 mPosX;
	S32 mPosY;
	bool mLoaded;
};

/**
 * This class is needed to override LLButton default handleToolTip function and
 * show SLURL as button tooltip.
 * *NOTE: dzaporozhan: This is a workaround. We could set tooltips for buttons
 * in createButtons function but landmark data is not available when Favorites Bar is
 * created. Thats why we are requesting landmark data after 
 */
class LLFavoriteLandmarkButton : public LLButton
{
public:

	BOOL handleToolTip(S32 x, S32 y, MASK mask)
	{
		std::string region_name = mLandmarkInfoGetter.getName();
		
		if (!region_name.empty())
		{
			LLToolTip::Params params;
			params.message = llformat("%s\n%s (%d, %d)", getLabelSelected().c_str(), region_name.c_str(), mLandmarkInfoGetter.getPosX(), mLandmarkInfoGetter.getPosY());
			params.sticky_rect = calcScreenRect();
			LLToolTipMgr::instance().show(params);
		}
		return TRUE;
	}

	/*virtual*/ BOOL	handleHover(S32 x, S32 y, MASK mask)
	{
		LLFavoritesBarCtrl* fb = dynamic_cast<LLFavoritesBarCtrl*>(getParent());

		if (fb)
		{
			fb->handleHover(x, y, mask);
		}

		return LLButton::handleHover(x, y, mask);
	}
	
	void setLandmarkID(const LLUUID& id){ mLandmarkInfoGetter.setLandmarkID(id); }
	const LLUUID& getLandmarkId() const { return mLandmarkInfoGetter.getLandmarkId(); }

	void onMouseEnter(S32 x, S32 y, MASK mask)
	{
		if (LLToolDragAndDrop::getInstance()->hasMouseCapture())
		{
			LLUICtrl::onMouseEnter(x, y, mask);
		}
		else
		{
			LLButton::onMouseEnter(x, y, mask);
		}
	}

protected:
	LLFavoriteLandmarkButton(const LLButton::Params& p) : LLButton(p) {}
	friend class LLUICtrlFactory;

private:
	LLLandmarkInfoGetter mLandmarkInfoGetter;
};

/**
 * This class is needed to override LLMenuItemCallGL default handleToolTip function and
 * show SLURL as button tooltip.
 * *NOTE: dzaporozhan: This is a workaround. We could set tooltips for buttons
 * in showDropDownMenu function but landmark data is not available when Favorites Bar is
 * created. Thats why we are requesting landmark data after 
 */
class LLFavoriteLandmarkMenuItem : public LLMenuItemCallGL
{
public:
	BOOL handleToolTip(S32 x, S32 y, MASK mask)
	{
		std::string region_name = mLandmarkInfoGetter.getName();
		if (!region_name.empty())
		{
			LLToolTip::Params params;
			params.message = llformat("%s\n%s (%d, %d)", getLabel().c_str(), region_name.c_str(), mLandmarkInfoGetter.getPosX(), mLandmarkInfoGetter.getPosY());
			params.sticky_rect = calcScreenRect();
			LLToolTipMgr::instance().show(params);
		}
		return TRUE;
	}
	
	void setLandmarkID(const LLUUID& id){ mLandmarkInfoGetter.setLandmarkID(id); }

	virtual BOOL handleMouseDown(S32 x, S32 y, MASK mask)
	{
		mMouseDownSignal(this, x, y, mask);
		return LLMenuItemCallGL::handleMouseDown(x, y, mask);
	}

	virtual BOOL handleMouseUp(S32 x, S32 y, MASK mask)
	{
		mMouseUpSignal(this, x, y, mask);
		return LLMenuItemCallGL::handleMouseUp(x, y, mask);
	}

	virtual BOOL handleHover(S32 x, S32 y, MASK mask)
	{
		if (fb)
		{
			fb->handleHover(x, y, mask);
		}

		return TRUE;
	}

	void initFavoritesBarPointer(LLFavoritesBarCtrl* fb) { this->fb = fb; }

protected:

	LLFavoriteLandmarkMenuItem(const LLMenuItemCallGL::Params& p) : LLMenuItemCallGL(p) {}
	friend class LLUICtrlFactory;

private:
	LLLandmarkInfoGetter mLandmarkInfoGetter;
	LLFavoritesBarCtrl* fb;
};

/**
 * This class was introduced just for fixing the following issue:
 * EXT-836 Nav bar: Favorites overflow menu passes left-mouse click through.
 * We must explicitly handle drag and drop event by returning TRUE
 * because otherwise LLToolDragAndDrop will initiate drag and drop operation
 * with the world.
 */
class LLFavoriteLandmarkToggleableMenu : public LLToggleableMenu
{
public:
	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg)
	{
		*accept = ACCEPT_NO;
		return TRUE;
	}

protected:
	LLFavoriteLandmarkToggleableMenu(const LLToggleableMenu::Params& p):
		LLToggleableMenu(p)
	{
	}

	friend class LLUICtrlFactory;
};

/**
 * This class is needed to update an item being copied to the favorites folder
 * with a sort field value (required to save favorites bar's tabs order).
 * See method handleNewFavoriteDragAndDrop for more details on how this class is used.
 */
class LLItemCopiedCallback : public LLInventoryCallback
{
public:
	LLItemCopiedCallback(S32 sortField): mSortField(sortField) {}

	virtual void fire(const LLUUID& inv_item)
	{
		LLViewerInventoryItem* item = gInventory.getItem(inv_item);

		if (item)
		{
			item->setSortField(mSortField);
			item->setComplete(TRUE);
			item->updateServer(FALSE);

			gInventory.updateItem(item);
			gInventory.notifyObservers();
		}

		LLView::getWindow()->setCursor(UI_CURSOR_ARROW);
	}

private:
	S32 mSortField;
};

// updateButtons's helper
struct LLFavoritesSort
{
	// Sorting by creation date and name
	// TODO - made it customizible using gSavedSettings
	bool operator()(const LLViewerInventoryItem* const& a, const LLViewerInventoryItem* const& b)
	{
		S32 sortField1 = a->getSortField();
		S32 sortField2 = b->getSortField();

		if (!(sortField1 < 0 && sortField2 < 0))
		{
			return sortField2 > sortField1;
		}

		time_t first_create = a->getCreationDate();
		time_t second_create = b->getCreationDate();
		if (first_create == second_create)
		{
			return (LLStringUtil::compareDict(a->getName(), b->getName()) < 0);
		}
		else
		{
			return (first_create > second_create);
		}
	}
};

LLFavoritesBarCtrl::Params::Params()
: chevron_button_tool_tip("chevron_button_tool_tip"),
  image_drag_indication("image_drag_indication")
{
}

LLFavoritesBarCtrl::LLFavoritesBarCtrl(const LLFavoritesBarCtrl::Params& p)
:	LLUICtrl(p),
	mFont(p.font.isProvided() ? p.font() : LLFontGL::getFontSansSerifSmall()),
	mPopupMenuHandle(),
	mInventoryItemsPopupMenuHandle(),
	mChevronButtonToolTip(p.chevron_button_tool_tip),
	mImageDragIndication(p.image_drag_indication),
	mShowDragMarker(FALSE),
	mLandingTab(NULL),
	mLastTab(NULL),
	mTabsHighlightEnabled(TRUE)
{
	// Register callback for menus with current registrar (will be parent panel's registrar)
	LLUICtrl::CommitCallbackRegistry::currentRegistrar().add("Favorites.DoToSelected",
		boost::bind(&LLFavoritesBarCtrl::doToSelected, this, _2));

	// Add this if we need to selectively enable items
	LLUICtrl::EnableCallbackRegistry::currentRegistrar().add("Favorites.EnableSelected",
		boost::bind(&LLFavoritesBarCtrl::enableSelected, this, _2));
	
	gInventory.addObserver(this);
}

LLFavoritesBarCtrl::~LLFavoritesBarCtrl()
{
	gInventory.removeObserver(this);

	LLView::deleteViewByHandle(mPopupMenuHandle);
	LLView::deleteViewByHandle(mInventoryItemsPopupMenuHandle);
}

BOOL LLFavoritesBarCtrl::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg)
{
	*accept = ACCEPT_NO;

	switch (cargo_type)
	{

	case DAD_LANDMARK:
		{
			/*
			 * add a callback to the end drag event.
			 * the callback will disconnet itself immediately after execution
			 * this is done because LLToolDragAndDrop is a common tool so it shouldn't
			 * be overloaded with redundant callbacks.
			 */
			if (!mEndDragConnection.connected())
			{
				mEndDragConnection = LLToolDragAndDrop::getInstance()->setEndDragCallback(boost::bind(&LLFavoritesBarCtrl::onEndDrag, this));
			}

			// Copy the item into the favorites folder (if it's not already there).
			LLInventoryItem *item = (LLInventoryItem *)cargo_data;

			if (LLFavoriteLandmarkButton* dest = dynamic_cast<LLFavoriteLandmarkButton*>(findChildByLocalCoords(x, y)))
			{
				setLandingTab(dest);
			}
			/*
			 * the condition dest == NULL can be satisfied not only in the case
			 * of dragging to the right from the last tab of the favbar. there is a
			 * small gap between each tab. if the user drags something exactly there
			 * then mLandingTab will be set to NULL and the dragged item will be pushed
			 * to the end of the favorites bar. this is incorrect behavior. that's why
			 * we need an additional check which excludes the case described previously
			 * making sure that the mouse pointer is beyond the last tab.
			 */
			else if (mLastTab && x >= mLastTab->getRect().mRight)
			{
				setLandingTab(NULL);
			}

			// check if we are dragging an existing item from the favorites bar
			if (item && mDragItemId == item->getUUID())
			{
				*accept = ACCEPT_YES_SINGLE;

				showDragMarker(TRUE);

				if (drop)
				{
					handleExistingFavoriteDragAndDrop(x, y);
					showDragMarker(FALSE);
				}
			}
			else
			{
				LLUUID favorites_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_FAVORITE);
				if (item->getParentUUID() == favorites_id)
				{
					llwarns << "Attemt to copy a favorite item into the same folder." << llendl;
					break;
				}

				*accept = ACCEPT_YES_COPY_MULTI;

				showDragMarker(TRUE);

				if (drop)
				{
					handleNewFavoriteDragAndDrop(item, favorites_id, x, y);
					showDragMarker(FALSE);
				}
			}
		}
		break;
	default:
		break;
	}

	return TRUE;
}

void LLFavoritesBarCtrl::handleExistingFavoriteDragAndDrop(S32 x, S32 y)
{
	LLFavoriteLandmarkButton* dest = dynamic_cast<LLFavoriteLandmarkButton*>(mLandingTab);

	// there is no need to handle if an item was dragged onto itself
	if (dest && dest->getLandmarkId() == mDragItemId)
	{
		return;
	}

	if (dest)
	{
		updateItemsOrder(mItems, mDragItemId, dest->getLandmarkId());
	}
	else
	{
		mItems.push_back(gInventory.getItem(mDragItemId));
	}

	saveItemsOrder(mItems);

	LLToggleableMenu* menu = (LLToggleableMenu*) mPopupMenuHandle.get();

	if (menu && menu->getVisible())
	{
		menu->setVisible(FALSE);
		showDropDownMenu();
	}
}

void LLFavoritesBarCtrl::handleNewFavoriteDragAndDrop(LLInventoryItem *item, const LLUUID& favorites_id, S32 x, S32 y)
{
	LLFavoriteLandmarkButton* dest = dynamic_cast<LLFavoriteLandmarkButton*>(mLandingTab);

	// there is no need to handle if an item was dragged onto itself
	if (dest && dest->getLandmarkId() == mDragItemId)
	{
		return;
	}

	if (dest)
	{
		insertBeforeItem(mItems, dest->getLandmarkId(), item->getUUID());
	}
	else
	{
		mItems.push_back(gInventory.getItem(item->getUUID()));
	}

	int sortField = 0;
	LLPointer<LLItemCopiedCallback> cb;

	// current order is saved by setting incremental values (1, 2, 3, ...) for the sort field
	for (LLInventoryModel::item_array_t::iterator i = mItems.begin(); i != mItems.end(); ++i)
	{
		LLViewerInventoryItem* currItem = *i;

		if (currItem->getUUID() == item->getUUID())
		{
			cb = new LLItemCopiedCallback(++sortField);
		}
		else
		{
			currItem->setSortField(++sortField);
			currItem->setComplete(TRUE);
			currItem->updateServer(FALSE);

			gInventory.updateItem(currItem);
		}
	}

	copy_inventory_item(
			gAgent.getID(),
			item->getPermissions().getOwner(),
			item->getUUID(),
			favorites_id,
			std::string(),
			cb);

	llinfos << "Copied inventory item #" << item->getUUID() << " to favorites." << llendl;
}

//virtual
void LLFavoritesBarCtrl::changed(U32 mask)
{
	if (mFavoriteFolderId.isNull())
	{
		mFavoriteFolderId = gInventory.findCategoryUUIDForType(LLAssetType::AT_FAVORITE);
		
		if (mFavoriteFolderId.notNull())
		{
			gInventory.fetchDescendentsOf(mFavoriteFolderId);
		}
	}	
	else
	{
		updateButtons(getRect().getWidth());
	}
}

//virtual
void LLFavoritesBarCtrl::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	updateButtons(width);

	LLUICtrl::reshape(width, height, called_from_parent);
}

void LLFavoritesBarCtrl::draw()
{
	LLUICtrl::draw();

	if (mShowDragMarker)
	{
		S32 w = mImageDragIndication->getWidth() / 2;
		S32 h = mImageDragIndication->getHeight() / 2;

		if (mLandingTab)
		{
			// mouse pointer hovers over an existing tab
			LLRect rect = mLandingTab->getRect();
			mImageDragIndication->draw(rect.mLeft - w/2, rect.getHeight(), w, h);
		}
		else if (mLastTab)
		{
			// mouse pointer hovers over the favbar empty space (right to the last tab)
			LLRect rect = mLastTab->getRect();
			mImageDragIndication->draw(rect.mRight, rect.getHeight(), w, h);
		}
	}
}

LLXMLNodePtr LLFavoritesBarCtrl::getButtonXMLNode()
{
	LLXMLNodePtr buttonXMLNode = NULL;
	bool success = LLUICtrlFactory::getLayeredXMLNode("favorites_bar_button.xml", buttonXMLNode);
	if (!success)
	{
		llwarns << "Unable to read xml file with button for Favorites Bar: favorites_bar_button.xml" << llendl;
		buttonXMLNode = NULL;
	}
	return buttonXMLNode;
}

void LLFavoritesBarCtrl::updateButtons(U32 bar_width)
{
	mItems.clear();

	if (!collectFavoriteItems(mItems))
	{
		return;
	}

	static LLXMLNodePtr buttonXMLNode = getButtonXMLNode();
	if (buttonXMLNode.isNull())
	{
		return;
	}

	S32 buttonWidth = 120; //default value
	buttonXMLNode->getAttributeS32("width", buttonWidth);
	S32 buttonHGap = 2; // default value
	buttonXMLNode->getAttributeS32("left", buttonHGap);
	
	S32 count = mItems.count();

	const S32 buttonHPad = LLUI::sSettingGroups["config"]->getS32("ButtonHPad");
	const S32 chevron_button_width = mFont->getWidth(">>") + buttonHPad * 2;

	S32 buttons_space = bar_width - buttonHGap;

	S32 first_drop_down_item = count;

	// Calculating, how much buttons can fit in the bar
	S32 buttons_width = 0;
	for (S32 i = 0; i < count; ++i)
	{
		buttons_width += buttonWidth + buttonHGap;
		if (buttons_width > buttons_space)
		{
			// There is no space for all buttons.
			// Calculating the number of buttons, that are fit with chevron button
			buttons_space -= chevron_button_width + buttonHGap;
			while (i >= 0 && buttons_width > buttons_space)
			{
				buttons_width -= buttonWidth + buttonHGap;
				i--;
			}
			first_drop_down_item = i + 1; // First item behind visible items
			
			break;
		}
	}

	bool recreate_buttons = true;

	// If inventory items are not changed up to mFirstDropDownItem, no need to recreate them
	if (mFirstDropDownItem == first_drop_down_item && (mItemNamesCache.size() == count || mItemNamesCache.size() == mFirstDropDownItem))
	{
		S32 i;
		for (i = 0; i < mFirstDropDownItem; ++i)
		{
			if (mItemNamesCache.get(i) != mItems.get(i)->getName())
			{
				break;
			}
		}
		if (i == mFirstDropDownItem)
		{
			recreate_buttons = false;
		}
	}

	if (recreate_buttons)
	{
		mFirstDropDownItem = first_drop_down_item;

		mItemNamesCache.clear();
		for (S32 i = 0; i < mFirstDropDownItem; i++)
		{
			mItemNamesCache.put(mItems.get(i)->getName());
		}

		// Rebuild the buttons only
		// child_list_t is a linked list, so safe to erase from the middle if we pre-incrament the iterator
		for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); )
		{
			child_list_const_iter_t cur_it = child_it++;
			LLView* viewp = *cur_it;
			LLButton* button = dynamic_cast<LLButton*>(viewp);
			if (button)
			{
				removeChild(button);
				delete button;
			}
		}

		createButtons(mItems, buttonXMLNode, buttonWidth, buttonHGap);
	}

	// Chevron button
	if (mFirstDropDownItem != count)
	{
		// Chevron button should stay right aligned
		LLView *chevron_button = findChildView(std::string(">>"), FALSE);
		if (chevron_button)
		{
			LLRect rect;
			rect.setOriginAndSize(bar_width - chevron_button_width - buttonHGap, 0, chevron_button_width, getRect().getHeight());
			chevron_button->setRect(rect);
			chevron_button->setVisible(TRUE);
			mChevronRect = rect;
		}
		else
		{
			static LLButton::Params default_button_params(LLUICtrlFactory::getDefaultParams<LLButton>());
			std::string flat_icon			= "transparent.j2c";
			std::string hover_icon			= default_button_params.image_unselected.name;
			std::string hover_icon_selected	= default_button_params.image_selected.name;

			LLButton::Params bparams;

			LLRect rect;
			rect.setOriginAndSize(bar_width - chevron_button_width - buttonHGap, 0, chevron_button_width, getRect().getHeight());

			bparams.follows.flags (FOLLOWS_LEFT | FOLLOWS_BOTTOM);
			bparams.image_unselected.name(flat_icon);
			bparams.image_disabled.name(flat_icon);
			bparams.image_selected.name(hover_icon_selected);
			bparams.image_hover_selected.name(hover_icon_selected);
			bparams.image_disabled_selected.name(hover_icon_selected);
			bparams.image_hover_unselected.name(hover_icon);
			bparams.rect (rect);
			bparams.tab_stop(false);
			bparams.font(mFont);
			bparams.name(">>");
			bparams.tool_tip(mChevronButtonToolTip);
			bparams.click_callback.function(boost::bind(&LLFavoritesBarCtrl::showDropDownMenu, this));

			addChildInBack(LLUICtrlFactory::create<LLButton> (bparams));

			mChevronRect = rect;
		}
	}
	else
	{
		// Hide chevron button if all items are visible on bar
		LLView *chevron_button = findChildView(std::string(">>"), FALSE);
		if (chevron_button)
		{
			chevron_button->setVisible(FALSE);
		}
	}
}


void LLFavoritesBarCtrl::createButtons(const LLInventoryModel::item_array_t &items, const LLXMLNodePtr &buttonXMLNode, S32 buttonWidth, S32 buttonHGap)
{
	S32 curr_x = buttonHGap;
	// Adding buttons

	LLFavoriteLandmarkButton* fav_btn = NULL;
	mLandingTab = mLastTab = NULL;

	for(S32 i = mFirstDropDownItem -1, j = 0; i >= 0; i--)
	{
		LLViewerInventoryItem* item = items.get(j++);

		fav_btn = LLUICtrlFactory::defaultBuilder<LLFavoriteLandmarkButton>(buttonXMLNode, this, NULL);
		if (NULL == fav_btn)
		{
			llwarns << "Unable to create button for landmark: " << item->getName() << llendl;
			continue;
		}

		fav_btn->setLandmarkID(item->getUUID());
		
		// change only left and save bottom
		fav_btn->setOrigin(curr_x, fav_btn->getRect().mBottom);
		fav_btn->setFont(mFont);
		fav_btn->setName(item->getName());
		fav_btn->setLabel(item->getName());
		fav_btn->setToolTip(item->getName());
		fav_btn->setCommitCallback(boost::bind(&LLFavoritesBarCtrl::onButtonClick, this, item->getUUID()));
		fav_btn->setRightMouseDownCallback(boost::bind(&LLFavoritesBarCtrl::onButtonRightClick, this, item->getUUID(), _1, _2, _3,_4 ));

		fav_btn->LLUICtrl::setMouseDownCallback(boost::bind(&LLFavoritesBarCtrl::onButtonMouseDown, this, item->getUUID(), _1, _2, _3, _4));
		fav_btn->LLUICtrl::setMouseUpCallback(boost::bind(&LLFavoritesBarCtrl::onButtonMouseUp, this, item->getUUID(), _1, _2, _3, _4));

		sendChildToBack(fav_btn);

		curr_x += buttonWidth + buttonHGap;
	}

	mLastTab = fav_btn;
}


BOOL LLFavoritesBarCtrl::postBuild()
{
	// make the popup menu available
	LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_favorites.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if (!menu)
	{
		menu = LLUICtrlFactory::getDefaultWidget<LLMenuGL>("inventory_menu");
	}
	menu->setBackgroundColor(LLUIColorTable::instance().getColor("MenuPopupBgColor"));
	mInventoryItemsPopupMenuHandle = menu->getHandle();

	return TRUE;
}

BOOL LLFavoritesBarCtrl::collectFavoriteItems(LLInventoryModel::item_array_t &items)
{
	if (mFavoriteFolderId.isNull())
		return FALSE;
	
	LLInventoryModel::cat_array_t cats;

	LLIsType is_type(LLAssetType::AT_LANDMARK);
	gInventory.collectDescendentsIf(mFavoriteFolderId, cats, items, LLInventoryModel::EXCLUDE_TRASH, is_type);

	std::sort(items.begin(), items.end(), LLFavoritesSort());

	if (needToSaveItemsOrder(items))
	{
		S32 sortField = 0;
		for (LLInventoryModel::item_array_t::iterator i = items.begin(); i != items.end(); ++i)
		{
			(*i)->setSortField(++sortField);
		}
	}

	return TRUE;
}

void LLFavoritesBarCtrl::showDropDownMenu()
{
	if (mPopupMenuHandle.isDead())
	{
		LLToggleableMenu::Params menu_p;
		menu_p.name("favorites menu");
		menu_p.can_tear_off(false);
		menu_p.visible(false);
		menu_p.scrollable(true);
		menu_p.max_scrollable_items = 10;
		menu_p.preferred_width = DROP_DOWN_MENU_WIDTH;

		LLToggleableMenu* menu = LLUICtrlFactory::create<LLFavoriteLandmarkToggleableMenu>(menu_p);
		mPopupMenuHandle = menu->getHandle();
	}

	LLToggleableMenu* menu = (LLToggleableMenu*)mPopupMenuHandle.get();

	if(menu)
	{
		if (!menu->toggleVisibility())
			return;

		mItems.clear();

		if (!collectFavoriteItems(mItems))
		{
			return;
		}

		S32 count = mItems.count();

		// Check it there are changed items, since last call
		if (mItemNamesCache.size() == count)
		{
			S32 i;
			for (i = mFirstDropDownItem; i < count; i++)
			{
				if (mItemNamesCache.get(i) != mItems.get(i)->getName())
				{
					break;
				}
			}

			// Check passed, just show the menu
			if (i == count)
			{
				menu->buildDrawLabels();
				menu->updateParent(LLMenuGL::sMenuContainer);

				menu->setButtonRect(mChevronRect, this);

				LLMenuGL::showPopup(this, menu, getRect().getWidth() - menu->getRect().getWidth(), 0);
				return;
			}
		}

		// Add menu items to cache, if there is only names of buttons
		if (mItemNamesCache.size() == mFirstDropDownItem)
		{
			for (S32 i = mFirstDropDownItem; i < count; i++)
			{
				mItemNamesCache.put(mItems.get(i)->getName());
			}
		}

		menu->empty();

		U32 max_width = llmin(DROP_DOWN_MENU_WIDTH, getRect().getWidth());
		U32 widest_item = 0;

		for(S32 i = mFirstDropDownItem; i < count; i++)
		{
			LLViewerInventoryItem* item = mItems.get(i);
			const std::string& item_name = item->getName();

			LLFavoriteLandmarkMenuItem::Params item_params;
			item_params.name(item_name);
			item_params.label(item_name);
			
			item_params.on_click.function(boost::bind(&LLFavoritesBarCtrl::onButtonClick, this, item->getUUID()));
			LLFavoriteLandmarkMenuItem *menu_item = LLUICtrlFactory::create<LLFavoriteLandmarkMenuItem>(item_params);
			menu_item->initFavoritesBarPointer(this);
			menu_item->setRightMouseDownCallback(boost::bind(&LLFavoritesBarCtrl::onButtonRightClick, this,item->getUUID(),_1,_2,_3,_4));
			menu_item->LLUICtrl::setMouseDownCallback(boost::bind(&LLFavoritesBarCtrl::onButtonMouseDown, this, item->getUUID(), _1, _2, _3, _4));
			menu_item->LLUICtrl::setMouseUpCallback(boost::bind(&LLFavoritesBarCtrl::onButtonMouseUp, this, item->getUUID(), _1, _2, _3, _4));
			menu_item->setLandmarkID(item->getUUID());

			// Check whether item name wider than menu
			if (menu_item->getNominalWidth() > max_width)
			{
				S32 chars_total = item_name.length();
				S32 chars_fitted = 1;
				menu_item->setLabel(LLStringExplicit(""));
				S32 label_space = max_width - menu_item->getFont()->getWidth("...") -
					menu_item->getNominalWidth(); // This returns width of menu item with empty label (pad pixels)

				while (chars_fitted < chars_total && menu_item->getFont()->getWidth(item_name, 0, chars_fitted) < label_space)
				{
					chars_fitted++;
				}
				chars_fitted--; // Rolling back one char, that doesn't fit

				menu_item->setLabel(item_name.substr(0, chars_fitted) + "...");
			}
			widest_item = llmax(widest_item, menu_item->getNominalWidth());

			menu->addChild(menu_item);
		}

		menu->buildDrawLabels();
		menu->updateParent(LLMenuGL::sMenuContainer);

		menu->setButtonRect(mChevronRect, this);

		LLMenuGL::showPopup(this, menu, getRect().getWidth() - max_width, 0);
		
	}
}

void LLFavoritesBarCtrl::onButtonClick(LLUUID item_id)
{
	// We only have one Inventory, gInventory. Some day this should be better abstracted.
	LLInvFVBridgeAction::doAction(item_id,&gInventory);
}

void LLFavoritesBarCtrl::onButtonRightClick( LLUUID item_id,LLView* fav_button,S32 x,S32 y,MASK mask)
{
	mSelectedItemID = item_id;
	
	LLMenuGL* menu = (LLMenuGL*)mInventoryItemsPopupMenuHandle.get();
	if (!menu)
	{
		return;
	}
	
	// Release mouse capture so hover events go to the popup menu
	// because this is happening during a mouse down.
	gFocusMgr.setMouseCapture(NULL);

	menu->updateParent(LLMenuGL::sMenuContainer);
	LLMenuGL::showPopup(fav_button, menu, x, y);
}

BOOL LLFavoritesBarCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = childrenHandleRightMouseDown( x, y, mask) != NULL;
	if(!handled && !gMenuHolder->hasVisibleMenu())
	{
		show_navbar_context_menu(this,x,y);
		handled = true;
	}
	
	return handled;
}
void copy_slurl_to_clipboard_cb(std::string& slurl)
{
	gClipboard.copyFromString(utf8str_to_wstring(slurl));
}


bool LLFavoritesBarCtrl::enableSelected(const LLSD& userdata)
{
    std::string param = userdata.asString();

    if (param == std::string("can_paste"))
    {
        return isClipboardPasteable();
    }

    return false;
}

void LLFavoritesBarCtrl::doToSelected(const LLSD& userdata)
{
	std::string action = userdata.asString();
	llinfos << "Action = " << action << " Item = " << mSelectedItemID.asString() << llendl;
	
	LLViewerInventoryItem* item = gInventory.getItem(mSelectedItemID);
	if (!item)
		return;
	
	if (action == "open")
	{
		teleport_via_landmark(item->getAssetUUID());
	}
	else if (action == "about")
	{
		LLSD key;
		key["type"] = "landmark";
		key["id"] = mSelectedItemID;

		LLSideTray::getInstance()->showPanel("panel_places", key);
	}
	else if (action == "copy_slurl")
	{
		LLVector3d posGlobal;
		LLLandmarkActions::getLandmarkGlobalPos(mSelectedItemID, posGlobal);

		if (!posGlobal.isExactlyZero())
		{
			LLLandmarkActions::getSLURLfromPosGlobal(posGlobal, copy_slurl_to_clipboard_cb);
		}
	}
	else if (action == "show_on_map")
	{
		LLFloaterWorldMap* worldmap_instance = LLFloaterWorldMap::getInstance();

		LLVector3d posGlobal;
		LLLandmarkActions::getLandmarkGlobalPos(mSelectedItemID, posGlobal);

		if (!posGlobal.isExactlyZero() && worldmap_instance)
		{
			worldmap_instance->trackLocation(posGlobal);
			LLFloaterReg::showInstance("world_map", "center");
		}
	}
	else if (action == "cut")
	{
	}
	else if (action == "copy")
	{
		LLInventoryClipboard::instance().store(mSelectedItemID);
	}
	else if (action == "paste")
	{
		pastFromClipboard();
	}
	else if (action == "delete")
	{
		gInventory.removeItem(mSelectedItemID);
	}
}

BOOL LLFavoritesBarCtrl::isClipboardPasteable() const
{
	if (!LLInventoryClipboard::instance().hasContents())
	{
		return FALSE;
	}

	LLDynamicArray<LLUUID> objects;
	LLInventoryClipboard::instance().retrieve(objects);
	S32 count = objects.count();
	for(S32 i = 0; i < count; i++)
	{
		const LLUUID &item_id = objects.get(i);

		// Can't paste folders
		const LLInventoryCategory *cat = gInventory.getCategory(item_id);
		if (cat)
		{
			return FALSE;
		}

		const LLInventoryItem *item = gInventory.getItem(item_id);
		if (item && LLAssetType::AT_LANDMARK != item->getType())
		{
			return FALSE;
		}
	}
	return TRUE;
}

void LLFavoritesBarCtrl::pastFromClipboard() const
{
	LLInventoryModel* model = &gInventory;
	if(model && isClipboardPasteable())
	{
		LLInventoryItem* item = NULL;
		LLDynamicArray<LLUUID> objects;
		LLInventoryClipboard::instance().retrieve(objects);
		S32 count = objects.count();
		LLUUID parent_id(mFavoriteFolderId);
		for(S32 i = 0; i < count; i++)
		{
			item = model->getItem(objects.get(i));
			if (item)
			{
				copy_inventory_item(
					gAgent.getID(),
					item->getPermissions().getOwner(),
					item->getUUID(),
					parent_id,
					std::string(),
					LLPointer<LLInventoryCallback>(NULL));
			}
		}
	}
}

void LLFavoritesBarCtrl::onButtonMouseDown(LLUUID id, LLUICtrl* ctrl, S32 x, S32 y, MASK mask)
{
	mDragItemId = id;
	mStartDrag = TRUE;

	S32 screenX, screenY;
	localPointToScreen(x, y, &screenX, &screenY);

	LLToolDragAndDrop::getInstance()->setDragStart(screenX, screenY);
}

void LLFavoritesBarCtrl::onButtonMouseUp(LLUUID id, LLUICtrl* ctrl, S32 x, S32 y, MASK mask)
{
	mStartDrag = FALSE;
	mDragItemId = LLUUID::null;
}

void LLFavoritesBarCtrl::onEndDrag()
{
	mEndDragConnection.disconnect();

	showDragMarker(FALSE);
	mDragItemId = LLUUID::null;
	LLView::getWindow()->setCursor(UI_CURSOR_ARROW);
}

BOOL LLFavoritesBarCtrl::handleHover(S32 x, S32 y, MASK mask)
{
	if (mDragItemId != LLUUID::null && mStartDrag)
	{
		S32 screenX, screenY;
		localPointToScreen(x, y, &screenX, &screenY);

		if(LLToolDragAndDrop::getInstance()->isOverThreshold(screenX, screenY))
		{
			LLToolDragAndDrop::getInstance()->beginDrag(
				DAD_LANDMARK, mDragItemId,
				LLToolDragAndDrop::SOURCE_LIBRARY);

			mStartDrag = FALSE;

			return LLToolDragAndDrop::getInstance()->handleHover(x, y, mask);
		}
	}

	return TRUE;
}

LLUICtrl* LLFavoritesBarCtrl::findChildByLocalCoords(S32 x, S32 y)
{
	LLUICtrl* ctrl = 0;
	S32 screenX, screenY;
	const child_list_t* list = getChildList();

	localPointToScreen(x, y, &screenX, &screenY);

	// look for a child which contains the point (screenX, screenY) in it's rectangle
	for (child_list_const_iter_t i = list->begin(); i != list->end(); ++i)
	{
		LLRect rect;
		localRectToScreen((*i)->getRect(), &rect);

		if (rect.pointInRect(screenX, screenY))
		{
			ctrl = dynamic_cast<LLUICtrl*>(*i);
			break;
		}
	}

	return ctrl;
}

BOOL LLFavoritesBarCtrl::needToSaveItemsOrder(const LLInventoryModel::item_array_t& items)
{
	BOOL result = FALSE;

	// if there is an item without sort order field set, we need to save items order
	for (LLInventoryModel::item_array_t::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		if ((*i)->getSortField() < 0)
		{
			result = TRUE;
			break;
		}
	}

	return result;
}

void LLFavoritesBarCtrl::saveItemsOrder(LLInventoryModel::item_array_t& items)
{
	int sortField = 0;

	// current order is saved by setting incremental values (1, 2, 3, ...) for the sort field
	for (LLInventoryModel::item_array_t::iterator i = items.begin(); i != items.end(); ++i)
	{
		LLViewerInventoryItem* item = *i;

		item->setSortField(++sortField);
		item->setComplete(TRUE);
		item->updateServer(FALSE);

		gInventory.updateItem(item);
	}

	gInventory.notifyObservers();
}

LLInventoryModel::item_array_t::iterator LLFavoritesBarCtrl::findItemByUUID(LLInventoryModel::item_array_t& items, const LLUUID& id)
{
	LLInventoryModel::item_array_t::iterator result = items.end();

	for (LLInventoryModel::item_array_t::iterator i = items.begin(); i != items.end(); ++i)
	{
		if ((*i)->getUUID() == id)
		{
			result = i;
			break;
		}
	}

	return result;
}

void LLFavoritesBarCtrl::updateItemsOrder(LLInventoryModel::item_array_t& items, const LLUUID& srcItemId, const LLUUID& destItemId)
{
	LLViewerInventoryItem* srcItem = gInventory.getItem(srcItemId);
	LLViewerInventoryItem* destItem = gInventory.getItem(destItemId);

	items.erase(findItemByUUID(items, srcItem->getUUID()));
	items.insert(findItemByUUID(items, destItem->getUUID()), srcItem);
}

void LLFavoritesBarCtrl::insertBeforeItem(LLInventoryModel::item_array_t& items, const LLUUID& beforeItemId, const LLUUID& insertedItemId)
{
	LLViewerInventoryItem* beforeItem = gInventory.getItem(beforeItemId);
	LLViewerInventoryItem* insertedItem = gInventory.getItem(insertedItemId);

	items.insert(findItemByUUID(items, beforeItem->getUUID()), insertedItem);
}

// EOF
