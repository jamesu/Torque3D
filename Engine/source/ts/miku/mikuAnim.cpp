#include "ts/miku/mikuShapeLoader.h"
#include "ts/miku/mikuModel.h"

#include "console/console.h"
#include "core/stream/fileStream.h" 
#include "core/stringTable.h"
#include "math/mathIO.h"
#include "ts/tsShape.h"
#include "ts/tsShapeInstance.h"
#include "materials/materialManager.h"
#include "console/persistenceManager.h"
#include "ts/collada/colladaShapeLoader.h"
#include "ts/collada/colladaUtils.h"
#include "ts/collada/colladaAppNode.h"
#include "ts/collada/colladaAppMesh.h"
#include "ts/collada/colladaAppMaterial.h"
#include "ts/collada/colladaAppSequence.h"
#include "ts/miku/mikuAppMaterial.h"
#include "ts/miku/mikuAnim.h"


#include <stdio.h>
#include <windows.h>

extern StringTableEntry convertSJIS(const char *data);

MikuAnim::MikuAnim()
{
}

MikuAnim::~MikuAnim()
{
}

S32 QSORT_CALLBACK MikuAnim::_keyframeSort( const BoneKeyframe* a, const BoneKeyframe* b )
{
   BoneKeyframe *ea = (BoneKeyframe *) (a);
   BoneKeyframe *eb = (BoneKeyframe *) (b);

   if (ea->boneIdx > eb->boneIdx)
   {
      return 1;
   }
   else if (ea->boneIdx < eb->boneIdx)
   {
      return -1;
   }
   else
   {
      return ea->frameIdx - eb->frameIdx;
   }
}

S32 QSORT_CALLBACK MikuAnim::_morphKeyframeSort( const MorphKeyframe* a, const MorphKeyframe* b )
{
   MorphKeyframe *ea = (MorphKeyframe *) (a);
   MorphKeyframe *eb = (MorphKeyframe *) (b);

   if (ea->morphIdx > eb->morphIdx)
   {
      return 1;
   }
   else if (ea->morphIdx < eb->morphIdx)
   {
      return -1;
   }
   else
   {
      return ea->frameIdx - eb->frameIdx;
   }
}

bool MikuAnim::read(Stream &s)
{
   char header[31];
   char name[21];

   header[30] = '\0';
   name[20] = '\0';

   s.read(30, &header);
   s.read(20, &name);

   if (dStricmp(header, "Vocaloid Motion Data 0002") != 0)
      return false;

   mModelName = convertSJIS(name);

   U32 numKeyframes;
   s.read(&numKeyframes);
   mBoneKeyframes.reserve(numKeyframes);
   
   char table[64];
   mLength = 0;

   for (U32 i=0; i<numKeyframes; i++)
   {
      s.read(15, name);
      name[15] = '\0';

      mBoneKeyframes.increment();
      BoneKeyframe &kf = mBoneKeyframes.last();

      String sName = convertSJIS(name);
      sName.intern();

      S32 idx = mBoneList.indexOf(sName);
      if (idx == -1)
      {
         mBoneList.push_back(sName);
         kf.boneIdx = mBoneList.size()-1;
      }
      else
      {
         kf.boneIdx = idx;
      }

      s.read(&kf.frameIdx);
      mathRead(s, &kf.translation);
      mathRead(s, &kf.rotation);

      s.read(64, &table);
      kf.readInterpolationTable(table);

      // Expand anim length
      mLength = kf.frameIdx > mLength ? kf.frameIdx : mLength;
   }

   // Sort keyframes by bone id and keyframe number
   mBoneKeyframes.sort(&MikuAnim::_keyframeSort);

   // Assign bone names
   mBoneInfos.setSize(mBoneList.size());
   for (U32 i=0; i<mBoneInfos.size(); i++)
   {
      mBoneInfos[i].name = mBoneList[i];
   }

   S32 lastBoneIdx = -1;
   BoneInfo *lastBone = NULL;

   // Determine start and end points for bones
   for (U32 i=0; i<numKeyframes; i++)
   {
      BoneKeyframe &kf = mBoneKeyframes[i];
      if (lastBoneIdx != kf.boneIdx)
      {
         lastBone = mBoneInfos.address() + kf.boneIdx;
         lastBoneIdx = kf.boneIdx;
         lastBone->startFrameIdx = i;
         lastBone->numFrames = 1;
      }
      else
      {
         lastBone->numFrames++;
      }
   }

   //
   // Now load morphs
   //
   s.read(&numKeyframes);
   mMorphKeyframes.reserve(numKeyframes);

   for (U32 i=0; i<numKeyframes; i++)
   {
      s.read(15, name);
      name[15] = '\0';

      mMorphKeyframes.increment();
      MorphKeyframe &kf = mMorphKeyframes.last();

      String sName = convertSJIS(name);
      sName.intern();

      S32 idx = mMorphList.indexOf(sName);
      if (idx == -1)
      {
         mMorphList.push_back(sName);
         kf.morphIdx = mMorphList.size()-1;
      }
      else
      {
         kf.morphIdx = idx;
      }

      s.read(&kf.frameIdx);
      s.read(&kf.weight);

      // Expand anim length
      mLength = kf.frameIdx > mLength ? kf.frameIdx : mLength;
   }

   // Sort keyframes by morph id and keyframe number
   mMorphKeyframes.sort(&MikuAnim::_morphKeyframeSort);

   // Assign bone names
   mMorphInfos.setSize(mMorphList.size());
   for (U32 i=0; i<mMorphInfos.size(); i++)
   {
      mMorphInfos[i].name = mMorphList[i];
   }

   S32 lastMorphIdx = -1;
   MorphInfo *lastMorph = NULL;

   // Determine start and end points for morphs
   for (U32 i=0; i<numKeyframes; i++)
   {
      MorphKeyframe &kf = mMorphKeyframes[i];
      if (lastMorphIdx != kf.morphIdx)
      {
         lastMorph = mMorphInfos.address() + kf.morphIdx;
         lastMorphIdx = kf.morphIdx;
         lastMorph->startFrameIdx = i;
         lastMorph->numFrames = 1;
      }
      else
      {
         lastMorph->numFrames++;
      }
   }

   return true;
}

