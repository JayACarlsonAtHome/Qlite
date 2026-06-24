// File:    Setup/include/Qlite/Setup/contracts.hpp
// Purpose: Qlite contracts shim — pre/post/assert with a stable call-site spelling
//          that lights up NATIVE ISO C++26 contracts (P2900) when the compiler
//          provides them, and falls back to runtime fail-fast checks otherwise.
//
// Three modes, selected automatically:
//   (A) NATIVE   : __cpp_contracts defined (e.g. gcc16 -std=c++26 -fcontracts).
//                  Macros expand to `contract_assert(expr)` — a first-class C++26
//                  contract assertion. Its evaluation semantic (ignore/observe/
//                  enforce) is chosen by the build via -fcontract-evaluation-semantic.
//   (B) RUNTIME  : contracts enabled but the compiler has no native contracts
//                  (e.g. clang today). Macros do a runtime check + fail-fast abort.
//   (C) DISABLED : QLITE_CONTRACTS_ENABLED == 0. Macros are zero-cost no-ops.
//
// WHY MACROS (and not native pre()/post() at the call sites): native pre()/post()
// must appear on the FUNCTION DECLARATOR; a macro cannot inject text there. So the
// portable shim maps QLITE_PRE/POST to `contract_assert` placed in the body (a
// precondition checked at entry / a postcondition checked at the point of use) —
// semantically equivalent to the runtime form, and genuinely native under (A).
// Functions that want true declarator-level pre()/post() use the native syntax
// directly, guarded by `#if defined(__cpp_contracts)` (see contracts_enforce_test).
#pragma once

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace contracts {

// A contract violation aborts after reporting — a fail-fast safety net, deterministic
// for tests. Used only by the RUNTIME fallback (B); NATIVE mode (A) reports/terminates
// via the language contract-violation handler per the chosen evaluation semantic.
[[noreturn]] inline void
violation(const char* kind, const char* expr,
          const std::source_location loc = std::source_location::current()) {
    std::fprintf(stderr, "Qlite contract %s violated: (%s) at %s:%u in %s\n",
                 kind, expr, loc.file_name(),
                 static_cast<unsigned>(loc.line()), loc.function_name());
    std::abort();
}

} // namespace contracts

// QLITE_CONTRACTS_ENABLED: 1 (default) = checks active; define to 0
// (e.g. -DQLITE_CONTRACTS_ENABLED=0 for a release/perf build) to compile the checks
// out to zero cost. Default behaviour is unchanged unless explicitly disabled.
#ifndef QLITE_CONTRACTS_ENABLED
#  define QLITE_CONTRACTS_ENABLED 1
#endif

#if QLITE_CONTRACTS_ENABLED

#  if defined(__cpp_contracts)
// ---- (A) NATIVE C++26 contracts (P2900) -----------------------------------------
// pre/post are point-checked via contract_assert (see header note). assert maps 1:1.
// QLITE_CONTRACTS_NATIVE is exposed so tests/code can branch on the active mode.
#    define QLITE_CONTRACTS_NATIVE 1

#    define QLITE_ASSERT(expr) contract_assert(expr)
#    define QLITE_PRE(expr)    contract_assert(expr)
#    define QLITE_POST(expr)   contract_assert(expr)

#  else
// ---- (B) RUNTIME fallback (no native contracts) ---------------------------------
#    define QLITE_CONTRACTS_NATIVE 0

#    define QLITE_ASSERT(expr) \
        ((expr) ? (void)0 : ::contracts::violation("assert", #expr))

// Precondition: place at the top of a function body.
#    define QLITE_PRE(expr) \
        ((expr) ? (void)0 : ::contracts::violation("precondition", #expr))

// Postcondition (point check). True return-value postconditions use native pre/post
// or an explicit scope guard; this checks the condition at the point of use.
#    define QLITE_POST(expr) \
        ((expr) ? (void)0 : ::contracts::violation("postcondition", #expr))

#  endif // __cpp_contracts

#else // ---- (C) contracts disabled — zero-cost no-ops (release/perf builds) -------

#  define QLITE_CONTRACTS_NATIVE 0
#  define QLITE_ASSERT(expr) ((void)0)
#  define QLITE_PRE(expr)    ((void)0)
#  define QLITE_POST(expr)   ((void)0)

#endif
