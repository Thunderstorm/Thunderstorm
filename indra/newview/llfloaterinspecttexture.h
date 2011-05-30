/** 
* @file llfloaterinspecttexture.h

* $/LicenseInfo$
*/

#ifndef LL_LLFLOATERINSPECTTEXTURE_H
#define LL_LLFLOATERINSPECTTEXTURE_H

#include "llfloater.h"
#include "llscrolllistctrl.h"
#include "lltexturectrl.h"


//class LLTool;
class LLObjectSelection;
class LLScrollListCtrl;
class LLUICtrl;

class LLFloaterInspectTexture : public LLFloater
{
	friend class LLFloaterReg;
public:

//	static void show(void* ignored = NULL);
	void onOpen(const LLSD& key);
	virtual BOOL postBuild();
	void dirty();
	LLUUID getSelectedUUID();
	virtual void draw();
	virtual void refresh();
//	static BOOL isVisible();
	virtual void onFocusReceived();
	void onClickCpToInvSelected();
	void onClickCpToInvAll();
	void onSelectObject();
	LLScrollListCtrl* mTextureList;
	LLTextureCtrl*    texture_ctrl;
	std::map<LLUUID, bool> unique_textures;
protected:
	// protected members
	void setDirty() { mDirty = TRUE; }
	bool mDirty;

private:
	
	LLFloaterInspectTexture(const LLSD& key);
	virtual ~LLFloaterInspectTexture(void);
	// static data
//	static LLFloaterInspectTexture* sInstance;

	LLSafeHandle<LLObjectSelection> mObjectSelection;
};

#endif //LL_LLFLOATERINSPECTTEXTURE_H
