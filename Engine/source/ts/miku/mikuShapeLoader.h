#ifndef _MIKU_SHAPE_LOADER_H_
#define _MIKU_SHAPE_LOADER_H_

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
class MikuAppMaterial;
class MikuAppNode;
class MikuAnim;

class MikuShapeLoader : public TSShapeLoader
{
public:
   MikuModel*             root;
   MikuAnim*              animRoot;

public:
   Vector<MikuAppNode*>   addedBoneList;

   MikuShapeLoader(MikuModel* _root);
   MikuShapeLoader(MikuAnim* _root);
   ~MikuShapeLoader();

   void enumerateScene();
   bool ignoreNode(const String& name);
   bool ignoreMesh(const String& name);
   void computeBounds(Box3F& bounds);
};

#endif