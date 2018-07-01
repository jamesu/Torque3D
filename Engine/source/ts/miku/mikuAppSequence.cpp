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
#include "ts/miku/mikuAppSequence.h"


MikuAppSequence::MikuAppSequence(MikuAnim *a) :
   mAnim(a)
{
   fps = 30.0f;
}

MikuAppSequence::~MikuAppSequence()
{
}

F32 MikuAppSequence::getStart() const { return 0.0f; }
F32 MikuAppSequence::getEnd() const { return (F32)mAnim->getLength() / fps; }

U32 MikuAppSequence::getFlags() const { return TSShape::Blend; }
F32 MikuAppSequence::getPriority() const { return 5; }
F32 MikuAppSequence::getBlendRefTime() const { return -1.0f; }

