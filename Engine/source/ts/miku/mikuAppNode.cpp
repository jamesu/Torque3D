#include "ts/miku/mikuModel.h"
#include "ts/miku/mikuShapeLoader.h"
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
#include "ts/loader/tsShapeLoader.h"
#include "ts/miku/mikuAppNode.h"
#include "ts/miku/mikuAppMesh.h"
#include "ts/miku/mikuAnim.h"
#include "ts/miku/mikuIKChain.h"


void MikuAppNode::buildMeshList()
{
   if (mIsMeshNode)
   {

	   // Only one mesh
	   mMeshes.push_back(new MikuAppMesh(mLoader, mModel, this, mIsEdgeNode));

   }

	// TODO: rigid body list
}

void MikuAppNode::buildChildList()
{
   if (mBoneId == -1)
      return;

   if (mModel == NULL)
      return;

	for (U32 i=0; i<mModel->mBones.size(); i++)
	{
		if (mModel->mBones[i].parentId == mBoneId)
      {
         MikuAppNode *node = MikuAppNode::genAppNodeForBone(mLoader, this, i);
         mChildNodes.push_back(node);

         mLoader->addedBoneList.push_back(node);
      }
	}
}

MikuAppNode* MikuAppNode::genAppNodeForBone(MikuShapeLoader *loader, MikuAppNode *parent, U32 index)
{
	MikuAppNode *node = new MikuAppNode(loader, loader->root, parent, index);
	return node;
}


MikuAppNode::MikuAppNode(MikuShapeLoader *loader, MikuModel *m, MikuAppNode *parent, S32 boneIdx) :
   mLoader(loader),
   mModel(m),
   mAnim(NULL),
   mParentNode(parent),
	mBoneId(boneIdx),
   mIsMeshNode(false),
   mIsEdgeNode(false)
{
   mRootBoneId = parent == NULL ? mBoneId : parent->mRootBoneId;
	mName = boneIdx == -1 ? dStrdup("mesh") : dStrdup(mModel->mNormalBoneNames[boneIdx]);
	mParentName = dStrdup(parent ? parent->mName : mModel->mEnglishName);
}

MikuAppNode::MikuAppNode(MikuShapeLoader *loader, MikuAnim *a, S32 boneIdx) :
   mLoader(loader),
   mModel(NULL),
   mAnim(a),
   mParentNode(NULL),
	mBoneId(boneIdx),
   mIsMeshNode(false),
   mIsEdgeNode(false)
{
   mRootBoneId = mBoneId;
   mName = boneIdx == -1 ? dStrdup("root") : dStrdup(mAnim->mBoneInfos[boneIdx].name);
	mParentName = dStrdup("");
}


MikuAppNode::~MikuAppNode()
{
}

MatrixF MikuAppNode::getNodeTransform(F32 time)
{
   // Anim mode
   if (mAnim)
   {
      // Get relative transform and dump
      MatrixF xfm(1);
      MatrixF child(1);

      // For time == 0, identity should suffice
      if (time == TSShapeLoader::DefaultTime)
      {
         return xfm;
      }

      Point3F outPos;
      QuatF outRot;
      mAnim->calcBoneAt(&mAnim->mBoneInfos[mBoneId], time * 30.0f, outPos, outRot);

      // Convert transform
      outPos = Point3F(outPos.x, outPos.z, -outPos.y);
      outRot = QuatF(outRot.x, outRot.z, outRot.y, outRot.w);

      xfm.scale(ColladaUtils::getOptions().unit);
      
      outRot.setMatrix(&child);
      child.setPosition(outPos);

      ColladaUtils::convertTransform(xfm);
      xfm.mul(child);

      return xfm;
   }

	if (mBoneId >= 0)
	{
		MikuModel::Bone *bone = &mModel->mBones[mBoneId];
		MikuModel::Bone *parentBone = bone->parentId >= 0 ? &mModel->mBones[bone->parentId] : NULL;
		MatrixF xfm(1);

		if (parentBone)
		{
			Point3F srcToDest = bone->defaultPosition - parentBone->defaultPosition;
         srcToDest = Point3F(srcToDest.x, srcToDest.z, srcToDest.y);
			xfm.setPosition(srcToDest);

			MatrixF parent = mParentNode->getNodeTransform(time);
			xfm.mul(parent);
		}
		else
		{
         MatrixF child(1);
         xfm.scale(ColladaUtils::getOptions().unit);
         ColladaUtils::convertTransform(xfm);

			child.setPosition(Point3F(bone->defaultPosition.x, bone->defaultPosition.z, bone->defaultPosition.y));
         xfm.mul(child);
		}

		return xfm;
	}
	else
	{
		// mesh?
      MatrixF xfm(1);
      MatrixF child(1);
      xfm.scale(ColladaUtils::getOptions().unit);
      ColladaUtils::convertTransform(xfm);

      return xfm;
	}
}

bool MikuAppNode::isEqual(AppNode* node)
{
	return mModel == ((MikuAppNode*)node)->mModel && mBoneId == ((MikuAppNode*)node)->mBoneId;
}

bool MikuAppNode::animatesTransform(const AppSequence* appSeq)
{
	return mAnim ? true : false;
}

const char* MikuAppNode::getName()
{
	return mName;
}

const char* MikuAppNode::getParentName()
{
	return mParentName;
}

bool MikuAppNode::getFloat(const char* propName, F32& defaultVal)
{
	return false;
}

bool MikuAppNode::getInt(const char* propName, S32& defaultVal)
{
	return false;
}

bool MikuAppNode::getBool(const char* propName, bool& defaultVal)
{
	return false;
}

bool MikuAppNode::isBillboard()   
{
	return false;
}

bool MikuAppNode::isBillboardZAxis() 
{
	return false;
}

bool MikuAppNode::isParentRoot() 
{
	return ( mParentNode == NULL );
}

bool MikuAppNode::isDummy()
{
	return false;
}

bool MikuAppNode::isBounds()
{
	return false;
}

bool MikuAppNode::isSequence()
{
	return false;
}

bool MikuAppNode::isIKChain()
{
	return false;
}
