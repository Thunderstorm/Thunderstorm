#define _USE_MATH_DEFINES
#include "linden_common.h"

#include "tsbvhexporter.h"

#include <boost/tokenizer.hpp>

#include "lldatapacker.h"
#include "llkeyframemotion.h"
#include "llquantize.h"

#define INCHES_TO_METERS 0.02540005f

static LLVector3 revQ(F32 p0, F32 p1, F32 p2, F32 p3, F32 e)
{
	double dp0 = p0;
	double dp1 = p1;
	double dp2 = p2;
	double dp3 = p3;
	double de = e;
	double sint2 = 2.0*(dp0*dp2 + de*dp1*dp3);
	F32 t1;
	F32 t2 = asin(sint2);
	F32 t3;
	LLVector3 ret;

	if(fabs(sint2) > .9999995) {
		t1 = atan2(dp1, dp0);
		if(sint2 < 0.f) {
			t2 = asin(-1.f);
		} else {
			t2 = asin(1.f);
		}
		t3 = 0.f;
	} else {
		t1 = atan2(2.0 * (dp0 * dp1 - de * dp2 * dp3), 1.0 - 2.0 * (dp1*dp1 + dp2*dp2));
		t3 = atan2(2.0 * (dp0 * dp3 - de * dp1 * dp2), 1.0 - 2.0 * (dp2*dp2 + dp3*dp3));
	}
	ret.set(t1, t2, t3);
	ret = ret * RAD_TO_DEG;

	return ret;
}

static LLVector3 revMayaQ(LLQuaternion q, LLQuaternion::Order order)
{
	LLVector3 ret;
	LLVector3 tmp;

	switch(order) {
		case LLQuaternion::ZYX:
			tmp = revQ(q.mQ[VW], q.mQ[VZ], q.mQ[VY], q.mQ[VX], -1.f);
			ret.set(tmp.mV[VZ], tmp.mV[VY], tmp.mV[VX]);
			break;
		case LLQuaternion::XZY:
			tmp = revQ(q.mQ[VW], q.mQ[VX], q.mQ[VZ], q.mQ[VY], -1.f);
			ret.set(tmp.mV[VX], tmp.mV[VZ], tmp.mV[VY]);
			break;
		case LLQuaternion::YZX:
			tmp = revQ(q.mQ[VW], q.mQ[VY], q.mQ[VZ], q.mQ[VX], 1.f);
			ret.set(tmp.mV[VZ], tmp.mV[VX], tmp.mV[VY]);
			break;
		case LLQuaternion::XYZ:
			tmp = revQ(q.mQ[VW], q.mQ[VX], q.mQ[VY], q.mQ[VZ], 1.f);
			ret.set(tmp.mV[VX], tmp.mV[VY], tmp.mV[VZ]);
			break;
		default:
			// ERROR
			break;
	}
	return ret;
}

