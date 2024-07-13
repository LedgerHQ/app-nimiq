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
#include "os.h"
#include "lcx_blake2.h"

#include "nimiq_utils.h"
#include "nimiq_staking_utils.h"
#include "base32.h"

// The maximum allowed NIM amount. Equal to JavaScript's Number.MAX_SAFE_INTEGER, see Coin::MAX_SAFE_VALUE in
// primitives/src/coin.rs in core-rs-albatross.
#define MAX_SAFE_INTEGER 9007199254740991

static const uint8_t AMOUNT_MAX_SIZE = 17;

void iban_check(char in[32], char *check) {
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
        if (counter >= 70) {
            PRINTF("Overflow in iban check");
            THROW(0x6700);
        }
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
            PRINTF("Invalid ascii code in iban check");
            THROW(0x6a80);
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
}

void print_address(uint8_t *in, char *out) {
    unsigned int counter = 4;
    char after_base32[32] = { 0 };

    base32_encode(in, 20, after_base32, 32);
    iban_check(after_base32, &out[2]);

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
}

void public_key_to_address(uint8_t *in, uint8_t *out) {
    // See lcx_blake2.h and lcx_hash.h in Ledger sdk
    unsigned char blake2b_hash[32];
    cx_blake2b_t blake2b_context;
    cx_blake2b_init(&blake2b_context, /* hash length in bits */ 256);
    cx_hash(&blake2b_context.header, CX_LAST, in, 32, blake2b_hash, 32);
    os_memmove(out, blake2b_hash, 20); // the first 20 bytes of the hash are the Nimiq address
}

void print_public_key_as_address(uint8_t *in, char *out) {
    uint8_t address[20];
    public_key_to_address(in, address);
    print_address(address, out);
}

void print_hex(uint8_t *data, uint16_t dataLength, char *out, uint16_t outLength) {
    if (outLength < dataLength * 2 + 1) {
        // does not fit 2 hex chars per byte + string terminator
        PRINTF("print_hex: out buffer too small to fit hex");
        THROW(0x6700);
    }
    // not using Ledger's proprietary %.*H snprintf format, as it's non-standard
    // (see https://github.com/LedgerHQ/app-bitcoin/pull/200/files)
    for (uint16_t i = 0; i < dataLength; i++) {
        snprintf(out + i * 2, /* 2 hex chars + string terminator */ 3, "%02X", data[i]);
    }
}

void parse_amount(uint64_t amount, char *asset, char *out) {
    char buffer[AMOUNT_MAX_SIZE];
    uint64_t dVal = amount;
    int i, j;

    // If the amount can't be represented safely in JavaScript, signal an error
    if (amount > MAX_SAFE_INTEGER) {
        PRINTF("Invalid amount");
        THROW(0x6a80);
    }

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
        if (i >= AMOUNT_MAX_SIZE) {
            PRINTF("Overflow in parse_amount");
            THROW(0x6700);
        }
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
        // qualify amount
        out[j++] = ' ';
        strcpy(out + j, asset);
        out[j+strlen(asset)] = '\0';
    } else {
        out[j] = '\0';
    }

}

void parse_network_id(transaction_version_t version, uint8_t network_id, char *out) {
    if (network_id == (version == TRANSACTION_VERSION_LEGACY ? 42 : 24)) {
        strcpy(out, "Main");
    } else if (network_id == (version == TRANSACTION_VERSION_LEGACY ? 1 : 5)) {
        strcpy(out, "Test");
    } else if (network_id == (version == TRANSACTION_VERSION_LEGACY ? 2 : 6)) {
        strcpy(out, "Development");
    } else if (network_id == (version == TRANSACTION_VERSION_LEGACY ? 3 : 7)) {
        strcpy(out, "Bounty");
    } else {
        PRINTF("Invalid network");
        THROW(0x6a80);
    }
}

