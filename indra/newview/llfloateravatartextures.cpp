/** 
 * @file llfloateravatartextures.cpp
 * @brief Debugging view showing underlying avatar textures and baked textures.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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
#include "llfloateravatartextures.h"

// library headers
#include "llavatarnamecache.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "lltexturectrl.h"
#include "lluictrlfactory.h"
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"
// <edit>
#include "llfocusmgr.h"
#include "llnotificationsutil.h"
#include "llinventorypanel.h"
#include "llinventorydefines.h"
// </edit>

using namespace LLVOAvatarDefines;

LLFloaterAvatarTextures::LLFloaterAvatarTextures(const LLSD& id)
  : LLFloater(id),
	mID(id.asUUID())
{
}

LLFloaterAvatarTextures::~LLFloaterAvatarTextures()
{
}

BOOL LLFloaterAvatarTextures::postBuild()
{
	for (U32 i=0; i < TEX_NUM_INDICES; i++)
	{
		const std::string tex_name = LLVOAvatarDictionary::getInstance()->getTexture(ETextureIndex(i))->mName;
		mTextures[i] = getChild<LLTextureCtrl>(tex_name);
	}
	mTitle = getTitle();

	childSetAction("Dump", onClickDump, this);

	refresh();
	return TRUE;
}

void LLFloaterAvatarTextures::draw()
{
	refresh();
	LLFloater::draw();
}

static void update_texture_ctrl(LLVOAvatar* avatarp,
								 LLTextureCtrl* ctrl,
								 ETextureIndex te)
{
	LLUUID id = IMG_DEFAULT_AVATAR;
	const LLVOAvatarDictionary::TextureEntry* tex_entry = LLVOAvatarDictionary::getInstance()->getTexture(te);
	if (tex_entry->mIsLocalTexture)
	{
		if (avatarp->isSelf())
		{
			const LLWearableType::EType wearable_type = tex_entry->mWearableType;
			LLWearable *wearable = gAgentWearables.getWearable(wearable_type, 0);
			if (wearable)
			{
				LLLocalTextureObject *lto = wearable->getLocalTextureObject(te);
				if (lto)
				{
					id = lto->getID();
				}
			}
		}
	}
	else
	{
		id = avatarp->getTE(te)->getID();
	}
	//id = avatarp->getTE(te)->getID();
	if (id == IMG_DEFAULT_AVATAR)
	{
		ctrl->setImageAssetID(LLUUID::null);
		ctrl->setToolTip(tex_entry->mName + " : " + std::string("IMG_DEFAULT_AVATAR"));
	}
	else
	{
		ctrl->setImageAssetID(id);
		ctrl->setToolTip(tex_entry->mName + " : " + id.asString());
	}
}

static LLVOAvatar* find_avatar(const LLUUID& id)
{
	LLViewerObject *obj = gObjectList.findObject(id);
	while (obj && obj->isAttachment())
	{
		obj = (LLViewerObject *)obj->getParent();
	}

	if (obj && obj->isAvatar())
	{
		return (LLVOAvatar*)obj;
	}
	else
	{
		return NULL;
	}
}

void LLFloaterAvatarTextures::refresh()
{
// <edit>
//	if (gAgent.isGodlike())
//	{
// </edit>
		LLVOAvatar *avatarp = find_avatar(mID);
		if (avatarp)
		{
			LLAvatarName av_name;
			if (LLAvatarNameCache::get(avatarp->getID(), &av_name))
			{
				setTitle(mTitle + ": " + av_name.getCompleteName());
			}
			for (U32 i=0; i < TEX_NUM_INDICES; i++)
			{
				update_texture_ctrl(avatarp, mTextures[i], ETextureIndex(i));
			}
		}
		else
		{
			setTitle(mTitle + ": " + getString("InvalidAvatar") + " (" + mID.asString() + ")");
		}
// <edit>
//	}
// </edit>
}

// static
void LLFloaterAvatarTextures::onClickDump(void* data)
{

	LLFloaterAvatarTextures* self = (LLFloaterAvatarTextures*)data;
	LLVOAvatar* avatarp = find_avatar(self->mID);
	if (!avatarp) return;
// <edit> 
		std::string fullname;
		gCacheName->getFullName(avatarp->getID(), fullname);
		std::string msg;
		msg.assign("Avatar Textures : ");
		msg.append(fullname);
		msg.append("\n");
// </edit>
	for (S32 i = 0; i < avatarp->getNumTEs(); i++)
	{
// <edit>
		std::string submsg;// sumo for each text
		const LLTextureEntry* te = avatarp->getTE(i);
		if (!te) continue;
		LLUUID mUUID = te->getID();
		submsg.assign(LLVOAvatarDictionary::getInstance()->getTexture(ETextureIndex(i))->mName);
		submsg.append(" : ");
		if (mUUID == IMG_DEFAULT_AVATAR)
		{
			submsg.append("No texture") ;
		}
		else
		{
			submsg.append(mUUID.asString());
			msg.append(submsg);
			msg.append("\n");
			LLUUID mUUID = te->getID();
			LLAssetType::EType asset_type = LLAssetType::AT_TEXTURE;
			LLInventoryType::EType inv_type = LLInventoryType::IT_TEXTURE;
			const LLUUID folder_id = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(asset_type));
			if(folder_id.notNull())
			{
				std::string name;
				std::string desc;
				name.assign("temp.");
				desc.assign(mUUID.asString());
				name.append(mUUID.asString());
				LLUUID item_id;
				item_id.generate();
				LLPermissions perm;
					perm.init(gAgentID,	gAgentID, LLUUID::null, LLUUID::null);
				U32 next_owner_perm = PERM_MOVE | PERM_TRANSFER;
					perm.initMasks(PERM_ALL, PERM_ALL, PERM_NONE,PERM_NONE, next_owner_perm);
				S32 creation_date_now = time_corrected();
				LLPointer<LLViewerInventoryItem> item
					= new LLViewerInventoryItem(item_id,
										folder_id,
										perm,
										mUUID,
										asset_type,
										inv_type,
										name,
										desc,
										LLSaleInfo::DEFAULT,
										LLInventoryItemFlags::II_FLAGS_NONE,
										creation_date_now);
				item->updateServer(TRUE);

				gInventory.updateItem(item);
				gInventory.notifyObservers();
		
				LLInventoryPanel *active_panel = LLInventoryPanel::getActiveInventoryPanel();
				if (active_panel)
				{
					active_panel->openSelected();
					LLFocusableElement* focus = gFocusMgr.getKeyboardFocus();
					gFocusMgr.setKeyboardFocus(focus);
				}
			}
			else
			{
				llwarns << "Can't find a folder to put it in" << llendl;
			}
		}

	}
	LLSD args;
	args["MESSAGE"] = msg;
	LLNotificationsUtil::add("SystemMessage", args);
}
// </edit>