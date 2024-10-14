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

#ifndef _NIMIQ_ERROR_MACROS_H_
#define _NIMIQ_ERROR_MACROS_H_

#include "ledger_assert.h" // For LEDGER_ASSERT in ON_ERROR, ERROR_TO_SW, and files that include error_macros.h

#include "constants.h" // For error_t and sw_t
#include "utility_macros.h" // For VA_ARGS_* and DEBUG_EMIT macros

#if defined(NIMIQ_DEBUG) && NIMIQ_DEBUG
#include <string.h> // for strstr used in debug expression sanity check in ON_ERROR
#endif

/**
 * Return from the current function with an error code and print debug messages.
 *
 * error: the error code to return. Expressions of type void are not allowed.
 * message: an error message to print in debug builds. Note that in contrast to the conditional ON_ERROR and its
 *     derivative macros like RETURN_ON_ERROR, it is mandatory here, under the assumption that the non-conditional
 *     error is returned where the error originates, while ON_ERROR and its derivative macros are oftentimes used for
 *     forwarding an error.
 * Optional parameters: optional additional parameters of the message to pass to PRINTF.
 */
#define RETURN_ERROR(error, message, ...) \
    do { \
        PRINTF("    !!! Error 0x%02x returned in %s (file %s, line %d)\n", error, __func__, __FILE__, __LINE__); \
        PRINTF("        "); \
        PRINTF(message __VA_OPT__(,) __VA_ARGS__); \
        return (error); \
    } while(0)

/**
 * Conditionally run specified statements and print debug messages, if an expression results in an error.
 *
 * expression: an expression to evaluate. Non-zero return codes are considered errors.
 * statements: statements to run on error. The received error is available as variable "received_error", which is
 *     independent of any custom error code passed. On the other hand, variable "error" is either the received error, or
 *     a custom error code if passed. The type of error depends on the type of the passed custom error.
 * First optional parameter: a custom error code to assign to variable "error" can be passed. Expressions of type void
 *     are not allowed. To automatically convert an error of type error_t to a status word of type sw_t, ERROR_TO_SW()
 *     can be used.
 * Second and following optional parameters: a debug message and parameters to pass to PRINTF.
 *
 * Note: while this macro looks like it's injecting a lot of code (which would bloat the app binary), all the PRINTFs
 * and the sanity check are not included in production builds. Additionally, the compiler might be able to optimize
 * variables received_error and error away (both are const, and if a custom error is passed, both are set and read only
 * once (depending on statements), and if no custom error is passed, error will always get the same value as
 * received_error).
 */
#define ON_ERROR(expression, statements, ...) \
    do { \
        /* In debug builds, sanity check expressions for improper use of logical expressions, where their result, */ \
        /* which is 0 or 1, is assigned and used as received error, instead of the actually received error which */ \
        /* gets lost in the logical expression. */ \
        DEBUG_EMIT({ \
            bool is_received_error_used = ( \
                /* If no custom error is set, received_error is used in any case as it's assigned to error. */ \
                VA_ARGS_IF_EMPTY_EMIT( \
                    true, \
                    VA_ARGS_PICK_FIRST(__VA_ARGS__) \
                ) \
                /* If a custom error is set, check whether it or the statements use received_error, which might */ \
                /* also be via ERROR_TO_SW(). */ \
                VA_ARGS_IF_NOT_EMPTY_EMIT( \
                    strstr(#__VA_ARGS__, "received_error") || strstr(#__VA_ARGS__, "ERROR_TO_SW()") \
                    || strstr(#statements, "received_error") || strstr(#statements, "ERROR_TO_SW()"), \
                    VA_ARGS_PICK_FIRST(__VA_ARGS__) \
                ) \
            ); \
            /* Assert that the received error is either not used, or not the result of a logical expression. */ \
            LEDGER_ASSERT( \
                !is_received_error_used || !strstr(#expression, "||"), \
                "Using result of logical expression %s as error code", \
                #expression \
            ); \
        }) \
        const error_t received_error = (expression); \
        if (received_error) { \
            PRINTF("    !!! Error 0x%02x returned by %s in %s (file %s, line %d)\n", received_error, #expression, \
                __func__, __FILE__, __LINE__); \
            /* Print custom debug message if passed as second and following optional arguments. */ \
            VA_ARGS_IF_NOT_EMPTY_EMIT( \
                { PRINTF("        "); PRINTF(VA_ARGS_OMIT_FIRST(__VA_ARGS__)); }, \
                VA_ARGS_OMIT_FIRST(__VA_ARGS__) \
            ) \
            /* Assign received_error or, if passed, custom error to error. */ \
            /* No curly braces around the assignment, to avoid it being scoped to a separate scope. */ \
            /* Silence warning regarding unused variable, as the statements are not required to use variable error, */ \
            /* which should then be optimized away by the compiler. */ \
            _Pragma("GCC diagnostic push") \
            _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") \
            VA_ARGS_IF_EMPTY_EMIT( \
                const error_t error = received_error;, \
                VA_ARGS_PICK_FIRST(__VA_ARGS__) \
            ) \
            VA_ARGS_IF_NOT_EMPTY_EMIT( \
                const typeof(VA_ARGS_PICK_FIRST(__VA_ARGS__)) error = (VA_ARGS_PICK_FIRST(__VA_ARGS__));, \
                VA_ARGS_PICK_FIRST(__VA_ARGS__) \
            ) \
            _Pragma("GCC diagnostic pop") \
            /* Run specified statements. */ \
            { statements } \
        } \
    } while(0)

/**
 * Conditionally return from the current function, if an expression results in an error.
 *
 * See ON_ERROR for parameter descriptions.
 */
#define RETURN_ON_ERROR(expression, ...) \
    ON_ERROR(expression, { return error; }, __VA_ARGS__)

/**
 * Conditionally jump to a goto label, if an expression results in an error.
 *
 * expression: as in ON_ERROR
 * goto_label: the goto label to jump to.
 * First optional parameter: a variable name to assign the error to.
 * Second, third and following optional parameters: are forwarded to ON_ERROR. See there for parameter descriptions.
 */
#define GOTO_ON_ERROR(expression, goto_label, ...) \
    ON_ERROR( \
        expression, \
        { \
            /* Assign error to specified variable name, if such was specified. */ \
            VA_ARGS_IF_NOT_EMPTY_EMIT( \
                { VA_ARGS_PICK_FIRST(__VA_ARGS__) = error; }, \
                VA_ARGS_PICK_FIRST(__VA_ARGS__) \
            ) \
            goto goto_label; \
        }, \
        VA_ARGS_OMIT_FIRST(__VA_ARGS__) \
    )

/**
 * For usage as custom error in ON_ERROR and derivative macros to automatically convert an error of type error_t to a
 * status word of type sw_t.
 */
sw_t error_to_sw(error_t error);
#define ERROR_TO_SW() error_to_sw(received_error)

// Decorators

#define UNUSED_PARAMETER(parameter) __attribute__((unused)) parameter

// FALL_THROUGH and WARN_UNUSED_RESULT are copied over from ledger-secure-sdk's decorators.h, such that they're known to
// the IDE, even if the ledger-secure-sdk source is not available to it.

#ifndef FALL_THROUGH
#if defined(__GNUC__) && __GNUC__ >= 7 || defined(__clang__) && __clang_major__ >= 12
#define FALL_THROUGH __attribute__((fallthrough))
#else
#define FALL_THROUGH ((void) 0)
#endif
#endif

#ifndef WARN_UNUSED_RESULT
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#endif

#endif // _NIMIQ_ERROR_MACROS_H_