bool parse_normal_tx_data(uint8_t *data, uint16_t data_length, tx_data_normal_or_staking_outgoing_t *out) {
    // Make sure we don't get called with more data than we can fit on the extra data field.
    if (data_length > LENGTH_NORMAL_TX_DATA_MAX) {
        PRINTF("Extra data too long");
        THROW(0x6700);
    }

    // initiate with empty string / empty data
    strcpy(out->extra_data_label, "");
    strcpy(out->extra_data, "");

    if (data == NULL || data_length == 0) {
        return false;
    }

    // Check if it's the special Cashlink data which we do not want to display.
    if (data_length == CASHLINK_MAGIC_NUMBER_LENGTH
        && memcmp(data, CASHLINK_MAGIC_NUMBER, CASHLINK_MAGIC_NUMBER_LENGTH) == 0) {
        return true;
    }
    // Check if there is any non-printable ASCII characters
    if (!isPrintableAscii(data, data_length)) {
        strcpy(out->extra_data_label, "Data Hex");
        print_hex(data, data_length, out->extra_data, sizeof(out->extra_data));
        return false;
    }

    // If there is not, copy the string to be displayed
    strcpy(out->extra_data_label, "Data");
    strncpy(out->extra_data, (char *) data, data_length);
    out->extra_data[data_length] = '\0'; // Make sure the string is terminated

    return false;
}

void parse_htlc_creation_data(transaction_version_t version, uint8_t *data, uint16_t data_length, uint8_t *sender,
    account_type_t sender_type, uint32_t validity_start_height, tx_data_htlc_creation_t *out) {
    if (version != TRANSACTION_VERSION_LEGACY) {
        PRINTF("HTLC creation not implemented yet for Albatross");
        THROW(0x9484);
    }

    // Process refund address
    uint8_t *refund_address_bytes = readSubBuffer(20, &data, &data_length);
    print_address(refund_address_bytes, out->refund_address);
    out->is_refund_address_sender_address = memcmp(refund_address_bytes, sender, 20) == 0;

    if (out->is_refund_address_sender_address && sender_type != ACCOUNT_TYPE_BASIC) {
        // Although the refund address can be any address, specifying a contract as refund address is not recommendable
        // because for the contract address there is no key that could create the required signature for the htlc refund
        // proof. Protect the user from this scenario, as far as we can detect it.
        PRINTF("HTLC refund address should not be a contract");
        THROW(0x6a80);
    }

    // Process redeem address
    uint8_t *redeem_address_bytes = readSubBuffer(20, &data, &data_length);
    print_address(redeem_address_bytes, out->redeem_address);

    // Process hash algorithm
    hash_algorithm_t hash_algorithm = readUInt8(&data, &data_length);
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
            PRINTF("Invalid hash algorithm or blacklisted ARGON2d");
            THROW(0x6a80);
    }
    out->is_using_sha256 = hash_algorithm == HASH_ALGORITHM_SHA256;

    // Process hash root
    uint8_t hash_size = hash_algorithm == HASH_ALGORITHM_SHA512 ? 64 : 32;
    uint8_t *hash_bytes = readSubBuffer(hash_size, &data, &data_length);
    // Print the hash as hex. Note that %.*h is a non-standard format implemented by the ledger sdk for printing data
    // as hex (see os_printf.c).
    snprintf(out->hash_root, MIN(sizeof(out->hash_root), hash_size * 2 + 1), "%.*h", hash_size, hash_bytes);

    // Process hash count
    uint8_t hash_count = readUInt8(&data, &data_length);
    snprintf(out->hash_count, sizeof(out->hash_count), "%u", hash_count);

    // Process timeout
    uint32_t timeout = readUInt32(&data, &data_length);
    // note: not %lu (for unsigned long int) because int is already 32bit on ledgers (see "Memory Alignment" in Ledger
    // docu), additionally Ledger's own implementation of sprintf does not support %lu (see os_printf.c)
    snprintf(out->timeout, sizeof(out->timeout), "%u", timeout);
    out->is_timing_out_soon = timeout < validity_start_height
        || timeout - validity_start_height < HTLC_TIMEOUT_SOON_THRESHOLD;

    if (data_length != 0) {
        PRINTF("Htlc data too long");
        THROW(0x6700);
    }
}

