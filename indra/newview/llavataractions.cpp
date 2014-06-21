/** 
 * @file llavataractions.cpp
 * @brief Friend-related actions (add, remove, offer teleport, etc)
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llavataractions.h"

#include "boost/lambda/lambda.hpp"	// for lambda::constant

#include "llavatarnamecache.h"	// IDEVO
#include "llsd.h"
#include "lldarray.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "roles_constants.h"    // for GP_MEMBER_INVITE

#include "llagent.h"
#include "llappviewer.h"		// for gLastVersionChannel
#include "llcachename.h"
#include "llcallingcard.h"		// for LLAvatarTracker
#include "llfloateravatarpicker.h"	// for LLFloaterAvatarPicker
#include "llfloatergroupinvite.h"
#include "llfloatergroups.h"
#include "llfloaterreg.h"
#include "llfloaterpay.h"
#include "llfloaterworldmap.h"
#include "llgiveinventory.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"	// for gInventory.findCategoryUUIDForType
#include "llinventorypanel.h"
#include "llimview.h"			// for gIMMgr
#include "llmutelist.h"
#include "llnotificationsutil.h"	// for LLNotificationsUtil
#include "llpaneloutfitedit.h"
#include "llpanelprofile.h"
#include "llrecentpeople.h"
#include "llsidetray.h"
#include "lltrans.h"
// [SL:KB] - Patch : UI-ProfileGroupFloater | Checked: 2010-09-08 (Catznip-2.1.2c) | Added: Catznip-2.1.2c
#include "llviewercontrol.h"
// [/SL:KB]
#include "llviewerobjectlist.h"
#include "llviewermessage.h"	// for handle_lure
#include "llviewerregion.h"
#include "llimfloater.h"
#include "lltrans.h"
#include "llcallingcard.h"
#include "llslurl.h"			// IDEVO
#include "llviewercontrol.h"
#include "llfloaterreporter.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "llvoavatar.h"
#include "llworld.h"
#include "llviewermenu.h"
// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
#include "rlvhandler.h"
// [/RLVa:KB]

// static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id, const std::string& name)
{
	if(id == gAgentID)
	{
		LLNotificationsUtil::add("AddSelfFriend");
		return;
	}

	LLSD args;
	args["NAME"] = LLSLURL("agent", id, "completename").getSLURLString();
	LLSD payload;
	payload["id"] = id;
	payload["name"] = name;
    
    	LLNotificationsUtil::add("AddFriendWithMessage", args, payload, &callbackAddFriendWithMessage);

	// add friend to recent people list
	LLRecentPeople::instance().add(id);
}

void on_avatar_name_friendship(const LLUUID& id, const LLAvatarName av_name)
{
	LLAvatarActions::requestFriendshipDialog(id, av_name.getCompleteName());
}

// static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id)
{
	if(id.isNull())
	{
		return;
	}

	LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_friendship, _1, _2));
}

// static
void LLAvatarActions::removeFriendDialog(const LLUUID& id)
{
	if (id.isNull())
		return;

	uuid_vec_t ids;
	ids.push_back(id);
	removeFriendsDialog(ids);
}

// static
void LLAvatarActions::removeFriendsDialog(const uuid_vec_t& ids)
{
	if(ids.size() == 0)
		return;

	LLSD args;
	std::string msgType;
	if(ids.size() == 1)
	{
		LLUUID agent_id = ids[0];
		LLAvatarName av_name;
		if(LLAvatarNameCache::get(agent_id, &av_name))
		{
			args["NAME"] = av_name.mDisplayName;
		}

		msgType = "RemoveFromFriends";
	}
	else
	{
		msgType = "RemoveMultipleFromFriends";
	}

	LLSD payload;
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		payload["ids"].append(*it);
	}

	LLNotificationsUtil::add(msgType,
		args,
		payload,
		&handleRemove);
}

// static
void LLAvatarActions::offerTeleport(const LLUUID& invitee)
{
	if (invitee.isNull())
		return;

	LLDynamicArray<LLUUID> ids;
	ids.push_back(invitee);
	offerTeleport(ids);
}

// static
void LLAvatarActions::offerTeleport(const uuid_vec_t& ids) 
{
	if (ids.size() == 0)
		return;

	handle_lure(ids);
}

static void on_avatar_name_cache_start_im(const LLUUID& agent_id,
										  const LLAvatarName& av_name)
{
	std::string name = av_name.getCompleteName();
	LLUUID session_id = gIMMgr->addSession(name, IM_NOTHING_SPECIAL, agent_id);
	if (session_id != LLUUID::null)
	{
		LLIMFloater::show(session_id);
	}
	make_ui_sound("UISndStartIM");
}

// static
void LLAvatarActions::startIM(const LLUUID& id)
{
	if (id.isNull())
		return;

// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
	if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canStartIM(id)) )
	{
		LLUUID idSession = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, id);
		if ( (idSession.notNull()) && (!gIMMgr->hasSession(idSession)) )
		{
			make_ui_sound("UISndInvalidOp");
			RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTIM, LLSD().with("RECIPIENT", LLSLURL("agent", id, "completename").getSLURLString()));
			return;
		}
	}
// [/RLVa:KB]

	LLAvatarNameCache::get(id,
		boost::bind(&on_avatar_name_cache_start_im, _1, _2));
}

// static
void LLAvatarActions::endIM(const LLUUID& id)
{
	if (id.isNull())
		return;
	
	LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, id);
	if (session_id != LLUUID::null)
	{
		gIMMgr->leaveSession(session_id);
	}
}

static void on_avatar_name_cache_start_call(const LLUUID& agent_id,
											const LLAvatarName& av_name)
{
	std::string name = av_name.getCompleteName();
	LLUUID session_id = gIMMgr->addSession(name, IM_NOTHING_SPECIAL, agent_id, true);
	if (session_id != LLUUID::null)
	{
		gIMMgr->startCall(session_id);
	}
	make_ui_sound("UISndStartIM");
}

// static
void LLAvatarActions::startCall(const LLUUID& id)
{
	if (id.isNull())
	{
		return;
	}

// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
	if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canStartIM(id)) )
	{
		LLUUID idSession = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, id);
		if ( (idSession.notNull()) && (!gIMMgr->hasSession(idSession)) )
		{
			make_ui_sound("UISndInvalidOp");
			RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTIM, LLSD().with("RECIPIENT", LLSLURL("agent", id, "completename").getSLURLString()));
			return;
		}
	}
// [/RLVa:KB]

	LLAvatarNameCache::get(id,
		boost::bind(&on_avatar_name_cache_start_call, _1, _2));
}

// static
void LLAvatarActions::startAdhocCall(const uuid_vec_t& ids)
{
	if (ids.size() == 0)
	{
		return;
	}

	// convert vector into LLDynamicArray for addSession
	LLDynamicArray<LLUUID> id_array;
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
		const LLUUID& idAgent = *it;
		if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canStartIM(idAgent)) )
		{
			make_ui_sound("UISndInvalidOp");
			RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTCONF, LLSD().with("RECIPIENT", LLSLURL("agent", idAgent, "completename").getSLURLString()));
			return;
		}
		id_array.push_back(idAgent);
// [/RLVa:KB]
//		id_array.push_back(*it);
	}

	// create the new ad hoc voice session
	const std::string title = LLTrans::getString("conference-title");
	LLUUID session_id = gIMMgr->addSession(title, IM_SESSION_CONFERENCE_START,
										   ids[0], id_array, true);
	if (session_id == LLUUID::null)
	{
		return;
	}

	gIMMgr->autoStartCallOnStartup(session_id);

	make_ui_sound("UISndStartIM");
}

/* AD *TODO: Is this function needed any more?
	I fixed it a bit(added check for canCall), but it appears that it is not used
	anywhere. Maybe it should be removed?
// static
bool LLAvatarActions::isCalling(const LLUUID &id)
{
	if (id.isNull() || !canCall())
	{
		return false;
	}

	LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, id);
	return (LLIMModel::getInstance()->findIMSession(session_id) != NULL);
}*/

