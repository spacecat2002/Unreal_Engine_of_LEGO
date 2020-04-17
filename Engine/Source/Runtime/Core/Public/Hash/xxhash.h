// Copyright (c) Epic Games

#pragma once

#include "ThirdParty/xxhash/xxh3.h"

struct FXxHash
{
	static CORE_API uint32 HashBuffer32(const void* BlockPtr, SIZE_T BlockSize);
	static CORE_API uint64 HashBuffer64(const void* BlockPtr, SIZE_T BlockSize);
};