void parse_vesting_creation_data(transaction_version_t version, uint8_t *data, uint16_t data_length, uint8_t *sender,
    account_type_t sender_type, uint64_t tx_amount, tx_data_vesting_creation_t *out) {
    if (version != TRANSACTION_VERSION_LEGACY) {
        PRINTF("Vesting creation not implemented yet for Albatross");
        THROW(0x9484);
    }

    // Note that this method could be quite heavy on the stack (depending on how well the compiler optimizes it). It
    // could be refactored by allocating less variables by printing them directly or re-using variables, but at the cost
    // of less readable code.

    // Process owner address
    uint8_t *owner_address_bytes = readSubBuffer(20, &data, &data_length);
    print_address(owner_address_bytes, out->owner_address);
    out->is_owner_address_sender_address = memcmp(owner_address_bytes, sender, 20) == 0;

    if (out->is_owner_address_sender_address && sender_type != ACCOUNT_TYPE_BASIC) {
        // Although the owner address can be any address, specifying a contract as owner is not recommendable because
        // for the contract address there is no key that could create the required signature for the vesting proof.
        // Protect the user from this scenario, as far as we can detect it.
        PRINTF("Vesting owner address should not be a contract");
        THROW(0x6a80);
    }

    // Read vesting parameters from data, depending on what is specified, and assign default values otherwise
    uint32_t start_block = 0;
    uint32_t step_block_count;
    uint64_t step_amount = tx_amount;
    uint64_t total_locked_amount = tx_amount;
    if (data_length == 4) {
        step_block_count = readUInt32(&data, &data_length);
    } else {
        start_block = readUInt32(&data, &data_length);
        step_block_count = readUInt32(&data, &data_length);
        step_amount = readUInt64(&data, &data_length);

        if (data_length == 8) {
            total_locked_amount = readUInt64(&data, &data_length);
        }
    }

    if (data_length != 0) {
        PRINTF("Vesting data too long");
        THROW(0x6700);
    }

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
        if (helper_uint64 > UINT32_MAX) {
            // While this is theoretically possible in valid vesting contracts, for example for total_locked_amount ==
            // MAX_SAFE_INTEGER and step_amount == 1, this exceeds the currently supported number of blocks of the Nimiq
            // blockchain and would lock funds for thousands to billions of years and is therefore a nonsense config
            // that we want to protect users from.
            // TODO re-evaluate this for Nimiq 2.0
            PRINTF("Vesting steps exceed number of possible Nimiq blocks");
            THROW(0x9850);
        }

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
        if (helper_uint64 + start_block > UINT32_MAX) {
            PRINTF("Vesting end exceeds number of possible Nimiq blocks");
            THROW(0x9850);
        }
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
    parse_amount(step_amount, "NIM", out->step_amount);
    parse_amount(first_step_amount, "NIM", out->first_step_amount);
    parse_amount(last_step_amount, "NIM", out->last_step_amount);
    parse_amount(pre_vested_amount, "NIM", out->pre_vested_amount);
}

// Buffer utils
// Note that these favor sanity checks and code readability over execution speed or low stack / memory usage.

/**
 * Returns the leading part of a buffer as pointer in the original buffer, and advances the buffer pointer by length of
 * the extracted sub buffer. Notably, no copy of the sub buffer is created.
 * @param subBufferLength - The length of the sub buffer to read.
 * @param in_out_buffer - Buffer to read sub buffer from, after which the buffer pointer gets advanced by length read.
 * @param in_out_bufferLength - Buffer length to check sub buffer length against. Afterwards reduced by length read.
 * @return A pointer to the sub buffer. This is a pointer in the original buffer (not to a copy) or NULL.
 */
