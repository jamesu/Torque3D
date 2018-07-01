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
#include "ts/miku/mikuAppNode.h"
#include "ts/miku/mikuIKChain.h"


MikuIKChain::MikuIKChain(MikuShapeLoader *l, MikuModel *m, S32 ikID) :
   mLoader(l),
   mModel(m),
   mIKId(ikID),
   mDestBone(NULL),
   mTargetBone(NULL),
   mEffectorCount(0),
   mMaxAngle(0)
{
   MikuModel::Ik &ik = mModel->mIks[mIKId];

   mEffectorCount = ik.maxIterations;
   mMaxAngle = ik.maxAngle;
}
   
void MikuIKChain::buildChildList()
{
   MikuModel::Ik &ik = mModel->mIks[mIKId];

   // Add nodes for chain
   for (U32 i=0; i<ik.numLinks; i++)
   {
      MikuAppNode *node = MikuAppNode::genAppNodeForBone(mLoader, NULL, mModel->mIkChains[ik.linkListIdx + i]);
      mChildNodes.push_back(node);
   }
   
   if (ik.destId >= 0)
   {
      mDestBone = MikuAppNode::genAppNodeForBone(mLoader, NULL, ik.destId);
   }

   if (ik.targetId >= 0)
   {
      mTargetBone = MikuAppNode::genAppNodeForBone(mLoader, NULL, ik.targetId);
   }
}

MikuIKChain::~MikuIKChain()
{
   if (mTargetBone)
      delete mTargetBone;
   if (mDestBone)
      delete mDestBone;
}

AppNode* MikuIKChain::getDestBone()
{
   if (mChildNodes.size() == 0)
      buildChildList();
   return mDestBone;
}

AppNode* MikuIKChain::getTargetBone()
{
   if (mChildNodes.size() == 0)
      buildChildList();
   return mTargetBone;
}

S16 MikuIKChain::getChainLength()
{
   if (mChildNodes.size() == 0)
      buildChildList();
   return mChildNodes.size();
}

S16 MikuIKChain::getEffectorCount()
{
   return mEffectorCount;
}

F32 MikuIKChain::getMaxAngle()
{
   return mMaxAngle;
}
