// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Misc/MemStack.h"
#include "Templates/Function.h"

/* A collection of distance metrics between two values of the same type */
namespace RBFDistanceMetric
{
	/* Returns the Euclidean (L2) distance between two coordinate vectors. */
	static inline float Euclidean(const FVector& A, const FVector& B)
	{
		return FVector::Distance(A, B);
	}

	/* Returns the Manhattan (L1), or Taxi-cab distance between two coordinate vectors. */
	static inline float Manhattan(const FVector& A, const FVector& B)
	{
		FVector AbsDiff = (A - B).GetAbs();
		return AbsDiff.X + AbsDiff.Y + AbsDiff.Z;
	}

	/* Returns the arc length between two unit vectors (i.e. the distance between two
	   points on a unit sphere, traveling along the surface of the sphere) */
	static inline float ArcLength(const FVector& A, const FVector B)
	{
		return FMath::Acos(A.GetSafeNormal() | B.GetSafeNormal());
	}


	/* Returns a straight-up Euclidean distance between two rotation values expressed
	   in radians.
	*/
	static inline float Euclidean(const FRotator& A, const FRotator& B)
	{
		return Euclidean(FVector(FMath::DegreesToRadians(A.Roll),
								 FMath::DegreesToRadians(A.Pitch),
								 FMath::DegreesToRadians(A.Yaw)),
						 FVector(FMath::DegreesToRadians(B.Roll),
								 FMath::DegreesToRadians(B.Pitch),
								 FMath::DegreesToRadians(B.Yaw)));
	}

	/* Returns the arc-length distance, on a unit sphere, between two rotation vectors.
	*/
	static inline float ArcLength(const FRotator& A, const FRotator& B)
	{
		return FMath::Acos(A.Vector() | B.Vector());
	}

	/* Returns the Euclidean (L2) distance between two quaternion values expressed.
	*/
	static inline float Euclidean(const FQuat& A, const FQuat& B)
	{
		return (A - B).Size();
	}

	/* Returns the arc-length distance, on a unit sphere, between two quaternions.
	*/
	static inline float ArcLength(const FQuat& A, const FQuat& B)
	{
		return A.GetNormalized().AngularDistance(B.GetNormalized());
	}

	/* Returns the swing arc length distance between two quaternions, using a specific 
	   twist basis vector as reference.
	*/
	static inline float SwingAngle(const FQuat& A, const FQuat& B, const FVector& TwistAxis)
	{
		return ArcLength(A.RotateVector(TwistAxis), B.RotateVector(TwistAxis));
	}

	/* Returns the twist arc length distance between two quaternions, using a specific 
	   twist basis vector as reference.
	*/
	static inline float TwistAngle(const FQuat& A, const FQuat& B, const FVector& TwistAxis)
	{
		return FMath::Abs(A.GetTwistAngle(TwistAxis) - B.GetTwistAngle(TwistAxis));
	}
}


/* A collection of smoothing kernels, all of which map the input of zero to 1.0 and 
   all values on either side as monotonically decreasing as they move away from zero.
   The width of the falloff can be specified using the Sigma parameter.
   NOTE: These kernels have been tuned to give similar falloff convergence for the same 
   value of sigma. */
namespace RBFKernel
{
	/* A simple linear falloff, clamping at zero out when the norm of Value exceeds Sigma */
	static inline float Linear(float Value, float Sigma)
	{
		return (Sigma - FMath::Clamp(Value, 0.0f, Sigma)) / Sigma;
	}

	/* A gaussian falloff */
	static inline float Gaussian(float Value, float Sigma)
	{
		return FMath::Exp(-0.5f * FMath::Square(Value / (Sigma / 2.0f)));
	}

	/* An exponential falloff with a sharp peak */
	static inline float Exponential(float Value, float Sigma)
	{
		return FMath::Exp(-2.0f * Value / Sigma);
	}

	/* A cubic falloff, with identical clamping behavior to the linear falloff, 
	   but with a smooth peak */
	static inline float Cubic(float Value, float Sigma)
	{
		Value /= Sigma;
		return FMath::Max(1.f - (Value * Value * Value), 0.f);
	}

	/* A quintic falloff, with identical clamping behavior to the linear falloff, 
	   but with a flatter peak than cubic */
	static inline float Quintic(float Value, float Sigma)
	{
		Value /= Sigma;
		return FMath::Max(1.f - FMath::Pow(Value, 5.0f), 0.f);
	}
}


// An implementation detail for the RBF interpolator to hide the use of Eigen from components
// outside AnimGraphRuntime.
class FRBFInterpolatorBase
{
protected:
	FRBFInterpolatorBase(TArray<float>&& UpperKernel, int32 Size);

