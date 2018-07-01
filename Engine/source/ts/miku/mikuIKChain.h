#ifndef _MIKU_IKCHAIN_H_
#define _MIKU_IKCHAIN_H_

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

class MikuModel;
class MikuShapeLoader;

// Represents an IK chain. Children should be a list of bones affected by chain in the correct order.

// NOTE: bones are remapped to their correct elements in the shape loader, we just need to provide the correct AppNode which == a bone

class MikuIKChain : public AppNode
{
public:

   MikuIKChain(MikuShapeLoader *l, MikuModel *m, S32 ikID);
   ~MikuIKChain();

   MikuShapeLoader *mLoader;
   MikuModel *mModel;
   S32 mIKId;

   AppNode* mDestBone;
   AppNode* mTargetBone;
   
   S16 mEffectorCount;
   F32 mMaxAngle;

   virtual AppNode* getDestBone();
   virtual AppNode* getTargetBone();

   virtual S16 getChainLength();
   virtual S16 getEffectorCount();
   virtual F32 getMaxAngle();

   virtual bool isIkChain() { return true; }
   
   void buildMeshList(void) {;} 
   virtual void buildChildList();

   virtual MatrixF getNodeTransform(F32 time) { MatrixF m(1); return m; }

   virtual bool isEqual(AppNode* node) { MikuIKChain *c = dynamic_cast<MikuIKChain*>(node); return (c && c->mIKId == mIKId); } 

   virtual bool animatesTransform(const AppSequence* appSeq) { return false; }

   virtual const char* getName() { return "IkChain"; }

   virtual const char* getParentName() { return ""; }

   virtual bool getFloat(const char* propName, F32& defaultVal) { return false; }

   virtual bool getInt(const char* propName, S32& defaultVal) { return false; }

   virtual bool getBool(const char* propName, bool& defaultVal) { return false; }

   virtual bool isBillboard() { return false; }

   virtual bool isBillboardZAxis() { return false; }

   virtual bool isParentRoot() { return false; }

   virtual bool isDummy() { return false; }

   virtual bool isBounds() { return false; }

   virtual bool isSequence() { return false; }

   // True if this is part of an IK chain
   virtual bool isIKChain() { return true; }
};

#endif