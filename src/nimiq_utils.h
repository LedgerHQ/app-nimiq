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

#ifndef _NIMIQ_UTILS_H_
#define _NIMIQ_UTILS_H_

#include <stdint.h>
#include <stdbool.h>
#ifdef TEST
#include <stdio.h>
#define THROW(code) { printf("error: %d", code); return; }
#define PRINTF(msg, arg) printf(msg, arg)
#define PIC(code) code
#define TARGET_NANOS 1
#else
#include "os.h"
#endif // TEST

#define MAX_BIP32_PATH_LENGTH 10

#define LENGTH_NORMAL_TX_DATA_MAX 64
// Ascii (1 char per byte + string terminator) or hex (2 char per byte + string terminator).
#define STRING_LENGTH_NORMAL_TX_DATA_MAX (LENGTH_NORMAL_TX_DATA_MAX * 2 + 1)
#define STRING_LENGTH_UINT8 4 // "0" to "255" (any uint8)
#define STRING_LENGTH_UINT32 11 // "0" to "4294967295" (any uint32)
#define STRING_LENGTH_NIM_AMOUNT 22 // "0" to "90071992547.40991 NIM" (MAX_SAFE_INTEGER Luna in NIM)
#define STRING_LENGTH_USER_FRIENDLY_ADDRESS 45

#define CASHLINK_MAGIC_NUMBER "\x00\x82\x80\x92\x87"
#define CASHLINK_MAGIC_NUMBER_LENGTH 5

#define MESSAGE_SIGNING_PREFIX "\x16Nimiq Signed Message:\n" // 0x16 (decimal 22) is the prefix length

#define TX_FLAG_CONTRACT_CREATION 0x1
#define MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HEX (0x1 << 0)
#define MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HASH (0x1 << 1)

// TODO The threshold for short timeouts should be re-evaluated for Nimiq 2.0. However, keeping the current threshold is
//  no security risk at the time of switching to 2.0, as the threshold would be rather needed to be increased than
//  reduced, thus ui steps skipped for short timeouts will be displayed even though we could skip them.
#define HTLC_TIMEOUT_SOON_THRESHOLD (60 * 24 * 31 * 2); // ~ 2 months at 1 minute block time

typedef enum {
    ACCOUNT_TYPE_BASIC = 0,
    ACCOUNT_TYPE_VESTING = 1,
    ACCOUNT_TYPE_HTLC = 2,
} account_type_t;

typedef enum {
    TRANSACTION_VERSION_LEGACY,
    // TRANSACTION_VERSION_ALBATROSS,
} transaction_version_t;

typedef enum {
    TRANSACTION_TYPE_NORMAL,
    TRANSACTION_TYPE_VESTING_CREATION,
    TRANSACTION_TYPE_HTLC_CREATION,
} transaction_type_t;

typedef enum {
    HASH_ALGORITHM_BLAKE2B = 1,
    HASH_ALGORITHM_ARGON2D = 2,
    HASH_ALGORITHM_SHA256 = 3,
    HASH_ALGORITHM_SHA512 = 4,
} hash_algorithm_t;

typedef enum {
    MESSAGE_DISPLAY_TYPE_ASCII,
    MESSAGE_DISPLAY_TYPE_HEX,
    MESSAGE_DISPLAY_TYPE_HASH,
} message_display_type_t;

// Data printed for display.
// Note that this does not include any information about where the funds are coming from (a regular account, htlc,
// vesting contract, which address, ...) as this is not too relevant for the user and also not displayed by other apps
// like the Bitcoin app.
// Also note that while pre-processing all the data is nice, it's a bit wasteful in allocated memory usage. If we'd run
// into issues with low memory, we should switch to printing data on demand in the flow ux steps' init methods.

typedef struct {
    char recipient[STRING_LENGTH_USER_FRIENDLY_ADDRESS];
    char extra_data_label[9]; // "Data" or "Data Hex" + string terminator
    char extra_data[STRING_LENGTH_NORMAL_TX_DATA_MAX];
} tx_data_normal_t;

