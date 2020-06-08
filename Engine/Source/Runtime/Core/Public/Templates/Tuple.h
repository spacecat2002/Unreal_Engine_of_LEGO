// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/AreTypesEqual.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/Decay.h"
#include "Delegates/IntegerSequence.h"
#include "Templates/Invoke.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/MemoryLayout.h"
#include "Templates/TypeHash.h"
#include "Templates/IsConstructible.h"

class FArchive;

template <typename... Types>
struct TTuple;

namespace UE4Tuple_Private
{
	enum EForwardingConstructor { ForwardingConstructor };
	enum EOtherTupleConstructor { OtherTupleConstructor };

	// This only exists to have something to expand a parameter pack into, for concept checking
	template <typename... ArgTypes>
	void ConceptCheckingHelper(ArgTypes&&...);

	template <typename T, uint32 Index>
	struct TTupleElement
	{
		template <
			typename ArgType,
			typename TEnableIf<TIsConstructible<T, ArgType&&>::Value>::Type* = nullptr
		>
		explicit TTupleElement(EForwardingConstructor, ArgType&& Arg)
			: Value(Forward<ArgType>(Arg))
		{
		}

		TTupleElement()
			: Value()
		{
		}

		TTupleElement(TTupleElement&&) = default;
		TTupleElement(const TTupleElement&) = default;
		TTupleElement& operator=(TTupleElement&&) = default;
		TTupleElement& operator=(const TTupleElement&) = default;

		T Value;
	};

	template <uint32 Index, uint32 TupleSize>
	struct TTupleElementHelper
	{
		template <typename DeducedType, typename TupleType>
		static FORCEINLINE decltype(auto) GetImpl(const volatile TTupleElement<DeducedType, Index>&, TupleType&& Tuple)
		{
			// Brackets are important here - we want a reference type to be returned, not object type
			return (ForwardAsBase<TupleType, TTupleElement<DeducedType, Index>>(Tuple).Value);
		}

		template <typename TupleType>
		static FORCEINLINE decltype(auto) Get(TupleType&& Tuple)
		{
			return GetImpl(Tuple, Forward<TupleType>(Tuple));
		}
	};

	template <>
	struct TTupleElementHelper<0, 2>
	{
		template <typename TupleType>
		static FORCEINLINE decltype(auto) Get(TupleType&& Tuple)
		{
			// Brackets are important here - we want a reference type to be returned, not object type
			return (Forward<TupleType>(Tuple).Key);
		}
	};

	template <>
	struct TTupleElementHelper<1, 2>
	{
		template <typename TupleType>
		static FORCEINLINE decltype(auto) Get(TupleType&& Tuple)
		{
			// Brackets are important here - we want a reference type to be returned, not object type
			return (Forward<TupleType>(Tuple).Value);
		}
	};

	template <uint32 ArgCount, uint32 ArgToCompare>
	struct FEqualityHelper
	{
		template <typename TupleType>
		FORCEINLINE static bool Compare(const TupleType& Lhs, const TupleType& Rhs)
		{
			return Lhs.template Get<ArgToCompare>() == Rhs.template Get<ArgToCompare>() && FEqualityHelper<ArgCount, ArgToCompare + 1>::Compare(Lhs, Rhs);
		}
	};

	template <uint32 ArgCount>
	struct FEqualityHelper<ArgCount, ArgCount>
	{
		template <typename TupleType>
		FORCEINLINE static bool Compare(const TupleType& Lhs, const TupleType& Rhs)
		{
			return true;
		}
	};

	template <uint32 NumArgs, uint32 ArgToCompare = 0, bool Last = ArgToCompare + 1 == NumArgs>
	struct TLessThanHelper
	{
		template <typename TupleType>
		FORCEINLINE static bool Do(const TupleType& Lhs, const TupleType& Rhs)
		{
			return Lhs.template Get<ArgToCompare>() < Rhs.template Get<ArgToCompare>() || (!(Rhs.template Get<ArgToCompare>() < Lhs.template Get<ArgToCompare>()) && TLessThanHelper<NumArgs, ArgToCompare + 1>::Do(Lhs, Rhs));
		}
	};