//static
bool LLAvatarActions::canCall()
{
		return LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking();
}

// static
void LLAvatarActions::startConference(const uuid_vec_t& ids)
{
	// *HACK: Copy into dynamic array
	LLDynamicArray<LLUUID> id_array;
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
		const LLUUID& idAgent = *it;
		if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canStartIM(idAgent)) )
		{
			make_ui_sound("UISndInvalidOp");
			RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTCONF, LLSD().with("RECIPIENT", LLSLURL("agent", idAgent, "completename").getSLURLString()));
			return;
		}
		id_array.push_back(idAgent);
// [/RLVa:KB]
//		id_array.push_back(*it);
	}
	const std::string title = LLTrans::getString("conference-title");
	LLUUID session_id = gIMMgr->addSession(title, IM_SESSION_CONFERENCE_START, ids[0], id_array);
	if (session_id != LLUUID::null)
	{
		LLIMFloater::show(session_id);
	}
	make_ui_sound("UISndStartIM");
}

// static
void LLAvatarActions::showProfile(const LLUUID& id)
{
	if (id.notNull())
	{
// [SL:KB] - Patch : UI-ProfileGroupFloater | 
		if ( (!gSavedSettings.getBOOL("ShowProfileFloaters")) || ((gAgent.getID() == id)) )
		{
// [/SL:KB]		
			LLSD params;
			params["id"] = id;
			params["open_tab_name"] = "panel_profile";

			//Show own profile
			if(gAgent.getID() == id)
			{
				LLSideTray::getInstance()->showPanel("panel_me", params);
			}
			//Show other user profile
			else
			{
				LLSideTray::getInstance()->showPanel("panel_profile_view", params);
			}
		}
		else
		{
			/* AO: Ignore web profile
			// PROFILES: open in webkit window
			std::string full_name;
			if (gCacheName->getFullName(id,full_name))
			{
				std::string agent_name = LLCacheName::buildUsername(full_name);
				llinfos << "opening web profile for " << agent_name << llendl;		
				std::string url = getProfileURL(agent_name);
				LLWeb::loadWebURLInternal(url);
			}
			else
			{
				llwarns << "no name info for agent id " << id << llendl;
			}
			*/
			
			LLSD params;
                        params["id"] = id;
                        params["open_tab_name"] = "panel_profile";
			//Show own profile
			if(gAgent.getID() == id)
			{
				LLSideTray::getInstance()->showPanel("panel_me", params);
			}
			else
			{
				// [SL:KB] - Patch : UI-ProfileGroupFloater
				LLFloaterReg::showInstance("floater_profile_view", LLSD().with("id", id));
			}
		}
	}
}

// static
void LLAvatarActions::showOnMap(const LLUUID& id)
{
	LLAvatarName av_name;
	if (!LLAvatarNameCache::get(id, &av_name))
	{
		LLAvatarNameCache::get(id, boost::bind(&LLAvatarActions::showOnMap, id));
		return;
	}

	gFloaterWorldMap->trackAvatar(id, av_name.mDisplayName);
	LLFloaterReg::showInstance("world_map");
}

// static
void LLAvatarActions::pay(const LLUUID& id)
{
	LLNotification::Params params("BusyModePay");
	params.functor.function(boost::bind(&LLAvatarActions::handlePay, _1, _2, id));

	if (gAgent.getBusy())
	{
		// warn users of being in busy mode during a transaction
		LLNotifications::instance().add(params);
	}
	else
	{
		LLNotifications::instance().forceResponse(params, 1);
	}
}

