/*******************************************************************************
 *   Ledger Nimiq App
 *   (c) 2018 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#ifndef _NIMIQ_UTILITY_MACROS_H_
#define _NIMIQ_UTILITY_MACROS_H_

#if defined(TEST) && TEST
#define PRINTF(...) printf(__VA_ARGS__)
#define LEDGER_ASSERT(test, ...) assert(test)
#define PIC(code) code
#define TARGET_NANOS 1
#endif // TEST

#if defined(NIMIQ_DEBUG) && NIMIQ_DEBUG
#define DEBUG_EMIT(...) __VA_ARGS__
#else
#define DEBUG_EMIT(...) /* drop contents */
#endif // NIMIQ_DEBUG

#define VA_ARGS(...) __VA_ARGS__
#define VA_ARGS_DROP(...) /* drop contents */
#define VA_ARGS_PICK_FIRST(first, ...) first
#define VA_ARGS_OMIT_FIRST(first, ...) __VA_ARGS__
#define VA_ARGS_IF_NOT_EMPTY_EMIT(content, ...) __VA_OPT__(content)
#define VA_ARGS_IF_EMPTY_EMIT(content, ...) VA_ARGS##__VA_OPT__(_DROP)(content)

#ifndef MIN
/**
 * Get the minimum of two values. This should only be used with constant expressions to avoid an expression with runtime
 * cost or side effects running twice.
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif // MIN

#ifndef MAX
/**
 * Get the maximum of two values. This should only be used with constant expressions to avoid an expression with runtime
 * cost or side effects running twice.
 */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif // MAX

#define STRING_LENGTH_WITH_SUFFIX(length_a, suffix) (length_a - /* string terminator of string a */ 1 + sizeof(suffix))

#define STRUCT_MEMBER_SIZE(struct_type, member) (sizeof(((struct_type *) 0)->member))

/**
 * Copy data of fixed size to a destination of fixed size. This macro performs a static assertion at compile time, that
 * the destination fits the data.
 * In order for the static assertion to work, the sizes of source and destination have to be known at compile time, e.g.
 * by being a buffer of fixed size or a constant string (for which sizeof includes the string terminator).
 */
#define COPY_FIXED_SIZE(destination, source) \
    ({ \
        _Static_assert( \
            sizeof(destination) >= sizeof(source), \
            "Copy destination too short.\n" \
        ); \
        memcpy(destination, source, sizeof(source)); \
    })

#endif // _NIMIQ_UTILITY_MACROS_H_