bool MikuAnim::write(Stream &s)
{
   return false;
}

void MikuAnim::calcBoneAt(BoneInfo *bone, F32 frameTime, Point3F &outPos, QuatF &outRot)
{
   BoneInfo *bm = bone;
   float frame = frameTime;
   U32 k1, k2 = 0;
   U32 i;
   float time1;
   float time2;
   Point3F pos1, pos2;
   QuatF rot1, rot2;
   BoneKeyframe *keyFrameForInterpolation;
   float x, y, z, ww;
   float w;
   short idx;

   /* clamp frame to the defined last frame */
   if (frame > mBoneKeyframes[bm->startFrameIdx + bm->numFrames - 1].frameIdx)
      frame = mBoneKeyframes[bm->startFrameIdx + bm->numFrames - 1].frameIdx;

   /* find key frames between which the given frame exists */
   for (i = 0; i < bm->numFrames; i++) {
      if (frame <= mBoneKeyframes[bm->startFrameIdx + i].frameIdx) {
         k2 = i;
         break;
      }
   }

   /* bounding */
   if (k2 >= bm->numFrames)
      k2 = bm->numFrames - 1;
   if (k2 <= 1)
      k1 = 0;
   else
      k1 = k2 - 1;

   /* get the pos/rot at each key frame */
   time1 = mBoneKeyframes[bm->startFrameIdx + k1].frameIdx;
   time2 = mBoneKeyframes[bm->startFrameIdx + k2].frameIdx;
   keyFrameForInterpolation = mBoneKeyframes.address() + (bm->startFrameIdx + k2);
   pos1 = mBoneKeyframes[bm->startFrameIdx + k1].translation;
   rot1 = mBoneKeyframes[bm->startFrameIdx + k1].rotation;
   pos2 = mBoneKeyframes[bm->startFrameIdx + k2].translation;
   rot2 = mBoneKeyframes[bm->startFrameIdx + k2].rotation;

   /*
   if (m_overrideFirst && mc->looped) {
      // replace the first position/rotation at the first frame with end-of-motion ones
      if (k1 == 0 || time1 == 0.0f) {
         pos1 = bm->keyFrameList[bm->numKeyFrame - 1].pos;
         rot1 = bm->keyFrameList[bm->numKeyFrame - 1].rot;
      }
      if (k2 == 0 || time2 == 0.0f) {
         pos2 = bm->keyFrameList[bm->numKeyFrame - 1].pos;
         rot2 = bm->keyFrameList[bm->numKeyFrame - 1].rot;
      }
   }*/

   /* calculate the position and rotation */
   if (time1 != time2) {
      if (frame <= time1) {
         outPos = pos1;
         outRot = rot1;
      } else if (frame >= time2) {
         outPos = pos2;
         outRot = rot2;
      } else {
         /* lerp */
         w = (frame - time1) / (time2 - time1);
         idx = (short)(w * MIKU_INTERPOLATIONTABLE_SIZE);
         if (keyFrameForInterpolation->isLinear[0]) {
            x = pos1.x * (1.0f - w) + pos2.x * w;
         } else {
            ww = keyFrameForInterpolation->interpolationTable[0][idx] + (keyFrameForInterpolation->interpolationTable[0][idx + 1] - keyFrameForInterpolation->interpolationTable[0][idx]) * (w * MIKU_INTERPOLATIONTABLE_SIZE - idx);
            x = pos1.x * (1.0f - ww) + pos2.x * ww;
         }
         if (keyFrameForInterpolation->isLinear[1]) {
            y = pos1.y * (1.0f - w) + pos2.y * w;
         } else {
            ww = keyFrameForInterpolation->interpolationTable[1][idx] + (keyFrameForInterpolation->interpolationTable[1][idx + 1] - keyFrameForInterpolation->interpolationTable[1][idx]) * (w * MIKU_INTERPOLATIONTABLE_SIZE - idx);
            y = pos1.y * (1.0f - ww) + pos2.y * ww;
         }
         if (keyFrameForInterpolation->isLinear[2]) {
            z = pos1.z * (1.0f - w) + pos2.z * w;
         } else {
            ww = keyFrameForInterpolation->interpolationTable[2][idx] + (keyFrameForInterpolation->interpolationTable[2][idx + 1] - keyFrameForInterpolation->interpolationTable[2][idx]) * (w * MIKU_INTERPOLATIONTABLE_SIZE - idx);
            z = pos1.z * (1.0f - ww) + pos2.z * ww;
         }
         outPos = Point3F(x, y, z);
         if (keyFrameForInterpolation->isLinear[3]) {
            outRot = rot1.slerp(rot2, w);
         } else {
            ww = keyFrameForInterpolation->interpolationTable[3][idx] + (keyFrameForInterpolation->interpolationTable[3][idx + 1] - keyFrameForInterpolation->interpolationTable[3][idx]) * (w * MIKU_INTERPOLATIONTABLE_SIZE - idx);
            outRot = rot1.slerp(rot2, ww);
         }
      }
   } else {
      /* both keys have the same time, just apply one of them */
      outPos = pos1;
      outRot = rot1;
   }

   /*if (m_overrideFirst && m_noBoneSmearFrame > 0.0f) {
      // lerp with the initial position/rotation at the time of starting motion
      w = (float) (m_noBoneSmearFrame / MOTIONCONTROLLER_BONESTARTMARGINFRAME);
      mc->pos = mc->pos.lerp(mc->snapPos, btScalar(w));
      mc->rot = btTransform(btTransform(mc->rot).getBasis() * (1.0f - w) + btTransform(mc->snapRot).getBasis() * w).getRotation();
   }*/
}

