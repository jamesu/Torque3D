#include "ts/miku/mikuShapeLoader.h"
#include "ts/miku/mikuAppMaterial.h"
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
#include "ts/miku/mikuShapeLoader.h"
#include "ts/miku/mikuModel.h"
#include "ts/miku/mikuAppMaterial.h"


extern String cleanString(const String& str);


MikuAppMaterial::MikuAppMaterial(MikuModel *m, U32 index) : mModel(m), mIndex(index)
{
	char buf[256];
	dSprintf(buf, 256, "%s_%i", m->mEnglishName, mIndex);
	mName = buf;
   flags = 0;
}

MikuAppMaterial::~MikuAppMaterial()
{
}

String MikuAppMaterial::getName() const
{
	return mName;
}

Material *MikuAppMaterial::createMaterial(const Torque::Path& path) const
{
	// The filename and material name are used as TorqueScript identifiers, so
	// clean them up first
	String cleanFile = cleanString(TSShapeLoader::getShapePath().getFileName());
	String cleanName = cleanString(getName());

	// Prefix the material name with the filename (if not done already by TSShapeConstructor prefix)
	if (!cleanName.startsWith(cleanFile))
		cleanName = cleanFile + "_" + cleanName;

	const MikuModel::Material *mat = &mModel->mMaterials[mIndex];

	// Determine the blend operation for this material
	Material::BlendOp blendOp = (flags & TSMaterialList::Translucent) ? Material::LerpAlpha : Material::None;
	if (flags & TSMaterialList::Additive)
		blendOp = Material::Add;
	else if (flags & TSMaterialList::Subtractive)
		blendOp = Material::Sub;

	// Create the Material definition
	const String oldScriptFile = Con::getVariable("$Con::File");
	Con::setVariable("$Con::File", path.getFullPath());   // modify current script path so texture lookups are correct
	Material *newMat = MATMGR->allocateAndRegister( cleanName, getName() );
	Con::setVariable("$Con::File", oldScriptFile);        // restore script path

   // If we have edges, render the damn edge!

   if (mModel->mMaterials[mIndex].edge)
   {
      newMat->mEdge = true;
   }

   {
	   newMat->mDiffuseMapFilename[0] = mModel->mMaterialTextures[mIndex];

	   newMat->mDiffuse[0] = ColorF(mat->diffuse.x, mat->diffuse.y, mat->diffuse.z);
	   newMat->mSpecularPower[0] = mat->specular;
	   newMat->mSpecular[0] = ColorF(mat->specularColor.x, mat->specularColor.y, mat->specularColor.z);

	   newMat->mDoubleSided = false;
	   newMat->mTranslucent = (bool)(flags & TSMaterialList::Translucent);
	   newMat->mTranslucentBlendOp = blendOp;
      
      newMat->mToonShade[0] = true;
      if (mat->toon > 0)
      {
         char buf[64];
         dSprintf(buf, sizeof(buf), "toon%02i", mat->toon);
         newMat->mToonShade[0] = true;
         newMat->mToonShadeTexture[0] = buf;
      }
   }

	return newMat;
}
