// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RBF/RBFInterpolator.h"

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wshadow\"")
#endif

#include "Eigen/LU"

#if defined(__GCC__)
_Pragma("GCC diagnostic pop")
#endif

FRBFInterpolatorBase::FRBFInterpolatorBase(
	TArray<float>&& UpperKernel, 
	int32 Size
)
{
	using namespace Eigen;

	MatrixXf FullKernel;

	// Construct the full kernel from the upper half calculated in the templated
	// portion.
	FullKernel = MatrixXf::Identity(Size, Size);

	for (int32 c = 0, i = 0; i < Size; i++)	
	{
		for (int32 j = i; j < Size; j++)
		{
			FullKernel(i, j) = FullKernel(j, i) = UpperKernel[c++];
		}
	}

	// Usually the RBF formulation is computed by solving for:
	// 
	//   A * w = T
	//
	// Where A is the target kernel is a symmetric matrix containing the distance 
	// between each node, w is the weights we want, and T is the target vector whose
	// values we want to interpolate.
	//
	// However, in our case, we consider the activation of each node's output value
	// to be a part of a N-dimensional vector, whose size is the same as the node count,
	// and therefore, collectively, same as the target kernel's dimensions. 
	// Each row is all zeros except for the activation value of 1.0 for a node at that
	// node's index, effectively forming an identity matrix.
	// 
	// This allows us to reformulate the problem as:
	//
	//   A * w = I
	//
	// Or in other words:  
	//
	//   w = A^-1
	//
	// Eigen will now happily pick LU factorization with partial pivoting for 
	// the inverse, which is blazingly fast.

	Coeffs.Init(0.0f, Size * Size);
	Map<MatrixXf> Result(Coeffs.GetData(), Size, Size);
	Result = FullKernel.inverse();
}
