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

// source: http://d.hatena.ne.jp/snaka72/20090429/1241027781
 
BOOL convSJIStoUTF8( BYTE* pSource, BYTE* pDist, int* pSize ) 
{
	*pSize = 0;
 
	// Convert SJIS -> UTF-16
	const int nSize = ::MultiByteToWideChar( 932, 0, (LPCSTR)pSource, -1, NULL, 0 );
 
	UTF16* buffUtf16 = new UTF16[ nSize + 2 ];
	::MultiByteToWideChar( 932, 0, (LPCSTR)pSource, -1, (LPWSTR)buffUtf16, nSize );
 
	// Convert UTF-16 -> UTF-8
	const int nSizeUtf8 = ::WideCharToMultiByte( CP_UTF8, 0, (LPCWSTR)buffUtf16, -1, NULL, 0, NULL, NULL );
	if ( !pDist ) {
		*pSize = nSizeUtf8;
		delete buffUtf16;
		return TRUE;
	}
 
	BYTE* buffUtf8 = new BYTE[ nSizeUtf8 * 2 ];
	ZeroMemory( buffUtf8, nSizeUtf8 * 2 );
	::WideCharToMultiByte( CP_UTF8, 0, (LPCWSTR)buffUtf16, -1, (LPSTR)buffUtf8, nSizeUtf8, NULL, NULL );
 
	*pSize = lstrlen( (LPCWSTR)buffUtf8 );
	memcpy( pDist, buffUtf8, nSizeUtf8 );
 
	delete buffUtf16;
	delete buffUtf8;
 
	return TRUE;
}
 
/*
 * convert: sjis -> utf8
 */
BOOL sjis2utf8(BYTE* source, BYTE** dest) {
	// Calculate result size
	int size = 0;
	convSJIStoUTF8( source, NULL, &size );
 
	// Peform convert
	*dest = new BYTE[ size + 1 ];
	ZeroMemory( *dest, size + 1 );
	convSJIStoUTF8( source, *dest, &size );
 
	return TRUE;
}


MikuModel::MikuModel()
{
}

MikuModel::~MikuModel()
{
}

StringTableEntry convertSJIS(const char *data)
{
	U8 *dest = NULL;
	sjis2utf8((BYTE*)data, &dest);
	if (dest)
	{
		StringTableEntry ret = StringTable->insert((const char*)dest);
		delete[] dest;
		return ret;
	}
	else
	{
		return StringTable->insert("");
	}
}

