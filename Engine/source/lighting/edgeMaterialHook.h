#ifndef _EDGEMATERIALHOOK_H_
#define _EDGEMATERIALHOOK_H_

#ifndef _MATINSTANCEHOOK_H_
#include "materials/matInstanceHook.h"
#endif
#ifndef _MATINSTANCE_H_
#include "materials/matInstance.h"
#endif

class EdgeMatInstance : public MatInstance
{
   typedef MatInstance Parent;

   bool mLightmappedMaterial;
public:
   EdgeMatInstance( Material *mat );
   virtual ~EdgeMatInstance() {}

   virtual bool setupPass( SceneRenderState *state, const SceneData &sgData );
};

class EdgeMaterialHook : public MatInstanceHook
{
public:

   EdgeMaterialHook(BaseMatInstance *matInst);

   // MatInstanceHook
   virtual ~EdgeMaterialHook();
   virtual const MatInstanceHookType& getType() const { return Type; }

   /// The material hook type.
   static const MatInstanceHookType Type;

   virtual BaseMatInstance *getMatInstance() { return mEdgeMatInst; }

   BaseMatInstance *mEdgeMatInst;

protected:

   static void _overrideFeatures(   ProcessedMaterial *mat,
                                    U32 stageNum,
                                    MaterialFeatureData &fd, 
                                    const FeatureSet &features );
};

#endif // _EDGEMATERIALHOOK_H_