	template <uint32 NumArgs, uint32 ArgToCompare>
	struct TLessThanHelper<NumArgs, ArgToCompare, true>
	{
		template <typename TupleType>
		FORCEINLINE static bool Do(const TupleType& Lhs, const TupleType& Rhs)
		{
			return Lhs.template Get<ArgToCompare>() < Rhs.template Get<ArgToCompare>();
		}
	};

	template <uint32 NumArgs>
	struct TLessThanHelper<NumArgs, NumArgs, false>
	{
		template <typename TupleType>
		FORCEINLINE static bool Do(const TupleType& Lhs, const TupleType& Rhs)
		{
			return false;
		}
	};

	template <typename Indices, typename... Types>
	struct TTupleStorage;

	template <uint32... Indices, typename... Types>
	struct TTupleStorage<TIntegerSequence<uint32, Indices...>, Types...> : TTupleElement<Types, Indices>...
	{
		template <
			typename... ArgTypes,
			decltype(ConceptCheckingHelper(TTupleElement<Types, Indices>(ForwardingConstructor, DeclVal<ArgTypes&&>())...))* = nullptr
		>
		explicit TTupleStorage(EForwardingConstructor, ArgTypes&&... Args)
			: TTupleElement<Types, Indices>(ForwardingConstructor, Forward<ArgTypes>(Args))...
		{
		}

		TTupleStorage() = default;
		TTupleStorage(TTupleStorage&&) = default;
		TTupleStorage(const TTupleStorage&) = default;
		TTupleStorage& operator=(TTupleStorage&&) = default;
		TTupleStorage& operator=(const TTupleStorage&) = default;

		template <uint32 Index, typename TEnableIf<(Index < sizeof...(Types))>::Type* = nullptr> FORCEINLINE decltype(auto) Get() const&  { return TTupleElementHelper<Index, sizeof...(Types)>::Get(static_cast<const TTupleStorage& >(*this)); }
		template <uint32 Index, typename TEnableIf<(Index < sizeof...(Types))>::Type* = nullptr> FORCEINLINE decltype(auto) Get() const&& { return TTupleElementHelper<Index, sizeof...(Types)>::Get(static_cast<const TTupleStorage&&>(*this)); }
		template <uint32 Index, typename TEnableIf<(Index < sizeof...(Types))>::Type* = nullptr> FORCEINLINE decltype(auto) Get()      &  { return TTupleElementHelper<Index, sizeof...(Types)>::Get(static_cast<      TTupleStorage& >(*this)); }
		template <uint32 Index, typename TEnableIf<(Index < sizeof...(Types))>::Type* = nullptr> FORCEINLINE decltype(auto) Get()      && { return TTupleElementHelper<Index, sizeof...(Types)>::Get(static_cast<      TTupleStorage&&>(*this)); }
	};

	// Specialization of 2-TTuple to give it the API of TPair.
	template <typename InKeyType, typename InValueType>
	struct TTupleStorage<TIntegerSequence<uint32, 0, 1>, InKeyType, InValueType>
	{
		typedef InKeyType   KeyType;
		typedef InValueType ValueType;

		template <
			typename KeyInitType,
			typename ValueInitType,
			typename TEnableIf<TAnd<TIsConstructible<KeyType, KeyInitType&&>, TIsConstructible<ValueType, ValueInitType&&>>::Value>::Type* = nullptr
		>
		explicit TTupleStorage(EForwardingConstructor, KeyInitType&& KeyInit, ValueInitType&& ValueInit)
			: Key  (Forward<KeyInitType  >(KeyInit  ))
			, Value(Forward<ValueInitType>(ValueInit))
		{
		}

		TTupleStorage()
			: Key()
			, Value()
		{
		}

		TTupleStorage(TTupleStorage&&) = default;
		TTupleStorage(const TTupleStorage&) = default;
		TTupleStorage& operator=(TTupleStorage&&) = default;
		TTupleStorage& operator=(const TTupleStorage&) = default;

