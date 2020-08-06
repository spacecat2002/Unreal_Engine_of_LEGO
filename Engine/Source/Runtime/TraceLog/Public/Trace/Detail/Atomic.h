// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
template <typename Type> Type	AtomicLoadRelaxed(Type volatile* Source);
template <typename Type> Type	AtomicLoadAcquire(Type volatile* Source);
template <typename Type> void	AtomicStoreRelaxed(Type volatile* Target, Type Value);
template <typename Type> void	AtomicStoreRelease(Type volatile* Target, Type Value);
template <typename Type> bool	AtomicCompareExchangeRelaxed(Type volatile* Target, Type New, Type Expected);
template <typename Type> bool	AtomicCompareExchangeAcquire(Type volatile* Target, Type New, Type Expected);
template <typename Type> bool	AtomicCompareExchangeRelease(Type volatile* Target, Type New, Type Expected);
template <typename Type> Type	AtomicAddRelaxed(Type volatile* Target, Type Value);
template <typename Type> Type	AtomicAddRelease(Type volatile* Target, Type Value);
template <typename Type> Type	AtomicAddAcquire(Type volatile* Target, Type Value);
void							PlatformYield();

} // namespace Private
} // namespace Trace

////////////////////////////////////////////////////////////////////////////////
#if PLATFORM_CPU_X86_FAMILY
#include <emmintrin.h>
#endif
#include <atomic>

////////////////////////////////////////////////////////////////////////////////
#define IS_MSVC					0
#define IS_GCC_COMPATIBLE		0
#if defined(_MSC_VER) && !defined(__clang__)
#	undef  IS_MSVC
#	define IS_MSVC				1
#elif defined(__clang__) || defined(__GNUC__)
#	undef  IS_GCC_COMPATIBLE
#	define IS_GCC_COMPATIBLE	1
#endif

////////////////////////////////////////////////////////////////////////////////
#if IS_MSVC
#	include <intrin.h>
#	if defined(_M_ARM) || defined(_M_ARM64)
#		if defined(_M_ARM)
#			include <armintr.h>
#		else
#			include <arm64intr.h>
#		endif
#	endif
#endif

namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
inline void PlatformYield()
{
#if PLATFORM_CPU_X86_FAMILY
	_mm_pause();
#elif PLATFORM_CPU_ARM_FAMILY
#	if IS_MSVC
		__yield();
#	else
		__builtin_arm_yield();
#	endif
#else
	#error Unsupported architecture!
#endif
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline Type AtomicLoadRelaxed(Type volatile* Source)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Source;
	return T->load(std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline Type AtomicLoadAcquire(Type volatile* Source)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Source;
	return T->load(std::memory_order_acquire);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline void AtomicStoreRelaxed(Type volatile* Target, Type Value)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Target;
	T->store(Value, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline void AtomicStoreRelease(Type volatile* Target, Type Value)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Target;
	T->store(Value, std::memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline bool AtomicCompareExchangeRelaxed(Type volatile* Target, Type New, Type Expected)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Target;
	return T->compare_exchange_weak(Expected, New, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline bool AtomicCompareExchangeAcquire(Type volatile* Target, Type New, Type Expected)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Target;
	return T->compare_exchange_weak(Expected, New, std::memory_order_acquire);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline bool AtomicCompareExchangeRelease(Type volatile* Target, Type New, Type Expected)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Target;
	return T->compare_exchange_weak(Expected, New, std::memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline Type AtomicAddRelaxed(Type volatile* Target, Type Value)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Target;
	return T->fetch_add(Value, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline Type AtomicAddAcquire(Type volatile* Target, Type Value)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Target;
	return T->fetch_add(Value, std::memory_order_acquire);
}

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
inline Type AtomicAddRelease(Type volatile* Target, Type Value)
{
	std::atomic<Type>* T = (std::atomic<Type>*) Target;
	return T->fetch_add(Value, std::memory_order_release);
}

#undef IS_GCC_COMPATIBLE
#undef IS_MSVC

} // namespace Private
} // namespace Trace