uint8_t *readSubBuffer(uint16_t subBufferLength, uint8_t **in_out_buffer, uint16_t *in_out_bufferLength) {
    if (subBufferLength == 0) return NULL;
    if (in_out_buffer == NULL || *in_out_buffer == NULL || in_out_bufferLength == NULL) {
        PRINTF("Buffer invalid");
        THROW(0x6a80);
    }
    if (*in_out_bufferLength < subBufferLength) {
        PRINTF("Buffer too short");
        THROW(0x6700);
    }
    uint8_t *subBuffer = *in_out_buffer;
    // Advance buffer and reduce the remaining buffer length.
    *in_out_buffer += subBufferLength;
    *in_out_bufferLength -= subBufferLength;
    return subBuffer;
}

uint8_t readUInt8(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength) {
    return *(readSubBuffer(1, in_out_buffer, in_out_bufferLength));
}

uint16_t readUInt16(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength) {
    return (((uint16_t) readUInt8(in_out_buffer, in_out_bufferLength)) << 8)
        | readUInt8(in_out_buffer, in_out_bufferLength);
}

uint32_t readUInt32(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength) {
    return (((uint32_t) readUInt16(in_out_buffer, in_out_bufferLength)) << 16)
        | readUInt16(in_out_buffer, in_out_bufferLength);
}

uint64_t readUInt64(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength) {
    return (((uint64_t) readUInt32(in_out_buffer, in_out_bufferLength)) << 32)
        | readUInt32(in_out_buffer, in_out_bufferLength);
}

/**
 * Read a varint compatible with serde / postcard serialization, see https://postcard.jamesmunns.com/wire-format.html,
 * also called LEB, see https://en.wikipedia.org/wiki/LEB128. Note that this serialization format is different from, for
 * example, Bitcoin's variable length integers.
 */
uint32_t readUVarInt(uint8_t maxBits, uint8_t **in_out_buffer, uint16_t *in_out_bufferLength) {
    // This is currently an incomplete implementation, as it only supports reading single-byte varints, i.e. values up
    // to 127, as we currently don't need reading higher values. Parameter maxBits is also mostly ignored yet in this
    // implementation, we only check that it is not less that what's read from a single byte.
    if (maxBits < 7) {
        PRINTF("Unsupported varint length");
        THROW(0x9484);
    }
    uint8_t byte = readUInt8(in_out_buffer, in_out_bufferLength);
    uint8_t continuationBit = byte & 0x80; // the most significant bit is the continuation bit
    if (continuationBit) {
        PRINTF("Unsupported multi-byte varint");
        THROW(0x9484);
    }
    return byte;
}

bool readBool(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength) {
    return readUInt8(in_out_buffer, in_out_bufferLength) != 0;
}

/**
 * Read a vector of bytes, compatible with serde / postcard serialization of a rust Vec<u8>. No copy of the data is
 * created.
 */
uint16_t readVecU8(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength, uint8_t **out_vecData) {
    // A Vec is represented as a seq by serde, see https://serde.rs/data-model.html#types, which in turn is encoded as a
    // varint(usize) followed by the u8 data by postcard, see https://postcard.jamesmunns.com/wire-format.html#23---seq.
    // While usize is typically 32 bit or 64 bit, see https://postcard.jamesmunns.com/wire-format.html#isize-and-usize,
    // depending on the host size, we limit it to 16 bit here, as the memory of Ledger devices can't hold that long data
    // anyway. The u8 data is stored as individual bytes, see https://postcard.jamesmunns.com/wire-format.html#7---u8.
    uint16_t length = (uint16_t) readUVarInt(16, in_out_buffer, in_out_bufferLength);
    *out_vecData = readSubBuffer(length, in_out_buffer, in_out_bufferLength);
    return length;
}

uint8_t readBip32Path(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength, uint32_t *out_bip32Path) {
    uint8_t bip32PathLength = readUInt8(in_out_buffer, in_out_bufferLength);
    if (bip32PathLength < 1 || bip32PathLength > MAX_BIP32_PATH_LENGTH) {
        PRINTF("Invalid bip32 path length");
        THROW(0x6700);
    }
    for (uint8_t i = 0; i < bip32PathLength; i++) {
        out_bip32Path[i] = readUInt32(in_out_buffer, in_out_bufferLength);
    }
    return bip32PathLength;
}