		template <uint32 Index, typename TEnableIf<(Index < 2)>::Type* = nullptr> FORCEINLINE decltype(auto) Get() const& { return TTupleElementHelper<Index, 2>::Get(static_cast<const TTupleStorage&>(*this)); }
		template <uint32 Index, typename TEnableIf<(Index < 2)>::Type* = nullptr> FORCEINLINE decltype(auto) Get() const&& { return TTupleElementHelper<Index, 2>::Get(static_cast<const TTupleStorage&&>(*this)); }
		template <uint32 Index, typename TEnableIf<(Index < 2)>::Type* = nullptr> FORCEINLINE decltype(auto) Get()      & { return TTupleElementHelper<Index, 2>::Get(static_cast<TTupleStorage&>(*this)); }
		template <uint32 Index, typename TEnableIf<(Index < 2)>::Type* = nullptr> FORCEINLINE decltype(auto) Get()      && { return TTupleElementHelper<Index, 2>::Get(static_cast<      TTupleStorage&&>(*this)); }

		InKeyType   Key;
		InValueType Value;
	};

	template <typename Indices, typename... Types>
	struct TTupleImpl;

	template <uint32... Indices, typename... Types>
	struct TTupleImpl<TIntegerSequence<uint32, Indices...>, Types...> : TTupleStorage<TIntegerSequence<uint32, Indices...>, Types...>
	{
	private:
		typedef TTupleStorage<TIntegerSequence<uint32, Indices...>, Types...> Super;

	public:
		using Super::Get;

		template <
			typename... ArgTypes,
			decltype(ConceptCheckingHelper(Super(ForwardingConstructor, DeclVal<ArgTypes&&>()...)))* = nullptr
		>
		explicit TTupleImpl(EForwardingConstructor, ArgTypes&&... Args)
			: Super(ForwardingConstructor, Forward<ArgTypes>(Args)...)
		{
		}

		template <
			typename TupleType,
			decltype(ConceptCheckingHelper(Super(ForwardingConstructor, DeclVal<TupleType&&>().template Get<Indices>()...)))* = nullptr
		>
		explicit TTupleImpl(EOtherTupleConstructor, TupleType&& Other)
			: Super(ForwardingConstructor, Forward<TupleType>(Other).template Get<Indices>()...)
		{
		}

		TTupleImpl() = default;
		TTupleImpl(TTupleImpl&& Other) = default;
		TTupleImpl(const TTupleImpl& Other) = default;
		TTupleImpl& operator=(TTupleImpl&& Other) = default;
		TTupleImpl& operator=(const TTupleImpl& Other) = default;

		template <typename FuncType, typename... ArgTypes>
		decltype(auto) ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const
		{
			return ::Invoke(Func, Forward<ArgTypes>(Args)..., this->template Get<Indices>()...);
		}

		template <typename FuncType, typename... ArgTypes>
		decltype(auto) ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const
		{
			return ::Invoke(Func, this->template Get<Indices>()..., Forward<ArgTypes>(Args)...);
		}

		FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TTupleImpl& Tuple)
		{
			// This should be implemented with a fold expression when our compilers support it
			int Temp[] = { 0, (Ar << Tuple.template Get<Indices>(), 0)... };
			(void)Temp;
			return Ar;
		}

		FORCEINLINE friend void operator<<(FStructuredArchive::FSlot Slot, TTupleImpl& Tuple)
		{
			// This should be implemented with a fold expression when our compilers support it
			FStructuredArchive::FStream Stream = Slot.EnterStream();
			int Temp[] = { 0, (Stream.EnterElement() << Tuple.template Get<Indices>(), 0)... };
			(void)Temp;
		}

		FORCEINLINE friend bool operator==(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			// This could be implemented with a fold expression when our compilers support it
			return FEqualityHelper<sizeof...(Types), 0>::Compare(Lhs, Rhs);
		}