void MikuAnim::calcMorphAt(MorphInfo *morph, F32 frameTime, F32 &outWeight)
{
   outWeight = 0.0f;

   float frame = frameTime;
   unsigned long k1, k2 = 0;
   U32 i;
   float time1, time2, weight1, weight2 = 0.0f;
   float w;

   /* clamp frame to the defined last frame */
   if (frame > mMorphKeyframes[morph->startFrameIdx + morph->numFrames - 1].frameIdx)
      frame = mMorphKeyframes[morph->startFrameIdx + morph->numFrames - 1].frameIdx;

   /* find key frames between which the given frame exists */
   for (i = 0; i < morph->numFrames; i++) {
      if (frame <= mMorphKeyframes[morph->startFrameIdx + i].frameIdx) {
         k2 = i;
         break;
      }
   }

   /* bounding */
   if (k2 >= morph->numFrames)
      k2 = morph->numFrames - 1;
   if (k2 <= 1)
      k1 = 0;
   else
      k1 = k2 - 1;

   /* get the pos/rot at each key frame */
   time1 = mMorphKeyframes[(S32)(morph->startFrameIdx + k1)].frameIdx;
   time2 = mMorphKeyframes[(S32)(morph->startFrameIdx + k2)].frameIdx;

   weight1 = mMorphKeyframes[(S32)(morph->startFrameIdx + k1)].weight;
   weight2 = mMorphKeyframes[(S32)(morph->startFrameIdx + k2)].weight;

   /* get value between [time0-time1][weight1-weight2] */
   if (time1 != time2) {
      w = (frame - time1) / (time2 - time1);
      outWeight = weight1 * (1.0f - w) + weight2 * w;
   } else {
      outWeight = weight1;
   }
}

bool MikuAnim::hasMorphForTimePeriod(MorphInfo *morph, F32 start, F32 end)
{
   U32 i;

   // Check starting point
   bool found = false;
   for (i = 0; i < morph->numFrames; i++)
   {
      if (mMorphKeyframes[morph->startFrameIdx + i].frameIdx >= start && mMorphKeyframes[morph->startFrameIdx + i].frameIdx <= end)
      {
         // Check for end
         for (i; i < morph->numFrames; i++)
         {
            if (mMorphKeyframes[morph->startFrameIdx + i].frameIdx >= end)
            {
               found = true;
               break;
            }
         }

         break;
      }
   }

   return found;
}

MikuAnim* MikuAnim::checkAndLoadMikuAnim(const Torque::Path &path)
{
	FileStream fs;
	if (fs.open(path, Torque::FS::File::Read))
	{
		MikuAnim *a = new MikuAnim();
		if (!a->read(fs))
		{
			delete a;
		}
		else
		{
			return a;
		}
	}
	return NULL;
}
