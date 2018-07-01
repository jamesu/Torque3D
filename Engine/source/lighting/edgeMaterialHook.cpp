#include "platform/platform.h"
#include "lighting/edgeMaterialHook.h"

#include "materials/materialManager.h"
#include "materials/customMaterialDefinition.h"
#include "materials/materialFeatureTypes.h"
#include "materials/materialFeatureData.h"
#include "shaderGen/featureType.h"
#include "shaderGen/featureMgr.h"
#include "scene/sceneRenderState.h"
#include "terrain/terrFeatureTypes.h"


const MatInstanceHookType EdgeMaterialHook::Type( "EdgeMat" );

EdgeMaterialHook::EdgeMaterialHook(BaseMatInstance *matInst)
   : mEdgeMatInst( NULL )
{
   mEdgeMatInst = (MatInstance*)matInst->getMaterial()->createMatInstance();
   mEdgeMatInst->getFeaturesDelegate().bind( &EdgeMaterialHook::_overrideFeatures );
   mEdgeMatInst->setUserObject(matInst->getUserObject());
   mEdgeMatInst->init(  matInst->getRequestedFeatures(), 
                        matInst->getVertexFormat() );
}

EdgeMaterialHook::~EdgeMaterialHook()
{
   SAFE_DELETE( mEdgeMatInst );
}

void EdgeMaterialHook::_overrideFeatures( ProcessedMaterial *mat,
                                                         U32 stageNum,
                                                         MaterialFeatureData &fd, 
                                                         const FeatureSet &features )
{
   // Basically strip everything except edge rendering
   fd.features.clear();

   if (stageNum == 0)
   {
      fd.features.addFeature( MFT_VertTransform );
      fd.features.addFeature( MFT_EdgeRender );
      fd.features.addFeature( MFT_HDROut );
   }
}
