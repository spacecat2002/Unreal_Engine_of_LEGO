// Copyright Epic Games, Inc. All Rights Reserved.

#include <type_traits>
#include "Templates/CopyQualifiersAndRefsFromTo.h"

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  ,                int  >,                int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  ,                int  >, const          int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  ,                int  >,       volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  ,                int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& ,                int  >,                int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& ,                int  >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& ,                int  >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& ,                int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&,                int  >,                int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&,                int  >, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&,                int  >,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&,                int  >, const volatile int&&>, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  , const          int  >, const          int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  , const          int  >, const          int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  , const          int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  , const          int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& , const          int  >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& , const          int  >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& , const          int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& , const          int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&, const          int  >, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&, const          int  >, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&, const          int  >, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&, const          int  >, const volatile int&&>, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  ,       volatile int  >,       volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  ,       volatile int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  ,       volatile int  >,       volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  ,       volatile int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& ,       volatile int  >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& ,       volatile int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& ,       volatile int  >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& ,       volatile int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&,       volatile int  >,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&,       volatile int  >, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&,       volatile int  >,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&,       volatile int  >, const volatile int&&>, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  , const volatile int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  , const volatile int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  , const volatile int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  , const volatile int  >, const volatile int  >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& , const volatile int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& , const volatile int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& , const volatile int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& , const volatile int  >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&, const volatile int  >, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&, const volatile int  >, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&, const volatile int  >, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&, const volatile int  >, const volatile int&&>, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  ,                int& >,                int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  ,                int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  ,                int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  ,                int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& ,                int& >,                int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& ,                int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& ,                int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& ,                int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&,                int& >,                int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&,                int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&,                int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&,                int& >, const volatile int& >, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  , const          int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  , const          int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  , const          int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  , const          int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& , const          int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& , const          int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& , const          int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& , const          int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&, const          int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&, const          int& >, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&, const          int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&, const          int& >, const volatile int& >, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  ,       volatile int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  ,       volatile int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  ,       volatile int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  ,       volatile int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& ,       volatile int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& ,       volatile int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& ,       volatile int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& ,       volatile int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&,       volatile int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&,       volatile int& >, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&,       volatile int& >,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&,       volatile int& >, const volatile int& >, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  ,                int&&>,                int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  ,                int&&>, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  ,                int&&>,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  ,                int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& ,                int&&>,                int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& ,                int&&>, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& ,                int&&>,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& ,                int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&,                int&&>,                int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&,                int&&>, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&,                int&&>,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&,                int&&>, const volatile int&&>, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  , const          int&&>, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  , const          int&&>, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  , const          int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  , const          int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& , const          int&&>, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& , const          int&&>, const          int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& , const          int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& , const          int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&, const          int&&>, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&, const          int&&>, const          int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&, const          int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&, const          int&&>, const volatile int&&>, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  ,       volatile int&&>,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  ,       volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  ,       volatile int&&>,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  ,       volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& ,       volatile int&&>,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& ,       volatile int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& ,       volatile int&&>,       volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& ,       volatile int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int&&,       volatile int&&>,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&,       volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&,       volatile int&&>,       volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&,       volatile int&&>, const volatile int&&>, "");

static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int  , const volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int  , const volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int  , const volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int  , const volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<               int& , const volatile int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int& , const volatile int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int& , const volatile int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int& , const volatile int&&>, const volatile int& >, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const          int&&, const volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<      volatile int&&, const volatile int&&>, const volatile int&&>, "");
static_assert(std::is_same_v<TCopyQualifiersAndRefsFromTo_T<const volatile int&&, const volatile int&&>, const volatile int&&>, "");
