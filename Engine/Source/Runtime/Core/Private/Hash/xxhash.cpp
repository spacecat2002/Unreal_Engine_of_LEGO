// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hash/xxhash.h"

//////////////////////////////////////////////////////////////////////////

THIRD_PARTY_INCLUDES_START

#include "ThirdParty/xxhash/xxhash.c"
#include "ThirdParty/xxhash/xxh3.h"

THIRD_PARTY_INCLUDES_END

//////////////////////////////////////////////////////////////////////////

uint32 
FXxHash::HashBuffer32(const void* BlockPtr, SIZE_T BlockSize)
{
	XXH64_state_t State;
	XXH64_reset(&State, 0);

	XXH64_update(&State, BlockPtr, BlockSize);
	return uint32(XXH64_digest(&State));
}

uint64 
FXxHash::HashBuffer64(const void* BlockPtr, SIZE_T BlockSize)
{
	XXH64_state_t State;
	XXH64_reset(&State, 0);

	XXH64_update(&State, BlockPtr, BlockSize);
	return XXH64_digest(&State);
}