bool MikuModel::read(Stream &s)
{
	const char *headerStr = "Pmd";
	char buffer[4096];

	// Header=
	s.read(3, buffer);

	if (dMemcmp(buffer, headerStr, 3) != 0)
		return false;

	F32 version;
	s.read(&version);

	if (version != 1.0)
		return false;

	s.read(20, buffer);
	buffer[20] = '\0';
	mName = convertSJIS(buffer);

	s.read(256, buffer);
	buffer[256] = '\0';
	mComment = convertSJIS(buffer);

	// Verts
	U32 numVerts;
	s.read(&numVerts);
	mVerts.setSize(numVerts);

	for (U32 i=0; i<numVerts; i++)
	{
		Vert &v = mVerts[i];

		mathRead(s, &v.pos);
		mathRead(s, &v.normal);
		mathRead(s, &v.texcoord);

		s.read(&v.boneIndex[0]);
		s.read(&v.boneIndex[1]);
		s.read(&v.boneWeight);
		s.read(&v.edge);
	}

	// Faces
	U32 numIndices;
	s.read(&numIndices);
	mIndices.setSize(numIndices);

	s.read(numIndices*2, mIndices.address()); // assumes little endian

	#ifdef TORQUE_BIG_ENDIAN
	for (U32 i=0; i<numIndices; i++)
	{
		mIndices[i] = endianSwap(mIndices[i]);
	}
	#endif
	
	// Materials
	U32 numMaterials;
	s.read(&numMaterials);
	mMaterials.setSize(numMaterials);
	mMaterialNames.setSize(numMaterials);
	mMaterialTextures.setSize(numMaterials);

	for (U32 i=0; i<numMaterials; i++)
	{
		Material &v = mMaterials[i];

		mathRead(s, &v.diffuse);
		s.read(&v.specular);
		mathRead(s, &v.specularColor);
		mathRead(s, &v.ambientColor);
		s.read(&v.toon);
		s.read(&v.edge);
		s.read(&v.numIndices);

		s.read(20, buffer);
		buffer[20] = '\0';
		mMaterialTextures[i] = StringTable->insert(buffer);
	}
	
	// Bones
	U16 numBones;
	s.read(&numBones);
	mBones.setSize(numBones);
	mNormalBoneNames.setSize(numBones);
	mEnglishBoneNames.setSize(numBones);

	for (U32 i=0; i<numBones; i++)
	{
		Bone &v = mBones[i];

		s.read(20, buffer);
		buffer[20] = '\0';

		mNormalBoneNames[i] = convertSJIS(buffer);

		s.read(&v.parentId);
		s.read(&v.childId);
		s.read(&v.type);
		s.read(&v.targetId);
		mathRead(s, &v.defaultPosition);
	}

	// IKs
	U16 numIks;
	s.read(&numIks);
	mIks.setSize(numIks);

	for (U32 i=0; i<numIks; i++)
	{
		Ik &v = mIks[i];

		s.read(&v.destId);
		s.read(&v.targetId);

		s.read(&v.numLinks);
		s.read(&v.maxIterations);
		s.read(&v.maxAngle);
		
		v.linkListIdx = mIkChains.size();
		for (U32 j=0; j<v.numLinks; j++)
		{
			U16 chainIdx;
			s.read(&chainIdx);
			mIkChains.push_back(chainIdx);
		}
	}

	// Morphs
	U16 numMorphs;
	s.read(&numMorphs);

	mMorphs.setSize(numMorphs);
	mNormalMorphNames.setSize(numMorphs);
	mEnglishMorphNames.setSize(numMorphs);

	for (U32 i=0; i<numMorphs; i++)
	{
		Morph &v = mMorphs[i];

		s.read(20, buffer);
		buffer[20] = '\0';
		mNormalMorphNames[i] = convertSJIS(buffer);

		s.read(&v.numVerts);
		s.read(&v.type);

		v.morphFaceIdx = mMorphIndexes.size();

		for (U32 j=0; j<v.numVerts; j++)
		{
			U32 vertIdx = 0;
			Point3F pos;
			s.read(&vertIdx);
			mathRead(s, &pos);

			mMorphIndexes.push_back(vertIdx);
			mMorphVerts.push_back(pos);
		}
	}

	U8 numFaceMorphs;
	s.read(&numFaceMorphs);
	mMorphsToDisplay.setSize(numFaceMorphs);

	for (U32 i=0; i<numFaceMorphs; i++)
	{
		s.read(&mMorphsToDisplay[i]);
	}

	// Bone groups
	U8 numBoneGroups;
	s.read(&numBoneGroups);
	mBoneGroupNames.setSize(numBoneGroups);
	mEnglishBoneGroupNames.setSize(numBoneGroups);

	for (U32 i=0; i<numBoneGroups; i++)
	{
		s.read(50, buffer);
		buffer[50] = '\0';
		mBoneGroupNames[i] = convertSJIS(buffer);
	}

	U32 numDisplayedBones;
	s.read(&numDisplayedBones);
	mBoneGroupList.setSize(numDisplayedBones);
	mBoneGroupGroup.setSize(numDisplayedBones);

	for (U32 i=0; i<numDisplayedBones; i++)
	{
		s.read(&mBoneGroupList[i]);
		s.read(&mBoneGroupGroup[i]);
	}

	U8 hasEnglish;
	s.read(&hasEnglish);

	if (hasEnglish == 1)
	{
		// English model info
		s.read(20, buffer);
		buffer[20] = '\0';
		mEnglishName = buffer;

		s.read(256, buffer);
		buffer[256] = '\0';
		mEnglishComment = buffer;

		for (U32 i=0; i<numBones; i++)
		{
			s.read(20, buffer);
			buffer[20] = '\0';
			mEnglishBoneNames[i] = StringTable->insert(buffer);
		}

		mEnglishMorphNames[0] = NULL;
		for (U32 i=0; i<numMorphs-1; i++)
		{
			s.read(20, buffer);
			buffer[20] = '\0';
			mEnglishMorphNames[i+1] = StringTable->insert(buffer);
		}
	
		for (U32 i=0; i<numBoneGroups; i++)
		{
			s.read(50, buffer);
			buffer[50] = '\0';
			mEnglishBoneGroupNames[i] = StringTable->insert(buffer);
		}
	}

	// Toon textures
	mToonTextures.setSize(10);

	for (U32 i=0; i<10; i++)
	{
		s.read(100, buffer);
		buffer[100] = '\0';
		mToonTextures[i] = StringTable->insert(buffer);
	}

	U32 numRigidBodies;
	s.read(&numRigidBodies);
	mRigidBodys.setSize(numRigidBodies);
	mRigidBodyNames.setSize(numRigidBodies);

	for (U32 i=0; i<numRigidBodies; i++)
	{
		RigidBody &v = mRigidBodys[i];

		s.read(20, buffer);
		buffer[20] = '\0';
		mRigidBodyNames[i] = StringTable->insert(buffer);

		s.read(&v.boneId);
		s.read(&v.collisionGroup);
		s.read(&v.collisionMask);
		s.read(&v.shape);
		s.read(&v.width);
		s.read(&v.radius);
		s.read(&v.depth);
		mathRead(s, &v.pos);
		mathRead(s, &v.rot);
		s.read(&v.mass);
		s.read(&v.linearDampCoeff);
		s.read(&v.angularDampCoeff);
		s.read(&v.resititutionCoeff);
		s.read(&v.frictionCoeff);
		s.read(&v.bodyType);
	}

	U32 numPhysicsConstraints;
	s.read(&numPhysicsConstraints);
	mPhysicsConstraints.setSize(numPhysicsConstraints);
	mPhysicsConstraintNames.setSize(numPhysicsConstraints);

	for (U32 i=0; i<numPhysicsConstraints; i++)
	{
		PhysicsConstraint &v = mPhysicsConstraints[i];

		s.read(20, buffer);
		buffer[20] = '\0';
		mPhysicsConstraintNames[i] = StringTable->insert(buffer);

		s.read(&v.firstId);
		s.read(&v.secondId);
		mathRead(s, &v.pos);
		mathRead(s, &v.rot);
		mathRead(s, &v.lowerPos);
		mathRead(s, &v.upperPos);
		mathRead(s, &v.lowerRot);
		mathRead(s, &v.upperRot);
		mathRead(s, &v.stiffness);
		mathRead(s, &v.rotStiffness);
	}

	return true;
}