void parseTx(transaction_version_t version, uint8_t *buffer, uint16_t buffer_length, parsed_tx_t *out) {
    if (version != TRANSACTION_VERSION_LEGACY && version != TRANSACTION_VERSION_ALBATROSS) {
        PRINTF("Unsupported transaction version");
        THROW(0x6a80);
    }

    // For serialization format see serialize_content in primitives/transaction/src/lib.rs in core-rs-albatross.

    // Read the recipient data
    uint16_t data_length = readUInt16(&buffer, &buffer_length);
    PRINTF("data length: %u\n", data_length);
    uint8_t *data = readSubBuffer(data_length, &buffer, &buffer_length);

    // Read the sender
    uint8_t *sender = readSubBuffer(20, &buffer, &buffer_length);
    uint8_t sender_type = readUInt8(&buffer, &buffer_length);

    // Read the recipient
    uint8_t *recipient = readSubBuffer(20, &buffer, &buffer_length);
    uint8_t recipient_type = readUInt8(&buffer, &buffer_length);

    // Process the value field
    uint64_t value = readUInt64(&buffer, &buffer_length);
    PRINTF("value: %lu\n", value);
    parse_amount(value, "NIM", out->value);
    PRINTF("amount: %s\n", out->value);

    // Process the fee field
    uint64_t fee = readUInt64(&buffer, &buffer_length);
    PRINTF("fee: %lu\n", fee);
    parse_amount(fee, "NIM", out->fee);
    PRINTF("fee amount: %s\n", out->fee);

    // Read the validity start height
    uint32_t validity_start_height = readUInt32(&buffer, &buffer_length);

    // Process the network field
    uint8_t network_id = readUInt8(&buffer, &buffer_length);
    parse_network_id(version, network_id, out->network);

    // Process the flags field
    uint8_t flags = readUInt8(&buffer, &buffer_length);
    PRINTF("flags: %u\n", flags);

    // Read the sender data
    uint16_t sender_data_length = 0;
    uint8_t *sender_data = NULL;
    if (version == TRANSACTION_VERSION_ALBATROSS) {
        sender_data_length = readVecU8(&buffer, &buffer_length, &sender_data);
        PRINTF("sender data length: %u\n", sender_data_length);
    }

    if (buffer_length != 0) {
        PRINTF("Transaction too long");
        THROW(0x6700);
    }

    // Note: the transaction validity checks here are mostly for good measure and not entirely thorough or strict as
    // they don't need to be, because an invalid transaction will be rejected by the network nodes, even if we let it
    // pass here in the app.

    if (sender_type == ACCOUNT_TYPE_STAKING) {
        // Outgoing staking transaction from the staking contract.
        if (flags || data_length) {
            // Would theoretically be allowed, but we don't support that yet. E.g. we don't support an unstaking tx to
            // at the same time create a contract. It can also not be a transaction to the staking contract, because
            // the sender and recipient address can not be the same.
            PRINTF("Invalid flags or recipient data");
            THROW(0x6a80);
        }
        if (!is_staking_contract(sender)) {
            // This does not currently seem to be enforced by core-rs-albatross but probably should be. The check here
            // can be removed once core-rs-albatross checks for it.
            PRINTF("Sender must be staking contract");
            THROW(0x6a80);
        }

        staking_outgoing_data_type_t staking_outgoing_type = parse_staking_outgoing_data(version, sender_data,
            sender_data_length);
        out->transaction_type = TRANSACTION_TYPE_STAKING_OUTGOING;

        switch (staking_outgoing_type) {
            case REMOVE_STAKE:
                out->transaction_label_type = TRANSACTION_LABEL_TYPE_STAKING_REMOVE_STAKE;
                break;
            default:
                // Note that validator transactions are not supported yet.
                PRINTF("Invalid outgoing staking transaction data type");
                THROW(0x6a80);
        }

        // Print the recipient address and set unused data to empty string.
        print_address(recipient, out->type_specific.normal_or_staking_outgoing_tx.recipient);
        strcpy(out->type_specific.normal_or_staking_outgoing_tx.extra_data_label, "");
        strcpy(out->type_specific.normal_or_staking_outgoing_tx.extra_data, "");

        return;
    }

    switch (recipient_type) {
        case ACCOUNT_TYPE_BASIC: {
            if (flags || sender_data_length) {
                // Signaling flag and sender data would theoretically be allowed, but we don't support that yet.
                PRINTF("Invalid flags or sender data");
                THROW(0x6a80);
            }

            bool is_cashlink = parse_normal_tx_data(data, data_length,
                &out->type_specific.normal_or_staking_outgoing_tx);
            PRINTF("data: %s - is Cashlink: %d\n", out->type_specific.normal_or_staking_outgoing_tx.extra_data,
                is_cashlink);

            out->transaction_type = TRANSACTION_TYPE_NORMAL;
            out->transaction_label_type = is_cashlink
                ? TRANSACTION_LABEL_TYPE_CASHLINK
                : TRANSACTION_LABEL_TYPE_REGULAR_TRANSACTION;

            // Print the recipient address
            // We're ignoring the sender, as it's not too relevant where the funds are coming from.
            print_address(recipient, out->type_specific.normal_or_staking_outgoing_tx.recipient);
            break;
        }

        case ACCOUNT_TYPE_VESTING:
        case ACCOUNT_TYPE_HTLC: {
            if (flags != TX_FLAG_CONTRACT_CREATION || sender_data_length) {
                // Contract creation flag must be set, and no other flags are allowed at the same time.
                // Sender data would theoretically be allowed, but we don't currently support that.
                PRINTF("Invalid flags or sender data");
                THROW(0x6a80);
            }

            // Note that we're ignoring the recipient address for contract creation transactions as it must be the
            // deterministically calculated contract address, otherwise it's an invalid transaction rejected by the
            // network nodes.
            if (recipient_type == ACCOUNT_TYPE_VESTING) {
                parse_vesting_creation_data(version, data, data_length, sender, sender_type, value,
                    &out->type_specific.vesting_creation_tx);
                out->transaction_type = TRANSACTION_TYPE_VESTING_CREATION;
                out->transaction_label_type = TRANSACTION_LABEL_TYPE_VESTING_CREATION;
            } else { // ACCOUNT_TYPE_HTLC
                parse_htlc_creation_data(version, data, data_length, sender, sender_type, validity_start_height,
                    &out->type_specific.htlc_creation_tx);
                out->transaction_type = TRANSACTION_TYPE_HTLC_CREATION;
                out->transaction_label_type = TRANSACTION_LABEL_TYPE_HTLC_CREATION;
            }
            break;
        }

        case ACCOUNT_TYPE_STAKING: {
            // Incoming staking transaction to the staking contract.
            if ((flags && flags != TX_FLAG_SIGNALING) || sender_data_length) {
                // Flags must be either unset or the signaling flag.
                // Sender data would theoretically be allowed, but we don't currently support that.
                PRINTF("Invalid flags or sender data");
                THROW(0x6a80);
            }
            if (!is_staking_contract(recipient)) {
                // This does not currently seem to be enforced by core-rs-albatross but probably should be. The check
                // here can be removed once core-rs-albatross checks for it.
                PRINTF("Recipient must be staking contract");
                THROW(0x6a80);
            }

            parse_staking_incoming_data(version, data, data_length, sender, &out->type_specific.staking_incoming_tx);
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
                    PRINTF("Invalid incoming staking transaction data type");
                    THROW(0x6a80);
            }

            if ((flags == TX_FLAG_SIGNALING)
                != is_signaling_transaction_data(out->type_specific.staking_incoming_tx.type)) {
                PRINTF("Signaling flag mismatch");
                THROW(0x6a80);
            }
            break;
        }

        default:
            PRINTF("Invalid recipient type");
            THROW(0x6a80);
    }
}

bool isPrintableAscii(uint8_t *data, uint16_t dataLength) {
    for (uint16_t i = 0; i < dataLength; i++) {
        if ((data[i] < /* space */ 32) || (data[i] > /* tilde */ 126)) return false;
    }
    return true;
}
