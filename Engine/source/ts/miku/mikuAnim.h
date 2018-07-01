#ifndef _MIKU_ANIM_H_
#define _MIKU_ANIM_H_

#ifndef _MMATH_H_
#include "math/mMath.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _TSSHAPE_H_
#include "ts/tsShape.h"
#endif
#ifndef _APPNODE_H_
#include "ts/loader/appNode.h"
#endif
#ifndef _APPMESH_H_
#include "ts/loader/appMesh.h"
#endif
#ifndef _APPSEQUENCE_H_
#include "ts/loader/appSequence.h"
#endif

#define MIKU_INTERPOLATIONTABLE_SIZE 64

class MikuAnim
{
public:
	MikuAnim();
	~MikuAnim();

   String mModelName;

   typedef struct MorphInfo {
      String name;
      U32 startFrameIdx;
      U32 numFrames;
   };
   
   typedef struct MorphKeyframe {
      U32 morphIdx;
      U32 frameIdx;

      F32 weight;
   };

   typedef struct BoneInfo {
      String name;
      U32 startFrameIdx;
      U32 numFrames;
   };

	typedef struct BoneKeyframe {
		U32 boneIdx;
      U32 frameIdx;

      Point3F translation;
      QuatF rotation;

      bool isLinear[4];
      F32 *interpolationTable[4];

      BoneKeyframe()
      {
         dMemset(interpolationTable, '\0', sizeof(interpolationTable));
         dMemset(isLinear, '\0', sizeof(isLinear));
      }

      ~BoneKeyframe()
      {
         for (U32 i=0; i<4; i++)
         {
            if (interpolationTable[i])
            {
               dFree(interpolationTable[i]);
            }
         }
      }

      /* ipfunc: t->value for 4-point (3-dim.) bezier curve */
      inline F32 ipfunc(F32 t, F32 p1, F32 p2)
      {
         return ((1 + 3 * p1 - 3 * p2) * t * t * t + (3 * p2 - 6 * p1) * t * t + 3 * p1 * t);
      }

      /* ipfuncd: derivation of ipfunc */
      inline F32 ipfuncd(float t, float p1, float p2)
      {
         return ((3 + 9 * p1 - 9 * p2) * t * t + (6 * p2 - 12 * p1) * t + 3 * p1);
      }

      void readInterpolationTable(char *ip)
      {
         short i, d;
         float x1, x2, y1, y2;
         float inval, t, v, tt;

         /* check if they are just a linear function */
         for (i = 0; i < 4; i++)
            isLinear[i] = (ip[0 + i] == ip[4 + i] && ip[8 + i] == ip[12 + i]) ? true : false;

         /* make X (0.0 - 1.0) -> Y (0.0 - 1.0) mapping table */
         for (i = 0; i < 4; i++) {
            if (isLinear[i]) {
               /* table not needed */
               interpolationTable[i] = NULL;
               continue;
            }
            interpolationTable[i] = (float *) dMalloc(sizeof(F32) * (MIKU_INTERPOLATIONTABLE_SIZE + 1));
            x1 = ip[   i] / 127.0f;
            y1 = ip[ 4 + i] / 127.0f;
            x2 = ip[ 8 + i] / 127.0f;
            y2 = ip[12 + i] / 127.0f;
            for (d = 0; d < MIKU_INTERPOLATIONTABLE_SIZE; d++) {
               inval = (float) d / (float) MIKU_INTERPOLATIONTABLE_SIZE;
               /* get Y value for given inval */
               t = inval;
               while (1) {
                  v = ipfunc(t, x1, x2) - inval;
                  if (fabsf(v) < 0.0001f) break;
                  tt = ipfuncd(t, x1, x2);
                  if (tt == 0.0f) break;
                  t -= v / tt;
               }
               interpolationTable[i][d] = ipfunc(t, y1, y2);
            }
            interpolationTable[i][MIKU_INTERPOLATIONTABLE_SIZE] = 1.0f;
         }
      }

   } Bone;

   Vector<String> mBoneList;
   Vector<BoneKeyframe> mBoneKeyframes;
   Vector<BoneInfo> mBoneInfos;

   Vector<String> mMorphList;
   Vector<MorphInfo> mMorphInfos;
   Vector<MorphKeyframe> mMorphKeyframes;

   F32 mLength;

   F32 getLength() { return mLength; }

   bool hasMorph();

	bool read(Stream &s);
	bool write(Stream &s);

   void calcBoneAt(BoneInfo *bone, F32 frameTime, Point3F &outPos, QuatF &outRot);
   void calcMorphAt(MorphInfo *morph, F32 frameTime, F32 &outWeight);

   bool hasMorphForTimePeriod(MorphInfo *morph, F32 start, F32 end);

   static S32 QSORT_CALLBACK _morphKeyframeSort( const MorphKeyframe* a, const MorphKeyframe* b );
   static S32 QSORT_CALLBACK _keyframeSort( const BoneKeyframe* a, const BoneKeyframe* b );

   static MikuAnim* checkAndLoadMikuAnim(const Torque::Path &path);
};

#endif