TSBVHExporter::TSBVHExporter()
{
	LLMatrix3 frame;
	frame.mMatrix[0][0] = 0.0;
	frame.mMatrix[0][1] = 1.0;
	frame.mMatrix[0][2] = 0.0;

	frame.mMatrix[1][0] = 0.0;
	frame.mMatrix[1][1] = 0.0;
	frame.mMatrix[1][2] = 1.0;

	frame.mMatrix[2][0] = 1.0;
	frame.mMatrix[2][1] = 0.0;
	frame.mMatrix[2][2] = 0.0;

	Joint *joint = new Joint("hip");

	joint->mOutName = "mPelvis";
	joint->mChildTreeMaxDepth = 6;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("abdomen");

	joint->mOutName = "mTorso";
	joint->mChildTreeMaxDepth = 5;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("chest");

	joint->mOutName = "mChest";
	joint->mChildTreeMaxDepth = 4;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("neck");

	joint->mOutName = "mNeck";
	joint->mChildTreeMaxDepth = 1;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("head");

	joint->mOutName = "mHead";
	joint->mChildTreeMaxDepth = 0;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("lCollar");

	joint->mOutName = "mCollarLeft";
	joint->mChildTreeMaxDepth = 3;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'Y';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'X';

	mJoints.push_back(joint);

	joint = new Joint("lShldr");

	joint->mOutName = "mShoulderLeft";
	joint->mChildTreeMaxDepth = 2;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'Z';
	joint->mOrder[1] = 'Y';
	joint->mOrder[2] = 'X';

	mJoints.push_back(joint);

	joint = new Joint("lForeArm");

	joint->mOutName = "mElbowLeft";
	joint->mChildTreeMaxDepth = 1;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'Y';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'X';

	mJoints.push_back(joint);

	joint = new Joint("lHand");

	joint->mOutName = "mWristLeft";
	joint->mChildTreeMaxDepth = 0;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'Z';
	joint->mOrder[1] = 'Y';
	joint->mOrder[2] = 'X';

	mJoints.push_back(joint);

	joint = new Joint("rCollar");

	joint->mOutName = "mCollarRight";
	joint->mChildTreeMaxDepth = 3;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'Y';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'X';

	mJoints.push_back(joint);

	joint = new Joint("rShldr");

	joint->mOutName = "mShoulderRight";
	joint->mChildTreeMaxDepth = 2;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'Z';
	joint->mOrder[1] = 'Y';
	joint->mOrder[2] = 'X';

	mJoints.push_back(joint);

	joint = new Joint("rForeArm");

	joint->mOutName = "mElbowRight";
	joint->mChildTreeMaxDepth = 1;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'Y';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'X';

	mJoints.push_back(joint);

	joint = new Joint("rHand");

	joint->mOutName = "mWristRight";
	joint->mChildTreeMaxDepth = 0;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'Z';
	joint->mOrder[1] = 'Y';
	joint->mOrder[2] = 'X';

	mJoints.push_back(joint);

	joint = new Joint("lThigh");

	joint->mOutName = "mHipLeft";
	joint->mChildTreeMaxDepth = 2;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("lShin");

	joint->mOutName = "mKneeLeft";
	joint->mChildTreeMaxDepth = 1;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("lFoot");

	joint->mOutName = "mAnkleLeft";
	joint->mChildTreeMaxDepth = 0;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Y';
	joint->mOrder[2] = 'Z';

	mJoints.push_back(joint);

	joint = new Joint("rThigh");

	joint->mOutName = "mHipRight";
	joint->mChildTreeMaxDepth = 2;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("rShin");

	joint->mOutName = "mKneeRight";
	joint->mChildTreeMaxDepth = 1;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Z';
	joint->mOrder[2] = 'Y';

	mJoints.push_back(joint);

	joint = new Joint("rFoot");

	joint->mOutName = "mAnkleRight";
	joint->mChildTreeMaxDepth = 0;
	joint->mFrameMatrix = frame;
	joint->mOffsetMatrix.setIdentity();
	joint->mOrder[0] = 'X';
	joint->mOrder[1] = 'Y';
	joint->mOrder[2] = 'Z';

	mJoints.push_back(joint);
}

TSBVHExporter::~TSBVHExporter()
{
	std::for_each(mJoints.begin(),mJoints.end(),DeletePointer());
}

