/** 
 * @fslslbridge.h 
 * @FSLSLBridge header 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Viewer Project, Inc.
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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * $/LicenseInfo$
 */
#ifndef FS_LSLBRIDGE_H
#define FS_LSLBRIDGE_H

#include "llviewerprecompiledheaders.h"
#include "llchat.h"
#include "llhttpclient.h"
#include "llinventorymodel.h"
#include "fslslbridgerequest.h"
#include "llviewerinventory.h"
#include "llinventoryobserver.h"
#include "lleventtimer.h"

//
//-TT Client LSL Bridge File
//

class FSLSLBridge : public LLSingleton<FSLSLBridge>, public LLHTTPClient::Responder //, public LLVOInventoryListener
{
	static const U8 BRIDGE_POINT = 127;

	friend class FSLSLBridgeScriptCallback;
	friend class FSLSLBridgeRezCallback;
	friend class FSLSLBridgeInventoryObserver;
	friend class FSLSLBridgeCleanupTimer;

public:
	FSLSLBridge();
	~FSLSLBridge();

	bool lslToViewer(std::string message, LLUUID fromID, LLUUID ownerID);
	bool viewerToLSL(std::string message, FSLSLBridgeRequestResponder *responder = NULL);

	void initBridge();
	void recreateBridge();
	void processAttach(LLViewerObject *object, const LLViewerJointAttachment *attachment);
	void startCreation();

	bool getBridgeCreating() {return mBridgeCreating; };
	void setBridgeCreating(bool status) { mBridgeCreating = status; };

	void setBridge(LLViewerInventoryItem* item) { mpBridge = item; };
	LLViewerInventoryItem* getBridge() { return mpBridge; };

	void checkBridgeScriptName(std::string fileName);
	std::string currentFullName() { return mCurrentFullName; }
	void finishBridge();

protected:

	/*virtual*/ void inventoryChanged(LLViewerObject* obj,
									 LLInventoryObject::object_list_t* inv,
									 S32 serial_num,
									 void* queue);
	LLUUID	mScriptItemID;	//internal script ID

private:
	std::string				mCurrentURL;
	int						mCurrentChannel;
	bool					mBridgeCreating;
	LLViewerInventoryItem*	mpBridge;
	std::string				mCurrentFullName;

protected:
	LLViewerInventoryItem* findInvObject(std::string obj_name, LLUUID catID, LLAssetType::EType type);
	LLUUID findFSCategory();
	bool isItemAttached(LLUUID iID);
	void createNewBridge();
	void create_script_inner(LLViewerObject* object);
	bool isOldBridgeVersion(LLInventoryItem *item);
	void reportToNearbyChat(std::string message);
	void cleanUpBridgeFolder();
	void setupBridge(LLViewerObject *object);
	void initCreationStep();
	void cleanUpBridge();
};


class FSLSLBridgeRezCallback : public LLInventoryCallback
{
public:
	FSLSLBridgeRezCallback();
	void fire(const LLUUID& inv_item);

protected:
	~FSLSLBridgeRezCallback();
};

class FSLSLBridgeScriptCallback : public LLInventoryCallback
{
public:
	FSLSLBridgeScriptCallback();
	void fire(const LLUUID& inv_item);
	std::string prepUploadFile();

protected:
	~FSLSLBridgeScriptCallback();
};

class FSLSLBridgeInventoryObserver : public LLInventoryFetchDescendentsObserver
{
public:
	FSLSLBridgeInventoryObserver(const LLUUID& cat_id = LLUUID::null):LLInventoryFetchDescendentsObserver(cat_id) {}
	/*virtual*/ void done() { gInventory.removeObserver(this); FSLSLBridge::instance().startCreation(); }

protected:
	~FSLSLBridgeInventoryObserver() {}

};

class FSLSLBridgeCleanupTimer : public LLEventTimer
{
public:
	FSLSLBridgeCleanupTimer(F32 period) : LLEventTimer(period) {}
	BOOL tick();
	void startTimer() { mEventTimer.start(); }
	void stopTimer() { mEventTimer.stop(); }

};

#endif // FS_LSLBRIDGE_H