		FORCEINLINE friend bool operator!=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return !(Lhs == Rhs);
		}

		FORCEINLINE friend bool operator<(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return TLessThanHelper<sizeof...(Types)>::Do(Lhs, Rhs);
		}

		FORCEINLINE friend bool operator<=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return !(Rhs < Lhs);
		}

		FORCEINLINE friend bool operator>(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return Rhs < Lhs;
		}

		FORCEINLINE friend bool operator>=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return !(Lhs < Rhs);
		}
	};

	template <
		typename LhsType,
		typename RhsType,
		uint32... Indices,
		decltype(ConceptCheckingHelper((DeclVal<LhsType&>().template Get<Indices>() = DeclVal<RhsType&&>().template Get<Indices>(), 0)...))* = nullptr
	>
	static void Assign(LhsType& Lhs, RhsType&& Rhs, TIntegerSequence<uint32, Indices...>)
	{
		// This should be implemented with a fold expression when our compilers support it
		int Temp[] = { 0, (Lhs.template Get<Indices>() = Forward<RhsType>(Rhs).template Get<Indices>(), 0)... };
		(void)Temp;
	}

	template <typename... Types>
	FORCEINLINE TTuple<typename TDecay<Types>::Type...> MakeTupleImpl(Types&&... Args)
	{
		return TTuple<typename TDecay<Types>::Type...>(Forward<Types>(Args)...);
	}

	template <typename IntegerSequence>
	struct TTransformTuple_Impl;

	template <uint32... Indices>
	struct TTransformTuple_Impl<TIntegerSequence<uint32, Indices...>>
	{
		template <typename TupleType, typename FuncType>
		static decltype(auto) Do(TupleType&& Tuple, FuncType Func)
		{
			return MakeTupleImpl(Func(Forward<TupleType>(Tuple).template Get<Indices>())...);
		}
	};

	template <typename IntegerSequence>
	struct TVisitTupleElements_Impl;

	template <uint32... Indices>
	struct TVisitTupleElements_Impl<TIntegerSequence<uint32, Indices...>>
	{
		// We need a second function to do the invocation for a particular index, to avoid the pack expansion being
		// attempted on the indices and tuples simultaneously.
		template <uint32 Index, typename FuncType, typename... TupleTypes>
		FORCEINLINE static void InvokeFunc(FuncType&& Func, TupleTypes&&... Tuples)
		{
			::Invoke(Forward<FuncType>(Func), Forward<TupleTypes>(Tuples).template Get<Index>()...);
		}

		template <typename FuncType, typename... TupleTypes>
		static void Do(FuncType&& Func, TupleTypes&&... Tuples)
		{
			// This should be implemented with a fold expression when our compilers support it
			int Temp[] = { 0, (InvokeFunc<Indices>(Forward<FuncType>(Func), Forward<TupleTypes>(Tuples)...), 0)... };
			(void)Temp;
		}
	};


	template <typename TupleType>
	struct TCVTupleArity;

	template <typename... Types>
	struct TCVTupleArity<const volatile TTuple<Types...>>
	{
		enum { Value = sizeof...(Types) };
	};

	template <uint32 ArgToCombine, uint32 ArgCount>
	struct TGetTupleHashHelper
	{
		template <typename TupleType>
		FORCEINLINE static uint32 Do(uint32 Hash, const TupleType& Tuple)
		{
			return TGetTupleHashHelper<ArgToCombine + 1, ArgCount>::Do(HashCombine(Hash, GetTypeHash(Tuple.template Get<ArgToCombine>())), Tuple);
		}
	};

	template <uint32 ArgIndex>
	struct TGetTupleHashHelper<ArgIndex, ArgIndex>
	{
		template <typename TupleType>
		FORCEINLINE static uint32 Do(uint32 Hash, const TupleType& Tuple)
		{
			return Hash;
		}
	};

	template <typename... Given, typename... Deduced>
	std::enable_if_t<TAnd<TIsConstructible<Given, Deduced&&>...>::Value> ConstructibleConceptCheck(Deduced&&...);

	template <typename... Given, typename... Deduced>
	decltype(ConceptCheckingHelper((DeclVal<Given>() = DeclVal<Deduced&&>(), 0)...)) AssignableConceptCheck(Deduced&&...);
}

template <typename... Types>
struct TTuple : UE4Tuple_Private::TTupleImpl<TMakeIntegerSequence<uint32, sizeof...(Types)>, Types...>
{
private:
	typedef UE4Tuple_Private::TTupleImpl<TMakeIntegerSequence<uint32, sizeof...(Types)>, Types...> Super;

public:
	template <
		typename... ArgTypes,
		decltype(UE4Tuple_Private::ConstructibleConceptCheck<Types...>(DeclVal<ArgTypes&&>()...))* = nullptr
	>
	explicit TTuple(ArgTypes&&... Args)
		: Super(UE4Tuple_Private::ForwardingConstructor, Forward<ArgTypes>(Args)...)
	{
	}

	template <
		typename... OtherTypes,
		decltype(UE4Tuple_Private::ConstructibleConceptCheck<Types...>(DeclVal<OtherTypes&&>()...))* = nullptr
	>
	TTuple(TTuple<OtherTypes...>&& Other)
		: Super(UE4Tuple_Private::OtherTupleConstructor, MoveTemp(Other))
	{
	}

