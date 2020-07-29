// Copyright Epic Games, Inc. All Rights Reserved.

#define EXPLICIT_VECTOR4 1
#define EXPLICIT_MATRIX 1

#include "Math/Vector.isph"
#include "Math/Matrix.isph"
#include "Math/Quat.isph"
#include "Math/Transform.isph"
#include "Animation/AnimationCurves.isph"

export void FillComponentSpaceTransforms(uniform FTransform ComponentSpaceData[],
										const uniform FTransform LocalTransformsData[],
										const uniform unsigned int16 FillComponentSpaceTransformsRequiredBones[],
										const uniform unsigned int8 * uniform ParentBones,
										const uniform int ParentBoneSize,
										const uniform int ParentIndexOffset,
										const uniform int NumBones)
{
	for (uniform int i = 1; i < NumBones; i++)
	{
		const uniform int BoneIndex = FillComponentSpaceTransformsRequiredBones[i];
		uniform FTransform SpaceBase = ComponentSpaceData[BoneIndex];

		// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
		const uniform unsigned int8 * uniform ParentIndexPtr = ParentBones + ((ParentBoneSize * BoneIndex) + ParentIndexOffset);
		const uniform int ParentIndex = *((uniform int * uniform)ParentIndexPtr);
		const uniform FTransform ParentSpaceBase = ComponentSpaceData[ParentIndex];

		const uniform FTransform LocalTransform = LocalTransformsData[BoneIndex];

		TransformMultiply(SpaceBase, LocalTransform, ParentSpaceBase);
		SpaceBase.Rotation = VectorNormalizeQuaternion(SpaceBase.Rotation);

		ComponentSpaceData[BoneIndex] = SpaceBase;
	}
}

static inline float Lerp(float A, float B, float Alpha)
{
	return (1.0f-Alpha)*A + Alpha*B;
}

export void LerpCurves(const struct FBlendedCurve* uniform Curve1, const struct FBlendedCurve* uniform Curve2, uniform const float Alpha)
{
	foreach (CurveId = 0 ... Curve1->CurveWeights.ArrayNum)
	{
		Curve1->CurveWeights.DataPtr[CurveId] = Lerp(Curve1->CurveWeights.DataPtr[CurveId], Curve2->CurveWeights.DataPtr[CurveId], Alpha);
	}

	foreach (CurveSet = 0 ... Curve1->CurveWeights.ArrayNum / 32)
	{
		Curve1->ValidCurveWeights.Data[CurveSet] |= Curve2->ValidCurveWeights.Data[CurveSet];
	}
}
