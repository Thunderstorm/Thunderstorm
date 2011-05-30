/** 
 * @file llfloaterinspecttexture.cpp
 * FCTEAM was here
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterinspecttexture.h"
#include "llagent.h"
#include "llfloaterreg.h"
#include "llfloatertools.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llselectmgr.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "lluictrlfactory.h"
#include "llviewertexture.h"
#include "llpartdata.h"
#include "llviewerpartsource.h"
#include "lltexturectrl.h"
#include "llnotificationsutil.h"
#include "llinventorypanel.h"
#include "llinventorydefines.h"

LLFloaterInspectTexture::LLFloaterInspectTexture(const LLSD& key)
  : LLFloater(key),
	mDirty(FALSE)
{
	mCommitCallbackRegistrar.add("Inspect.CpToInvSelected",	boost::bind(&LLFloaterInspectTexture::onClickCpToInvSelected, this));
	mCommitCallbackRegistrar.add("Inspect.CpToInvAll",	boost::bind(&LLFloaterInspectTexture::onClickCpToInvAll, this));
	mCommitCallbackRegistrar.add("Inspect.SelectObject",	boost::bind(&LLFloaterInspectTexture::onSelectObject, this));
}

BOOL LLFloaterInspectTexture::postBuild()
{
	mTextureList = getChild<LLScrollListCtrl>("object_list");
	texture_ctrl = getChild<LLTextureCtrl>("imagette");
	
	refresh();
	
	return TRUE;
}

LLFloaterInspectTexture::~LLFloaterInspectTexture(void)
{

}

void LLFloaterInspectTexture::onOpen(const LLSD& key)
{

	mObjectSelection = LLSelectMgr::getInstance()->getSelection();
	refresh();
}
void LLFloaterInspectTexture::onClickCpToInvSelected()
{
	if(mTextureList->getAllSelected().size() == 0) return;
	LLScrollListItem* first_selected =mTextureList->getFirstSelected();
	if (first_selected)
	{
		LLUUID mUUID = first_selected->getUUID();
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
		std::string msg = "Texture : ";
		msg.append(mUUID.asString());
		msg.append(" saved in temporary in your inventory");
		LLSD args;
		args["MESSAGE"] = msg;
		LLNotificationsUtil::add("SystemMessage", args);
	}
}

void LLFloaterInspectTexture::onClickCpToInvAll()
{
	typedef std::map<LLUUID, bool>::iterator map_iter;
	std::string msg = "Textures : \n";
	for(map_iter i = unique_textures.begin(); i != unique_textures.end(); ++i)
	{
		LLUUID mUUID = (*i).first;
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
		msg.append(mUUID.asString());
		msg.append("\n");
	}
	msg.append(" saved in temporary in your inventory ");
	LLSD args;
	args["MESSAGE"] = msg;
	LLNotificationsUtil::add("SystemMessage", args);
}

void LLFloaterInspectTexture::onSelectObject()
{
	if(LLFloaterInspectTexture::getSelectedUUID() != LLUUID::null)
	{
		getChildView("button selection")->setEnabled(true);
		getChildView("button all")->setEnabled(true);
		texture_ctrl->setImageAssetID(getSelectedUUID());
	}
}

LLUUID LLFloaterInspectTexture::getSelectedUUID()
{
	if(mTextureList->getAllSelected().size() > 0)
	{
		LLScrollListItem* first_selected =mTextureList->getFirstSelected();
		if (first_selected)
		{
			return first_selected->getUUID();
		}
		
	}
	return LLUUID::null;
}

void LLFloaterInspectTexture::refresh()
{
	unique_textures.clear();

	std::string uuid_str;
	std::string height_str;
	std::string width_str;
	std::string type_str;
	S32 pos = mTextureList->getScrollPos();
	getChildView("button selection")->setEnabled(false);
	getChildView("button all")->setEnabled(false);
	LLUUID selected_uuid;
	S32 selected_index = mTextureList->getFirstSelectedIndex();
	if(selected_index > -1)
	{
		LLScrollListItem* first_selected =
			mTextureList->getFirstSelected();
		if (first_selected)
		{
			selected_uuid = first_selected->getUUID();
		}
	}
	mTextureList->operateOnAll(LLScrollListCtrl::OP_DELETE);

	for (LLObjectSelection::valid_iterator iter = mObjectSelection->valid_begin();
		 iter != mObjectSelection->valid_end(); iter++)
	{
		LLSelectNode* obj = *iter;
		if (obj->mCreationDate == 0)
		{	
			continue;
		}

		U8 te_count = obj->getObject()->getNumTEs();

		typedef std::map< LLUUID, std::vector<U8> > map_t;
		map_t faces_per_texture;
		for (U8 j = 0; j < (te_count+2); j++)
		{
			LLUUID image_id;

			if( j== te_count)
			{
					if(!obj->getObject()->isSculpted())continue;
						LLSculptParams *sculpt_params = (LLSculptParams *)(obj->getObject()->getParameterEntry(LLNetworkData::PARAMS_SCULPT));
						image_id = sculpt_params->getSculptTexture();
						height_str = "";
						width_str = "";
						type_str= "Sculpt";
			}
			else if(j == te_count+1)
			{
					if(!obj->getObject()->isParticleSource())continue;
					image_id = obj->getObject()->mPartSourcep->getImage()->getID();
						height_str = "";
						width_str = "";
						type_str= "Part.";	
			}
			else
			{
					if (!obj->isTESelected(j)) continue;
						LLViewerTexture* img = obj->getObject()->getTEImage(j);
						image_id = img->getID();
						height_str = llformat("%d", img->getHeight());
						width_str =  llformat("%d", img->getWidth());
						type_str  = "  - ";			

			}
			if(unique_textures[image_id])continue;// no doublon
			unique_textures[image_id] = true;
			image_id.toString(uuid_str);

			LLSD row;

			row["id"] = image_id;

			int i = 0;
			row["columns"][i]["column"] = "uuid_text";
			row["columns"][i]["type"] = "text";
			row["columns"][i]["value"] = uuid_str;
			++i;
			row["columns"][i]["column"] = "height";
			row["columns"][i]["type"] = "text";
			row["columns"][i]["value"] = height_str;
			++i;
			row["columns"][i]["column"] = "width";
			row["columns"][i]["type"] = "text";
			row["columns"][i]["value"] = width_str;;
			++i;
			row["columns"][i]["column"] = "types";
			row["columns"][i]["type"] = "text";
			row["columns"][i]["value"] = type_str;
			++i;

			mTextureList->addElement(row, ADD_TOP);
		}
	}
	if(selected_index > -1 && mTextureList->getItemIndex(selected_uuid) == selected_index)
	{
		mTextureList->selectNthItem(selected_index);
	}
	else
	{
		mTextureList->selectNthItem(0);
	}
	onSelectObject();
	mTextureList->setScrollPos(pos);

}

void LLFloaterInspectTexture::onFocusReceived()
{
	LLToolMgr::getInstance()->setTransientTool(LLToolCompInspect::getInstance());
	LLFloater::onFocusReceived();
}


void LLFloaterInspectTexture::dirty()
{
	setDirty();
	unique_textures.clear();
}

void LLFloaterInspectTexture::draw()
{
	if (mDirty)
	{
		refresh();
		mDirty = FALSE;
	}

	LLFloater::draw();
}