	template <
		typename... OtherTypes,
		decltype(UE4Tuple_Private::ConstructibleConceptCheck<Types...>(DeclVal<const OtherTypes&>()...))* = nullptr
	>
	TTuple(const TTuple<OtherTypes...>& Other)
		: Super(UE4Tuple_Private::OtherTupleConstructor, Other)
	{
	}

	TTuple() = default;
	TTuple(TTuple&&) = default;
	TTuple(const TTuple&) = default;
	TTuple& operator=(TTuple&&) = default;
	TTuple& operator=(const TTuple&) = default;

	template <
		typename... OtherTypes,
		decltype(UE4Tuple_Private::AssignableConceptCheck<Types&...>(DeclVal<const OtherTypes&>()...))* = nullptr
	>
	TTuple& operator=(const TTuple<OtherTypes...>& Other)
	{
		UE4Tuple_Private::Assign(*this, Other, TMakeIntegerSequence<uint32, sizeof...(Types)>{});
		return *this;
	}

	template <
		typename... OtherTypes,
		decltype(UE4Tuple_Private::AssignableConceptCheck<Types&...>(DeclVal<OtherTypes&&>()...))* = nullptr
	>
	TTuple& operator=(TTuple<OtherTypes...>&& Other)
	{
		UE4Tuple_Private::Assign(*this, MoveTemp(Other), TMakeIntegerSequence<uint32, sizeof...(OtherTypes)>{});
		return *this;
	}
};

template <typename... Types>
FORCEINLINE uint32 GetTypeHash(const TTuple<Types...>& Tuple)
{
	return UE4Tuple_Private::TGetTupleHashHelper<1u, sizeof...(Types)>::Do(GetTypeHash(Tuple.template Get<0>()), Tuple);
}

FORCEINLINE uint32 GetTypeHash(const TTuple<>& Tuple)
{
	return 0;
}

namespace Freeze
{
	template<typename KeyType, typename ValueType>
	void IntrinsicWriteMemoryImage(FMemoryImageWriter& Writer, const TTuple<KeyType, ValueType>& Object, const FTypeLayoutDesc&)
	{
		Writer.WriteObject(Object.Key);
		Writer.WriteObject(Object.Value);
	}

	template<typename KeyType, typename ValueType>
	void IntrinsicUnfrozenCopy(const FMemoryUnfreezeContent& Context, const TTuple<KeyType, ValueType>& Object, void* OutDst)
	{
		TTuple<KeyType, ValueType>* DstObject = (TTuple<KeyType, ValueType>*)OutDst;
		Context.UnfreezeObject(Object.Key, &DstObject->Key);
		Context.UnfreezeObject(Object.Value, &DstObject->Value);
	}

	template<typename KeyType, typename ValueType>
	uint32 IntrinsicAppendHash(const TTuple<KeyType, ValueType>* DummyObject, const FTypeLayoutDesc& TypeDesc, const FPlatformTypeLayoutParameters& LayoutParams, FSHA1& Hasher)
	{
		return Freeze::AppendHashPair(StaticGetTypeLayoutDesc<KeyType>(), StaticGetTypeLayoutDesc<ValueType>(), LayoutParams, Hasher);
	}

	template<typename KeyType, typename ValueType>
	uint32 IntrinsicGetTargetAlignment(const TTuple<KeyType, ValueType>* DummyObject, const FTypeLayoutDesc& TypeDesc, const FPlatformTypeLayoutParameters& LayoutParams)
	{
		const uint32 KeyAlignment = GetTargetAlignment(StaticGetTypeLayoutDesc<KeyType>(), LayoutParams);
		const uint32 ValueAlignment = GetTargetAlignment(StaticGetTypeLayoutDesc<ValueType>(), LayoutParams);
		return FMath::Min(FMath::Max(KeyAlignment, ValueAlignment), LayoutParams.MaxFieldAlignment);
	}
}

DECLARE_TEMPLATE_INTRINSIC_TYPE_LAYOUT((template <typename KeyType, typename ValueType>), (TTuple<KeyType, ValueType>));

/**
 * Traits class which calculates the number of elements in a tuple.
 */
