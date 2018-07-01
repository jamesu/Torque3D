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
#include "ts/miku/mikuAppMesh.h"


MikuAppMesh::MikuAppMesh(MikuShapeLoader *loader, MikuModel *m, MikuAppNode *node, bool isEdge) : mLoader(loader), mModel(m), mAppNode(node)
{
}

MikuAppMesh::~MikuAppMesh()
{
}

const char * MikuAppMesh::getName(bool allowFixed)
{
   return mModel->mEnglishName;
}

MatrixF MikuAppMesh::getMeshTransform(F32 time)
{
   return MatrixF(1);//mAppNode->getNodeTransform(time);
}

F32 MikuAppMesh::getVisValue(F32 time)
{
   return 1.0f;
}

bool MikuAppMesh::getFloat(const char* propName, F32& defaultVal)
{
   return false;
}

bool MikuAppMesh::getInt(const char* propName, S32& defaultVal)
{
   return false;
}

bool  MikuAppMesh::getBool(const char* propName, bool& defaultVal)
{
   return false;
}

bool MikuAppMesh::animatesVis(const AppSequence* appSeq) { return false; }
bool MikuAppMesh::animatesMatFrame(const AppSequence* appSeq) { return false; }
bool MikuAppMesh::animatesFrame(const AppSequence* appSeq) { return false; }

bool MikuAppMesh::isBillboard()
{
   return false;
}

bool MikuAppMesh::isBillboardZAxis()
{
   return false;
}

bool MikuAppMesh::isSkin() 
{
   return true;
}

void MikuAppMesh::lookupSkinData()
{
   // Only lookup skin data once
   if (!isSkin() || weight.size())
      return;

   boneIndex.reserve(mModel->mVerts.size());
   vertexIndex.reserve(mModel->mVerts.size());
   weight.reserve(mModel->mVerts.size());

   for (U32 i=0; i<mModel->mVerts.size(); i++)
   {
      const MikuModel::Vert &v = mModel->mVerts[i];
      U16 boneIdx = v.boneIndex[0];

      if (v.boneWeight > 0)
      {
         boneIndex.push_back(boneIdx);
         vertexIndex.push_back(i);
         weight.push_back(v.boneWeight / 100.0f);
      }
      
      boneIdx = v.boneIndex[1];

      if ((100 - v.boneWeight) > 0)
      {
         boneIndex.push_back(boneIdx);
         vertexIndex.push_back(i);
         weight.push_back((100 - v.boneWeight) / 100.0f);
      }
   }

}

S32 QSORT_CALLBACK MikuAppMesh::boneCmpFunction(const void * a, const void * b)
{
   const MikuAppNode *n1 = *(const MikuAppNode**)a;
   const MikuAppNode *n2 = *(const MikuAppNode**)b;

   return n1->mBoneId - n2->mBoneId;
}

void MikuAppMesh::lockMesh(F32 t, const MatrixF& objectOffset)
{
   // Grab the data from MikuModel::mVerts etc
   points.reserve(mModel->mVerts.size());
   uvs.reserve(mModel->mVerts.size());
   normals.reserve(mModel->mVerts.size());
   edgeVerts.reserve(mModel->mVerts.size());

   for (U32 i=0; i<mModel->mVerts.size(); i++)
   {
      Point3F tmpVert = mModel->mVerts[i].pos;
      Point3F tmpNormal = mModel->mVerts[i].normal;

      tmpVert = Point3F(tmpVert.x, tmpVert.z, tmpVert.y);
      tmpNormal = Point3F(tmpNormal.x, tmpNormal.z, tmpNormal.y);

      objectOffset.mulP(tmpVert);

      points.push_back(tmpVert);
      uvs.push_back(mModel->mVerts[i].texcoord);
      normals.push_back(tmpNormal);
      edgeVerts.push_back(mModel->mVerts[i].edge);
   }

   U32 numFaces = mModel->mIndices.size() / 3;
   U32 primCount = 0;
   primitives.reserve(numFaces);
   indices.reserve(mModel->mIndices.size());

   U32 idxCount = 0;

   for (U32 j=0; j<mModel->mMaterials.size(); j++)
   {
      MikuModel::Material &mat = mModel->mMaterials[j];
      U32 nextIdxCount = idxCount + mat.numIndices;
      
      primitives.increment();

      TSDrawPrimitive& primitive = primitives.last();
      primitive.start = indices.size();
      primitive.matIndex = (TSDrawPrimitive::Triangles | TSDrawPrimitive::Indexed) | j;
      primitive.numElements = mat.numIndices;

      for (U32 i=idxCount; i<nextIdxCount; i++)
      {
         indices.push_back(mModel->mIndices[i]);
      }

      idxCount = nextIdxCount;
   }
   
   // Ensure master bone list is in the correct order
   dQsort( mLoader->addedBoneList.address(),  mLoader->addedBoneList.size(), sizeof(MikuAppNode*), &boneCmpFunction);

   for (U32 i=0; i<mLoader->addedBoneList.size(); i++)
   {
      MatrixF mat = mLoader->addedBoneList[i]->getNodeTransform(TSShapeLoader::DefaultTime);
      mat.inverse();
      initialTransforms.push_back(mat);
   }

   // Add reference bones in correct order
   bones.setSize(mLoader->addedBoneList.size());

   for (U32 i=0; i<mLoader->addedBoneList.size(); i++)
   {
      MikuAppNode *node = MikuAppNode::genAppNodeForBone(mLoader, mLoader->addedBoneList[0], mLoader->addedBoneList[i]->mBoneId);
      bones[i] = node;
   }

   // Add morph verts for initial frame
   if (t == TSShapeLoader::DefaultTime)
   {
      for (U32 i=1; i<mModel->mMorphs.size(); i++)
      {
         MikuModel::Morph &morph = mModel->mMorphs[i];
         baseMorphIndexes.push_back(morph.numVerts);

         Con::printf("Morph[%i]... sz %i offset %i", i, morph.numVerts, morphIndexes.size());

         // Copy indexes
         for (U32 j=morph.morphFaceIdx; j<morph.morphFaceIdx+morph.numVerts; j++)
         {
            Point3F tmpVert = mModel->mMorphVerts[j];
            //objectOffset.mulP(tmpVert);

            // Real index is in the base data
            U32 baseIdx = mModel->mMorphIndexes[j];
            morphIndexes.push_back(mModel->mMorphIndexes[baseIdx]); // Real index is in the base data
            morphVerts.push_back(Point3F(tmpVert.x, tmpVert.z, tmpVert.y));

            Con::printf("  vert[%i] == %f,%f,%f", mModel->mMorphIndexes[baseIdx], tmpVert.x, tmpVert.z, tmpVert.y);
         }
      }
   }
}