bool MikuModel::write(Stream &s)
{
	return false;
}

void  MikuModel::dumpMaterials(Torque::Path path)
{
   Torque::Path scriptPath(path);
   scriptPath.setFileName("materials");
   scriptPath.setExtension("cs");

   // First see what materials we need to update
   PersistenceManager persistMgr;
   for ( U32 iMat = 0; iMat < AppMesh::appMaterials.size(); iMat++ )
   {
      MikuAppMaterial *mat = dynamic_cast<MikuAppMaterial*>( AppMesh::appMaterials[iMat] );
      if ( mat )
      {
         ::Material *mappedMat;
         if ( Sim::findObject( MATMGR->getMapEntry( mat->getName() ), mappedMat ) )
         {
            // Only update existing materials if forced to
            if ( ColladaUtils::getOptions().forceUpdateMaterials )
               persistMgr.setDirty( mappedMat );
         }
         else
         {
            // Create a new material definition
            persistMgr.setDirty( mat->createMaterial( scriptPath ), scriptPath.getFullPath() );
         }
      }
   }

   if ( persistMgr.getDirtyList().empty() )
      return;

   persistMgr.saveDirty();
}

MikuModel* MikuModel::checkAndLoadMikuModel(const Torque::Path& path)
{
	FileStream fs;
	if (fs.open(path, Torque::FS::File::Read))
	{
		MikuModel *m = new MikuModel();
		if (!m->read(fs))
		{
			delete m;
		}
		else
		{
			return m;
		}
	}
	return NULL;
}

ConsoleFunction(testMiku, void, 1, 1, "")
{
	MikuModel m;

	FileStream fs;
	fs.open("Miku_Hatsune.pmd", Torque::FS::File::Read);

	if (m.read(fs))
	{
		Con::errorf("Loaded test model");
	}
	else
	{
		Con::errorf("Error loading test model");
	}
}

ConsoleFunction(testMikuAnim, void, 1, 1, "")
{
	MikuAnim a;

	FileStream fs;
	fs.open("kishimen.vmd", Torque::FS::File::Read);

	if (a.read(fs))
	{
      Con::errorf("Loaded anims. %i keyframes, %i bones", a.mBoneKeyframes.size(), a.mBoneList.size());
      for (U32 i=0 ;i<a.mBoneList.size(); i++)
      {
         Con::errorf("  Bone[%i] == %s", i, a.mBoneList[i].c_str());
      }

      // Try figuring out a bone position
      Point3F outPos;
      QuatF outRot;
      a.calcBoneAt(&a.mBoneInfos[0], 0, outPos, outRot);
      a.calcBoneAt(&a.mBoneInfos[0], 60, outPos, outRot);
	}
	else
	{
		Con::errorf("Error loading test model");
	}
}

bool MikuModel::hasEdgeVerts()
{
   bool edgeVerts = false;

   for (U32 i=0; i<mVerts.size(); i++)
   {
      if (mVerts[i].edge)
      {
         edgeVerts = true;
         break;
      }
   }

   return edgeVerts;
}
