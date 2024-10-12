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
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// From Ledger SDK
#include "lcx_blake2.h"

#include "nimiq_utils.h"
#include "nimiq_staking_utils.h"
#include "base32.h"

// The maximum allowed NIM amount. Equal to JavaScript's Number.MAX_SAFE_INTEGER, see Coin::MAX_SAFE_VALUE in
// primitives/src/coin.rs in core-rs-albatross.
#define MAX_SAFE_INTEGER 9007199254740991

#define AMOUNT_MAX_SIZE 17

WARN_UNUSED_RESULT
error_t iban_check(char in[32], char *check) {
    unsigned int counter = 0;
    unsigned int offset = 0;
    unsigned int modulo = 0;

    int partial_uint = 0;

    char address[36] = { 0 };
    char total_number[71] = { 0 };
    char partial_number[10] = { 0 };

    // According to IBAN standard, "NQ00" needs to be appended to the original address to calculate the checksum
    strncpy(&address[0], &in[0], 32);
    address[32] = 'N';
    address[33] = 'Q';
    address[34] = '0';
    address[35] = '0';

    // Convert the address to a number-only string
    for (unsigned int i = 0; i < 36; i++) {
        LEDGER_ASSERT(
            // This assertion should hold, given the number of iterations and the fact that at least two address chars
            // are digits, which increase the counter only by one.
            counter < 70,
            "Overflow in iban check"
        );
        if (address[i] >= 48 && address[i] <= 57) {
            total_number[counter++] = address[i];
        } else if (address[i] >= 65 && address[i] <= 90) {
            snprintf(&total_number[counter++], 3, "%d", address[i] - 55);
            // Letters convert to a two digit number, increase the counter one more time
            counter++;
        } else if (address[i] >= 97 && address[i] <= 122) {
            snprintf(&total_number[counter++], 3, "%d", address[i] - 87);
            // Letters convert to a two digit number, increase the counter one more time
            counter++;
        } else {
            RETURN_ERROR(
                ERROR_INCORRECT_DATA,
                "Invalid ascii code in iban check\n"
            );
        }
    }

    // Compute modulo-97 on the resulting number (do it in 32-bit pieces)
    counter = 0;
    for (unsigned int i = 0; i < 10; i++) {
        strncpy(&partial_number[offset], &total_number[counter], 9 - offset);
        counter += 9 - offset;
        for (unsigned int j = 0; j < 9; j++) {
            if (partial_number[j] != '\0') {
                partial_uint = 10 * partial_uint + (partial_number[j] - '0');
            } else {
                break;
            }
        }

        modulo = partial_uint % 97;
        snprintf(partial_number, 3, "%02d", modulo);
        partial_uint = 0;
        offset = 2;
    }

    snprintf(check, 3, "%02d", 98 - modulo);

    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t print_address(uint8_t *in, char *out) {
    unsigned int counter = 4;
    char after_base32[33] = { 0 }; // includes one extra byte for the string terminator

    RETURN_ON_ERROR(
        base32_encode(in, 20, after_base32, sizeof(after_base32))
    );
    RETURN_ON_ERROR(
        iban_check(after_base32, &out[2])
    );

    out[0] = 'N';
    out[1] = 'Q';

    // Insert spaces for readability
    for (unsigned int i = 0; i < 8; i++) {
        out[counter++] = ' ';
        memcpy(&out[counter], &after_base32[i*4], 4);
        counter += 4;
    }

    // Make sure that the address string is always null-terminated
    out[44] = '\0';

    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t public_key_to_address(uint8_t *in, uint8_t *out) {
    // See lcx_blake2.h and lcx_hash.h in Ledger sdk
    unsigned char blake2b_hash[32];
    cx_blake2b_t blake2b_context;
    RETURN_ON_ERROR(
        cx_blake2b_init_no_throw(&blake2b_context, /* hash length in bits */ 256)
        || cx_hash_no_throw(&blake2b_context.header, CX_LAST, in, 32, blake2b_hash, 32),
        ERROR_CRYPTOGRAPHY
    );
    memmove(out, blake2b_hash, 20); // the first 20 bytes of the hash are the Nimiq address
    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t print_public_key_as_address(uint8_t *in, char *out) {
    uint8_t address[20];
    RETURN_ON_ERROR(
        public_key_to_address(in, address)
    );
    RETURN_ON_ERROR(
        print_address(address, out)
    );
    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t print_hex(uint8_t *data, uint16_t data_length, char *out, uint16_t out_length) {
    RETURN_ON_ERROR(
        // Check that it fits 2 hex chars per byte + string terminator.
        out_length < data_length * 2 + 1,
        ERROR_INVALID_LENGTH,
        "Out buffer too small to fit hex\n"
    );
    // not using Ledger's proprietary %.*H snprintf format, as it's non-standard
    // (see https://github.com/LedgerHQ/app-bitcoin/pull/200/files)
    for (uint16_t i = 0; i < data_length; i++) {
        snprintf(out + i * 2, /* 2 hex chars + string terminator */ 3, "%02X", data[i]);
    }
    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t parse_amount(uint64_t amount, const char * const asset, char *out) {
    char buffer[AMOUNT_MAX_SIZE]; // Notably this is not a \0 terminated string because the added dot occupies one spot.
    uint64_t dVal = amount;
    int i, j;

    // If the amount can't be represented safely in JavaScript, signal an error
    RETURN_ON_ERROR(
        amount > MAX_SAFE_INTEGER,
        ERROR_INCORRECT_DATA,
        "Invalid amount\n"
    );

    memset(buffer, 0, AMOUNT_MAX_SIZE);
    for (i = 0; dVal > 0 || i < 7; i++) {
        if (dVal > 0) {
            buffer[i] = (dVal % 10) + '0';
            dVal /= 10;
        } else {
            buffer[i] = '0';
        }
        if (i == 4) { // satoshis to nim: 1 nim = 100 000 satoshis
            i += 1;
            buffer[i] = '.';
        }
        LEDGER_ASSERT(
            // This assertion should hold, if AMOUNT_MAX_SIZE is defined correctly and the loop works correctly.
            i < AMOUNT_MAX_SIZE,
            "Overflow in parse_amount"
        );
    }
    // reverse order
    for (i -= 1, j = 0; i >= 0 && j < AMOUNT_MAX_SIZE-1; i--, j++) {
        out[j] = buffer[i];
    }
    // strip trailing 0s
    for (j -= 1; j > 0; j--) {
        if (out[j] != '0') break;
    }
    j += 1;

    // strip trailing .
    if (out[j-1] == '.') j -= 1;

    if (asset) {
        RETURN_ON_ERROR(
            j + /* added space */ 1 + strlen(asset) + /* string terminator */ 1 > STRING_LENGTH_NIM_AMOUNT,
            ERROR_INCORRECT_DATA,
            "Result is longer than STRING_LENGTH_NIM_AMOUNT\n"
        );
        // qualify amount
        out[j++] = ' ';
        strcpy(out + j, asset);
        out[j+strlen(asset)] = '\0';
    } else {
        out[j] = '\0';
    }

    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t parse_network_id(transaction_version_t version, uint8_t network_id, char *out) {
    if (network_id == (version == TRANSACTION_VERSION_LEGACY ? 42 : 24)) {
        strcpy(out, "Main");
    } else if (network_id == (version == TRANSACTION_VERSION_LEGACY ? 1 : 5)) {
        strcpy(out, "Test");
    } else if (network_id == (version == TRANSACTION_VERSION_LEGACY ? 2 : 6)) {
        strcpy(out, "Development");
    } else if (network_id == (version == TRANSACTION_VERSION_LEGACY ? 3 : 7)) {
        strcpy(out, "Bounty");
    } else {
        RETURN_ERROR(
            ERROR_INCORRECT_DATA,
            "Invalid network\n"
        );
    }
    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t parse_normal_tx_data(uint8_t *data, uint16_t data_length, tx_data_normal_or_staking_outgoing_t *out,
    bool *out_is_cashlink) {
    // initiate with empty string / empty data
    strcpy(out->extra_data_label, "");
    strcpy(out->extra_data, "");
    *out_is_cashlink = false;

    // Make sure we don't get called with more data than we can fit on the extra data field.
    RETURN_ON_ERROR(
        data_length > LENGTH_NORMAL_TX_DATA_MAX,
        ERROR_INVALID_LENGTH,
        "Extra data too long\n"
    );

    if (data == NULL || data_length == 0) {
        return ERROR_NONE;
    }

    // Check if it's the special Cashlink data which we do not want to display.
    if (data_length == CASHLINK_MAGIC_NUMBER_LENGTH
        && memcmp(data, CASHLINK_MAGIC_NUMBER, CASHLINK_MAGIC_NUMBER_LENGTH) == 0) {
        *out_is_cashlink = true;
        return ERROR_NONE;
    }
    // Check if there is any non-printable ASCII characters
    if (!is_printable_ascii(data, data_length)) {
        strcpy(out->extra_data_label, "Data Hex");
        RETURN_ON_ERROR(
            print_hex(data, data_length, out->extra_data, sizeof(out->extra_data))
        );
        return ERROR_NONE;
    }

    // If there is not, copy the string to be displayed
    strcpy(out->extra_data_label, "Data");
    strncpy(out->extra_data, (char *) data, data_length);
    out->extra_data[data_length] = '\0'; // Make sure the string is terminated

    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t parse_htlc_creation_data(transaction_version_t version, uint8_t *data, uint16_t data_length, uint8_t *sender,
    account_type_t sender_type, uint32_t validity_start_height, tx_data_htlc_creation_t *out) {
    RETURN_ON_ERROR(
        version != TRANSACTION_VERSION_LEGACY,
        ERROR_NOT_SUPPORTED,
        "HTLC creation not implemented yet for Albatross\n"
    );

    // Process refund address
    uint8_t *refund_address_bytes;
    RETURN_ON_ERROR(
        !read_sub_buffer(20, &data, &data_length, &refund_address_bytes),
        ERROR_READ
    );
    RETURN_ON_ERROR(
        print_address(refund_address_bytes, out->refund_address)
    );
    out->is_refund_address_sender_address = memcmp(refund_address_bytes, sender, 20) == 0;

    RETURN_ON_ERROR(
        // Although the refund address can be any address, specifying a contract as refund address is not recommendable
        // because for the contract address there is no key that could create the required signature for the htlc refund
        // proof. Protect the user from this scenario, as far as we can detect it.
        out->is_refund_address_sender_address && sender_type != ACCOUNT_TYPE_BASIC,
        ERROR_INCORRECT_DATA,
        "HTLC refund address should not be a contract\n"
    );

    // Process redeem address
    uint8_t *redeem_address_bytes;
    RETURN_ON_ERROR(
        !read_sub_buffer(20, &data, &data_length, &redeem_address_bytes),
        ERROR_READ
    );
    RETURN_ON_ERROR(
        print_address(redeem_address_bytes, out->redeem_address)
    );

    // Process hash algorithm
    hash_algorithm_t hash_algorithm;
    _Static_assert(
        sizeof(hash_algorithm) == 1,
        "hash_algorithm has more than one byte. Need to take endianness into account when reading into a u8 pointer.\n"
    );
    RETURN_ON_ERROR(
        !read_u8(&data, &data_length, &hash_algorithm),
        ERROR_READ
    );
    switch (hash_algorithm) {
        case HASH_ALGORITHM_BLAKE2B:
            strcpy(out->hash_algorithm, "BLAKE2b");
            break;
        case HASH_ALGORITHM_SHA256:
            strcpy(out->hash_algorithm, "SHA-256");
            break;
        case HASH_ALGORITHM_SHA512:
            strcpy(out->hash_algorithm, "SHA-512");
            break;
        default:
            // Invalid hash algorithm. Notably, ARGON2d is blacklisted for HTLCs.
            RETURN_ERROR(
                ERROR_INCORRECT_DATA,
                "Invalid hash algorithm or blacklisted ARGON2d\n"
            );
    }
    out->is_using_sha256 = hash_algorithm == HASH_ALGORITHM_SHA256;

    // Process hash root
    uint8_t hash_size = hash_algorithm == HASH_ALGORITHM_SHA512 ? 64 : 32;
    uint8_t *hash_bytes;
    RETURN_ON_ERROR(
        !read_sub_buffer(hash_size, &data, &data_length, &hash_bytes),
        ERROR_READ
    );
    // Print the hash as hex.
    RETURN_ON_ERROR(
        print_hex(hash_bytes, hash_size, out->hash_root, sizeof(out->hash_root))
    );

    // Process hash count
    uint8_t hash_count;
    RETURN_ON_ERROR(
        !read_u8(&data, &data_length, &hash_count),
        ERROR_READ
    );
    snprintf(out->hash_count, sizeof(out->hash_count), "%u", hash_count);

    // Process timeout
    uint32_t timeout;
    RETURN_ON_ERROR(
        !read_u32(&data, &data_length, &timeout),
        ERROR_READ
    );
    // note: not %lu (for unsigned long int) because int is already 32bit on ledgers (see "Memory Alignment" in Ledger
    // docu), additionally Ledger's own implementation of sprintf does not support %lu (see os_printf.c)
    snprintf(out->timeout, sizeof(out->timeout), "%u", timeout);
    out->is_timing_out_soon = timeout < validity_start_height
        || timeout - validity_start_height < HTLC_TIMEOUT_SOON_THRESHOLD;

    RETURN_ON_ERROR(
        data_length != 0,
        ERROR_INVALID_LENGTH,
        "Htlc data too long\n"
    );

    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t parse_vesting_creation_data(transaction_version_t version, uint8_t *data, uint16_t data_length, uint8_t *sender,
    account_type_t sender_type, uint64_t tx_amount, tx_data_vesting_creation_t *out) {
    RETURN_ON_ERROR(
        version != TRANSACTION_VERSION_LEGACY,
        ERROR_NOT_SUPPORTED,
        "Vesting creation not implemented yet for Albatross\n"
    );

    // Note that this method could be quite heavy on the stack (depending on how well the compiler optimizes it). It
    // could be refactored by allocating less variables by printing them directly or re-using variables, but at the cost
    // of less readable code.

    // Process owner address
    uint8_t *owner_address_bytes;
    RETURN_ON_ERROR(
        !read_sub_buffer(20, &data, &data_length, &owner_address_bytes),
        ERROR_READ
    );
    RETURN_ON_ERROR(
        print_address(owner_address_bytes, out->owner_address)
    );
    out->is_owner_address_sender_address = memcmp(owner_address_bytes, sender, 20) == 0;

    RETURN_ON_ERROR(
        // Although the owner address can be any address, specifying a contract as owner is not recommendable because
        // for the contract address there is no key that could create the required signature for the vesting proof.
        // Protect the user from this scenario, as far as we can detect it.
        out->is_owner_address_sender_address && sender_type != ACCOUNT_TYPE_BASIC,
        ERROR_INCORRECT_DATA,
        "Vesting owner address should not be a contract\n"
    );

    // Read vesting parameters from data, depending on what is specified, and assign default values otherwise
    uint32_t start_block = 0;
    uint32_t step_block_count;
    uint64_t step_amount = tx_amount;
    uint64_t total_locked_amount = tx_amount;
    if (data_length == 4) {
        RETURN_ON_ERROR(
            !read_u32(&data, &data_length, &step_block_count),
            ERROR_READ
        );
    } else {
        RETURN_ON_ERROR(
            !read_u32(&data, &data_length, &start_block)
            || !read_u32(&data, &data_length, &step_block_count)
            || !read_u64(&data, &data_length, &step_amount),
            ERROR_READ
        );

        if (data_length == 8) {
            RETURN_ON_ERROR(
                !read_u64(&data, &data_length, &total_locked_amount),
                ERROR_READ
            );
        }
    }

    RETURN_ON_ERROR(
        data_length != 0,
        ERROR_INVALID_LENGTH,
        "Vesting data too long\n"
    );

    // Translate into more user friendly information for display
    uint32_t step_count;
    uint32_t period;
    uint32_t first_step_block_count;
    uint64_t first_step_amount;
    uint64_t last_step_amount;
    uint64_t pre_vested_amount;
    uint64_t helper_uint64;
    if (!step_block_count || !step_amount) {
        // Special case in vesting contracts where all funds are immediately vested and total_locked_amount is ignored.
        // Set / overwrite all parameters accordingly. Also checked separately to avoid division by zero in other cases.
        start_block = 0;
        step_block_count = 0;
        step_amount = tx_amount;
        total_locked_amount = 0;

        step_count = 0;
        period = 0;
        first_step_block_count = 0;
        first_step_amount = tx_amount;
        last_step_amount = tx_amount;
        pre_vested_amount = tx_amount;
    } else {
        // Normal case. Set so far undefined variables for user friendly information.

        // Actual vesting step count, potentially including steps that do not actually unlock real contract funds if
        // total_locked_amount > tx_amount
        helper_uint64 = (total_locked_amount / step_amount) + /* round up */ !!(total_locked_amount % step_amount);
        RETURN_ON_ERROR(
            // While this is theoretically possible in valid vesting contracts, for example for total_locked_amount ==
            // MAX_SAFE_INTEGER and step_amount == 1, this exceeds the currently supported number of blocks of the Nimiq
            // blockchain and would lock funds for thousands to billions of years and is therefore a nonsense config
            // that we want to protect users from.
            // TODO re-evaluate this for Nimiq 2.0
            helper_uint64 > UINT32_MAX,
            ERROR_INCORRECT_DATA,
            "Vesting steps exceed number of possible Nimiq blocks\n"
        );

        // step_count
        if (total_locked_amount <= tx_amount) {
            // Each vesting step actually unlocks user funds.
            step_count = (uint32_t) helper_uint64;
        } else {
            // The specified locked amount is higher than the actual contract amount, i.e. additional steps need
            // to pass to vest the virtual, excess locked amount until the actual amount starts getting vested.
            // Only count steps that actually unlock funds, subtracting steps that entirely only vest the virtual,
            // excess locked amount.
            // Note that (total_locked_amount - tx_amount) / step_amount is smaller than helper_uint64 and there is no
            // risk of overflowing.
            step_count = ((uint32_t) helper_uint64) - (total_locked_amount - tx_amount) / step_amount;
        }

        // period
        helper_uint64 = /* actual vesting step count */ helper_uint64 * step_block_count;
        RETURN_ON_ERROR(
            helper_uint64 + start_block > UINT32_MAX,
            ERROR_INCORRECT_DATA,
            "Vesting end exceeds number of possible Nimiq blocks\n"
        );
        period = (uint32_t) helper_uint64;

        // remaining values for the standard total_locked_amount == tx_amount case
        first_step_block_count = step_block_count;
        first_step_amount = step_amount;
        last_step_amount = total_locked_amount % step_amount ? total_locked_amount % step_amount : step_amount;
        pre_vested_amount = 0;

        // adaptions for total_locked_amount != tx_amount
        if (tx_amount > total_locked_amount) {
            // Not actually the entire contract amount is locked, i.e. some amount is pre-vested.
            pre_vested_amount = tx_amount - total_locked_amount;
        } else if (tx_amount < total_locked_amount) {
            // An additional, virtual, excess locked amount of total_locked_amount - tx_amount needs to be vested before
            // the actual contract funds start getting vested.
            // As we do not include steps that entirely only vest the locked excess amount in our step count and
            // consider the step at which the first actual funds get unlocked as our first step, calculate the blocks
            // that need to pass for the actual first funds to become available.
            // This is guaranteed to not overflow as also the entire period was checked to fit an uint32.
            first_step_block_count = step_block_count // blocks for the step that actually unlocks the first funds
                + ((total_locked_amount - tx_amount) / step_amount) * step_block_count; // blocks for excess steps
            // As the first step that unlocks actual funds can also be partly filled by the excess amount, calculate how
            // much of the actual funds is actually unlocked.
            first_step_amount = step_amount - ((total_locked_amount - tx_amount) % step_amount);
        }
    }

    // Print data
    out->is_multi_step = step_count > 1;
    // note: not %lu (for unsigned long int) because int is already 32bit on ledgers (see "Memory Alignment" in Ledger
    // docu), additionally Ledger's own implementation of sprintf does not support %lu (see os_printf.c)
    snprintf(out->start_block, sizeof(out->start_block), "%u", start_block);
    snprintf(out->period, sizeof(out->period), "%u block%c", period, period != 1 ? 's' : '\0');
    snprintf(out->step_count, sizeof(out->step_count), "%u", step_count);
    snprintf(out->step_block_count, sizeof(out->step_block_count), "%u block%c", step_block_count,
        step_block_count != 1 ? 's' : '\0');
    snprintf(out->first_step_block_count, sizeof(out->first_step_block_count), "%u block%c", first_step_block_count,
        first_step_block_count != 1 ? 's' : '\0');
    snprintf(out->first_step_block, sizeof(out->first_step_block), "%u",
        start_block + first_step_block_count); // guaranteed to not overflow as also start_block + period <= UINT32_MAX
    RETURN_ON_ERROR(
        parse_amount(step_amount, "NIM", out->step_amount)
        || parse_amount(first_step_amount, "NIM", out->first_step_amount)
        || parse_amount(last_step_amount, "NIM", out->last_step_amount)
        || parse_amount(pre_vested_amount, "NIM", out->pre_vested_amount),
        ERROR_INCORRECT_DATA
    );

    return ERROR_NONE;
}

// Buffer utils
// Note that these favor sanity checks and code readability over execution speed or low stack / memory usage.

/**
 * Reads the leading part of a buffer as pointer in the original buffer, and advances the buffer pointer by length of
 * the extracted sub buffer. Notably, no copy of the sub buffer is created.
 * @param sub_buffer_length - The length of the sub buffer to read.
 * @param in_out_buffer - Buffer to read sub buffer from, after which the buffer pointer gets advanced by length read.
 * @param in_out_buffer_length - Buffer length to check sub buffer length against. Afterwards reduced by length read.
 * @param out_sub_buffer - A pointer to the sub buffer. This is a pointer in the original buffer (not to a copy) or NULL
 * @return true on success, false on error
 */
WARN_UNUSED_RESULT
bool read_sub_buffer(uint16_t sub_buffer_length, uint8_t **in_out_buffer, uint16_t *in_out_buffer_length,
    uint8_t **out_sub_buffer) {
    *out_sub_buffer = NULL;
    if (sub_buffer_length == 0) {
        return true;
    }
    if (in_out_buffer == NULL || *in_out_buffer == NULL || in_out_buffer_length == NULL) {
        PRINTF("Buffer invalid\n");
        return false;
    }
    if (*in_out_buffer_length < sub_buffer_length) {
        PRINTF("Buffer too short\n");
        return false;
    }
    *out_sub_buffer = *in_out_buffer;
    // Advance buffer and reduce the remaining buffer length.
    *in_out_buffer += sub_buffer_length;
    *in_out_buffer_length -= sub_buffer_length;
    return true;
}

WARN_UNUSED_RESULT
bool read_u8(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length, uint8_t *out_value) {
    uint8_t *uint8_pointer;
    RETURN_ON_ERROR(
        !read_sub_buffer(1, in_out_buffer, in_out_buffer_length, &uint8_pointer),
        false
    );
    *out_value = *uint8_pointer;
    return true;
}

WARN_UNUSED_RESULT
bool read_u16(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length, uint16_t *out_value) {
    uint8_t a, b;
    RETURN_ON_ERROR(
        !read_u8(in_out_buffer, in_out_buffer_length, &a)
        || !read_u8(in_out_buffer, in_out_buffer_length, &b),
        false
    );
    *out_value = (((uint16_t) a) << 8) | b;
    return true;
}

WARN_UNUSED_RESULT
bool read_u32(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length, uint32_t *out_value) {
    uint16_t a, b;
    RETURN_ON_ERROR(
        !read_u16(in_out_buffer, in_out_buffer_length, &a)
        || !read_u16(in_out_buffer, in_out_buffer_length, &b),
        false
    );
    *out_value = (((uint32_t) a) << 16) | b;
    return true;
}

WARN_UNUSED_RESULT
bool read_u64(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length, uint64_t *out_value) {
    uint32_t a, b;
    RETURN_ON_ERROR(
        !read_u32(in_out_buffer, in_out_buffer_length, &a)
        || !read_u32(in_out_buffer, in_out_buffer_length, &b),
        false
    );
    *out_value = (((uint64_t) a) << 32) | b;
    return true;
}

WARN_UNUSED_RESULT
bool read_bool(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length, bool *out_value) {
    uint8_t uint8_value;
    RETURN_ON_ERROR(
        !read_u8(in_out_buffer, in_out_buffer_length, &uint8_value),
        false
    );
    *out_value = uint8_value != 0;
    return true;
}

/**
 * Read a varint compatible with serde / postcard serialization, see https://postcard.jamesmunns.com/wire-format.html,
 * also called LEB, see https://en.wikipedia.org/wiki/LEB128. Note that this serialization format is different from, for
 * example, Bitcoin's variable length integers.
 */
WARN_UNUSED_RESULT
bool read_serde_uvarint(uint8_t max_bits, uint8_t **in_out_buffer, uint16_t *in_out_buffer_length,
    uint32_t *out_value) {
    // This is currently an incomplete implementation, as it only supports reading single-byte varints, i.e. values up
    // to 127, as we currently don't need reading higher values. Parameter max_bits is also mostly ignored yet in this
    // implementation, we only check that it is not less that what's read from a single byte.
    RETURN_ON_ERROR(
        max_bits < 7,
        false,
        "Unsupported varint length\n"
    );
    uint8_t byte;
    RETURN_ON_ERROR(
        !read_u8(in_out_buffer, in_out_buffer_length, &byte),
        false
    );
    uint8_t continuationBit = byte & 0x80; // the most significant bit is the continuation bit
    RETURN_ON_ERROR(
        continuationBit,
        false,
        "Unsupported multi-byte varint\n"
    );
    *out_value = byte;
    return true;
}

/**
 * Read a vector of bytes, compatible with serde / postcard serialization of a rust Vec<u8>. No copy of the data is
 * created.
 */
WARN_UNUSED_RESULT
bool read_serde_vec_u8(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length, uint8_t **out_data,
    uint16_t *out_data_length) {
    // A Vec is represented as a seq by serde, see https://serde.rs/data-model.html#types, which in turn is encoded as a
    // varint(usize) followed by the u8 data by postcard, see https://postcard.jamesmunns.com/wire-format.html#23---seq.
    // While usize is typically 32 bit or 64 bit, see https://postcard.jamesmunns.com/wire-format.html#isize-and-usize,
    // depending on the host size, we limit it to 16 bit here, as the memory of Ledger devices can't hold that long data
    // anyway. The u8 data is stored as individual bytes, see https://postcard.jamesmunns.com/wire-format.html#7---u8.
    uint32_t data_length; // u32 as that's what read_serde_uvarint expects
    RETURN_ON_ERROR(
        !read_serde_uvarint(16, in_out_buffer, in_out_buffer_length, &data_length)
        || !read_sub_buffer(data_length, in_out_buffer, in_out_buffer_length, out_data),
        false
    );
    *out_data_length = data_length; // no risk of overflow, as we limited the reading of the uvarint to 16 bits
    return true;
}

/**
 * Read a bip32 path. The data is copied to the output. The output must have a size of at least MAX_BIP32_PATH_LENGTH.
 */
WARN_UNUSED_RESULT
bool read_bip32_path(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length, uint32_t *out_bip32_path,
    uint8_t *out_bip32_path_length) {
    RETURN_ON_ERROR(
        !read_u8(in_out_buffer, in_out_buffer_length, out_bip32_path_length),
        false
    );
    RETURN_ON_ERROR(
        *out_bip32_path_length < 1 || *out_bip32_path_length > MAX_BIP32_PATH_LENGTH,
        false,
        "Invalid bip32 path length\n"
    );
    for (uint8_t i = 0; i < *out_bip32_path_length; i++) {
        RETURN_ON_ERROR(
            !read_u32(in_out_buffer, in_out_buffer_length, &out_bip32_path[i]),
            false
        );
    }
    return true;
}

WARN_UNUSED_RESULT
error_t parse_tx(transaction_version_t version, uint8_t *buffer, uint16_t buffer_length, parsed_tx_t *out) {
    RETURN_ON_ERROR(
        version != TRANSACTION_VERSION_LEGACY && version != TRANSACTION_VERSION_ALBATROSS,
        ERROR_NOT_SUPPORTED,
        "Unsupported transaction version\n"
    );

    // For serialization format see serialize_content in primitives/transaction/src/lib.rs in core-rs-albatross.

    // Read the recipient data
    uint16_t data_length;
    RETURN_ON_ERROR(
        !read_u16(&buffer, &buffer_length, &data_length),
        ERROR_READ
    );
    PRINTF("data length: %u\n", data_length);
    uint8_t *data;
    RETURN_ON_ERROR(
        !read_sub_buffer(data_length, &buffer, &buffer_length, &data),
        ERROR_READ
    );

    // Read the sender
    uint8_t *sender, sender_type;
    RETURN_ON_ERROR(
        !read_sub_buffer(20, &buffer, &buffer_length, &sender)
        || !read_u8(&buffer, &buffer_length, &sender_type),
        ERROR_READ
    );

    // Read the recipient
    uint8_t *recipient, recipient_type;
    RETURN_ON_ERROR(
        !read_sub_buffer(20, &buffer, &buffer_length, &recipient)
        || !read_u8(&buffer, &buffer_length, &recipient_type),
        ERROR_READ
    );

    // Process the value field
    uint64_t value;
    RETURN_ON_ERROR(
        !read_u64(&buffer, &buffer_length, &value),
        ERROR_READ
    );
    PRINTF("value: %u\n", value);
    RETURN_ON_ERROR(
        parse_amount(value, "NIM", out->value)
    );
    PRINTF("amount: %s\n", out->value);

    // Process the fee field
    uint64_t fee;
    RETURN_ON_ERROR(
        !read_u64(&buffer, &buffer_length, &fee),
        ERROR_READ
    );
    PRINTF("fee: %u\n", fee);
    RETURN_ON_ERROR(
        parse_amount(fee, "NIM", out->fee)
    );
    PRINTF("fee amount: %s\n", out->fee);

    // Read the validity start height
    uint32_t validity_start_height;
    RETURN_ON_ERROR(
        !read_u32(&buffer, &buffer_length, &validity_start_height),
        ERROR_READ
    );

    // Process the network field
    uint8_t network_id;
    RETURN_ON_ERROR(
        !read_u8(&buffer, &buffer_length, &network_id),
        ERROR_READ
    );
    RETURN_ON_ERROR(
        parse_network_id(version, network_id, out->network)
    );

    // Process the flags field
    uint8_t flags;
    RETURN_ON_ERROR(
        !read_u8(&buffer, &buffer_length, &flags),
        ERROR_READ
    );
    PRINTF("flags: %u\n", flags);

    // Read the sender data
    uint16_t sender_data_length = 0;
    uint8_t *sender_data = NULL;
    if (version == TRANSACTION_VERSION_ALBATROSS) {
        RETURN_ON_ERROR(
            !read_serde_vec_u8(&buffer, &buffer_length, &sender_data, &sender_data_length),
            ERROR_READ
        );
        PRINTF("sender data length: %u\n", sender_data_length);
    }

    RETURN_ON_ERROR(
        buffer_length != 0,
        ERROR_INVALID_LENGTH,
        "Transaction too long\n"
    );

    // Note: the transaction validity checks here are mostly for good measure and not entirely thorough or strict as
    // they don't need to be, because an invalid transaction will be rejected by the network nodes, even if we let it
    // pass here in the app.

    if (sender_type == ACCOUNT_TYPE_STAKING) {
        // Outgoing staking transaction from the staking contract.
        RETURN_ON_ERROR(
            // Would theoretically be allowed, but we don't support that yet. E.g. we don't support an unstaking tx to
            // at the same time create a contract. It can also not be a transaction to the staking contract, because
            // the sender and recipient address can not be the same.
            flags || data_length,
            ERROR_NOT_SUPPORTED,
            "Invalid flags or recipient data\n"
        );
        RETURN_ON_ERROR(
            !is_staking_contract(sender),
            ERROR_INCORRECT_DATA,
            "Sender must be staking contract\n"
        );

        staking_outgoing_data_type_t staking_outgoing_type;
        RETURN_ON_ERROR(
            parse_staking_outgoing_data(version, sender_data, sender_data_length, &staking_outgoing_type)
        );
        out->transaction_type = TRANSACTION_TYPE_STAKING_OUTGOING;

        switch (staking_outgoing_type) {
            case REMOVE_STAKE:
                out->transaction_label_type = TRANSACTION_LABEL_TYPE_STAKING_REMOVE_STAKE;
                break;
            default:
                // Note that validator transactions are not supported yet.
                RETURN_ERROR(
                    ERROR_NOT_SUPPORTED,
                    "Invalid outgoing staking transaction data type\n"
                );
        }

        // Print the recipient address and set unused data to empty string.
        RETURN_ON_ERROR(
            print_address(recipient, out->type_specific.normal_or_staking_outgoing_tx.recipient)
        );
        strcpy(out->type_specific.normal_or_staking_outgoing_tx.extra_data_label, "");
        strcpy(out->type_specific.normal_or_staking_outgoing_tx.extra_data, "");

        return ERROR_NONE;
    }

    switch (recipient_type) {
        case ACCOUNT_TYPE_BASIC: {
            RETURN_ON_ERROR(
                // Signaling flag and sender data might theoretically be allowed, but we don't support that yet.
                flags || sender_data_length,
                ERROR_NOT_SUPPORTED,
                "Invalid flags or sender data\n"
            );

            bool is_cashlink;
            RETURN_ON_ERROR(
                parse_normal_tx_data(data, data_length, &out->type_specific.normal_or_staking_outgoing_tx, &is_cashlink)
            );
            PRINTF("data: %s - is Cashlink: %d\n", out->type_specific.normal_or_staking_outgoing_tx.extra_data,
                is_cashlink);

            out->transaction_type = TRANSACTION_TYPE_NORMAL;
            out->transaction_label_type = is_cashlink
                ? TRANSACTION_LABEL_TYPE_CASHLINK
                : TRANSACTION_LABEL_TYPE_REGULAR_TRANSACTION;

            // Print the recipient address
            // We're ignoring the sender, as it's not too relevant where the funds are coming from.
            RETURN_ON_ERROR(
                print_address(recipient, out->type_specific.normal_or_staking_outgoing_tx.recipient)
            );
            break;
        }

        case ACCOUNT_TYPE_VESTING:
        case ACCOUNT_TYPE_HTLC: {
            RETURN_ON_ERROR(
                // Contract creation flag must be set, and no other flags are allowed at the same time.
                // Sender data would theoretically be allowed, but we don't currently support that.
                flags != TX_FLAG_CONTRACT_CREATION || sender_data_length,
                ERROR_NOT_SUPPORTED,
                "Invalid flags or sender data\n"
            );

            // Note that we're ignoring the recipient address for contract creation transactions as it must be the
            // deterministically calculated contract address, otherwise it's an invalid transaction rejected by the
            // network nodes.
            if (recipient_type == ACCOUNT_TYPE_VESTING) {
                RETURN_ON_ERROR(
                    parse_vesting_creation_data(version, data, data_length, sender, sender_type, value,
                        &out->type_specific.vesting_creation_tx)
                );
                out->transaction_type = TRANSACTION_TYPE_VESTING_CREATION;
                out->transaction_label_type = TRANSACTION_LABEL_TYPE_VESTING_CREATION;
            } else { // ACCOUNT_TYPE_HTLC
                RETURN_ON_ERROR(
                    parse_htlc_creation_data(version, data, data_length, sender, sender_type, validity_start_height,
                        &out->type_specific.htlc_creation_tx)
                );
                out->transaction_type = TRANSACTION_TYPE_HTLC_CREATION;
                out->transaction_label_type = TRANSACTION_LABEL_TYPE_HTLC_CREATION;
            }
            break;
        }

        case ACCOUNT_TYPE_STAKING: {
            // Incoming staking transaction to the staking contract.
            RETURN_ON_ERROR(
                // Flags must be either unset or the signaling flag.
                // Sender data would theoretically be allowed, but we don't currently support that.
                (flags && flags != TX_FLAG_SIGNALING) || sender_data_length,
                ERROR_NOT_SUPPORTED,
                "Invalid flags or sender data\n"
            );
            RETURN_ON_ERROR(
                !is_staking_contract(recipient),
                ERROR_INCORRECT_DATA,
                "Recipient must be staking contract\n"
            );

            RETURN_ON_ERROR(
                parse_staking_incoming_data(version, data, data_length, sender,
                    &out->type_specific.staking_incoming_tx)
            );
            out->transaction_type = TRANSACTION_TYPE_STAKING_INCOMING;

            switch (out->type_specific.staking_incoming_tx.type) {
                case CREATE_STAKER:
                    out->transaction_label_type = TRANSACTION_LABEL_TYPE_STAKING_CREATE_STAKER;
                    break;
                case ADD_STAKE:
                    out->transaction_label_type = TRANSACTION_LABEL_TYPE_STAKING_ADD_STAKE;
                    break;
                case UPDATE_STAKER:
                    out->transaction_label_type = TRANSACTION_LABEL_TYPE_STAKING_UPDATE_STAKER;
                    break;
                case SET_ACTIVE_STAKE:
                    out->transaction_label_type = TRANSACTION_LABEL_TYPE_STAKING_SET_ACTIVE_STAKE;
                    break;
                case RETIRE_STAKE:
                    out->transaction_label_type = TRANSACTION_LABEL_TYPE_STAKING_RETIRE_STAKE;
                    break;
                default:
                    // Note that validator transactions are not supported yet.
                    RETURN_ERROR(
                        ERROR_NOT_SUPPORTED,
                        "Invalid incoming staking transaction data type\n"
                    );
            }

            RETURN_ON_ERROR(
                (flags == TX_FLAG_SIGNALING)
                    != is_signaling_transaction_data(out->type_specific.staking_incoming_tx.type),
                ERROR_INCORRECT_DATA,
                "Signaling flag mismatch\n"
            );
            break;
        }

        default:
            RETURN_ERROR(
                ERROR_INCORRECT_DATA,
                "Invalid recipient type\n"
            );
    }

    return ERROR_NONE;
}

bool is_printable_ascii(uint8_t *data, uint16_t data_length) {
    for (uint16_t i = 0; i < data_length; i++) {
        if ((data[i] < /* space */ 32) || (data[i] > /* tilde */ 126)) return false;
    }
    return true;
}
