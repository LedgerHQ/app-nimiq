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

#ifdef TEST
#include <stdio.h>
#define PRINTF(msg, arg) printf(msg, arg)
#define PIC(code) code
#define TARGET_NANOS 1
#else
#include "os_print.h"
#endif // TEST

#if defined(NIMIQ_DEBUG) && NIMIQ_DEBUG
#include <string.h> // for strstr used in debug expression sanity check in ON_ERROR
void app_exit(); // used in debug expression sanity check in ON_ERROR
#define DEBUG_EMIT(...) __VA_ARGS__
#else
#define DEBUG_EMIT(...) /* drop contents */
#endif

#define VA_ARGS(...) __VA_ARGS__
#define VA_ARGS_DROP(...) /* drop contents */
#define VA_ARGS_PICK_FIRST(first, ...) first
#define VA_ARGS_OMIT_FIRST(first, ...) __VA_ARGS__
#define VA_ARGS_IF_NOT_EMPTY_EMIT(content, ...) __VA_OPT__(content)
#define VA_ARGS_IF_EMPTY_EMIT(content, ...) VA_ARGS##__VA_OPT__(_DROP)(content)

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
            if ( \
                ( \
                    /* The statements use received_error. Can also be via ERROR_TO_SW(). */ \
                    (strstr(#statements, "received_error") || strstr(#statements, "ERROR_TO_SW()")) \
                    || \
                    /* Or a custom error is set which uses received_error. Can also be via ERROR_TO_SW(). */ \
                    VA_ARGS_IF_NOT_EMPTY_EMIT( \
                        (strstr(#__VA_ARGS__, "received_error") || strstr(#__VA_ARGS__, "ERROR_TO_SW()")), \
                        VA_ARGS_PICK_FIRST(__VA_ARGS__) \
                    ) \
                    /* Or no custom error is set, in which case received_error is used as the error. */ \
                    VA_ARGS_IF_EMPTY_EMIT( \
                        true, \
                        VA_ARGS_PICK_FIRST(__VA_ARGS__) \
                    ) \
                ) \
                /* And it's a logical expression that uses || */ \
                && (strstr(#expression, "||") /* || strstr(#expression, "&&") */) \
            ) { \
                PRINTF( \
                    "    !!! Error: using result of logical expression %s as error code, which is 0 or 1.\n", \
                    #expression \
                ); \
                app_exit(); \
            } \
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
#define ERROR_TO_SW() \
    ( \
        received_error == ERROR_READ || received_error == ERROR_INVALID_LENGTH ? SW_WRONG_DATA_LENGTH \
        : received_error == ERROR_INCORRECT_DATA ? SW_INCORRECT_DATA \
        : received_error == ERROR_NOT_SUPPORTED ? SW_NOT_SUPPORTED \
        : received_error == ERROR_CRYPTOGRAPHY ? SW_CRYPTOGRAPHY_FAIL \
        : received_error == ERROR_NONE ? SW_OK \
        : /* ERROR_TRUE, ERROR_UNEXPECTED or an error which is incorrectly missing here */ SW_BAD_STATE \
    )

// WARN_UNUSED_RESULT is copied over from ledger-secure-sdk's decorators.h, such that it's known to the IDE, even if the
// ledger-secure-sdk source is not available to it.

#ifndef WARN_UNUSED_RESULT
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#endif

#endif // _NIMIQ_ERROR_MACROS_H_