	// A square matrix of the solved coefficients.
public:
	TArray<float> Coeffs;
};


template<typename T>
class TRBFInterpolator
	: public FRBFInterpolatorBase
{
public:
	using NodeArrayT = TArray<T>;
	using WeightFuncT = TFunction<float(const T& A, const T& B)>;

	/* Construct an RBF interpolator, taking in a set of sparse nodes and a weight computation
	   function that computes the distance between two nodes, and optionally smooths the distance
	   with a smoothing kernel.

	*/
	TRBFInterpolator(
		const NodeArrayT& InNodes,
		WeightFuncT InWeightFunc)
		: FRBFInterpolatorBase(MakeUpperKernel(InNodes, InWeightFunc), InNodes.Num())
		, Nodes(InNodes)
		, WeightFunc(InWeightFunc)
	{
	}

	/* Given a value, compute the weight values to use to calculate each node's contribution
	   to that value's location.
	*/
	template<typename InAllocator>
	void Interpolate(
		TArray<float, InAllocator>& OutWeights,
		const T& Value,
		bool bClip = true,
		bool bNormalize = false)
	{
		int NumNodes = Nodes.Num();
		if (NumNodes > 1)
		{
			TArray<float, TMemStackAllocator<> > ValueWeights;
			ValueWeights.SetNum(Nodes.Num());

			for (int32 i = 0; i < NumNodes; i++)
			{
				ValueWeights[i] = WeightFunc(Value, Nodes[i]);
			}

			OutWeights.Reset(NumNodes);
			for (int32 i = 0; i < NumNodes; i++)
			{
				const float* C = &Coeffs[i * NumNodes];
				float W = 0.0f;

				for (int32 j = 0; j < NumNodes; j++)
				{
					W += C[j] * ValueWeights[j];
				}

				OutWeights.Add(W);
			}


			if (bNormalize)
			{
				// Clip here behaves differently than it does when no normalization
				// is taking place. Instead of clipping blindly, we rescale the values based
				// on the minimum value and then use the normalization to bring the values
				// within the 0-1 range.
				if (bClip)
				{
					float MaxNegative = 0.0f;
					for (int32 i = 0; i < NumNodes; i++)
					{
						if (OutWeights[i] < MaxNegative)
							MaxNegative = OutWeights[i];
					}
					for (int32 i = 0; i < NumNodes; i++)
					{
						OutWeights[i] += MaxNegative;
					}
				}

				float TotalWeight = 0.0f;
				for (int32 i = 0; i < NumNodes; i++)
				{
					TotalWeight += OutWeights[i];
				}
				for (int32 i = 0; i < NumNodes; i++)
				{
					// Clamp to clear up any precision issues. This may make the weights not
					// quite add up to 1.0, but that should be sufficient for our needs.
					OutWeights[i] = FMath::Clamp(OutWeights[i] / TotalWeight, 0.0f, 1.0f);
				}
			}
			else if (bClip)
			{
				// This can easily happen when the value being interpolated is outside of the
				// convex hull bounded by the nodes, resulting in an extrapolation.
				for (int32 i = 0; i < NumNodes; i++)
				{
					OutWeights[i] = FMath::Clamp(OutWeights[i], 0.0f, 1.0f);
				}
			}
		}
		else if (NumNodes == 1)
		{
			OutWeights.Reset(1);
			OutWeights.Add(1);
		}
		else
		{
			OutWeights.Reset(0);
		}
	}


private:
	static TArray<float> MakeUpperKernel(
		const NodeArrayT& InNodes,
		WeightFuncT& InWeightFunc)
	{
		// If there are less than two nodes, nothing to do, since the interpolated value
		// will be the same across the entire space. This is handled in Interpolate().
		int32 NumNodes = InNodes.Num();
		if (NumNodes < 2)
		{
			return {};
		}

		// Compute the upper diagonal of the target kernel for solving the weight coefficients.
		TArray<float> UpperKernel;
		UpperKernel.Reserve(NumNodes * (NumNodes - 1) / 2);

		// We need to include the diagonal itself, since we can't guarantee that the weight
		// function returns 1.0 for nodes of the same coordinates.
		for (int32 i = 0; i < NumNodes; i++)
		{
			for (int32 j = i; j < NumNodes; j++)
			{
				UpperKernel.Add(InWeightFunc(InNodes[i], InNodes[j]));
			}
		}
		return UpperKernel;
	}


	const NodeArrayT& Nodes;
	WeightFuncT WeightFunc;
};
