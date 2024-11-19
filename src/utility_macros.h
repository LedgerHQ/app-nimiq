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

#define STRUCT_MEMBER_SIZE(struct_type, member) (sizeof(((struct_type *) NULL)->member))

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

#define POINTER_SIZE (sizeof(void *))
/**
 * Get the offset of a pointer from 32bit based memory alignment. Memory access of pointers to data types larger than
 * one byte need to be aligned to 32bit words / POINTER_SIZE (or 16bit for shorts, which we don't handle separately here
 * and always use 32bit alignment), as required by Ledger Nano S's hardware, and for other devices by the compiler flags
 * (presumably, as other devices do in fact support non-aligned memory access, however without memory alignment, the app
 * froze on the unaligned memset in review_entries_launch_use_case_review since using the SDK's Makefile.standard_app
 * for building as of 5542f4b6, but not before that change. I did not further investigate, which exact flag introduced
 * the issue, or whether maybe the accessed pointer was 32bit aligned before that change by coincidence). Note that the
 * speculos emulator does not enforce any memory alignment, which is why related issues do not show there.
 * In any case, it's a good idea to conform to memory alignment.
 */
#define POINTER_MEMORY_ALIGNMENT_OFFSET(pointer) (((uintptr_t) pointer) % POINTER_SIZE)

/**
 * Declare a pointer of given type within G_io_apdu_buffer for temporary use, while G_io_apdu_buffer is not otherwise
 * used or accessed. The pointer is ensured to be memory aligned, see POINTER_MEMORY_ALIGNMENT_OFFSET, and its type is
 * checked to fit G_io_apdu_buffer.
 */
#define DECLARE_TEMPORARY_G_IO_APDU_BUFFER_POINTER(pointer_type, variable_name) \
    _Static_assert( \
        /* The memory alignment offset can not be statically computed at compile time, therefore we assume for the */ \
        /* static assertion that the largest padding POINTER_SIZE - 1 has to be applied. */ \
        sizeof(*((pointer_type) NULL)) <= sizeof(G_io_apdu_buffer) - (POINTER_SIZE - 1), \
        "Pointer type does not fit G_io_apdu_buffer\n" \
    ); \
    pointer_type variable_name = (pointer_type) ( \
        G_io_apdu_buffer + \
        ( \
            POINTER_MEMORY_ALIGNMENT_OFFSET(G_io_apdu_buffer) \
                ? POINTER_SIZE - POINTER_MEMORY_ALIGNMENT_OFFSET(G_io_apdu_buffer) \
                : 0 \
        ) \
    );

#endif // _NIMIQ_UTILITY_MACROS_H_