typedef struct {
    bool is_refund_address_sender_address;
    bool is_timing_out_soon;
    bool is_using_sha256;
    char redeem_address[STRING_LENGTH_USER_FRIENDLY_ADDRESS];
    char refund_address[STRING_LENGTH_USER_FRIENDLY_ADDRESS];
    char hash_root[129]; // hash root can be up to 64 bytes; displayed as hex + string terminator requires 129 chars
    char hash_algorithm[8]; // "BLAKE2b", "SHA-256" or "SHA-512"
    char hash_count[STRING_LENGTH_UINT8];
    char timeout[STRING_LENGTH_UINT32];
} tx_data_htlc_creation_t;

typedef struct {
    bool is_owner_address_sender_address;
    bool is_multi_step;
    char owner_address[STRING_LENGTH_USER_FRIENDLY_ADDRESS];
    char start_block[STRING_LENGTH_UINT32];
    char period[STRING_LENGTH_UINT32 + 7]; // any 32bit unsigned int + " blocks"
    char step_count[STRING_LENGTH_UINT32];
    char step_block_count[STRING_LENGTH_UINT32 + 7]; // any 32bit unsigned int + " blocks"
    // Note: first_step_block_count, first_step_block, first_step_amount and pre_vested_amount are not all needed at the
    // same time and could therefore be moved into a union to save some memory. As however tx_data_htlc_creation_t in
    // the txContent_t.type_specific union is bigger anyways, we currently don't have to do this optimization.
    char first_step_block_count[STRING_LENGTH_UINT32 + 7]; // any 32bit unsigned int + " blocks"
    char first_step_block[STRING_LENGTH_UINT32];
    char step_amount[STRING_LENGTH_NIM_AMOUNT];
    char first_step_amount[STRING_LENGTH_NIM_AMOUNT];
    char last_step_amount[STRING_LENGTH_NIM_AMOUNT];
    char pre_vested_amount[STRING_LENGTH_NIM_AMOUNT];
} tx_data_vesting_creation_t;

typedef struct {
    union {
        tx_data_normal_t normal_tx;
        tx_data_htlc_creation_t htlc_creation_tx;
        tx_data_vesting_creation_t vesting_creation_tx;
    } type_specific;

    transaction_type_t transaction_type;
    char transaction_type_label[12]; // "Transaction", "Cashlink", "HTLC / Swap" or "Vesting"
    char value[STRING_LENGTH_NIM_AMOUNT];
    char fee[STRING_LENGTH_NIM_AMOUNT];
    char network[12]; // "Main", "Test", "Development" or "Bounty"
} txContent_t;

void parseTx(transaction_version_t version, uint8_t *buffer, uint16_t buffer_length, txContent_t *out);

void print_address(uint8_t *in, char *out);

void print_public_key_as_address(uint8_t *in, char *out);

void print_hex(uint8_t *data, uint16_t dataLength, char *out, uint16_t outLength);

void parse_amount(uint64_t amount, char *asset, char *out);

void parse_network_id(uint8_t network_id, char *out);

bool parse_normal_tx_data(uint8_t *data, uint16_t data_length, tx_data_normal_t *out);

void parse_htlc_creation_data(uint8_t *data, uint16_t data_length, uint8_t *sender, account_type_t sender_type,
    uint32_t validity_start_height, tx_data_htlc_creation_t *out);

void parse_vesting_creation_data(uint8_t *data, uint16_t data_length, uint8_t *sender, account_type_t sender_type,
    uint64_t tx_amount, tx_data_vesting_creation_t *out);

uint8_t *readSubBuffer(uint16_t subBufferLength, uint8_t **in_out_buffer, uint16_t *in_out_bufferLength);

uint8_t readUInt8(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength);

uint16_t readUInt16(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength);

uint32_t readUInt32(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength);

uint64_t readUInt64(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength);

uint32_t readUVarInt(uint8_t maxBits, uint8_t **in_out_buffer, uint16_t *in_out_bufferLength);

uint16_t readVecU8(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength, uint8_t **out_vecData);

uint8_t readBip32Path(uint8_t **in_out_buffer, uint16_t *in_out_bufferLength, uint32_t *out_bip32Path);

bool isPrintableAscii(uint8_t *data, uint16_t dataLength);

#endif // _NIMIQ_UTILS_H_