// static
void LLAvatarActions::kick(const LLUUID& id)
{
	LLSD payload;
	payload["avatar_id"] = id;
	LLNotifications::instance().add("KickUser", LLSD(), payload, handleKick);
}

// static
void LLAvatarActions::freeze(const LLUUID& id)
{
	LLSD payload;
	payload["avatar_id"] = id;
	LLNotifications::instance().add("FreezeUser", LLSD(), payload, handleFreeze);
}

// static
void LLAvatarActions::unfreeze(const LLUUID& id)
{
	LLSD payload;
	payload["avatar_id"] = id;
	LLNotifications::instance().add("UnFreezeUser", LLSD(), payload, handleUnfreeze);
}

//static 
void LLAvatarActions::csr(const LLUUID& id, std::string name)
{
	if (name.empty()) return;
	
	std::string url = "http://csr.lindenlab.com/agent/";
	
	// slow and stupid, but it's late
	S32 len = name.length();
	for (S32 i = 0; i < len; i++)
	{
		if (name[i] == ' ')
		{
			url += "%20";
		}
		else
		{
			url += name[i];
		}
	}
	
	LLWeb::loadURL(url);
}

//static 
void LLAvatarActions::share(const LLUUID& id)
{
	LLSD key;
	LLSideTray::getInstance()->showPanel("sidepanel_inventory", key);


	LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL,id);

	if (!gIMMgr->hasSession(session_id))
	{
		startIM(id);
	}

	if (gIMMgr->hasSession(session_id))
	{
		// we should always get here, but check to verify anyways
		LLIMModel::getInstance()->addMessage(session_id, SYSTEM_FROM, LLUUID::null, LLTrans::getString("share_alert"), false);
	}
}

namespace action_give_inventory
{
	typedef std::set<LLUUID> uuid_set_t;

	/**
	 * Returns a pointer to 'Add More' inventory panel of Edit Outfit SP.
	 */
	static LLInventoryPanel* get_outfit_editor_inventory_panel()
	{
		LLPanelOutfitEdit* panel_outfit_edit = dynamic_cast<LLPanelOutfitEdit*>(LLSideTray::getInstance()->getPanel("panel_outfit_edit"));
		if (NULL == panel_outfit_edit) return NULL;

		LLInventoryPanel* inventory_panel = panel_outfit_edit->findChild<LLInventoryPanel>("folder_view");
		return inventory_panel;
	}

	/**
	 * @return active inventory panel, or NULL if there's no such panel
	 */
	static LLInventoryPanel* get_active_inventory_panel()
	{
		LLInventoryPanel* active_panel = LLInventoryPanel::getActiveInventoryPanel(FALSE);
		if (!active_panel)
		{
			active_panel = get_outfit_editor_inventory_panel();
		}

		return active_panel;
	}

	/**
	 * Checks My Inventory visibility.
	 */
	static bool is_give_inventory_acceptable()
	{
		LLInventoryPanel* active_panel = get_active_inventory_panel();
		if (!active_panel) return false;

		// check selection in the panel
		const uuid_set_t inventory_selected_uuids = active_panel->getRootFolder()->getSelectionList();
		if (inventory_selected_uuids.empty()) return false; // nothing selected

		bool acceptable = false;
		uuid_set_t::const_iterator it = inventory_selected_uuids.begin();
		const uuid_set_t::const_iterator it_end = inventory_selected_uuids.end();
		for (; it != it_end; ++it)
		{
			LLViewerInventoryCategory* inv_cat = gInventory.getCategory(*it);
			// any category can be offered.
			if (inv_cat)
			{
				acceptable = true;
				continue;
			}

			LLViewerInventoryItem* inv_item = gInventory.getItem(*it);
			// check if inventory item can be given
			if (LLGiveInventory::isInventoryGiveAcceptable(inv_item))
			{
				acceptable = true;
				continue;
			}

			// there are neither item nor category in inventory
			acceptable = false;
			break;
		}
		return acceptable;
	}

	static void build_residents_string(const std::vector<LLAvatarName> avatar_names, std::string& residents_string)
	{
		llassert(avatar_names.size() > 0);

		const std::string& separator = LLTrans::getString("words_separator");
		for (std::vector<LLAvatarName>::const_iterator it = avatar_names.begin(); ; )
		{
			LLAvatarName av_name = *it;
			residents_string.append(av_name.mDisplayName);
			if	(++it == avatar_names.end())
			{
				break;
			}
			residents_string.append(separator);
		}
	}

	static void build_items_string(const uuid_set_t& inventory_selected_uuids , std::string& items_string)
	{
		llassert(inventory_selected_uuids.size() > 0);

		const std::string& separator = LLTrans::getString("words_separator");
		for (uuid_set_t::const_iterator it = inventory_selected_uuids.begin(); ; )
		{
			LLViewerInventoryCategory* inv_cat = gInventory.getCategory(*it);
			if (NULL != inv_cat)
			{
				items_string = inv_cat->getName();
				break;
			}
			LLViewerInventoryItem* inv_item = gInventory.getItem(*it);
			if (NULL != inv_item)
			{
				items_string.append(inv_item->getName());
			}
			if(++it == inventory_selected_uuids.end())
			{
				break;
			}
			items_string.append(separator);
		}
	}

	struct LLShareInfo : public LLSingleton<LLShareInfo>
	{
		std::vector<LLAvatarName> mAvatarNames;
		uuid_vec_t mAvatarUuids;
	};