template <typename TupleType>
struct TTupleArity : UE4Tuple_Private::TCVTupleArity<const volatile TupleType>
{
};


/**
 * Makes a TTuple from some arguments.  The type of the TTuple elements are the decayed versions of the arguments.
 *
 * @param  Args  The arguments used to construct the tuple.
 * @return A tuple containing a copy of the arguments.
 *
 * Example:
 *
 * void Func(const int32 A, FString&& B)
 * {
 *     // Equivalent to:
 *     // TTuple<int32, const TCHAR*, FString> MyTuple(A, TEXT("Hello"), MoveTemp(B));
 *     auto MyTuple = MakeTuple(A, TEXT("Hello"), MoveTemp(B));
 * }
 */
template <typename... Types>
FORCEINLINE TTuple<typename TDecay<Types>::Type...> MakeTuple(Types&&... Args)
{
	return UE4Tuple_Private::MakeTupleImpl(Forward<Types>(Args)...);
}


/**
 * Creates a new TTuple by applying a functor to each of the elements.
 *
 * @param  Tuple  The tuple to apply the functor to.
 * @param  Func   The functor to apply.
 *
 * @return A new tuple of the transformed elements.
 *
 * Example:
 *
 * float        Overloaded(int32 Arg);
 * char         Overloaded(const TCHAR* Arg);
 * const TCHAR* Overloaded(const FString& Arg);
 *
 * void Func(const TTuple<int32, const TCHAR*, FString>& MyTuple)
 * {
 *     // Equivalent to:
 *     // TTuple<float, char, const TCHAR*> TransformedTuple(Overloaded(MyTuple.Get<0>()), Overloaded(MyTuple.Get<1>()), Overloaded(MyTuple.Get<2>())));
 *     auto TransformedTuple = TransformTuple(MyTuple, [](const auto& Arg) { return Overloaded(Arg); });
 * }
 */
template <typename FuncType, typename... Types>
FORCEINLINE decltype(auto) TransformTuple(TTuple<Types...>&& Tuple, FuncType Func)
{
	return UE4Tuple_Private::TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(MoveTemp(Tuple), MoveTemp(Func));
}

template <typename FuncType, typename... Types>
FORCEINLINE decltype(auto) TransformTuple(const TTuple<Types...>& Tuple, FuncType Func)
{
	return UE4Tuple_Private::TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(Tuple, MoveTemp(Func));
}


/**
 * Visits each element in the specified tuples in parallel and applies them as arguments to the functor.
 * All specified tuples must have the same number of elements.
 *
 * @param  Func    The functor to apply.
 * @param  Tuples  The tuples whose elements are to be applied to the functor.
 *
 * Example:
 *
 * void Func(const TTuple<int32, const TCHAR*, FString>& Tuple1, const TTuple<bool, float, FName>& Tuple2)
 * {
 *     // Equivalent to:
 *     // Functor(Tuple1.Get<0>(), Tuple2.Get<0>());
 *     // Functor(Tuple1.Get<1>(), Tuple2.Get<1>());
 *     // Functor(Tuple1.Get<2>(), Tuple2.Get<2>());
 *     VisitTupleElements(Functor, Tuple1, Tuple2);
 * }
 */
template <typename FuncType, typename FirstTupleType, typename... TupleTypes>
FORCEINLINE void VisitTupleElements(FuncType&& Func, FirstTupleType&& FirstTuple, TupleTypes&&... Tuples)
{
	UE4Tuple_Private::TVisitTupleElements_Impl<TMakeIntegerSequence<uint32, TTupleArity<typename TDecay<FirstTupleType>::Type>::Value>>::Do(Forward<FuncType>(Func), Forward<FirstTupleType>(FirstTuple), Forward<TupleTypes>(Tuples)...);
}

/**
 * Tie function for structured unpacking of tuples into individual variables.
 *
 * Example:
 *
 * TTuple<FString, float, TArray<int32>> SomeFunction();
 *
 * FString Ret1;
 * float Ret2;
 * TArray<int32> Ret3;
 *
 * Tie(Ret1, Ret2, Ret3) = SomeFunction();
 *
 * // Now Ret1, Ret2 and Ret3 contain the unpacked return values.
 */
template <typename... Types>
FORCEINLINE TTuple<Types&...> Tie(Types&... Args)
{
	return TTuple<Types&...>(Args...);
}
