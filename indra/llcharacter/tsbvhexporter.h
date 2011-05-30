#ifndef LL_TSBVHEXPORTER_H
#define LL_TSBVHEXPORTER_H

#include "llbvhloader.h"

class TSBVHExporter
{
public:
	TSBVHExporter();
	~TSBVHExporter();

	BOOL deserialize(LLDataPacker &dp);
	BOOL exportBVHFile(const char *filename);

protected:
	S32					mNumFrames;
	F32					mFrameTime;
	F32					mDuration;
	JointVector			mJoints;
	//ConstraintVector	mConstraints;
	//TranslationMap		mTranslations;

	S32					mPriority;
	BOOL				mLoop;
	F32					mLoopInPoint;
	F32					mLoopOutPoint;
	F32					mEaseIn;
	F32					mEaseOut;
	U32					mHand;
	std::string			mEmoteName;
};
#endif
