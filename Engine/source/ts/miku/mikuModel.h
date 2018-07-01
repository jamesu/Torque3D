#ifndef _MIKU_MODEL_H_
#define _MIKU_MODEL_H_

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

class MikuModel
{
public:
	MikuModel();
	~MikuModel();

	bool read(Stream &s);
	bool write(Stream &s);
	
	TSShape* generateShape();
	void dumpMaterials(Torque::Path path);

   bool hasEdgeVerts();

	String mName;
	String mComment;

	String mEnglishName;
	String mEnglishComment;

	typedef struct Vert {
		Point3F pos;
		Point3F normal;
		Point2F texcoord;
		U16 boneIndex[2];
		U8 boneWeight;
		U8 edge;
	} Vert;

	Vector<Vert> mVerts;
	Vector<U16> mIndices;

	typedef struct Material {
		Point4F diffuse;
		F32     specular;
		Point3F specularColor;
		Point3F ambientColor;
		U8      toon;
		U8      edge;
		U32     numIndices;
		//char    name[20];
	} Material;

	Vector<Material> mMaterials;
	Vector<StringTableEntry> mMaterialNames;
	Vector<StringTableEntry> mMaterialTextures;

	enum BoneType {
		BONE_UNKOWN=0,
		BONE_UNKNOWN1=1,
		BONE_UNKNOWN2=2,
		BONE_UNKNOWN3=3,
		BONE_IK=4,
		BONE_UNKNOWN5=5,
		BONE_UNKNOWN6=6,
		BONE_UNKNOWN7=7,
		BONE_UNKNOWN8=8,
		BONE_COROTATE=9,
	};

	typedef struct Bone {
		//char name[20];
		S16  parentId;
		S16  childId;
		U8   type;
		S16  targetId;
		Point3F defaultPosition;
	} Bone;

	Vector<Bone> mBones;
	Vector<StringTableEntry> mNormalBoneNames;
	Vector<StringTableEntry> mEnglishBoneNames;

	Vector<StringTableEntry> mBoneGroupNames;
	Vector<StringTableEntry> mEnglishBoneGroupNames;
	Vector<U16> mBoneGroupList;
	Vector<U8>  mBoneGroupGroup;

	typedef struct Ik {
		S16 destId;
		S16 targetId;
		U8  numLinks;
		S16 maxIterations;
		F32 maxAngle;
		U32 linkListIdx;
	} Ik;

	Vector<Ik> mIks;
	Vector<U16> mIkChains;

	enum MorphType {
		MORPH_OTHER=0,
		MORPH_EYEBROW=1,
		MORPH_EYE=2,
		MORPH_LIP=3
	};

	typedef struct Morph {
		//char name[20];
		U32   numVerts;
		U8   type;
		U32  morphFaceIdx;
	} Morph;

	Vector<Morph>   mMorphs;
	Vector<U32>     mMorphIndexes;
	Vector<Point3F> mMorphVerts;
	Vector<StringTableEntry> mNormalMorphNames;
	Vector<StringTableEntry> mEnglishMorphNames;

	Vector<U16> mMorphsToDisplay;

	Vector<StringTableEntry> mToonTextures;

	enum RigidBodyType {
		RIGID_KINEMATIC=0,
		RIGID_SIMULATED=1,
		RIGID_ALIGNED=2
	};

	typedef struct RigidBody {
		//char name[20];
		U16     boneId;
		U8      collisionGroup;
		U16     collisionMask;
		U8      shape;
		F32     width;
		F32     radius;
		F32     depth;
		Point3F pos;
		EulerF  rot;
		F32     mass;
		F32     linearDampCoeff;
		F32     angularDampCoeff;
		F32     resititutionCoeff;
		F32     frictionCoeff;
		U8      bodyType;
	} RigidBody;

	Vector<RigidBody> mRigidBodys;
	Vector<StringTableEntry> mRigidBodyNames;

	typedef struct PhysicsConstraint {
		//char name[20];
		U32     firstId;
		U32     secondId;
		Point3F pos;
		EulerF  rot;
		Point3F lowerPos;
		Point3F upperPos;
		EulerF  lowerRot;
		EulerF  upperRot;
		Point3F stiffness;
		EulerF  rotStiffness;
	} PhysicsConstraint;

	Vector<PhysicsConstraint> mPhysicsConstraints;
	Vector<StringTableEntry> mPhysicsConstraintNames;

   static MikuModel* checkAndLoadMikuModel(const Torque::Path& path);

};

#endif