BOOL TSBVHExporter::deserialize(LLDataPacker &dp)
{
	U16 u16;
	U32 numJoints;
	U32 i;
	int j;

	//parse header
	dp.unpackU16(u16, "version");
	if(u16 != 1) return FALSE;
	dp.unpackU16(u16, "sub_version");
	if(u16 != 0) return FALSE;
	dp.unpackS32(mPriority, "base_priority");
	dp.unpackF32(mDuration, "duration");
	dp.unpackString(mEmoteName, "emote_name");
	dp.unpackF32(mLoopInPoint, "loop_in_point");
	dp.unpackF32(mLoopOutPoint, "loop_out_point");
	dp.unpackS32(mLoop, "loop");
	dp.unpackF32(mEaseIn, "ease_in_duration");
	dp.unpackF32(mEaseOut, "ease_out_duration");
	dp.unpackU32(mHand, "hand_pose");
	dp.unpackU32(numJoints, "num_joints");
	
	for(i = 0; i < numJoints; ++i) {
		LLQuaternion::Order order;
		LLQuaternion last_rot;
		Joint *joint = NULL;
		std::string jointName;
		dp.unpackString(jointName, "joint_name");

		JointVector::iterator ji;
		for(ji = mJoints.begin(); ji != mJoints.end(); ++ji) {
			joint = *ji;
			if(joint->mOutName == jointName) break;
		}
		if(ji == mJoints.end()) continue;

		dp.unpackS32(joint->mPriority, "joint_priority");

		dp.unpackS32(joint->mNumRotKeys, "num_rot_keys");
		int last_frame = 1;

		order = StringToOrder(joint->mOrder);

		for(j = 0; j < joint->mNumRotKeys; ++j) {
			U16 time_short; // = F32_to_U16(time, 0.f, mDuration);
			dp.unpackU16(time_short, "time");
			F32 time = U16_to_F32(time_short, 0.f, mDuration);
			int frame;

			Key key;

			if(j == 0) { //First Key
				if(i == 0) { //First Joint
					mFrameTime = time * 0.5; // first entry is always frame 2
					mNumFrames = (S32)floor(mDuration / mFrameTime + 0.5);
					int n;
					for(ji = mJoints.begin(); ji != mJoints.end(); ++ji) {
						Joint *pjoint = *ji;
						pjoint->mKeys.reserve(mNumFrames);
						for(n = 0; n < mNumFrames; ++n) {
							pjoint->mKeys.push_back(key);
						}
					}
					for(n = 0; n < mNumFrames; ++n) {
						mJoints[0]->mKeys[n].mPos[1] = 43.5285f;
					}
				}
				last_frame = 1;
				last_rot.loadIdentity();
				joint->mKeys[0].mIgnorePos = TRUE;
				joint->mKeys[0].mIgnoreRot = TRUE;
			}

			frame = (int)floor(time / mFrameTime + 0.5f);

			U16 x, y, z;
			//LLVector3 rot_vec = outRot.packToVector3();
			//rot_vec.quantize16(-1.f, 1.f, -1.f, 1.f);
			//x = F32_to_U16(rot_vec.mV[VX], -1.f, 1.f);
			//y = F32_to_U16(rot_vec.mV[VY], -1.f, 1.f);
			//z = F32_to_U16(rot_vec.mV[VZ], -1.f, 1.f);
			dp.unpackU16(x, "rot_angle_x");
			dp.unpackU16(y, "rot_angle_y");
			dp.unpackU16(z, "rot_angle_z");

			LLVector3 rot_vec;
			rot_vec.mV[VX] = U16_to_F32(x, -1.f, 1.f);
			rot_vec.mV[VY] = U16_to_F32(y, -1.f, 1.f);
			rot_vec.mV[VZ] = U16_to_F32(z, -1.f, 1.f);

			LLQuaternion outRot;
			LLQuaternion inRot;

			LLQuaternion frameRot( joint->mFrameMatrix );
			LLQuaternion frameRotInv = ~frameRot;

			LLQuaternion offsetRot( joint->mOffsetMatrix );

			outRot.unpackFromVector3(rot_vec);

			//Forward: LLQuaternion outRot =  frameRotInv * inRot * frameRot * offsetRot;
			inRot = frameRot * outRot * ~offsetRot * frameRotInv;

			rot_vec = revMayaQ(inRot, order);

			joint->mKeys[frame - 1].mRot[0] = rot_vec.mV[VX];
			joint->mKeys[frame - 1].mRot[1] = rot_vec.mV[VY];
			joint->mKeys[frame - 1].mRot[2] = rot_vec.mV[VZ];

			//interpolate in between
			int n;
			int num = frame - last_frame;
			LLQuaternion interp;

			for(n = last_frame + 1; n < frame; ++n) {
				interp = lerp(1.f/num * (n - last_frame), last_rot, inRot);

				rot_vec = revMayaQ(interp, order);

				joint->mKeys[n - 1].mRot[0] = rot_vec.mV[VX];
				joint->mKeys[n - 1].mRot[1] = rot_vec.mV[VY];
				joint->mKeys[n - 1].mRot[2] = rot_vec.mV[VZ];
				joint->mKeys[n - 1].mIgnoreRot = TRUE;
			}

			last_frame = frame;
			last_rot = inRot;
		}

		dp.unpackS32(joint->mNumPosKeys, "num_pos_keys");

		LLVector3 last_pos;
		LLVector3 current_pos;
		LLVector3 relKey(0.0f, 43.5285f, 0.0f);

		for(j = 0; j < joint->mNumPosKeys; ++j) { //assume this is for hip only
			U16 time_short; // = F32_to_U16(time, 0.f, mDuration);
			dp.unpackU16(time_short, "time");

			F32 time = U16_to_F32(time_short, 0.f, mDuration);

			if(j == 0) {
				last_pos.set(0.0f, 43.5285f, 0.0f);
				last_frame = 1;
			}

			int frame = (int)floor(time / mFrameTime + 0.5f);

			U16 x, y, z;
			//outPos.quantize16(-LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			//x = F32_to_U16(outPos.mV[VX], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			//y = F32_to_U16(outPos.mV[VY], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			//z = F32_to_U16(outPos.mV[VZ], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			dp.unpackU16(x, "pos_x");
			dp.unpackU16(y, "pos_y");
			dp.unpackU16(z, "pos_z");

			current_pos.mV[VX] = U16_to_F32(x, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			current_pos.mV[VY] = U16_to_F32(y, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			current_pos.mV[VZ] = U16_to_F32(z, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);

			//current_pos += relPos;

			current_pos /= INCHES_TO_METERS;

			LLQuaternion frameRot( joint->mFrameMatrix );

			current_pos = current_pos * ~frameRot + relKey;

			joint->mKeys[frame - 1].mPos[0] = current_pos.mV[VX];
			joint->mKeys[frame - 1].mPos[1] = current_pos.mV[VY];
			joint->mKeys[frame - 1].mPos[2] = current_pos.mV[VZ];

			//interpolate
			int n;
			int num = frame - last_frame;
			LLVector3 interp;

			for(n = last_frame + 1; n < frame; ++n) {
				interp = lerp(last_pos, current_pos, 1.f/num * (n - last_frame));

				joint->mKeys[n - 1].mPos[0] = interp.mV[VX];
				joint->mKeys[n - 1].mPos[1] = interp.mV[VY];
				joint->mKeys[n - 1].mPos[2] = interp.mV[VZ];
				joint->mKeys[n - 1].mIgnorePos = TRUE;
			}
			
			last_pos = current_pos;
			last_frame = frame;
		}
	}

	S32 num_constraints; // = (S32)mConstraints.size();
	dp.unpackS32(num_constraints, "num_constraints");
	// hopefully no contraints
	return TRUE;
}

BOOL TSBVHExporter::exportBVHFile(const char *path)
{S32 file_size;
	LLAPRFile infile ;
	infile.open(path, LL_APR_R);
	apr_file_t *fp = infile.getFileHandle();
	if(!fp) return FALSE;

	FileCloser fileCloser(fp);

	apr_file_puts("HIERARCHY\n"
	"ROOT hip\n"
	"{\n"
	"\tOFFSET 0.000000 0.000000 0.000000\n"
	"\tCHANNELS 6 Xposition Yposition Zposition Xrotation Zrotation Yrotation \n"
	"\tJOINT abdomen\n"
	"\t{\n"
	"\t\tOFFSET 0.000000 3.422050 0.000000\n"
	"\t\tCHANNELS 3 Xrotation Zrotation Yrotation \n"
	"\t\tJOINT chest\n"
	"\t\t{\n"
	"\t\t\tOFFSET 0.000000 8.486693 -0.684411\n"
	"\t\t\tCHANNELS 3 Xrotation Zrotation Yrotation \n"
	"\t\t\tJOINT neck\n"
	"\t\t\t{\n"
	"\t\t\t\tOFFSET 0.000000 10.266162 -0.273764\n"
	"\t\t\t\tCHANNELS 3 Xrotation Zrotation Yrotation \n"
	"\t\t\t\tJOINT head\n"
	"\t\t\t\t{\n"
	"\t\t\t\t\tOFFSET 0.000000 3.148285 0.000000\n"
	"\t\t\t\t\tCHANNELS 3 Xrotation Zrotation Yrotation \n"
	"\t\t\t\t\tEnd Site\n"
	"\t\t\t\t\t{\n"
	"\t\t\t\t\t\tOFFSET 0.000000 3.148289 0.000000\n"
	"\t\t\t\t\t}\n"
	"\t\t\t\t}\n"
	"\t\t\t}\n"
	"\t\t\tJOINT lCollar\n"
	"\t\t\t{\n"
	"\t\t\t\tOFFSET 3.422053 6.707223 -0.821293\n"
	"\t\t\t\tCHANNELS 3 Yrotation Zrotation Xrotation \n"
	"\t\t\t\tJOINT lShldr\n"
	"\t\t\t\t{\n"
	"\t\t\t\t\tOFFSET 3.285171 0.000000 0.000000\n"
	"\t\t\t\t\tCHANNELS 3 Zrotation Yrotation Xrotation \n"
	"\t\t\t\t\tJOINT lForeArm\n"
	"\t\t\t\t\t{\n"
	"\t\t\t\t\t\tOFFSET 10.129278 0.000000 0.000000\n"
	"\t\t\t\t\t\tCHANNELS 3 Yrotation Zrotation Xrotation \n"
	"\t\t\t\t\t\tJOINT lHand\n"
	"\t\t\t\t\t\t{\n"
	"\t\t\t\t\t\t\tOFFSET 8.486692 0.000000 0.000000\n"
	"\t\t\t\t\t\t\tCHANNELS 3 Zrotation Yrotation Xrotation \n"
	"\t\t\t\t\t\t\tEnd Site\n"
	"\t\t\t\t\t\t\t{\n"
	"\t\t\t\t\t\t\t\tOFFSET 4.106464 0.000000 0.000000\n"
	"\t\t\t\t\t\t\t}\n"
	"\t\t\t\t\t\t}\n"
	"\t\t\t\t\t}\n"
	"\t\t\t\t}\n"
	"\t\t\t}\n"
	"\t\t\tJOINT rCollar\n"
	"\t\t\t{\n"
	"\t\t\t\tOFFSET -3.558935 6.707223 -0.821293\n"
	"\t\t\t\tCHANNELS 3 Yrotation Zrotation Xrotation \n"
	"\t\t\t\tJOINT rShldr\n"
	"\t\t\t\t{\n"
	"\t\t\t\t\tOFFSET -3.148289 0.000000 0.000000\n"
	"\t\t\t\t\tCHANNELS 3 Zrotation Yrotation Xrotation \n"
	"\t\t\t\t\tJOINT rForeArm\n"
	"\t\t\t\t\t{\n"
	"\t\t\t\t\t\tOFFSET -10.266159 0.000000 0.000000\n"
	"\t\t\t\t\t\tCHANNELS 3 Yrotation Zrotation Xrotation \n"
	"\t\t\t\t\t\tJOINT rHand\n"
	"\t\t\t\t\t\t{\n"
	"\t\t\t\t\t\t\tOFFSET -8.349810 0.000000 0.000000\n"
	"\t\t\t\t\t\t\tCHANNELS 3 Zrotation Yrotation Xrotation \n"
	"\t\t\t\t\t\t\tEnd Site\n"
	"\t\t\t\t\t\t\t{\n"
	"\t\t\t\t\t\t\t\tOFFSET -4.106464 0.000000 0.000000\n"
	"\t\t\t\t\t\t\t}\n"
	"\t\t\t\t\t\t}\n"
	"\t\t\t\t\t}\n"
	"\t\t\t\t}\n"
	"\t\t\t}\n"
	"\t\t}\n"
	"\t}\n"
	"\tJOINT lThigh\n"
	"\t{\n"
	"\t\tOFFSET 5.338403 -1.642589 1.368821\n"
	"\t\tCHANNELS 3 Xrotation Zrotation Yrotation \n"
	"\t\tJOINT lShin\n"
	"\t\t{\n"
	"\t\t\tOFFSET -2.053232 -20.121670 0.000000\n"
	"\t\t\tCHANNELS 3 Xrotation Zrotation Yrotation \n"
	"\t\t\tJOINT lFoot\n"
	"\t\t\t{\n"
	"\t\t\t\tOFFSET 0.000000 -19.300380 -1.231939\n"
	"\t\t\t\tCHANNELS 3 Xrotation Yrotation Zrotation \n"
	"\t\t\t\tEnd Site\n"
	"\t\t\t\t{\n"
	"\t\t\t\t\tOFFSET 0.000000 -2.463878 4.653993\n"
	"\t\t\t\t}\n"
	"\t\t\t}\n"
	"\t\t}\n"
	"\t}\n"
	"\tJOINT rThigh\n"
	"\t{\n"
	"\t\tOFFSET -5.338403 -1.642589 1.368821\n"
	"\t\tCHANNELS 3 Xrotation Zrotation Yrotation \n"
	"\t\tJOINT rShin\n"
	"\t\t{\n"
	"\t\t\tOFFSET 2.053232 -20.121670 0.000000\n"
	"\t\t\tCHANNELS 3 Xrotation Zrotation Yrotation \n"
	"\t\t\tJOINT rFoot\n"
	"\t\t\t{\n"
	"\t\t\t\tOFFSET 0.000000 -19.300380 -1.231939\n"
	"\t\t\t\tCHANNELS 3 Xrotation Yrotation Zrotation \n"
	"\t\t\t\tEnd Site\n"
	"\t\t\t\t{\n"
	"\t\t\t\t\tOFFSET 0.000000 -2.463878 4.653993\n"
	"\t\t\t\t}\n"
	"\t\t\t}\n"
	"\t\t}\n"
	"\t}\n"
	"}\n"
	"MOTION\n", fp);

	apr_file_printf(fp, "Frames: %d\n", mNumFrames);
	apr_file_printf(fp, "Frame Time: %.6f\n", mFrameTime);

	int i;

	for(i = 0; i < mNumFrames; ++i) {
		JointVector::iterator ji;
		for(ji = mJoints.begin(); ji != mJoints.end(); ++ji) {
			Joint *joint = *ji;
			if(ji == mJoints.begin()) {
				apr_file_printf(fp, "%.6f %.6f %.6f ",
					joint->mKeys[i].mPos[0],
					joint->mKeys[i].mPos[1],
					joint->mKeys[i].mPos[2]);
			}
			apr_file_printf(fp, "%.6f %.6f %.6f ",
				joint->mKeys[i].mRot[ joint->mOrder[0] - 'X' ],
				joint->mKeys[i].mRot[ joint->mOrder[1] - 'X' ],
				joint->mKeys[i].mRot[ joint->mOrder[2] - 'X' ]);
		}

		apr_file_puts("\n", fp);
	}

	return TRUE;
}
