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
#include "ts/miku/mikuAppMaterial.h"
#include "ts/miku/mikuAnim.h"
#include "ts/miku/mikuAppSequence.h"
#include "ts/miku/mikuIKChain.h"


class MikuAppMorph : public AppMorph
{
public:

   MikuAppMorph(MikuModel *m, U32 index) : mModel(m), mAnim(0), mIndex(index) { ; }
   MikuAppMorph(MikuAnim *a, U32 index) : mModel(0), mAnim(a), mIndex(index) { ; }

   MikuModel *mModel;
   MikuAnim *mAnim;

   S32 mIndex;

   virtual const char* getName() const
   {
      if (mModel)
      {
         return mModel->mNormalMorphNames[mIndex];
      }
      else if (mAnim)
      {
         return mAnim->mMorphInfos[mIndex].name;
      }
      else
      {
         return "morph";
      }
   };

   virtual F32 getWeightAtPos(F32 time)
   {
      F32 outWeight = -1;
      if (mAnim)
         mAnim->calcMorphAt(&mAnim->mMorphInfos[mIndex], time * 30.0f, outWeight);
      return outWeight;
   }

   virtual bool isAnimated(F32 startTime, F32 endTime)
   {
      return mAnim && mAnim->hasMorphForTimePeriod(&mAnim->mMorphInfos[mIndex], startTime, endTime);
   }

   virtual U8 getType()
   {
      return mModel ? mModel->mMorphs[mIndex].type : 0;
   }
};

MikuShapeLoader::MikuShapeLoader(MikuModel* _root) : root(_root), animRoot(NULL)
{
   if (ColladaUtils::getOptions().upAxis == UPAXISTYPE_COUNT)
      ColladaUtils::getOptions().upAxis = UPAXISTYPE_Z_UP;

   ColladaUtils::getOptions().unit = 1;
}

MikuShapeLoader::MikuShapeLoader(MikuAnim* _root) : root(NULL), animRoot(_root)
{
   if (ColladaUtils::getOptions().upAxis == UPAXISTYPE_COUNT)
      ColladaUtils::getOptions().upAxis = UPAXISTYPE_Z_UP;

   ColladaUtils::getOptions().unit = 1;
}


MikuShapeLoader::~MikuShapeLoader()
{
}

void MikuShapeLoader::enumerateScene()
{
   if (animRoot)
   {
      // Import the nodes in one huge clump as we dont have any way of determining heirachy
      for (U32 i=0; i<animRoot->mBoneInfos.size(); i++)
      {
		   MikuAppNode *node = new MikuAppNode(this, animRoot, i);
		   node->mBoneId = i;
		   node->mAnim = animRoot;

		   if (!processNode(node))
		   {
			   delete node;
		   }
      }
      
      appSequences.push_back(new MikuAppSequence(animRoot));

      // Add morph infos
      for (U32 i=0; i<animRoot->mMorphInfos.size(); i++)
      {
         appMorphs.push_back(new MikuAppMorph(animRoot, i));
      }
   }
   else if (root)
   {
	   for (U32 i=0; i<root->mMaterials.size(); i++)
	   {
		   AppMesh::appMaterials.push_back(new MikuAppMaterial(root, i));
	   }

	   for (U32 i=0; i<root->mBones.size(); i++)
	   {
		   MikuModel::Bone *b = &root->mBones[i];
		   if (b->parentId >= 0)
			   continue;

		   MikuAppNode *node = new MikuAppNode(this, root, NULL, i);
		   node->mBoneId = i;
		   node->mModel = root;

         addedBoneList.push_back(node);

		   if (!processNode(node))
		   {
			   delete node;
		   }
	   }

      // Enumerate IK Chains
      for (U32 i=0; i<root->mIks.size(); i++)
      {
         MikuIKChain *chain = new MikuIKChain(this, root, i);
         ikChains.push_back(chain);
      }

      // Enumerate morph infos
      for (U32 i=0; i<root->mMorphs.size(); i++)
      {
         appMorphs.push_back(new MikuAppMorph(root, i));
      }

      MikuAppNode *node = NULL;

      // Add node for mesh
      node = new MikuAppNode(this, root, NULL, -1);
      node->mIsMeshNode = true;

	   if (!processNode(node))
	   {
         delete node;
	   }
   }
}

bool MikuShapeLoader::ignoreNode(const String& name)
{
	return false;
}

bool MikuShapeLoader::ignoreMesh(const String& name)
{
	return false;
}

void MikuShapeLoader::computeBounds(Box3F& bounds)
{
   TSShapeLoader::computeBounds(bounds);

   // Check if the model origin needs adjusting
   if ( bounds.isValidBox() &&
       (ColladaUtils::getOptions().adjustCenter ||
        ColladaUtils::getOptions().adjustFloor) )
   {
      // Compute shape offset
      Point3F shapeOffset = Point3F::Zero;
      if ( ColladaUtils::getOptions().adjustCenter )
      {
         bounds.getCenter( &shapeOffset );
         shapeOffset = -shapeOffset;
      }
      if ( ColladaUtils::getOptions().adjustFloor )
         shapeOffset.z = -bounds.minExtents.z;

      // Adjust bounds
      bounds.minExtents += shapeOffset;
      bounds.maxExtents += shapeOffset;

      // Now adjust all positions for root level nodes (nodes with no parent)
      for (S32 iNode = 0; iNode < shape->nodes.size(); iNode++)
      {
         if ( !appNodes[iNode]->isParentRoot() )
            continue;

         // Adjust default translation
         shape->defaultTranslations[iNode] += shapeOffset;

         // Adjust animated translations
         for (S32 iSeq = 0; iSeq < shape->sequences.size(); iSeq++)
         {
            const TSShape::Sequence& seq = shape->sequences[iSeq];
            if ( seq.translationMatters.test(iNode) )
            {
               for (S32 iFrame = 0; iFrame < seq.numKeyframes; iFrame++)
               {
                  S32 index = seq.baseTranslation + seq.translationMatters.count(iNode)*seq.numKeyframes + iFrame;
                  shape->nodeTranslations[index] += shapeOffset;
               }
            }
         }
      }
   }
}

