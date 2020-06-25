// Copyright Epic Games, Inc. All Rights Reserved.

#include <type_traits>
#include "Templates/Tuple.h"

// Pair keys have a different Get implementation, so test a pair and a non-pair

// Lvalue tuples with value elements
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int      >&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int      >&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int      >&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int      >&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int      >&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int      >&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int      >&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int, char>&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int, char>&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int, char>&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int, char>&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int, char>&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int, char>&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int, char>&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int, char>&>().Get<0>()), const volatile int&>::value, "");

// Rvalue tuples with value elements
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int      >&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int      >&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int      >&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int      >&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int      >&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int      >&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int      >&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int, char>&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int, char>&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int, char>&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int, char>&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int, char>&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int, char>&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int, char>&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int, char>&&>().Get<0>()), const volatile int&&>::value, "");

// Lvalue tuples with lvalue reference elements
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int&      >&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int&      >&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int&      >&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int&      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int&      >&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int&      >&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int&      >&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int&      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int&      >&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int&      >&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int&      >&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int&      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int&      >&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int&      >&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int&      >&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int&      >&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int&, char>&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int&, char>&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int&, char>&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int&, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int&, char>&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int&, char>&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int&, char>&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int&, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int&, char>&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int&, char>&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int&, char>&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int&, char>&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int&, char>&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int&, char>&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int&, char>&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int&, char>&>().Get<0>()), const volatile int&>::value, "");

// Rvalue tuples with lvalue reference elements
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int&      >&&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int&      >&&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int&      >&&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int&      >&&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int&      >&&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int&      >&&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int&      >&&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int&      >&&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int&      >&&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int&      >&&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int&      >&&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int&      >&&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int&      >&&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int&      >&&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int&      >&&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int&      >&&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int&, char>&&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int&, char>&&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int&, char>&&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int&, char>&&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int&, char>&&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int&, char>&&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int&, char>&&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int&, char>&&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int&, char>&&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int&, char>&&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int&, char>&&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int&, char>&&>().Get<0>()), const volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int&, char>&&>().Get<0>()),                int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int&, char>&&>().Get<0>()), const          int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int&, char>&&>().Get<0>()),       volatile int&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int&, char>&&>().Get<0>()), const volatile int&>::value, "");

// Lvalue tuples with rvalue reference elements
// Note that this behavior differs from normal member access in a struct.
// An rvalue reference member of a struct used in an expression is treated as an lvalue.
// An rvalue reference member of a tuple used in an expression is treated as an rvalue.
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int&&      >&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int&&      >&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int&&      >&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int&&      >&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int&&      >&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int&&      >&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int&&      >&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int&&      >&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int&&      >&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int&&      >&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int&&      >&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int&&      >&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int&&      >&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int&&      >&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int&&      >&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int&&      >&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int&&, char>&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int&&, char>&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int&&, char>&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int&&, char>&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int&&, char>&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int&&, char>&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int&&, char>&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int&&, char>&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int&&, char>&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int&&, char>&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int&&, char>&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int&&, char>&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int&&, char>&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int&&, char>&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int&&, char>&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int&&, char>&>().Get<0>()), const volatile int&&>::value, "");

// Rvalue tuples with rvalue reference elements
// Note that this behavior differs from normal member access in a struct.
// An rvalue reference member of a struct used in an expression is treated as an lvalue.
// An rvalue reference member of a tuple used in an expression is treated as an rvalue.
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int&&      >&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int&&      >&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int&&      >&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int&&      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int&&      >&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int&&      >&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int&&      >&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int&&      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int&&      >&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int&&      >&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int&&      >&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int&&      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int&&      >&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int&&      >&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int&&      >&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int&&      >&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<               int&&, char>&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const          int&&, char>&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<      volatile int&&, char>&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<               TTuple<const volatile int&&, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<               int&&, char>&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const          int&&, char>&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<      volatile int&&, char>&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const          TTuple<const volatile int&&, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<               int&&, char>&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const          int&&, char>&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<      volatile int&&, char>&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<      volatile TTuple<const volatile int&&, char>&&>().Get<0>()), const volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<               int&&, char>&&>().Get<0>()),                int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const          int&&, char>&&>().Get<0>()), const          int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<      volatile int&&, char>&&>().Get<0>()),       volatile int&&>::value, "");
static_assert(std::is_same<decltype(DeclVal<const volatile TTuple<const volatile int&&, char>&&>().Get<0>()), const volatile int&&>::value, "");
