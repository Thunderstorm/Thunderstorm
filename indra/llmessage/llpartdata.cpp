/** 
 * @file llpartdata.cpp
 * @brief Particle system data packing
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
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

#include "linden_common.h"

#include "llpartdata.h"
#include "message.h"

#include "lldatapacker.h"
#include "v4coloru.h"

#include "llsdutil.h"
#include "llsdutil_math.h"



const S32 PS_PART_DATA_BLOCK_SIZE = 4 + 2 + 4 + 4 + 2 + 2; // 18
const S32 PS_DATA_BLOCK_SIZE = 68 + PS_PART_DATA_BLOCK_SIZE; // 68 + 18 = 86


const F32 MAX_PART_SCALE = 4.f;

BOOL LLPartData::pack(LLDataPacker &dp)
{
	LLColor4U coloru;
	dp.packU32(mFlags, "pdflags");
	dp.packFixed(mMaxAge, "pdmaxage", FALSE, 8, 8);
	coloru.setVec(mStartColor);
	dp.packColor4U(coloru, "pdstartcolor");
	coloru.setVec(mEndColor);
	dp.packColor4U(coloru, "pdendcolor");
	dp.packFixed(mStartScale.mV[0], "pdstartscalex", FALSE, 3, 5);
	dp.packFixed(mStartScale.mV[1], "pdstartscaley", FALSE, 3, 5);
	dp.packFixed(mEndScale.mV[0], "pdendscalex", FALSE, 3, 5);
	dp.packFixed(mEndScale.mV[1], "pdendscaley", FALSE, 3, 5);
	return TRUE;
}

LLSD LLPartData::asLLSD() const
{
	LLSD sd = LLSD();
	sd["pdflags"] = ll_sd_from_U32(mFlags);
	sd["pdmaxage"] = mMaxAge;
	sd["pdstartcolor"] = ll_sd_from_color4(mStartColor);
	sd["pdendcolor"] = ll_sd_from_color4(mEndColor);
	sd["pdstartscale"] = ll_sd_from_vector2(mStartScale);
	sd["pdendscale"] = ll_sd_from_vector2(mEndScale);
	return sd;
}

bool LLPartData::fromLLSD(LLSD& sd)
{
	mFlags = ll_U32_from_sd(sd["pdflags"]);
	mMaxAge = (F32)sd["pdmaxage"].asReal();
	mStartColor = ll_color4_from_sd(sd["pdstartcolor"]);
	mEndColor = ll_color4_from_sd(sd["pdendcolor"]);
	mStartScale = ll_vector2_from_sd(sd["pdstartscale"]);
	mEndScale = ll_vector2_from_sd(sd["pdendscale"]);
	return true;
}


BOOL LLPartData::unpack(LLDataPacker &dp)
{
	LLColor4U coloru;

	dp.unpackU32(mFlags, "pdflags");
	dp.unpackFixed(mMaxAge, "pdmaxage", FALSE, 8, 8);

	dp.unpackColor4U(coloru, "pdstartcolor");
	mStartColor.setVec(coloru);
	dp.unpackColor4U(coloru, "pdendcolor");
	mEndColor.setVec(coloru);
	dp.unpackFixed(mStartScale.mV[0], "pdstartscalex", FALSE, 3, 5);
	dp.unpackFixed(mStartScale.mV[1], "pdstartscaley", FALSE, 3, 5);
	dp.unpackFixed(mEndScale.mV[0], "pdendscalex", FALSE, 3, 5);
	dp.unpackFixed(mEndScale.mV[1], "pdendscaley", FALSE, 3, 5);
	return TRUE;
}


void LLPartData::setFlags(const U32 flags)
{
	mFlags = flags;
}


void LLPartData::setMaxAge(const F32 max_age)
{
	mMaxAge = llclamp(max_age, 0.f, 30.f);
}


void LLPartData::setStartScale(const F32 xs, const F32 ys)
{
	mStartScale.mV[VX] = llmin(xs, MAX_PART_SCALE);
	mStartScale.mV[VY] = llmin(ys, MAX_PART_SCALE);
}


void LLPartData::setEndScale(const F32 xs, const F32 ys)
{
	mEndScale.mV[VX] = llmin(xs, MAX_PART_SCALE);
	mEndScale.mV[VY] = llmin(ys, MAX_PART_SCALE);
}


void LLPartData::setStartColor(const LLVector3 &rgb)
{
	mStartColor.setVec(rgb.mV[0], rgb.mV[1], rgb.mV[2]);
}


void LLPartData::setEndColor(const LLVector3 &rgb)
{
	mEndColor.setVec(rgb.mV[0], rgb.mV[1], rgb.mV[2]);
}

void LLPartData::setStartAlpha(const F32 alpha)
{
	mStartColor.mV[3] = alpha;
}
void LLPartData::setEndAlpha(const F32 alpha)
{
	mEndColor.mV[3] = alpha;
}


LLPartSysData::LLPartSysData()
{
	mCRC = 0;
	mFlags = 0;

	mPartData.mFlags = 0;
	mPartData.mStartColor = LLColor4(1.f, 1.f, 1.f, 1.f);
	mPartData.mEndColor = LLColor4(1.f, 1.f, 1.f, 1.f);
	mPartData.mStartScale = LLVector2(1.f, 1.f);
	mPartData.mEndScale = LLVector2(1.f, 1.f);
	mPartData.mMaxAge = 10.0;

	mMaxAge = 0.0;
	mStartAge = 0.0;
	mPattern = LL_PART_SRC_PATTERN_DROP;			// Pattern for particle velocity
	mInnerAngle = 0.0;								// Inner angle of PATTERN_ANGLE_*
	mOuterAngle = 0.0;								// Outer angle of PATTERN_ANGLE_*
	mBurstRate = 0.1f;								// How often to do a burst of particles
	mBurstPartCount = 1;							// How many particles in a burst
	mBurstSpeedMin = 1.f;						// Minimum particle velocity
	mBurstSpeedMax = 1.f;						// Maximum particle velocity
	mBurstRadius = 0.f;

	mNumParticles = 0;
}


BOOL LLPartSysData::pack(LLDataPacker &dp)
{
	dp.packU32(mCRC, "pscrc");
	dp.packU32(mFlags, "psflags");
	dp.packU8(mPattern, "pspattern");
	dp.packFixed(mMaxAge, "psmaxage", FALSE, 8, 8);
	dp.packFixed(mStartAge, "psstartage", FALSE, 8, 8);
	dp.packFixed(mInnerAngle, "psinnerangle", FALSE, 3, 5);
	dp.packFixed(mOuterAngle, "psouterangle", FALSE, 3, 5);
	dp.packFixed(mBurstRate, "psburstrate", FALSE, 8, 8);
	dp.packFixed(mBurstRadius, "psburstradius", FALSE, 8, 8);
	dp.packFixed(mBurstSpeedMin, "psburstspeedmin", FALSE, 8, 8);
	dp.packFixed(mBurstSpeedMax, "psburstspeedmax", FALSE, 8, 8);
	dp.packU8(mBurstPartCount, "psburstpartcount");

	dp.packFixed(mAngularVelocity.mV[0], "psangvelx", TRUE, 8, 7);
	dp.packFixed(mAngularVelocity.mV[1], "psangvely", TRUE, 8, 7);
	dp.packFixed(mAngularVelocity.mV[2], "psangvelz", TRUE, 8, 7);

	dp.packFixed(mPartAccel.mV[0], "psaccelx", TRUE, 8, 7);
	dp.packFixed(mPartAccel.mV[1], "psaccely", TRUE, 8, 7);
	dp.packFixed(mPartAccel.mV[2], "psaccelz", TRUE, 8, 7);

	dp.packUUID(mPartImageID, "psuuid");
	dp.packUUID(mTargetUUID, "pstargetuuid");
	mPartData.pack(dp);
	return TRUE;
}


BOOL LLPartSysData::unpack(LLDataPacker &dp)
{
	dp.unpackU32(mCRC, "pscrc");
	dp.unpackU32(mFlags, "psflags");
	dp.unpackU8(mPattern, "pspattern");
	dp.unpackFixed(mMaxAge, "psmaxage", FALSE, 8, 8);
	dp.unpackFixed(mStartAge, "psstartage", FALSE, 8, 8);
	dp.unpackFixed(mInnerAngle, "psinnerangle", FALSE, 3, 5);
	dp.unpackFixed(mOuterAngle, "psouterangle", FALSE, 3, 5);
	dp.unpackFixed(mBurstRate, "psburstrate", FALSE, 8, 8);
	mBurstRate = llmax(0.01f, mBurstRate);
	dp.unpackFixed(mBurstRadius, "psburstradius", FALSE, 8, 8);
	dp.unpackFixed(mBurstSpeedMin, "psburstspeedmin", FALSE, 8, 8);
	dp.unpackFixed(mBurstSpeedMax, "psburstspeedmax", FALSE, 8, 8);
	dp.unpackU8(mBurstPartCount, "psburstpartcount");

	dp.unpackFixed(mAngularVelocity.mV[0], "psangvelx", TRUE, 8, 7);
	dp.unpackFixed(mAngularVelocity.mV[1], "psangvely", TRUE, 8, 7);
	dp.unpackFixed(mAngularVelocity.mV[2], "psangvelz", TRUE, 8, 7);

	dp.unpackFixed(mPartAccel.mV[0], "psaccelx", TRUE, 8, 7);
	dp.unpackFixed(mPartAccel.mV[1], "psaccely", TRUE, 8, 7);
	dp.unpackFixed(mPartAccel.mV[2], "psaccelz", TRUE, 8, 7);

	dp.unpackUUID(mPartImageID, "psuuid");
	dp.unpackUUID(mTargetUUID, "pstargetuuid");
	mPartData.unpack(dp);
	return TRUE;
}

std::ostream& operator<<(std::ostream& s, const LLPartSysData &data)
{
	s << "Flags: " << std::hex << data.mFlags;
	s << " Pattern: " << std::hex << (U32) data.mPattern << "\n";
	s << "Age: [" << data.mStartAge << ", " << data.mMaxAge << "]\n";
	s << "Angle: [" << data.mInnerAngle << ", " << data.mOuterAngle << "]\n";
	s << "Burst Rate: " << data.mBurstRate << "\n";
	s << "Burst Radius: " << data.mBurstRadius << "\n";
	s << "Burst Speed: [" << data.mBurstSpeedMin << ", " << data.mBurstSpeedMax << "]\n";
	s << "Burst Part Count: " << std::hex << (U32) data.mBurstPartCount << "\n";
	s << "Angular Velocity: " << data.mAngularVelocity << "\n";
	s << "Accel: " << data.mPartAccel;
	return s;
}

BOOL LLPartSysData::isNullPS(const S32 block_num)
{
	U8 ps_data_block[PS_DATA_BLOCK_SIZE];
	U32 crc;

	S32 size;
	// Check size of block
	size = gMessageSystem->getSize("ObjectData", block_num, "PSBlock");
	
	if (!size)
	{
		return TRUE;
	}
	else if (size != PS_DATA_BLOCK_SIZE)
	{
		llwarns << "PSBlock is wrong size for particle system data - got " << size << ", expecting " << PS_DATA_BLOCK_SIZE << llendl;
		return TRUE;
	}
	gMessageSystem->getBinaryData("ObjectData", "PSBlock", ps_data_block, PS_DATA_BLOCK_SIZE, block_num, PS_DATA_BLOCK_SIZE);

	LLDataPackerBinaryBuffer dp(ps_data_block, PS_DATA_BLOCK_SIZE);
	dp.unpackU32(crc, "crc");

	if (crc == 0)
	{
		return TRUE;
	}
	return FALSE;
}


//static
BOOL LLPartSysData::packNull()
{
	U8 ps_data_block[PS_DATA_BLOCK_SIZE];
	gMessageSystem->addBinaryData("PSBlock", ps_data_block, 0);
	return TRUE;
}


BOOL LLPartSysData::packBlock()
{
	U8 ps_data_block[PS_DATA_BLOCK_SIZE];

	LLDataPackerBinaryBuffer dp(ps_data_block, PS_DATA_BLOCK_SIZE);
	pack(dp);

	// Add to message
	gMessageSystem->addBinaryData("PSBlock", ps_data_block, PS_DATA_BLOCK_SIZE);

	return TRUE;
}                                         


BOOL LLPartSysData::unpackBlock(const S32 block_num)
{
	U8 ps_data_block[PS_DATA_BLOCK_SIZE];

	// Check size of block
	S32 size = gMessageSystem->getSize("ObjectData", block_num, "PSBlock");

	if (size != PS_DATA_BLOCK_SIZE)
	{
		llwarns << "PSBlock is wrong size for particle system data - got " << size << ", expecting " << PS_DATA_BLOCK_SIZE << llendl;
		return FALSE;
	}

	// Get from message
	gMessageSystem->getBinaryData("ObjectData", "PSBlock", ps_data_block, PS_DATA_BLOCK_SIZE, block_num, PS_DATA_BLOCK_SIZE);

	LLDataPackerBinaryBuffer dp(ps_data_block, PS_DATA_BLOCK_SIZE);
	unpack(dp);

	return TRUE;
}

void LLPartSysData::clampSourceParticleRate()
{
	F32 particle_rate = 0;
	particle_rate = mBurstPartCount/mBurstRate;
	if (particle_rate > 256.f)
	{
		mBurstPartCount = llfloor(((F32)mBurstPartCount)*(256.f/particle_rate));
	}
}

void LLPartSysData::setPartAccel(const LLVector3 &accel)
{
	mPartAccel.mV[VX] = llclamp(accel.mV[VX], -100.f, 100.f);
	mPartAccel.mV[VY] = llclamp(accel.mV[VY], -100.f, 100.f);
	mPartAccel.mV[VZ] = llclamp(accel.mV[VZ], -100.f, 100.f);
}

LLSD LLPartSysData::asLLSD() const
{
	LLSD sd = LLSD();

	sd["LL_PART_INTERP_COLOR_MASK"] = (LLPartData::LL_PART_INTERP_COLOR_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_INTERP_SCALE_MASK"] = (LLPartData::LL_PART_INTERP_SCALE_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_BOUNCE_MASK"] = (LLPartData::LL_PART_BOUNCE_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_WIND_MASK"] = (LLPartData::LL_PART_WIND_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_FOLLOW_SRC_MASK"] = (LLPartData::LL_PART_FOLLOW_SRC_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_FOLLOW_VELOCITY_MASK"] = (LLPartData::LL_PART_FOLLOW_VELOCITY_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_TARGET_POS_MASK"] = (LLPartData::LL_PART_TARGET_POS_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_TARGET_LINEAR_MASK"] = (LLPartData::LL_PART_TARGET_LINEAR_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_EMISSIVE_MASK"] = (LLPartData::LL_PART_EMISSIVE_MASK & mPartData.mFlags) ? 1 : 0;
	sd["LL_PART_BEAM_MASK"] = (LLPartData::LL_PART_BEAM_MASK & mPartData.mFlags) ? 1 : 0;

	sd["ParticleMaxAge"] = mPartData.mMaxAge;
	sd["StartColor"] = ll_sd_from_color4(mPartData.mStartColor);
	sd["EndColor"] = ll_sd_from_color4(mPartData.mEndColor);
	sd["StartScale"] = ll_sd_from_vector2(mPartData.mStartScale);
	sd["EndScale"] = ll_sd_from_vector2(mPartData.mEndScale);

	sd["SourceMaxAge"] = mMaxAge;
	sd["SourceStartAge"] = mStartAge;

	sd["LL_PART_SRC_PATTERN_DROP"] = ( mPattern & LL_PART_SRC_PATTERN_DROP) ? 1 : 0;
	sd["LL_PART_SRC_PATTERN_EXPLODE"] = ( mPattern & LL_PART_SRC_PATTERN_EXPLODE) ? 1 : 0;
	sd["LL_PART_SRC_PATTERN_ANGLE"] = ( mPattern & LL_PART_SRC_PATTERN_ANGLE) ? 1 : 0;
	sd["LL_PART_SRC_PATTERN_ANGLE_CONE"] = ( mPattern & LL_PART_SRC_PATTERN_ANGLE_CONE) ? 1 : 0 ;
	sd["LL_PART_SRC_PATTERN_ANGLE_CONE_EMPTY"] = ( mPattern & LL_PART_SRC_PATTERN_ANGLE_CONE_EMPTY) ? 1 : 0;

	sd["InnerAngle"] = mInnerAngle;
	sd["OuterAngle"] = mOuterAngle;
	sd["AngularVelocity"] = ll_sd_from_vector3(mAngularVelocity);
	sd["BurstRate"] = mBurstRate;
	sd["BurstPartCount"] = mBurstPartCount;
	sd["BurstSpeedMin"] = mBurstSpeedMin;
	sd["BurstSpeedMax"] = mBurstSpeedMax;
	sd["BurstRadius"] = mBurstRadius;
	sd["PartImageID"] = mPartImageID.asString();
	sd["TargetId"] = mTargetUUID.asString();
	return sd;
}

bool LLPartSysData::fromLLSD(LLSD& sd)
{
	mPartData.mFlags = 0;
	if (sd["LL_PART_INTERP_COLOR_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_INTERP_COLOR_MASK;
	if (sd["LL_PART_INTERP_SCALE_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_INTERP_SCALE_MASK;
	if (sd["LL_PART_BOUNCE_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_BOUNCE_MASK;
	if (sd["LL_PART_WIND_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_WIND_MASK;
	if (sd["LL_PART_FOLLOW_SRC_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_FOLLOW_SRC_MASK;
	if (sd["LL_PART_FOLLOW_VELOCITY_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_FOLLOW_VELOCITY_MASK;
	if (sd["LL_PART_TARGET_POS_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_TARGET_POS_MASK;
	if (sd["LL_PART_TARGET_LINEAR_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_TARGET_LINEAR_MASK;
	if (sd["LL_PART_EMISSIVE_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_EMISSIVE_MASK;
	if (sd["LL_PART_BEAM_MASK"]) mPartData.mFlags |= LLPartData::LL_PART_BEAM_MASK;

	mPartData.mMaxAge = (F32)sd["ParticleMaxAge"].asReal();
	mPartData.mStartColor = ll_color4_from_sd(sd["StartColor"]);
	mPartData.mEndColor = ll_color4_from_sd(sd["EndColor"]);
	mPartData.mStartScale = ll_vector2_from_sd(sd["StartScale"]);
	mPartData.mEndScale = ll_vector2_from_sd(sd["EndScale"]);

	mMaxAge   = (F32)sd["SourceMaxAge"].asReal();
	mStartAge  = (F32)sd["SourceStartAge"].asReal();

	mPattern = 0;
	if (sd["LL_PART_SRC_PATTERN_DROP"]) mPattern |= LL_PART_SRC_PATTERN_DROP;
	if (sd["LL_PART_SRC_PATTERN_EXPLODE"]) mPattern |= LL_PART_SRC_PATTERN_EXPLODE;
	if (sd["LL_PART_SRC_PATTERN_ANGLE"]) mPattern |= LL_PART_SRC_PATTERN_ANGLE;
	if (sd["LL_PART_SRC_PATTERN_ANGLE_CONE"]) mPattern |= LL_PART_SRC_PATTERN_ANGLE_CONE;
	if (sd["LL_PART_SRC_PATTERN_ANGLE_CONE_EMPTY"]) mPattern |= LL_PART_SRC_PATTERN_ANGLE_CONE_EMPTY;

	mInnerAngle = (F32)sd["InnerAngle"].asReal();
	mOuterAngle = (F32)sd["OuterAngle"].asReal();
	mAngularVelocity = ll_vector3_from_sd(sd["AngularVelocity"]);
	mBurstRate = (F32)sd["BurstRate"].asReal();
	int burst_part_count = sd["BurstPartCount"];
	mBurstPartCount = (U8)burst_part_count;
	mBurstSpeedMin = (F32)sd["BurstSpeedMin"].asReal();
	mBurstSpeedMax = (F32)sd["BurstSpeedMax"].asReal();
	mBurstRadius = (F32)sd["BurstRadius"].asReal();

	mPartImageID = LLUUID(sd[("PartImageID")].asString());
	mTargetUUID = LLUUID(sd["TargetId"].asString());

	return true;
}