#ifndef _MIKU_SEQUENCE_H_
#define _MIKU_SEQUENCE_H_

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

class MikuAnim;

class MikuAppSequence : public AppSequence
{
public:

   MikuAppSequence(MikuAnim *a);
   ~MikuAppSequence();

   MikuAnim *mAnim;
   
   virtual void setActive(bool active) { }

   virtual S32 getNumTriggers() const { return 0; }
   virtual void getTrigger(S32 index, TSShape::Trigger& trigger) const { trigger.state = 0;}

   virtual const char* getName() const { return "ambient"; }

   virtual F32 getStart() const;
   virtual F32 getEnd() const;

   virtual U32 getFlags() const;
   virtual F32 getPriority() const;
   virtual F32 getBlendRefTime() const;

};

#endif