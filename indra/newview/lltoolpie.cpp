/** 
 * @file lltoolpie.cpp
 * @brief LLToolPie class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lltoolpie.h"

#include "indra_constants.h"
#include "llclickaction.h"
#include "llparcel.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llavatarnamecache.h"
#include "llviewercontrol.h"
#include "llfocusmgr.h"
//#include "llfirstuse.h"
#include "llfloaterland.h"
#include "llfloaterreg.h"
#include "llfloaterscriptdebug.h"
#include "lltooltip.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llkeyboard.h"
#include "llmediaentry.h"
#include "llmenugl.h"
#include "llmutelist.h"
#include "piemenu.h"	// ## Zi: Pie menu
#include "llresmgr.h"  // getMonetaryString
#include "llselectmgr.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolmgr.h"
#include "lltoolselect.h"
#include "lltrans.h"
#include "llviewercamera.h"
#include "llviewerparcelmedia.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerobject.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llviewermedia.h"
#include "llvoavatarself.h"
#include "llviewermediafocus.h"
#include "llworld.h"
#include "llui.h"
#include "llweb.h"
#include "pipeline.h"	// setHighlightObject
// [RLVa:KB] - Checked: 2010-03-06 (RLVa-1.2.0c)
#include "rlvhandler.h"
// [/RLVa:KB]

extern BOOL gDebugClicks;

static void handle_click_action_play();
static void handle_click_action_open_media(LLPointer<LLViewerObject> objectp);
static ECursorType cursor_from_parcel_media(U8 click_action);


LLToolPie::LLToolPie()
:	LLTool(std::string("Pie")),
	mGrabMouseButtonDown( FALSE ),
	mMouseOutsideSlop( FALSE ),
	mClickAction(0),
	mClickActionBuyEnabled( gSavedSettings.getBOOL("ClickActionBuyEnabled") ),
	mClickActionPayEnabled( gSavedSettings.getBOOL("ClickActionPayEnabled") )
{
}

BOOL LLToolPie::handleAnyMouseClick(S32 x, S32 y, MASK mask, EClickType clicktype, BOOL down)
{
	BOOL result = LLMouseHandler::handleAnyMouseClick(x, y, mask, clicktype, down);
	
	// This override DISABLES the keyboard focus reset that LLTool::handleAnyMouseClick adds.
	// LLToolPie will do the right thing in its pick callback.
	
	return result;
}

BOOL LLToolPie::handleMouseDown(S32 x, S32 y, MASK mask)
{
	//left mouse down always picks transparent
	mPick = gViewerWindow->pickImmediate(x, y, TRUE);
	mPick.mKeyMask = mask;
	mGrabMouseButtonDown = TRUE;
	
	pickLeftMouseDownCallback();

	return TRUE;
}

// Spawn context menus on right mouse down so you can drag over and select
// an item.
BOOL LLToolPie::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	// don't pick transparent so users can't "pay" transparent objects
	mPick = gViewerWindow->pickImmediate(x, y, FALSE);
	mPick.mKeyMask = mask;

	// claim not handled so UI focus stays same
	
	pickRightMouseDownCallback();
	
	return FALSE;
}

BOOL LLToolPie::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	LLToolMgr::getInstance()->clearTransientTool();
	return LLTool::handleRightMouseUp(x, y, mask);
}

BOOL LLToolPie::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	return LLViewerMediaFocus::getInstance()->handleScrollWheel(x, y, clicks);
}

// True if you selected an object.
BOOL LLToolPie::pickLeftMouseDownCallback()
{
	S32 x = mPick.mMousePt.mX;
	S32 y = mPick.mMousePt.mY;
	MASK mask = mPick.mKeyMask;
	if (mPick.mPickType == LLPickInfo::PICK_PARCEL_WALL)
	{
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getCollisionParcel();
		if (parcel)
		{
			LLViewerParcelMgr::getInstance()->selectCollisionParcel();
			if (parcel->getParcelFlag(PF_USE_PASS_LIST) 
				&& !LLViewerParcelMgr::getInstance()->isCollisionBanned())
			{
				// if selling passes, just buy one
				void* deselect_when_done = (void*)TRUE;
				LLPanelLandGeneral::onClickBuyPass(deselect_when_done);
			}
			else
			{
				// not selling passes, get info
				LLFloaterReg::showInstance("about_land");
			}
		}

		gFocusMgr.setKeyboardFocus(NULL);
		return LLTool::handleMouseDown(x, y, mask);
	}

	// didn't click in any UI object, so must have clicked in the world
	LLViewerObject *object = mPick.getObject();
	LLViewerObject *parent = NULL;

	if (mPick.mPickType != LLPickInfo::PICK_LAND)
	{
		LLViewerParcelMgr::getInstance()->deselectLand();
	}
	
	if (object)
	{
		parent = object->getRootEdit();
	}

	if (handleMediaClick(mPick))
	{
		return TRUE;
	}

	// If it's a left-click, and we have a special action, do it.
	if (useClickAction(mask, object, parent))
	{
// [RLVa:KB] - Checked: 2010-03-11 (RLVa-1.2.0e) | Modified: RLVa-1.1.0l
		// Block left-click special actions when fartouch restricted
		if ( (rlv_handler_t::isEnabled()) && 
			 (gRlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) && (!gRlvHandler.canTouch(object, mPick.mObjectOffset)) )
		{
			return TRUE;
		}
// [/RLVa:KB]

		mClickAction = 0;
		if (object && object->getClickAction()) 
		{
			mClickAction = object->getClickAction();
		}
		else if (parent && parent->getClickAction()) 
		{
			mClickAction = parent->getClickAction();
		}

		switch(mClickAction)
		{
		case CLICK_ACTION_TOUCH:
			// touch behavior down below...
			break;
		case CLICK_ACTION_SIT:
			{
				if (isAgentAvatarValid() && !gAgentAvatarp->isSitting()) // agent not already sitting
				{
					handle_object_sit_or_stand();
					// put focus in world when sitting on an object
					gFocusMgr.setKeyboardFocus(NULL);
					return TRUE;
				} // else nothing (fall through to touch)
			}
		case CLICK_ACTION_PAY:
			if ( mClickActionPayEnabled )
			{
				if ((object && object->flagTakesMoney())
					|| (parent && parent->flagTakesMoney()))
				{
					// pay event goes to object actually clicked on
					mClickActionObject = object;
					mLeftClickSelection = LLToolSelect::handleObjectSelection(mPick, FALSE, TRUE);
					if (LLSelectMgr::getInstance()->selectGetAllValid())
					{
						// call this right away, since we have all the info we need to continue the action
						selectionPropertiesReceived();
					}
					return TRUE;
				}
			}
			break;
		case CLICK_ACTION_BUY:
			if ( mClickActionBuyEnabled )
			{
				mClickActionObject = parent;
				mLeftClickSelection = LLToolSelect::handleObjectSelection(mPick, FALSE, TRUE, TRUE);
				if (LLSelectMgr::getInstance()->selectGetAllValid())
				{
					// call this right away, since we have all the info we need to continue the action
					selectionPropertiesReceived();
				}
				return TRUE;
			}
			break;
		case CLICK_ACTION_OPEN:
			if (parent && parent->allowOpen())
			{
				mClickActionObject = parent;
				mLeftClickSelection = LLToolSelect::handleObjectSelection(mPick, FALSE, TRUE, TRUE);
				if (LLSelectMgr::getInstance()->selectGetAllValid())
				{
					// call this right away, since we have all the info we need to continue the action
					selectionPropertiesReceived();
				}
			}
			return TRUE;	
		case CLICK_ACTION_PLAY:
			handle_click_action_play();
			return TRUE;
		case CLICK_ACTION_OPEN_MEDIA:
			// mClickActionObject = object;
			handle_click_action_open_media(object);
			return TRUE;
		case CLICK_ACTION_ZOOM:
			{	
				const F32 PADDING_FACTOR = 2.f;
				LLViewerObject* object = gObjectList.findObject(mPick.mObjectID);
				
				if (object)
				{
					gAgentCamera.setFocusOnAvatar(FALSE, ANIMATE);
					
					LLBBox bbox = object->getBoundingBoxAgent() ;
					F32 angle_of_view = llmax(0.1f, LLViewerCamera::getInstance()->getAspect() > 1.f ? LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect() : LLViewerCamera::getInstance()->getView());
					F32 distance = bbox.getExtentLocal().magVec() * PADDING_FACTOR / atan(angle_of_view);
				
					LLVector3 obj_to_cam = LLViewerCamera::getInstance()->getOrigin() - bbox.getCenterAgent();
					obj_to_cam.normVec();
					
					LLVector3d object_center_global = gAgent.getPosGlobalFromAgent(bbox.getCenterAgent());
					gAgentCamera.setCameraPosAndFocusGlobal(object_center_global + LLVector3d(obj_to_cam * distance), 
													  object_center_global, 
													  mPick.mObjectID );
				}
			}
			return TRUE;			
		default:
			// nothing
			break;
		}
	}

	// put focus back "in world"
	gFocusMgr.setKeyboardFocus(NULL);

	BOOL touchable = (object && object->flagHandleTouch()) 
					 || (parent && parent->flagHandleTouch());

	// Switch to grab tool if physical or triggerable
	if (object && 
		!object->isAvatar() && 
		((object->usePhysics() || (parent && !parent->isAvatar() && parent->usePhysics())) || touchable) 
		)
	{
// [RLVa:KB] - Checked: 2010-03-11 (RLVa-1.2.0e) | Modified: RLVa-1.1.0l
		// Triggered by left-clicking on a touchable object
		if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canTouch(object, mPick.mObjectOffset)) )
		{
			return LLTool::handleMouseDown(x, y, mask);
		}
// [/RLVa:KB]

		gGrabTransientTool = this;
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( LLToolGrab::getInstance() );
		return LLToolGrab::getInstance()->handleObjectHit( mPick );
	}
	
	LLHUDIcon* last_hit_hud_icon = mPick.mHUDIcon;
	if (!object && last_hit_hud_icon && last_hit_hud_icon->getSourceObject())
	{
		LLFloaterScriptDebug::show(last_hit_hud_icon->getSourceObject()->getID());
	}

	// If left-click never selects or spawns a menu
	// Eat the event.
	if (!gSavedSettings.getBOOL("LeftClickShowMenu"))
	{
		// mouse already released
		if (!mGrabMouseButtonDown)
		{
			return TRUE;
		}

		while( object && object->isAttachment() && !object->flagHandleTouch())
		{
			// don't pick avatar through hud attachment
			if (object->isHUDAttachment())
			{
				break;
			}
			object = (LLViewerObject*)object->getParent();
		}
		if (object && object == gAgentAvatarp)
		{
			// we left clicked on avatar, switch to focus mode
			LLToolMgr::getInstance()->setTransientTool(LLToolCamera::getInstance());
			gViewerWindow->hideCursor();
			LLToolCamera::getInstance()->setMouseCapture(TRUE);
			LLToolCamera::getInstance()->pickCallback(mPick);
			if(!gSavedSettings.getBOOL("ClickOnAvatarKeepsCamera"))		// keep camera in place when clicking on ourselves -Zi
				gAgentCamera.setFocusOnAvatar(TRUE, TRUE);

			return TRUE;
		}
	//////////
	//	// Could be first left-click on nothing
	//	LLFirstUse::useLeftClickNoHit();
	/////////
		
		// Eat the event
		return LLTool::handleMouseDown(x, y, mask);
	}

	if (gAgent.leftButtonGrabbed())
	{
		// if the left button is grabbed, don't put up the pie menu
		return LLTool::handleMouseDown(x, y, mask);
	}

	// Can't ignore children here.
	LLToolSelect::handleObjectSelection(mPick, FALSE, TRUE);

	// Spawn pie menu
	LLTool::handleRightMouseDown(x, y, mask);
	return TRUE;
}

BOOL LLToolPie::useClickAction(MASK mask, 
							   LLViewerObject* object, 
							   LLViewerObject* parent)
{
	return	mask == MASK_NONE
			&& object
			&& !object->isAttachment() 
			&& LLPrimitive::isPrimitive(object->getPCode())
			&& (object->getClickAction() 
				|| parent->getClickAction());

}

U8 final_click_action(LLViewerObject* obj)
{
	if (!obj) return CLICK_ACTION_NONE;
	if (obj->isAttachment()) return CLICK_ACTION_NONE;

	U8 click_action = CLICK_ACTION_TOUCH;
	LLViewerObject* parent = obj->getRootEdit();
	if (obj->getClickAction()
	    || (parent && parent->getClickAction()))
	{
		if (obj->getClickAction())
		{
			click_action = obj->getClickAction();
		}
		else if (parent && parent->getClickAction())
		{
			click_action = parent->getClickAction();
		}
	}
	return click_action;
}

ECursorType LLToolPie::cursorFromObject(LLViewerObject* object)
{
	LLViewerObject* parent = NULL;
	if (object)
	{
		parent = object->getRootEdit();
	}
	U8 click_action = final_click_action(object);
	ECursorType cursor = UI_CURSOR_ARROW;
	switch(click_action)
	{
	case CLICK_ACTION_SIT:
		{
//			if (isAgentAvatarValid() && !gAgentAvatarp->isSitting()) // not already sitting?
// [RLVa:KB] - Checked: 2010-03-06 (RLVa-1.2.0c) | Modified: RLVa-1.2.0g
			if ( (isAgentAvatarValid() && !gAgentAvatarp->isSitting()) && 
				 ((!rlv_handler_t::isEnabled()) || (gRlvHandler.canSit(object, LLToolPie::getInstance()->getHoverPick().mObjectOffset))) )
// [/RLVa:KB]
			{
				cursor = UI_CURSOR_TOOLSIT;
			}
		}
		break;
	case CLICK_ACTION_BUY:
		if ( mClickActionBuyEnabled )
		{
			cursor = UI_CURSOR_TOOLBUY;
		}
		break;
	case CLICK_ACTION_OPEN:
		// Open always opens the parent.
		if (parent && parent->allowOpen())
		{
			cursor = UI_CURSOR_TOOLOPEN;
		}
		break;
	case CLICK_ACTION_PAY:	
		if ( mClickActionPayEnabled )
		{
			if ((object && object->flagTakesMoney())
				|| (parent && parent->flagTakesMoney()))
			{
				//cursor = UI_CURSOR_TOOLBUY;  FIRESTORM - pay cursor is separate from buy cursor
				cursor = UI_CURSOR_TOOLPAY;
			}
		}
		break;
	case CLICK_ACTION_ZOOM:
			cursor = UI_CURSOR_TOOLZOOMIN;
			break;			
	case CLICK_ACTION_PLAY:
	case CLICK_ACTION_OPEN_MEDIA: 
		cursor = cursor_from_parcel_media(click_action);
		break;
	default:
		break;
	}
	return cursor;
}

void LLToolPie::resetSelection()
{
	mLeftClickSelection = NULL;
	mClickActionObject = NULL;
	mClickAction = 0;
}

// When we get object properties after left-clicking on an object
// with left-click = buy, if it's the same object, do the buy.

// static
void LLToolPie::selectionPropertiesReceived()
{
	// Make sure all data has been received.
	// This function will be called repeatedly as the data comes in.
	if (!LLSelectMgr::getInstance()->selectGetAllValid())
	{
		return;
	}

	LLObjectSelection* selection = LLToolPie::getInstance()->getLeftClickSelection();
	if (selection)
	{
		LLViewerObject* selected_object = selection->getPrimaryObject();
		// since we don't currently have a way to lock a selection, it could have changed
		// after we initially clicked on the object
		if (selected_object == LLToolPie::getInstance()->getClickActionObject())
		{
			U8 click_action = LLToolPie::getInstance()->getClickAction();
			switch (click_action)
			{
			case CLICK_ACTION_BUY:
				if ( LLToolPie::getInstance()->mClickActionBuyEnabled )
				{
					handle_buy();
				}
				break;
			case CLICK_ACTION_PAY:
				if ( LLToolPie::getInstance()->mClickActionPayEnabled )
				{
					handle_give_money_dialog();
				}
				break;
			case CLICK_ACTION_OPEN:
				LLFloaterReg::showInstance("openobject");
				break;
			default:
				break;
			}
		}
	}
	LLToolPie::getInstance()->resetSelection();
}

BOOL LLToolPie::handleHover(S32 x, S32 y, MASK mask)
{
	mHoverPick = gViewerWindow->pickImmediate(x, y, FALSE);
	// perform a separate pick that detects transparent objects since they respond to 1-click actions
	LLPickInfo click_action_pick = gViewerWindow->pickImmediate(x, y, TRUE);

	// Show screen-space highlight glow effect
	bool show_highlight = false;
	LLViewerObject *parent = NULL;
	LLViewerObject *object = mHoverPick.getObject();

// [RLVa:KB] - Checked: 2010-03-11 (RLVa-1.2.0e) | Modified: RLVa-1.1.0l
	// Block all special click action cursors when:
	//   - @fartouch=n restricted and the object is out of range
	//   - @interact=n restricted and the object isn't a HUD attachment
	if ( (object) && (rlv_handler_t::isEnabled()) && 
		( ((gRlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH))) && (!gRlvHandler.canTouch(object, mHoverPick.mObjectOffset)) || 
		  ((gRlvHandler.hasBehaviour(RLV_BHVR_INTERACT)) && (!object->isHUDAttachment())) ) )
	{
		gViewerWindow->setCursor(UI_CURSOR_ARROW);
		return TRUE;
	}
// [/RLVa:KB]

	if (object)
	{
		parent = object->getRootEdit();
	}

	LLViewerObject* click_action_object = click_action_pick.getObject();
	if (handleMediaHover(mHoverPick))
	{
		// *NOTE: If you think the hover glow conflicts with the media outline, you
		// could disable it here.
		show_highlight = true;
		// cursor set by media object
		lldebugst(LLERR_USER_INPUT) << "hover handled by LLToolPie (inactive)" << llendl;
	}
	else if (click_action_object && useClickAction(mask, click_action_object, click_action_object->getRootEdit()))
	{
		show_highlight = true;
		ECursorType cursor = cursorFromObject(click_action_object);
		gViewerWindow->setCursor(cursor);
		lldebugst(LLERR_USER_INPUT) << "hover handled by LLToolPie (inactive)" << llendl;
	}
// [RLVa:KB] - Checked: 2010-03-11 (RLVa-1.2.0e) | Added: RLVa-1.1.0l
	else if ( (object) && (rlv_handler_t::isEnabled()) && (!gRlvHandler.canTouch(object)) )
	{
		// Block showing the "grab" or "touch" cursor if we can't touch the object (@fartouch=n is handled above)
		gViewerWindow->setCursor(UI_CURSOR_ARROW);
	}
// [/RLVa:KB]
	else if ((object && !object->isAvatar() && object->usePhysics()) 
			 || (parent && !parent->isAvatar() && parent->usePhysics()))
	{
		show_highlight = true;
		gViewerWindow->setCursor(UI_CURSOR_TOOLGRAB);
		lldebugst(LLERR_USER_INPUT) << "hover handled by LLToolPie (inactive)" << llendl;
	}
	else if ( (object && object->flagHandleTouch()) 
			  || (parent && parent->flagHandleTouch()))
	{
		show_highlight = true;
		gViewerWindow->setCursor(UI_CURSOR_HAND);
		lldebugst(LLERR_USER_INPUT) << "hover handled by LLToolPie (inactive)" << llendl;
	}
	else
	{
		gViewerWindow->setCursor(UI_CURSOR_ARROW);
		lldebugst(LLERR_USER_INPUT) << "hover handled by LLToolPie (inactive)" << llendl;

		if(!object)
		{
			LLViewerMediaFocus::getInstance()->clearHover();
		}
	}

	static LLCachedControl<bool> enable_highlight(
		gSavedSettings, "RenderHoverGlowEnable", false);
	LLDrawable* drawable = NULL;
	if (enable_highlight && show_highlight && object)
	{
		drawable = object->mDrawable;
	}
	gPipeline.setHighlightObject(drawable);

	return TRUE;
}

BOOL LLToolPie::handleMouseUp(S32 x, S32 y, MASK mask)
{
	LLViewerObject* obj = mPick.getObject();

	handleMediaMouseUp();

	U8 click_action = final_click_action(obj);
	if (click_action != CLICK_ACTION_NONE)
	{
		switch(click_action)
		{
			// NOTE: mClickActionBuyEnabled flag enables/disables BUY action but setting cursor to default is okay
		case CLICK_ACTION_BUY:
			// NOTE: mClickActionPayEnabled flag enables/disables PAY action but setting cursor to default is okay
		case CLICK_ACTION_PAY:
		case CLICK_ACTION_OPEN:
		case CLICK_ACTION_ZOOM:
		case CLICK_ACTION_PLAY:
		case CLICK_ACTION_OPEN_MEDIA:
			// Because these actions open UI dialogs, we won't change
			// the cursor again until the next hover and GL pick over
			// the world.  Keep the cursor an arrow, assuming that 
			// after the user moves off the UI, they won't be on the
			// same object anymore.
			gViewerWindow->setCursor(UI_CURSOR_ARROW);
			// Make sure the hover-picked object is ignored.
			//gToolTipView->resetLastHoverObject();
			break;
		default:
			break;
		}
	}

	mGrabMouseButtonDown = FALSE;
	LLToolMgr::getInstance()->clearTransientTool();
	gAgentCamera.setLookAt(LOOKAT_TARGET_CONVERSATION, obj); // maybe look at object/person clicked on
	return LLTool::handleMouseUp(x, y, mask);
}


BOOL LLToolPie::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (gDebugClicks)
	{
		llinfos << "LLToolPie handleDoubleClick (becoming mouseDown)" << llendl;
	}

	if (gSavedSettings.getBOOL("DoubleClickAutoPilot"))
	{
		if (mPick.mPickType == LLPickInfo::PICK_LAND
			&& !mPick.mPosGlobal.isExactlyZero())
		{
			handle_go_to();
			return TRUE;
		}
		else if (mPick.mObjectID.notNull()
				 && !mPick.mPosGlobal.isExactlyZero())
		{
			handle_go_to();
			return TRUE;
		}
	}
	else if (gSavedSettings.getBOOL("DoubleClickTeleport"))
	{
		LLViewerObject* objp = mPick.getObject();
		LLViewerObject* parentp = objp ? objp->getRootEdit() : NULL;

		bool is_in_world = mPick.mObjectID.notNull() && objp && !objp->isHUDAttachment();
		bool is_land = mPick.mPickType == LLPickInfo::PICK_LAND;
		bool pos_non_zero = !mPick.mPosGlobal.isExactlyZero();
		bool has_touch_handler = (objp && objp->flagHandleTouch()) || (parentp && parentp->flagHandleTouch());
		bool has_click_action = final_click_action(objp);

		if (pos_non_zero && (is_land || (is_in_world && !has_touch_handler && !has_click_action)))
		{
			LLVector3d pos = mPick.mPosGlobal;
			pos.mdV[VZ] += gAgentAvatarp->getPelvisToFoot();
			gAgent.teleportViaLocationLookAt(pos);
			return TRUE;
		}
	}

	return FALSE;
}

static bool needs_tooltip(LLSelectNode* nodep)
{
	if (!nodep) 
		return false;

	LLViewerObject* object = nodep->getObject();
	LLViewerObject *parent = (LLViewerObject *)object->getParent();
	if (object->flagHandleTouch()
		|| (parent && parent->flagHandleTouch())
		|| object->flagTakesMoney()
		|| (parent && parent->flagTakesMoney())
		|| object->flagAllowInventoryAdd()
		)
	{
		return true;
	}

	U8 click_action = final_click_action(object);
	if (click_action != 0)
	{
		return true;
	}

	if (nodep->mValid)
	{
		bool anyone_copy = anyone_copy_selection(nodep);
		bool for_sale = for_sale_selection(nodep);
		if (anyone_copy || for_sale)
		{
			return true;
		}
	}
	return false;
}


BOOL LLToolPie::handleTooltipLand(std::string line, std::string tooltip_msg)
{
	LLViewerParcelMgr::getInstance()->setHoverParcel( mHoverPick.mPosGlobal );
	// 
	//  Do not show hover for land unless prefs are set to allow it.
	// 
	
	if (!gSavedSettings.getBOOL("ShowLandHoverTip")) return TRUE; 
	
	// Didn't hit an object, but since we have a land point we
	// must be hovering over land.
	
	LLParcel* hover_parcel = LLViewerParcelMgr::getInstance()->getHoverParcel();
	LLUUID owner;
	S32 width = 0;
	S32 height = 0;
	
	if ( hover_parcel )
	{
		owner = hover_parcel->getOwnerID();
		width = S32(LLViewerParcelMgr::getInstance()->getHoverParcelWidth());
		height = S32(LLViewerParcelMgr::getInstance()->getHoverParcelHeight());
	}
	
	// Line: "Land"
	line.clear();
	line.append(LLTrans::getString("TooltipLand"));
	if (hover_parcel)
	{
		line.append(hover_parcel->getName());
	}
	tooltip_msg.append(line);
	tooltip_msg.push_back('\n');
	
	// Line: "Owner: James Linden"
	line.clear();
	line.append(LLTrans::getString("TooltipOwner") + " ");
	
	if ( hover_parcel )
	{
		std::string name;
		if (LLUUID::null == owner)
		{
			line.append(LLTrans::getString("TooltipPublic"));
		}
		else if (hover_parcel->getIsGroupOwned())
		{
			if (gCacheName->getGroupName(owner, name))
			{
				line.append(name);
				line.append(LLTrans::getString("TooltipIsGroup"));
			}
			else
			{
				line.append(LLTrans::getString("RetrievingData"));
			}
		}
		else if(gCacheName->getFullName(owner, name))
		{
			line.append(name);
		}
		else
		{
			line.append(LLTrans::getString("RetrievingData"));
		}
	}
	else
	{
		line.append(LLTrans::getString("RetrievingData"));
	}
	tooltip_msg.append(line);
	tooltip_msg.push_back('\n');
	
	// Line: "no fly, not safe, no build"
	
	// Don't display properties for your land.  This is just
	// confusing, because you can do anything on your own land.
	if ( hover_parcel && owner != gAgent.getID() )
	{
		S32 words = 0;
		
		line.clear();
		// JC - Keep this in the same order as the checkboxes
		// on the land info panel
		if ( !hover_parcel->getAllowModify() )
		{
			if ( hover_parcel->getAllowGroupModify() )
			{
				line.append(LLTrans::getString("TooltipFlagGroupBuild"));
			}
			else
			{
				line.append(LLTrans::getString("TooltipFlagNoBuild"));
			}
			words++;
		}
		
		if ( !hover_parcel->getAllowTerraform() )
		{
			if (words) line.append(", ");
			line.append(LLTrans::getString("TooltipFlagNoEdit"));
			words++;
		}
		
		if ( hover_parcel->getAllowDamage() )
		{
			if (words) line.append(", ");
			line.append(LLTrans::getString("TooltipFlagNotSafe"));
			words++;
		}
		
		// Maybe we should reflect the estate's block fly bit here as well?  DK 12/1/04
		if ( !hover_parcel->getAllowFly() )
		{
			if (words) line.append(", ");
			line.append(LLTrans::getString("TooltipFlagNoFly"));
			words++;
		}
		
		if ( !hover_parcel->getAllowOtherScripts() )
		{
			if (words) line.append(", ");
			if ( hover_parcel->getAllowGroupScripts() )
			{
				line.append(LLTrans::getString("TooltipFlagGroupScripts"));
			}
			else
			{
				line.append(LLTrans::getString("TooltipFlagNoScripts"));
			}
			
			words++;
		}
		
		if (words) 
		{
			tooltip_msg.append(line);
			tooltip_msg.push_back('\n');
		}
	}
	
	if (hover_parcel && hover_parcel->getParcelFlag(PF_FOR_SALE))
	{
		LLStringUtil::format_map_t args;
		S32 price = hover_parcel->getSalePrice();
		args["[AMOUNT]"] = LLResMgr::getInstance()->getMonetaryString(price);
		line = LLTrans::getString("TooltipForSaleL$", args);
		tooltip_msg.append(line);
		tooltip_msg.push_back('\n');
	}
	
	// trim last newlines
	if (!tooltip_msg.empty())
	{
		tooltip_msg.erase(tooltip_msg.size() - 1);
		LLToolTipMgr::instance().show(tooltip_msg);
	}
	
	return TRUE;
}

BOOL LLToolPie::handleTooltipObject( LLViewerObject* hover_object, std::string line, std::string tooltip_msg)
{
	if ( hover_object->isHUDAttachment() )
	{
		// no hover tips for HUD elements, since they can obscure
		// what the HUD is displaying
		return TRUE;
	}
	
	if ( hover_object->isAttachment() )
	{
		// get root of attachment then parent, which is avatar
		LLViewerObject* root_edit = hover_object->getRootEdit();
		if (!root_edit)
		{
			// Strange parenting issue, don't show any text
			return TRUE;
		}
		hover_object = (LLViewerObject*)root_edit->getParent();
		if (!hover_object)
		{
			// another strange parenting issue, bail out
			return TRUE;
		}
	}
	
	line.clear();
	if (hover_object->isAvatar())
	{
		// only show tooltip if same inspector not already open
		LLFloater* existing_inspector = LLFloaterReg::findInstance("inspect_avatar");
		if (!existing_inspector 
			|| !existing_inspector->getVisible()
			|| existing_inspector->getKey()["avatar_id"].asUUID() != hover_object->getID())
		{
			// IDEVO: try to get display name + username
			std::string final_name;
			std::string full_name;
			if (!gCacheName->getFullName(hover_object->getID(), full_name))
			{
			LLNameValue* firstname = hover_object->getNVPair("FirstName");
			LLNameValue* lastname =  hover_object->getNVPair("LastName");
			if (firstname && lastname)
			{
					full_name = LLCacheName::buildFullName(
						firstname->getString(), lastname->getString());
				}
				else
				{
					full_name = LLTrans::getString("TooltipPerson");
				}
			}

			LLAvatarName av_name;
			if (LLAvatarNameCache::useDisplayNames() && 
				LLAvatarNameCache::get(hover_object->getID(), &av_name))
			{
//				final_name = av_name.getCompleteName();
// [RLVa:KB] - Checked: 2010-10-31 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
				final_name = (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? av_name.getCompleteName() : RlvStrings::getAnonym(av_name);
// [/RLVa:KB]
			}
			else
			{
//				final_name = full_name;
// [RLVa:KB] - Checked: 2010-10-31 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
				final_name = (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? full_name : RlvStrings::getAnonym(full_name);
// [/RLVa:KB]
			}

			// *HACK: We may select this object, so pretend it was clicked
			mPick = mHoverPick;
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.2a) | Added: RLVa-1.2.0e
			if ( (!rlv_handler_t::isEnabled()) || 
				 ( (gRlvHandler.canTouch(hover_object, mHoverPick.mObjectOffset)) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ) )
			{
// [/RLVa:KB]
				LLInspector::Params p;
				p.fillFrom(LLUICtrlFactory::instance().getDefaultParams<LLInspector>());
				p.message(final_name);
				p.image.name("Inspector_I");
				p.click_callback(boost::bind(showAvatarInspector, hover_object->getID()));
				p.visible_time_near(6.f);
				p.visible_time_far(3.f);
				p.delay_time(0.35f);
				p.wrap(false);
				
				LLToolTipMgr::instance().show(p);
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.2a) | Added: RLVa-1.2.0e
			}
			else
			{
				LLToolTipMgr::instance().show(final_name);
			}
// [/RLVa:KB]
		}
	}
	else
	{
		//
		//  We have hit a regular object (not an avatar or attachment)
		// 
		
		//
		//  Default prefs will suppress display unless the object is interactive
		//
		bool show_all_object_tips =
		(bool)gSavedSettings.getBOOL("ShowAllObjectHoverTip");			
		LLSelectNode *nodep = LLSelectMgr::getInstance()->getHoverNode();
		
		// only show tooltip if same inspector not already open
		LLFloater* existing_inspector = LLFloaterReg::findInstance("inspect_object");
		if (nodep &&
			(!existing_inspector 
			 || !existing_inspector->getVisible()
			 || existing_inspector->getKey()["object_id"].asUUID() != hover_object->getID()))
		{

			// Add price to tooltip for items on sale
			bool for_sale = for_sale_selection(nodep);
			if(for_sale)
			{
				LLStringUtil::format_map_t args;
				S32 price = nodep->mSaleInfo.getSalePrice();
				args["[AMOUNT]"] = LLResMgr::getInstance()->getMonetaryString(price);
				tooltip_msg.append(LLTrans::getString("TooltipPrice", args) );
			}

			if (nodep->mName.empty())
			{
				tooltip_msg.append(LLTrans::getString("TooltipNoName"));
			}
			else
			{
				tooltip_msg.append( nodep->mName );
			}
			
			bool has_media = false;
			bool is_time_based_media = false;
			bool is_web_based_media = false;
			bool is_media_playing = false;
			bool is_media_displaying = false;
			
			// Does this face have media?
			const LLTextureEntry* tep = hover_object->getTE(mHoverPick.mObjectFace);
			
			if(tep)
			{
				has_media = tep->hasMedia();
				const LLMediaEntry* mep = has_media ? tep->getMediaData() : NULL;
				if (mep)
				{
					viewer_media_t media_impl = LLViewerMedia::getMediaImplFromTextureID(mep->getMediaID());
					LLPluginClassMedia* media_plugin = NULL;
					
					if (media_impl.notNull() && (media_impl->hasMedia()))
					{
						is_media_displaying = true;
						//LLStringUtil::format_map_t args;
						
						media_plugin = media_impl->getMediaPlugin();
						if(media_plugin)
						{	
							if(media_plugin->pluginSupportsMediaTime())
							{
								is_time_based_media = true;
								is_web_based_media = false;
								//args["[CurrentURL]"] =  media_impl->getMediaURL();
								is_media_playing = media_impl->isMediaPlaying();
							}
							else
							{
								is_time_based_media = false;
								is_web_based_media = true;
								//args["[CurrentURL]"] =  media_plugin->getLocation();
							}
							//tooltip_msg.append(LLTrans::getString("CurrentURL", args));
						}
					}
				}
			}
			

			// Avoid showing tip over media that's displaying unless it's for sale
			// also check the primary node since sometimes it can have an action even though
			// the root node doesn't
			
			bool needs_tip = (!is_media_displaying || 
				              for_sale) &&
				(has_media || 
				 needs_tooltip(nodep) || 
				 needs_tooltip(LLSelectMgr::getInstance()->getPrimaryHoverNode()));
			
			if (show_all_object_tips || needs_tip)
			{
				// We may select this object, so pretend it was clicked
				mPick = mHoverPick;
// [RLVa:KB] - Checked: 2010-11-12 (RLVa-1.2.1g) | Modified: RLVa-1.2.1g
				if ( (!rlv_handler_t::isEnabled()) || (!gRlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) ||
					 (gRlvHandler.canTouch(hover_object, mHoverPick.mObjectOffset)) )
				{
// [/RLVa:KB]
					LLInspector::Params p;
					p.fillFrom(LLUICtrlFactory::instance().getDefaultParams<LLInspector>());
					p.message(tooltip_msg);
					p.image.name("Inspector_I");
					p.click_callback(boost::bind(showObjectInspector, hover_object->getID(), mHoverPick.mObjectFace));
					p.time_based_media(is_time_based_media);
					p.web_based_media(is_web_based_media);
					p.media_playing(is_media_playing);
					p.click_playmedia_callback(boost::bind(playCurrentMedia, mHoverPick));
					p.click_homepage_callback(boost::bind(VisitHomePage, mHoverPick));
					p.visible_time_near(6.f);
					p.visible_time_far(3.f);
					p.delay_time(0.35f);
					p.wrap(false);
					
					LLToolTipMgr::instance().show(p);
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.0e) | Added: RLVa-1.2.0e
				}
				else
				{
					LLToolTipMgr::instance().show(tooltip_msg);
				}
// [/RLVa:KB]
			}
		}
	}
	
	return TRUE;
}

BOOL LLToolPie::handleToolTip(S32 local_x, S32 local_y, MASK mask)
{
	if (!LLUI::sSettingGroups["config"]->getBOOL("ShowHoverTips")) return TRUE;
	if (!mHoverPick.isValid()) return TRUE;
// [RLVa:KB] - Checked: 2010-05-03 (RLVa-1.2.0g) | Modified: RLVa-1.2.0g
#ifdef RLV_EXTENSION_CMD_INTERACT
	if (gRlvHandler.hasBehaviour(RLV_BHVR_INTERACT)) return TRUE;
#endif // RLV_EXTENSION_CMD_INTERACT
// [/RLVa:KB]

	LLViewerObject* hover_object = mHoverPick.getObject();
	
	// update hover object and hover parcel
	LLSelectMgr::getInstance()->setHoverObject(hover_object, mHoverPick.mObjectFace);
	
	
	std::string tooltip_msg;
	std::string line;

	if ( hover_object )
	{
		handleTooltipObject(hover_object, line, tooltip_msg  );
	}
	else if (mHoverPick.mPickType == LLPickInfo::PICK_LAND)
	{
		handleTooltipLand(line, tooltip_msg);
	}

	return TRUE;
}

static void show_inspector(const char* inspector, const char* param, const LLUUID& source_id)
{
	LLSD params;
	params[param] = source_id;
	if (LLToolTipMgr::instance().toolTipVisible())
	{
		LLRect rect = LLToolTipMgr::instance().getToolTipRect();
		params["pos"]["x"] = rect.mLeft;
		params["pos"]["y"] = rect.mTop;
	}

	LLFloaterReg::showInstance(inspector, params);
}


static void show_inspector(const char* inspector,  LLSD& params)
{
	if (LLToolTipMgr::instance().toolTipVisible())
	{
		LLRect rect = LLToolTipMgr::instance().getToolTipRect();
		params["pos"]["x"] = rect.mLeft;
		params["pos"]["y"] = rect.mTop;
	}
	
	LLFloaterReg::showInstance(inspector, params);
}


// static
void LLToolPie::showAvatarInspector(const LLUUID& avatar_id)
{
	show_inspector("inspect_avatar", "avatar_id", avatar_id);
}

// static
void LLToolPie::showObjectInspector(const LLUUID& object_id)
{
	show_inspector("inspect_object", "object_id", object_id);
}


// static
void LLToolPie::showObjectInspector(const LLUUID& object_id, const S32& object_face)
{
	LLSD params;
	params["object_id"] = object_id;
	params["object_face"] = object_face;
	show_inspector("inspect_object", params);
}

// static
void LLToolPie::playCurrentMedia(const LLPickInfo& info)
{
	//FIXME: how do we handle object in different parcel than us?
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!parcel) return;
	
	LLPointer<LLViewerObject> objectp = info.getObject();
	
	// Early out cases.  Must clear media hover. 
	// did not hit an object or did not hit a valid face
	if ( objectp.isNull() ||
		info.mObjectFace < 0 || 
		info.mObjectFace >= objectp->getNumTEs() )
	{
		return;
	}
	
	// Does this face have media?
	const LLTextureEntry* tep = objectp->getTE(info.mObjectFace);
	if (!tep)
		return;
	
	const LLMediaEntry* mep = tep->hasMedia() ? tep->getMediaData() : NULL;
	if(!mep)
		return;
	
	//TODO: Can you Use it? 

	LLPluginClassMedia* media_plugin = NULL;
	
	viewer_media_t media_impl = LLViewerMedia::getMediaImplFromTextureID(mep->getMediaID());
		
	if(media_impl.notNull() && media_impl->hasMedia())
	{
		media_plugin = media_impl->getMediaPlugin();
		if (media_plugin && media_plugin->pluginSupportsMediaTime())
		{
			if(media_impl->isMediaPlaying())
			{
				media_impl->pause();
			}
			else 
			{
				media_impl->play();
			}
		}
	}


}

// static
void LLToolPie::VisitHomePage(const LLPickInfo& info)
{
	//FIXME: how do we handle object in different parcel than us?
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!parcel) return;
	
	LLPointer<LLViewerObject> objectp = info.getObject();
	
	// Early out cases.  Must clear media hover. 
	// did not hit an object or did not hit a valid face
	if ( objectp.isNull() ||
		info.mObjectFace < 0 || 
		info.mObjectFace >= objectp->getNumTEs() )
	{
		return;
	}
	
	// Does this face have media?
	const LLTextureEntry* tep = objectp->getTE(info.mObjectFace);
	if (!tep)
		return;
	
	const LLMediaEntry* mep = tep->hasMedia() ? tep->getMediaData() : NULL;
	if(!mep)
		return;
	
	//TODO: Can you Use it? 
	
	LLPluginClassMedia* media_plugin = NULL;
	
	viewer_media_t media_impl = LLViewerMedia::getMediaImplFromTextureID(mep->getMediaID());
	
	if(media_impl.notNull() && media_impl->hasMedia())
	{
		media_plugin = media_impl->getMediaPlugin();
		
		if (media_plugin && !(media_plugin->pluginSupportsMediaTime()))
		{
			media_impl->navigateHome();
		}
	}
}


void LLToolPie::handleDeselect()
{
	if(	hasMouseCapture() )
	{
		setMouseCapture( FALSE );  // Calls onMouseCaptureLost() indirectly
	}
	// remove temporary selection for pie menu
	LLSelectMgr::getInstance()->setHoverObject(NULL);
	LLSelectMgr::getInstance()->validateSelection();
}

LLTool* LLToolPie::getOverrideTool(MASK mask)
{
	if (gSavedSettings.getBOOL("EnableGrab"))
	{
		if (mask == MASK_CONTROL)
		{
			return LLToolGrab::getInstance();
		}
		else if (mask == (MASK_CONTROL | MASK_SHIFT))
		{
			return LLToolGrab::getInstance();
		}
	}
	return LLTool::getOverrideTool(mask);
}

void LLToolPie::stopEditing()
{
	if(	hasMouseCapture() )
	{
		setMouseCapture( FALSE );  // Calls onMouseCaptureLost() indirectly
	}
}

void LLToolPie::onMouseCaptureLost()
{
	mMouseOutsideSlop = FALSE;
	handleMediaMouseUp();
}


// true if x,y outside small box around start_x,start_y
BOOL LLToolPie::outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y)
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;

	return (dx <= -2 || 2 <= dx || dy <= -2 || 2 <= dy);
}


void LLToolPie::render()
{
	return;
}

static void handle_click_action_play()
{
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!parcel) return;

	LLViewerMediaImpl::EMediaStatus status = LLViewerParcelMedia::getStatus();
	switch(status)
	{
		case LLViewerMediaImpl::MEDIA_PLAYING:
			LLViewerParcelMedia::pause();
			break;

		case LLViewerMediaImpl::MEDIA_PAUSED:
			LLViewerParcelMedia::start();
			break;

		default:
			if (gSavedSettings.getBOOL("MediaEnableFilter"))
			{
				LLViewerParcelMedia::filterMediaUrl(parcel);
			}
			else
			{
				LLViewerParcelMedia::play(parcel);
			}
			break;
	}
}

bool LLToolPie::handleMediaClick(const LLPickInfo& pick)
{
	//FIXME: how do we handle object in different parcel than us?
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	LLPointer<LLViewerObject> objectp = pick.getObject();


	if (!parcel ||
		objectp.isNull() ||
		pick.mObjectFace < 0 || 
		pick.mObjectFace >= objectp->getNumTEs()) 
	{
		LLViewerMediaFocus::getInstance()->clearFocus();

		return false;
	}

	// Does this face have media?
	const LLTextureEntry* tep = objectp->getTE(pick.mObjectFace);
	if(!tep)
		return false;

	LLMediaEntry* mep = (tep->hasMedia()) ? tep->getMediaData() : NULL;
	if(!mep)
		return false;
	
	viewer_media_t media_impl = LLViewerMedia::getMediaImplFromTextureID(mep->getMediaID());

	if (gSavedSettings.getBOOL("MediaOnAPrimUI"))
	{
		if (!LLViewerMediaFocus::getInstance()->isFocusedOnFace(pick.getObject(), pick.mObjectFace) || media_impl.isNull())
		{
			// It's okay to give this a null impl
			LLViewerMediaFocus::getInstance()->setFocusFace(pick.getObject(), pick.mObjectFace, media_impl, pick.mNormal);
		}
		else
		{
			// Make sure keyboard focus is set to the media focus object.
			gFocusMgr.setKeyboardFocus(LLViewerMediaFocus::getInstance());
			
			media_impl->mouseDown(pick.mUVCoords, gKeyboard->currentMask(TRUE));
			mMediaMouseCaptureID = mep->getMediaID();
			setMouseCapture(TRUE);  // This object will send a mouse-up to the media when it loses capture.
		}

		return true;
	}

	LLViewerMediaFocus::getInstance()->clearFocus();

	return false;
}

bool LLToolPie::handleMediaHover(const LLPickInfo& pick)
{
	//FIXME: how do we handle object in different parcel than us?
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!parcel) return false;

	LLPointer<LLViewerObject> objectp = pick.getObject();

	// Early out cases.  Must clear media hover. 
	// did not hit an object or did not hit a valid face
	if ( objectp.isNull() ||
		pick.mObjectFace < 0 || 
		pick.mObjectFace >= objectp->getNumTEs() )
	{
		LLViewerMediaFocus::getInstance()->clearHover();
		return false;
	}

	// Does this face have media?
	const LLTextureEntry* tep = objectp->getTE(pick.mObjectFace);
	if(!tep)
		return false;
	
	const LLMediaEntry* mep = tep->hasMedia() ? tep->getMediaData() : NULL;
	if (mep
		&& gSavedSettings.getBOOL("MediaOnAPrimUI"))
	{		
		viewer_media_t media_impl = LLViewerMedia::getMediaImplFromTextureID(mep->getMediaID());
		
		if(media_impl.notNull())
		{
			// Update media hover object
			if (!LLViewerMediaFocus::getInstance()->isHoveringOverFace(objectp, pick.mObjectFace))
			{
				LLViewerMediaFocus::getInstance()->setHoverFace(objectp, pick.mObjectFace, media_impl, pick.mNormal);
			}
			
			// If this is the focused media face, send mouse move events.
			if (LLViewerMediaFocus::getInstance()->isFocusedOnFace(objectp, pick.mObjectFace))
			{
				media_impl->mouseMove(pick.mUVCoords, gKeyboard->currentMask(TRUE));
				gViewerWindow->setCursor(media_impl->getLastSetCursor());
			}
			else
			{
				// This is not the focused face -- set the default cursor.
				gViewerWindow->setCursor(UI_CURSOR_ARROW);
			}

			return true;
		}
	}
	
	// In all other cases, clear media hover.
	LLViewerMediaFocus::getInstance()->clearHover();

	return false;
}

bool LLToolPie::handleMediaMouseUp()
{
	bool result = false;
	if(mMediaMouseCaptureID.notNull())
	{
		// Face media needs to know the mouse went up.
		viewer_media_t media_impl = LLViewerMedia::getMediaImplFromTextureID(mMediaMouseCaptureID);
		if(media_impl)
		{
			// This will send a mouseUp event to the plugin using the last known mouse coordinate (from a mouseDown or mouseMove), which is what we want.
			media_impl->onMouseCaptureLost();
		}
		
		mMediaMouseCaptureID.setNull();	

		setMouseCapture(FALSE);

		result = true;		
	}	
	
	return result;
}

static void handle_click_action_open_media(LLPointer<LLViewerObject> objectp)
{
	//FIXME: how do we handle object in different parcel than us?
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!parcel) return;

	// did we hit an object?
	if (objectp.isNull()) return;

	// did we hit a valid face on the object?
	S32 face = LLToolPie::getInstance()->getPick().mObjectFace;
	if( face < 0 || face >= objectp->getNumTEs() ) return;
		
	// is media playing on this face?
	if (LLViewerMedia::getMediaImplFromTextureID(objectp->getTE(face)->getID()) != NULL)
	{
		handle_click_action_play();
		return;
	}

	std::string media_url = std::string ( parcel->getMediaURL () );
	std::string media_type = std::string ( parcel->getMediaType() );
	LLStringUtil::trim(media_url);

	LLWeb::loadURL(media_url);
}

static ECursorType cursor_from_parcel_media(U8 click_action)
{
	// HACK: This is directly referencing an impl name.  BAD!
	// This can be removed when we have a truly generic media browser that only 
	// builds an impl based on the type of url it is passed.
	
	//FIXME: how do we handle object in different parcel than us?
	ECursorType open_cursor = UI_CURSOR_ARROW;
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!parcel) return open_cursor;

	std::string media_url = std::string ( parcel->getMediaURL () );
	std::string media_type = std::string ( parcel->getMediaType() );
	LLStringUtil::trim(media_url);

	open_cursor = UI_CURSOR_TOOLMEDIAOPEN;

	LLViewerMediaImpl::EMediaStatus status = LLViewerParcelMedia::getStatus();
	switch(status)
	{
		case LLViewerMediaImpl::MEDIA_PLAYING:
			return click_action == CLICK_ACTION_PLAY ? UI_CURSOR_TOOLPAUSE : open_cursor;
		default:
			return UI_CURSOR_TOOLPLAY;
	}
}


// True if we handled the event.
BOOL LLToolPie::pickRightMouseDownCallback()
{
	S32 x = mPick.mMousePt.mX;
	S32 y = mPick.mMousePt.mY;
	MASK mask = mPick.mKeyMask;

	if (mPick.mPickType != LLPickInfo::PICK_LAND)
	{
		LLViewerParcelMgr::getInstance()->deselectLand();
	}

	// didn't click in any UI object, so must have clicked in the world
	LLViewerObject *object = mPick.getObject();
	LLViewerObject *parent = NULL;
	if(object)
		parent = object->getRootEdit();
	
	// Can't ignore children here.
	LLToolSelect::handleObjectSelection(mPick, FALSE, TRUE);

	// Spawn pie menu
	if (mPick.mPickType == LLPickInfo::PICK_LAND)
	{
		LLParcelSelectionHandle selection = LLViewerParcelMgr::getInstance()->selectParcelAt( mPick.mPosGlobal );
		gMenuHolder->setParcelSelection(selection);
		// ## Zi: Pie menu
		if(gSavedPerAccountSettings.getBOOL("UsePieMenu"))
			gPieMenuLand->show(x, y);
		// ## Zi: Pie menu
		else
		gMenuLand->show(x, y);

		showVisualContextMenuEffect();

	}
	else if (mPick.mObjectID == gAgent.getID() )
	{
		// ## Zi: Pie menu
		if(gSavedPerAccountSettings.getBOOL("UsePieMenu"))
		{
			if(!gPieMenuAvatarSelf)
			{
				//either at very early startup stage or at late quitting stage,
				//this event is ignored.
				return TRUE ;
			}

			gPieMenuAvatarSelf->show(x, y);
		}
		// ## Zi: Pie menu
		else
		{
			if(!gMenuAvatarSelf)
			{
				//either at very early startup stage or at late quitting stage,
				//this event is ignored.
				return TRUE ;
			}

			gMenuAvatarSelf->show(x, y);
		}
	}
	else if (object)
	{
		gMenuHolder->setObjectSelection(LLSelectMgr::getInstance()->getSelection());

		bool is_other_attachment = (object->isAttachment() && !object->isHUDAttachment() && !object->permYouOwner());
		if (object->isAvatar() 
			|| is_other_attachment)
		{
			// Find the attachment's avatar
			while( object && object->isAttachment())
			{
				object = (LLViewerObject*)object->getParent();
				llassert(object);
			}

			if (!object)
			{
				return TRUE; // unexpected, but escape
			}

			// Object is an avatar, so check for mute by id.
			LLVOAvatar* avatar = (LLVOAvatar*)object;
			std::string name = avatar->getFullname();
			std::string mute_msg;
			if (LLMuteList::getInstance()->isMuted(avatar->getID(), avatar->getFullname()))
			{
				mute_msg = LLTrans::getString("UnmuteAvatar");
			}
			else
			{
				mute_msg = LLTrans::getString("MuteAvatar");
			}

// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.0e) | Modified: RLVa-1.1.0l
			// Don't show the context menu on empty selection when fartouch restricted [see LLToolSelect::handleObjectSelection()]
			if ( (!rlv_handler_t::isEnabled()) || (!LLSelectMgr::getInstance()->getSelection()->isEmpty()) ||
				 (!gRlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) )
			{
// [/RLVa:KB]
				if (is_other_attachment)
				{
					gMenuAttachmentOther->getChild<LLUICtrl>("Avatar Mute")->setValue(mute_msg);
					// ## Zi: Pie menu
					if(gSavedPerAccountSettings.getBOOL("UsePieMenu"))
						gPieMenuAttachmentOther->show(x, y);
					// ## Zi: Pie menu
					else
					gMenuAttachmentOther->show(x, y);
				}
				else
				{
					gMenuAvatarOther->getChild<LLUICtrl>("Avatar Mute")->setValue(mute_msg);
					// ## Zi: Pie menu
					if(gSavedPerAccountSettings.getBOOL("UsePieMenu"))
						gPieMenuAvatarOther->show(x, y);
					// ## Zi: Pie menu
					else
					gMenuAvatarOther->show(x, y);
				}
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.0e) | Modified: RLVa-1.1.0l
			}
			else
			{
				make_ui_sound("UISndInvalidOp");
			}
// [/RLVa:KB]
		}
		else if (object->isAttachment())
		{
			// ## Zi: Pie menu
			if(gSavedPerAccountSettings.getBOOL("UsePieMenu"))
				gPieMenuAttachmentSelf->show(x, y);
			// ## Zi: Pie menu
			else
			gMenuAttachmentSelf->show(x, y);
		}
		else
		{
			// BUG: What about chatting child objects?
			std::string name;
			LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode();
			if (node)
			{
				name = node->mName;
			}
			std::string mute_msg;
			if (LLMuteList::getInstance()->isMuted(object->getID(), name))
			{
				mute_msg = LLTrans::getString("UnmuteObject");
			}
			else
			{
				mute_msg = LLTrans::getString("MuteObject2");
			}
			
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.el) | Modified: RLVa-1.1.0l
			// Don't show the pie menu on empty selection when fartouch/interaction restricted
			// (not entirely accurate in case of Tools / Select Only XXX [see LLToolSelect::handleObjectSelection()]
			if ( (!rlv_handler_t::isEnabled()) || (!LLSelectMgr::getInstance()->getSelection()->isEmpty()) ||
				 (!gRlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) )
			{
// [/RLVa:KB]
				gMenuHolder->getChild<LLUICtrl>("Object Mute")->setValue(mute_msg);
				// ## Zi: Pie menu
				if(gSavedPerAccountSettings.getBOOL("UsePieMenu"))
					gPieMenuObject->show(x, y);
				// ## Zi: Pie menu
				else
				gMenuObject->show(x, y);

				showVisualContextMenuEffect();
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.el) | Modified: RLVa-1.1.0l
			}
			else
			{
				make_ui_sound("UISndInvalidOp");
			}
// [/RLVa:KB]
		}
	}

	LLTool::handleRightMouseDown(x, y, mask);
	// We handled the event.
	return TRUE;
}

void LLToolPie::showVisualContextMenuEffect()
{
		// VEFFECT: ShowPie
		LLHUDEffectSpiral *effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_SPHERE, TRUE);
		effectp->setPositionGlobal(mPick.mPosGlobal);
		effectp->setColor(LLColor4U(gAgent.getEffectColor()));
		effectp->setDuration(0.25f);

}
