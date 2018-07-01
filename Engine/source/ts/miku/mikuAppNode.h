#ifndef _MIKU_APPNODE_H_
#define _MIKU_APPNODE_H_

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
#ifndef _TSSHAPE_LOADER_H_
#include "ts/loader/tsShapeLoader.h"
#endif

class MikuModel;
class MikuMesh;
class MikuShapeLoader;

class MikuAppNode : public AppNode
{
public:
   friend class TSShapeLoader;
   friend class MikuShapeLoader;

   // add attached meshes and child nodes to app node
   // the reason these are tracked by AppNode is that
   // AppNode is responsible for deleting all it's children
   // and attached meshes.
   virtual void buildMeshList();

   virtual void buildChildList();

   static MikuAppNode* genAppNodeForBone(MikuShapeLoader *loader, MikuAppNode *parent, U32 index);

   MikuAnim *mAnim;
	MikuModel *mModel;
	MikuAppNode *mParentNode;
	S32 mBoneId; // Bone id, if we point to a joint
   S32 mRootBoneId; // Root bone of this chain

   MikuShapeLoader *mLoader;

   bool mIsMeshNode;
   bool mIsEdgeNode;

public:

   MikuAppNode(MikuShapeLoader *loader, MikuModel *m, MikuAppNode *parent, S32 boneIdx);
   MikuAppNode(MikuShapeLoader *loader, MikuAnim *a, S32 boneIdx);

   virtual ~MikuAppNode();

   virtual MatrixF getNodeTransform(F32 time);

   virtual bool isEqual(AppNode* node);

   virtual bool animatesTransform(const AppSequence* appSeq);

   virtual const char* getName();

   virtual const char* getParentName();

   virtual bool getFloat(const char* propName, F32& defaultVal);

   virtual bool getInt(const char* propName, S32& defaultVal);

   virtual bool getBool(const char* propName, bool& defaultVal);

   virtual bool isBillboard();

   virtual bool isBillboardZAxis();

   virtual bool isParentRoot();

   virtual bool isDummy();

   virtual bool isBounds();

   virtual bool isSequence();

   // True if this is part of an IK chain
   virtual bool isIKChain();
};

#endif