#ifndef _MIKU_APPMESH_H_
#define _MIKU_APPMESH_H_

#ifndef _MIKU_APPNODE_H_
#include "ts/miku/mikuAppNode.h"
#endif

#ifndef _APPMESH_H_
#include "ts/loader/appMesh.h"
#endif

class MikuAppMesh : public AppMesh
{
public:
	MikuModel *mModel;
	MikuAppNode *mAppNode;

   MikuShapeLoader *mLoader;

public:
   MikuAppMesh(MikuShapeLoader *loader, MikuModel *m, MikuAppNode *node, bool isEdge);

   virtual ~MikuAppMesh();

   virtual const char * getName(bool allowFixed=true);

   virtual MatrixF getMeshTransform(F32 time);

   virtual F32 getVisValue(F32 time);

   virtual bool getFloat(const char* propName, F32& defaultVal);

   virtual bool  getInt(const char* propName, S32& defaultVal);

   virtual bool  getBool(const char* propName, bool& defaultVal);

   virtual bool animatesVis(const AppSequence* appSeq);
   virtual bool animatesMatFrame(const AppSequence* appSeq);
   virtual bool animatesFrame(const AppSequence* appSeq);

   virtual bool isBillboard();

   virtual bool isBillboardZAxis();

   virtual bool isSkin();

   virtual void lookupSkinData();

   virtual void lockMesh(F32 t, const MatrixF& objectOffset);

   static S32 QSORT_CALLBACK boneCmpFunction(const void * a, const void * b);
};

#endif