	static void give_inventory_cb(const LLSD& notification, const LLSD& response)
	{
		S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
		// if Cancel pressed
		if (option == 1)
		{
			return;
		}

		LLInventoryPanel* active_panel = get_active_inventory_panel();
		if (!active_panel) return;

		const uuid_set_t inventory_selected_uuids = active_panel->getRootFolder()->getSelectionList();
		if (inventory_selected_uuids.empty())
		{
			return;
		}

		S32 count = LLShareInfo::instance().mAvatarNames.size();
		bool shared = false;

		// iterate through avatars
		for(S32 i = 0; i < count; ++i)
		{
			const LLUUID& avatar_uuid = LLShareInfo::instance().mAvatarUuids[i];

			// We souldn't open IM session, just calculate session ID for logging purpose. See EXT-6710
			const LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, avatar_uuid);

			uuid_set_t::const_iterator it = inventory_selected_uuids.begin();
			const uuid_set_t::const_iterator it_end = inventory_selected_uuids.end();

			const std::string& separator = LLTrans::getString("words_separator");
			std::string noncopy_item_names;
			LLSD noncopy_items = LLSD::emptyArray();
			// iterate through selected inventory objects
			for (; it != it_end; ++it)
			{
				LLViewerInventoryCategory* inv_cat = gInventory.getCategory(*it);
				if (inv_cat)
				{
					LLGiveInventory::doGiveInventoryCategory(avatar_uuid, inv_cat, session_id);
					shared = true;
					break;
				}
				LLViewerInventoryItem* inv_item = gInventory.getItem(*it);
				if (!inv_item->getPermissions().allowCopyBy(gAgentID))
				{
					if (!noncopy_item_names.empty())
					{
						noncopy_item_names.append(separator);
					}
					noncopy_item_names.append(inv_item->getName());
					noncopy_items.append(*it);
				}
				else
				{
				LLGiveInventory::doGiveInventoryItem(avatar_uuid, inv_item, session_id);
					shared = true;
				}
			}
			if (noncopy_items.beginArray() != noncopy_items.endArray())
			{
				LLSD substitutions;
				substitutions["ITEMS"] = noncopy_item_names;
				LLSD payload;
				payload["agent_id"] = avatar_uuid;
				payload["items"] = noncopy_items;
				LLNotificationsUtil::add("CannotCopyWarning", substitutions, payload,
					&LLGiveInventory::handleCopyProtectedItem);
				break;
			}
		}
		if (shared)
		{
			LLFloaterReg::hideInstance("avatar_picker");
			LLNotificationsUtil::add("ItemsShared");
		}
	}

	/**
	 * Performs "give inventory" operations for provided avatars.
	 *
	 * Sends one requests to give all selected inventory items for each passed avatar.
	 * Avatars are represent by two vectors: names and UUIDs which must be sychronized with each other.
	 *
	 * @param avatar_names - avatar names request to be sent.
	 * @param avatar_uuids - avatar names request to be sent.
	 */
	static void give_inventory(const uuid_vec_t& avatar_uuids, const std::vector<LLAvatarName> avatar_names)
	{
		llassert(avatar_names.size() == avatar_uuids.size());

		LLInventoryPanel* active_panel = get_active_inventory_panel();
		if (!active_panel) return;

		const uuid_set_t inventory_selected_uuids = active_panel->getRootFolder()->getSelectionList();
		if (inventory_selected_uuids.empty())
		{
			return;
		}

		std::string residents;
		build_residents_string(avatar_names, residents);

		std::string items;
		build_items_string(inventory_selected_uuids, items);

		LLSD substitutions;
		substitutions["RESIDENTS"] = residents;
		substitutions["ITEMS"] = items;
		LLShareInfo::instance().mAvatarNames = avatar_names;
		LLShareInfo::instance().mAvatarUuids = avatar_uuids;
		LLNotificationsUtil::add("ShareItemsConfirmation", substitutions, LLSD(), &give_inventory_cb);
	}
}

//static
void LLAvatarActions::shareWithAvatars()
{
	using namespace action_give_inventory;

	LLFloaterAvatarPicker* picker =
		LLFloaterAvatarPicker::show(boost::bind(give_inventory, _1, _2), TRUE, FALSE);
	picker->setOkBtnEnableCb(boost::bind(is_give_inventory_acceptable));
	picker->openFriendsTab();
	LLNotificationsUtil::add("ShareNotification");
}


// static
bool LLAvatarActions::canShareSelectedItems(LLInventoryPanel* inv_panel /* = NULL*/)
{
	using namespace action_give_inventory;

	if (!inv_panel)
	{
		LLInventoryPanel* active_panel = get_active_inventory_panel();
		if (!active_panel) return false;
		inv_panel = active_panel;
	}

	// check selection in the panel
	LLFolderView* root_folder = inv_panel->getRootFolder();
	const uuid_set_t inventory_selected_uuids = root_folder->getSelectionList();
	if (inventory_selected_uuids.empty()) return false; // nothing selected

	bool can_share = true;
	uuid_set_t::const_iterator it = inventory_selected_uuids.begin();
	const uuid_set_t::const_iterator it_end = inventory_selected_uuids.end();
	for (; it != it_end; ++it)
	{
		LLViewerInventoryCategory* inv_cat = gInventory.getCategory(*it);
		// any category can be offered.
		if (inv_cat)
		{
			continue;
		}

		// check if inventory item can be given
		LLFolderViewItem* item = root_folder->getItemByID(*it);
		if (!item) return false;
		LLInvFVBridge* bridge = dynamic_cast<LLInvFVBridge*>(item->getListener());
		if (bridge && bridge->canShare())
		{
			continue;
		}

		// there are neither item nor category in inventory
		can_share = false;
		break;
	}

	return can_share;
}

// static
void LLAvatarActions::toggleBlock(const LLUUID& id)
{
	std::string name;

	gCacheName->getFullName(id, name); // needed for mute
	LLMute mute(id, name, LLMute::AGENT);

	if (LLMuteList::getInstance()->isMuted(mute.mID, mute.mName))
	{
		LLMuteList::getInstance()->remove(mute);
	}
	else
	{
		LLMuteList::getInstance()->add(mute);
	}
}

