#ifndef _MIKU_APPMATERIAL_H_
#define _MIKU_APPMATERIAL_H_

#ifndef _MIKU_MODEL_H_
#include "ts/miku/mikuModel.h"
#endif

class Material;

class MikuAppMaterial : public AppMaterial
{
public:
   const MikuModel*                   mModel;

   U32 mIndex;
   String mName;

   MikuAppMaterial(MikuModel *m, U32 index);
   virtual ~MikuAppMaterial();

   String getName() const;

   Material *createMaterial(const Torque::Path& path) const;
};

#endif