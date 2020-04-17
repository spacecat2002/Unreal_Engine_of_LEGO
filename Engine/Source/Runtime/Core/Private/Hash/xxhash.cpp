#include "Hash/xxhash.h"

//////////////////////////////////////////////////////////////////////////

#pragma warning(push)
#pragma warning(disable: 4459)

namespace xxhash_ue4 {

#include "ThirdParty/xxhash/xxhash.c"
#include "ThirdParty/xxhash/xxh3.h"

}

#pragma warning(pop)

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