// static
bool LLAvatarActions::canOfferTeleport(const LLUUID& id)
{
	// First use LLAvatarTracker::isBuddy()
	// If LLAvatarTracker::instance().isBuddyOnline function only is used
	// then for avatars that are online and not a friend it will return false.
	// But we should give an ability to offer a teleport for such avatars.
	if(LLAvatarTracker::instance().isBuddy(id))
	{
		return LLAvatarTracker::instance().isBuddyOnline(id);
	}

	return true;
}

// static
bool LLAvatarActions::canOfferTeleport(const uuid_vec_t& ids)
{
	bool result = true;
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		if(!canOfferTeleport(*it))
		{
			result = false;
			break;
		}
	}
	return result;
}

void LLAvatarActions::inviteToGroup(const LLUUID& id)
{
	LLFloaterGroupPicker* widget = LLFloaterReg::showTypedInstance<LLFloaterGroupPicker>("group_picker", LLSD(id));
	if (widget)
	{
		widget->center();
		widget->setPowersMask(GP_MEMBER_INVITE);
		widget->removeNoneOption();
		widget->setSelectGroupCallback(boost::bind(callback_invite_to_group, _1, id));
	}
}

//== private methods ========================================================================================

// static
bool LLAvatarActions::handleRemove(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	const LLSD& ids = notification["payload"]["ids"];
	for (LLSD::array_const_iterator itr = ids.beginArray(); itr != ids.endArray(); ++itr)
	{
		LLUUID id = itr->asUUID();
		const LLRelationship* ip = LLAvatarTracker::instance().getBuddyInfo(id);
		if (ip)
		{
			switch (option)
			{
			case 0: // YES
				if( ip->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS))
				{
					LLAvatarTracker::instance().empower(id, FALSE);
					LLAvatarTracker::instance().notifyObservers();
				}
				LLAvatarTracker::instance().terminateBuddy(id);
				LLAvatarTracker::instance().notifyObservers();
				break;

			case 1: // NO
			default:
				llinfos << "No removal performed." << llendl;
				break;
			}
		}
	}
	return false;
}

// static
bool LLAvatarActions::handlePay(const LLSD& notification, const LLSD& response, LLUUID avatar_id)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		gAgent.clearBusy();
	}

	LLFloaterPayUtil::payDirectly(&give_money, avatar_id, /*is_group=*/false);
	return false;
}

// static
void LLAvatarActions::callback_invite_to_group(LLUUID group_id, LLUUID id)
{
	uuid_vec_t agent_ids;
	agent_ids.push_back(id);
	
	LLFloaterGroupInvite::showForGroup(group_id, &agent_ids);
}


// static
bool LLAvatarActions::callbackAddFriendWithMessage(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		requestFriendship(notification["payload"]["id"].asUUID(), 
		    notification["payload"]["name"].asString(),
		    response["message"].asString());
	}
	return false;
}

// static
bool LLAvatarActions::handleKick(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		LLMessageSystem* msg = gMessageSystem;

		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID,		gAgent.getID() );
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgent.getSessionID());
		msg->addUUIDFast(_PREHASH_AgentID,   avatar_id );
		msg->addU32("KickFlags", KICK_FLAGS_DEFAULT );
		msg->addStringFast(_PREHASH_Reason,    response["message"].asString() );
		gAgent.sendReliableMessage();
	}
	return false;
}
bool LLAvatarActions::handleFreeze(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		LLMessageSystem* msg = gMessageSystem;

		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID,		gAgent.getID() );
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgent.getSessionID());
		msg->addUUIDFast(_PREHASH_AgentID,   avatar_id );
		msg->addU32("KickFlags", KICK_FLAGS_FREEZE );
		msg->addStringFast(_PREHASH_Reason, response["message"].asString() );
		gAgent.sendReliableMessage();
	}
	return false;
}
bool LLAvatarActions::handleUnfreeze(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	std::string text = response["message"].asString();
	if (option == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		LLMessageSystem* msg = gMessageSystem;

		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID,		gAgent.getID() );
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgent.getSessionID());
		msg->addUUIDFast(_PREHASH_AgentID,   avatar_id );
		msg->addU32("KickFlags", KICK_FLAGS_UNFREEZE );
		msg->addStringFast(_PREHASH_Reason,    text );
		gAgent.sendReliableMessage();
	}
	return false;
}

// static
void LLAvatarActions::requestFriendship(const LLUUID& target_id, const std::string& target_name, const std::string& message)
{
	const LLUUID calling_card_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	send_improved_im(target_id,
					 target_name,
					 message,
					 IM_ONLINE,
					 IM_FRIENDSHIP_OFFERED,
					 calling_card_folder_id);

	LLSD args;
	args["TO_NAME"] = target_name;

	LLSD payload;
	payload["from_id"] = target_id;
	payload["SUPPRESS_TOAST"] = true;
	LLNotificationsUtil::add("FriendshipOffered", args, payload);
}

//static
bool LLAvatarActions::isFriend(const LLUUID& id)
{
	return ( NULL != LLAvatarTracker::instance().getBuddyInfo(id) );
}

// static
bool LLAvatarActions::isBlocked(const LLUUID& id)
{
	std::string name;
	gCacheName->getFullName(id, name); // needed for mute
	return LLMuteList::getInstance()->isMuted(id, name);
}

// static
bool LLAvatarActions::canBlock(const LLUUID& id)
{
	std::string full_name;
	gCacheName->getFullName(id, full_name); // needed for mute
	bool is_linden = (full_name.find("Linden") != std::string::npos);
	bool is_self = id == gAgentID;
	return !is_self && !is_linden;
}

// [SL:KB] - Patch: UI-SidepanelPeople | Checked: 2010-12-03 (Catznip-2.4.0g) | Modified: Catznip-2.4.0g
void LLAvatarActions::report(const LLUUID& idAgent)
{
	LLAvatarName avName;
	LLAvatarNameCache::get(idAgent, &avName);
	
	LLFloaterReporter::showFromAvatar(idAgent, avName.getCompleteName());
}

bool LLAvatarActions::canZoomIn(const LLUUID& idAgent)
{
	return gObjectList.findObject(idAgent);
}

void LLAvatarActions::zoomIn(const LLUUID& idAgent)
{
	handle_zoom_to_object(idAgent);
}


//
// Parcel actions
//


// Defined in llworld.cpp
LLVector3d unpackLocalToGlobalPosition(U32 compact_local, const LLVector3d& region_origin);

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool getRegionAndPosGlobalFromAgentID(const LLUUID& idAgent, const LLViewerRegion** ppRegion, LLVector3d* pPosGlobal)
{
	// Try looking up the agent in gObjectList
	const LLViewerObject* pAvatarObj = gObjectList.findObject(idAgent);
	if (pAvatarObj)
	{
		if (ppRegion)
			*ppRegion = pAvatarObj->getRegion();
		if (pPosGlobal)
			*pPosGlobal = pAvatarObj->getPositionGlobal();
		return (pAvatarObj->isAvatar()) && (NULL != pAvatarObj->getRegion());
	}

	// Walk over each region we're connected to and try finding the agent on one of them
	LLWorld::region_list_t::const_iterator itRegion = LLWorld::getInstance()->getRegionList().begin();
	LLWorld::region_list_t::const_iterator endRegion = LLWorld::getInstance()->getRegionList().end();
	for (; itRegion != endRegion; ++itRegion)
	{
		const LLViewerRegion* pRegion = *itRegion;
		for (S32 idxRegionAgent = 0, cntRegionAgent = pRegion->mMapAvatars.count(); idxRegionAgent < cntRegionAgent; idxRegionAgent++)
		{
			if (pRegion->mMapAvatarIDs.get(idxRegionAgent) == idAgent)
			{
				if (ppRegion)
					*ppRegion = pRegion;
				if (pPosGlobal)
					*pPosGlobal = unpackLocalToGlobalPosition(pRegion->mMapAvatars.get(idxRegionAgent), pRegion->getOriginGlobal());
				return (NULL != pRegion);
			}
		}
	}

	// Couldn't find the agent anywhere
	return false;
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
inline bool getRegionFromAgentID(const LLUUID& idAgent, const LLViewerRegion** ppRegion)
{
	return getRegionAndPosGlobalFromAgentID(idAgent, ppRegion, NULL);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
inline bool getPosGlobalFromAgentID(const LLUUID& idAgent, LLVector3d& posGlobal)
{
	return getRegionAndPosGlobalFromAgentID(idAgent, NULL, &posGlobal);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool LLAvatarActions::canLandFreezeOrEject(const LLUUID& idAgent)
{
	uuid_vec_t idAgents;
	idAgents.push_back(idAgent);
	return canLandFreezeOrEjectMultiple(idAgents);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool LLAvatarActions::canLandFreezeOrEjectMultiple(uuid_vec_t& idAgents, bool fFilter /*=false*/)
{
	if (gAgent.isGodlikeWithoutAdminMenuFakery())
		return true;					// Gods can always freeze

	uuid_vec_t::iterator itAgent = idAgents.begin(); bool fCanFreeze = false;
	while ( (itAgent != idAgents.end()) && ((fFilter) || (!fCanFreeze)) )
	{
		const LLViewerRegion* pRegion = NULL; LLVector3d posGlobal;
		if (getRegionAndPosGlobalFromAgentID(*itAgent, &pRegion, &posGlobal))
		{
			// NOTE: we actually don't always need the parcel, but attempting to get it now will help with setting fBanEnabled when ejecting
			const LLParcel* pParcel = LLViewerParcelMgr::getInstance()->selectParcelAt(posGlobal)->getParcel();
			const LLVector3 posRegion = pRegion->getPosRegionFromGlobal(posGlobal);
			if ( (pRegion->getOwner() == gAgent.getID()) || (pRegion->isEstateManager()) || (pRegion->isOwnedSelf(posRegion)) ||
				 ((pRegion->isOwnedGroup(posRegion)) && (pParcel) && (gAgent.hasPowerInGroup(pParcel->getOwnerID(), GP_LAND_ADMIN))) )
			{
				fCanFreeze = true;
				++itAgent;
				continue;
			}
		}
		if (fFilter)
			itAgent = idAgents.erase(itAgent);
		else
			++itAgent;
	}
	return fCanFreeze;
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
void LLAvatarActions::landEject(const LLUUID& idAgent)
{
	llinfos << "landeject " << idAgent << llendl;
	uuid_vec_t idAgents;
	idAgents.push_back(idAgent);
	landEjectMultiple(idAgents);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
void LLAvatarActions::landEjectMultiple(const uuid_vec_t& idAgents)
{
	uuid_vec_t idEjectAgents(idAgents);
	if (!canLandFreezeOrEjectMultiple(idEjectAgents, true))
	{
		llwarns << "Not allowed to eject" << llendl;
		return;
	}

	LLSD args, payload; std::string strMsgName, strResidents; bool fBanEnabled = false;
	for (uuid_vec_t::const_iterator itAgent = idEjectAgents.begin(); itAgent != idEjectAgents.end(); ++itAgent)
	{
		const LLUUID& idAgent = *itAgent; LLVector3d posGlobal;
		if ( (!fBanEnabled) && (getPosGlobalFromAgentID(idAgent, posGlobal)) )
		{
			const LLParcel* pParcel = LLViewerParcelMgr::getInstance()->selectParcelAt(posGlobal)->getParcel();
			fBanEnabled = (pParcel) && (LLViewerParcelMgr::getInstance()->isParcelOwnedByAgent(pParcel, GP_LAND_MANAGE_BANNED));
		}

		if (idEjectAgents.begin() != itAgent)
			strResidents += "\n";
		strResidents += LLSLURL("agent", idAgent, (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? "completename" : "rlvanonym").getSLURLString();
		payload["ids"].append(*itAgent);
	}

	if (1 == payload["ids"].size())
	{
		args["AVATAR_NAME"] = strResidents;
		strMsgName = (fBanEnabled) ? "EjectAvatarFullname" : "EjectAvatarFullnameNoBan";
	}
	else
	{
		args["RESIDENTS"] = strResidents;
		strMsgName = (fBanEnabled) ? "EjectAvatarMultiple" : "EjectAvatarMultipleNoBan";
	}

	payload["ban_enabled"] = fBanEnabled;
	LLNotificationsUtil::add(strMsgName, args, payload, &callbackLandEject);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool LLAvatarActions::callbackLandEject(const LLSD& notification, const LLSD& response)
{
	S32 idxOption = LLNotificationsUtil::getSelectedOption(notification, response);
	if (2 == idxOption)							// Cancel button.
		return false;

	bool fBanEnabled = notification["payload"]["ban_enabled"].asBoolean();
	if ( (0 == idxOption) || (fBanEnabled) )	// Eject button (or Eject + Ban)
	{
		const LLSD& idAgents = notification["payload"]["ids"];
		for (LLSD::array_const_iterator itAgent = idAgents.beginArray(); itAgent != idAgents.endArray(); ++itAgent)
		{
			const LLUUID idAgent = itAgent->asUUID(); const LLViewerRegion* pAgentRegion = NULL;
			if (getRegionFromAgentID(idAgent, &pAgentRegion))
			{
				// This is tricky. It is similar to say if it is not an 'Eject' button, and it is also not an 'Cancel' button, 
				// and ban_enabled==true, it should be the 'Eject and Ban' button.
				U32 flags = ( (0 != idxOption) && (fBanEnabled)	) ? 0x1 : 0x0;

				gMessageSystem->newMessage("EjectUser");
				gMessageSystem->nextBlock("AgentData");
				gMessageSystem->addUUID("AgentID", gAgent.getID());
				gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
				gMessageSystem->nextBlock("Data");
				gMessageSystem->addUUID("TargetID", idAgent);
				gMessageSystem->addU32("Flags", flags);
				gMessageSystem->sendReliable(pAgentRegion->getHost());
			}
		}
	}
	return false;
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
void LLAvatarActions::landFreeze(const LLUUID& idAgent)
{
	llinfos << "landfreezing " << idAgent << llendl;
	uuid_vec_t idAgents;
	idAgents.push_back(idAgent);
	landFreezeMultiple(idAgents);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
void LLAvatarActions::landFreezeMultiple(const uuid_vec_t& idAgents)
{
	uuid_vec_t idEjectAgents(idAgents);
	if (!canLandFreezeOrEjectMultiple(idEjectAgents, true))
		return;

	LLSD args, payload; std::string strMsgName, strResidents;
	for (uuid_vec_t::const_iterator itAgent = idEjectAgents.begin(); itAgent != idEjectAgents.end(); ++itAgent)
	{
		const LLUUID& idAgent = *itAgent;
		if (idEjectAgents.begin() != itAgent)
			strResidents += "\n";
		strResidents += LLSLURL("agent", idAgent, (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? "completename" : "rlvanonym").getSLURLString();
		payload["ids"].append(*itAgent);
	}

	if (1 == payload["ids"].size())
	{
		args["AVATAR_NAME"] = strResidents;
		strMsgName = "FreezeAvatarFullname";
	}
	else
	{
		args["RESIDENTS"] = strResidents;
		strMsgName = "FreezeAvatarMultiple";
	}

	LLNotificationsUtil::add(strMsgName, args, payload, &callbackLandFreeze);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool LLAvatarActions::callbackLandFreeze(const LLSD& notification, const LLSD& response)
{
	S32 idxOption = LLNotificationsUtil::getSelectedOption(notification, response);
	if ( (0 == idxOption) || (1 == idxOption) )
	{
		U32 flags = (0 == idxOption) ? 0x0 : 0x1;

		const LLSD& idAgents = notification["payload"]["ids"];
		for (LLSD::array_const_iterator itAgent = idAgents.beginArray(); itAgent != idAgents.endArray(); ++itAgent)
		{
			const LLUUID idAgent = itAgent->asUUID(); const LLViewerRegion* pAgentRegion = NULL;
			if (getRegionFromAgentID(idAgent, &pAgentRegion))
			{
				gMessageSystem->newMessage("FreezeUser");
				gMessageSystem->nextBlock("AgentData");
				gMessageSystem->addUUID("AgentID", gAgent.getID());
				gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
				gMessageSystem->nextBlock("Data");
				gMessageSystem->addUUID("TargetID", idAgent);
				gMessageSystem->addU32("Flags", flags);
				gMessageSystem->sendReliable(pAgentRegion->getHost());
			}
		}
	}
	return false;
}

//
// Estate actions
//

typedef std::vector<std::string> strings_t;

// Copy/paste from LLPanelRegionInfo::sendEstateOwnerMessage
void sendEstateOwnerMessage(const LLViewerRegion* pRegion, const std::string& request, const LLUUID& invoice, const strings_t& strings)
{
	if (pRegion)
	{
		llinfos << "Sending estate request '" << request << "'" << llendl;
		gMessageSystem->newMessage("EstateOwnerMessage");
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
		gMessageSystem->nextBlock("MethodData");
		gMessageSystem->addString("Method", request);
		gMessageSystem->addUUID("Invoice", invoice);
		if(strings.empty())
		{
			gMessageSystem->nextBlock("ParamList");
			gMessageSystem->addString("Parameter", NULL);
		}
		else
		{
			strings_t::const_iterator it = strings.begin();
			strings_t::const_iterator end = strings.end();
			for(; it != end; ++it)
			{
				gMessageSystem->nextBlock("ParamList");
				gMessageSystem->addString("Parameter", *it);
			}
		}
		gMessageSystem->sendReliable(pRegion->getHost());
	}
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool LLAvatarActions::canEstateKickOrTeleportHome(const LLUUID& idAgent)
{
	uuid_vec_t idAgents;
	idAgents.push_back(idAgent);
	return canEstateKickOrTeleportHomeMultiple(idAgents);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool LLAvatarActions::canEstateKickOrTeleportHomeMultiple(uuid_vec_t& idAgents, bool fFilter /*=false*/)
{
	if (gAgent.isGodlikeWithoutAdminMenuFakery())
		return true;		// Gods can always kick

	uuid_vec_t::iterator itAgent = idAgents.begin(); bool fCanKick = false;
	while ( (itAgent != idAgents.end()) && ((fFilter) || (!fCanKick)) )
	{
		const LLViewerRegion* pRegion = NULL;
		if ( (getRegionFromAgentID(*itAgent, &pRegion)) && ((pRegion->getOwner() == gAgent.getID()) || (pRegion->isEstateManager())) )
		{
			fCanKick = true;
			++itAgent;		// Estate owners/managers can kick
			continue;
		}

		if (fFilter)
			itAgent = idAgents.erase(itAgent);
		else
			++itAgent;
	}
	return fCanKick;
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
void LLAvatarActions::estateKick(const LLUUID& idAgent)
{
	llinfos << "estatekick " << idAgent << llendl;
	uuid_vec_t idAgents;
	idAgents.push_back(idAgent);
	estateKickMultiple(idAgents);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
void LLAvatarActions::estateKickMultiple(const uuid_vec_t& idAgents)
{
	uuid_vec_t idEjectAgents(idAgents);
	if (!canEstateKickOrTeleportHomeMultiple(idEjectAgents, true))
		return;

	LLSD args, payload; std::string strMsgName, strResidents;
	for (uuid_vec_t::const_iterator itAgent = idEjectAgents.begin(); itAgent != idEjectAgents.end(); ++itAgent)
	{
		const LLUUID& idAgent = *itAgent;
		if (idEjectAgents.begin() != itAgent)
			strResidents += "\n";
		strResidents += LLSLURL("agent", idAgent, (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? "completename" : "rlvanonym").getSLURLString();
		payload["ids"].append(*itAgent);
	}

	if (1 == payload["ids"].size())
	{
		args["EVIL_USER"] = strResidents;
		strMsgName = "EstateKickUser";
	}
	else
	{
		args["RESIDENTS"] = strResidents;
		strMsgName = "EstateKickMultiple";
	}

	LLNotificationsUtil::add(strMsgName, args, payload, &callbackEstateKick);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool LLAvatarActions::callbackEstateKick(const LLSD& notification, const LLSD& response)
{
	S32 idxOption = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == idxOption)
	{
		const LLSD& idAgents = notification["payload"]["ids"];
		for (LLSD::array_const_iterator itAgent = idAgents.beginArray(); itAgent != idAgents.endArray(); ++itAgent)
		{
			const LLViewerRegion* pRegion = NULL;
			if (getRegionFromAgentID(itAgent->asUUID(), &pRegion))
			{
				strings_t strings;
				strings.push_back(itAgent->asString());

				sendEstateOwnerMessage(pRegion, "kickestate", LLUUID::generateNewID(), strings);
			}
		}
	}
	return false;
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
void LLAvatarActions::estateTeleportHome(const LLUUID& idAgent)
{
	llinfos << "estateTpHome " << idAgent << llendl;
	uuid_vec_t idAgents;
	idAgents.push_back(idAgent);
	estateTeleportHomeMultiple(idAgents);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
void LLAvatarActions::estateTeleportHomeMultiple(const uuid_vec_t& idAgents)
{
	uuid_vec_t idEjectAgents(idAgents);
	if (!canEstateKickOrTeleportHomeMultiple(idEjectAgents, true))
		return;

	LLSD args, payload; std::string strMsgName, strResidents;
	for (uuid_vec_t::const_iterator itAgent = idEjectAgents.begin(); itAgent != idEjectAgents.end(); ++itAgent)
	{
		const LLUUID& idAgent = *itAgent;
		if (idEjectAgents.begin() != itAgent)
			strResidents += "\n";
		strResidents += LLSLURL("agent", idAgent, (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? "completename" : "rlvanonym").getSLURLString();
		payload["ids"].append(*itAgent);
	}

	if (1 == payload["ids"].size())
	{
		args["AVATAR_NAME"] = strResidents;
		strMsgName = "EstateTeleportHomeUser";
	}
	else
	{
		args["RESIDENTS"] = strResidents;
		strMsgName = "EstateTeleportHomeMultiple";
	}

	LLNotificationsUtil::add(strMsgName, args, payload, &callbackEstateTeleportHome);
}

// static - Checked: 2010-12-03 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
bool LLAvatarActions::callbackEstateTeleportHome(const LLSD& notification, const LLSD& response)
{
	S32 idxOption = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == idxOption)
	{
		const LLSD& idAgents = notification["payload"]["ids"];
		for (LLSD::array_const_iterator itAgent = idAgents.beginArray(); itAgent != idAgents.endArray(); ++itAgent)
		{
			const LLViewerRegion* pRegion = NULL;
			if (getRegionFromAgentID(itAgent->asUUID(), &pRegion))
			{
				strings_t strings;
				strings.push_back(gAgent.getID().asString());
				strings.push_back(itAgent->asString());

				sendEstateOwnerMessage(pRegion, "teleporthomeuser", LLUUID::generateNewID(), strings);
			}
		}
	}
	return false;
}
// [/SL:KB]
