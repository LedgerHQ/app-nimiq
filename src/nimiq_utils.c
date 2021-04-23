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

#include "nimiq_utils.h"
#include "base32.h"

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
        if (70 <= counter) {
            THROW(0x6700); // buffer overflow, signal error
        }
        if (48 <= address[i] && 57 >= address[i]) {
            total_number[counter++] = address[i];
        } else if (65 <= address[i] && 90 >= address[i]) {
            snprintf(&total_number[counter++], 3, "%d", address[i] - 55);
            // Letters convert to a two digit number, increase the counter one more time
            counter++;
        } else if (97 <= address[i] && 122 >= address[i]) {
            snprintf(&total_number[counter++], 3, "%d", address[i] - 87);
            // Letters convert to a two digit number, increase the counter one more time
            counter++;
        } else {
            THROW(0x6a80); // invalid ascii code, signal error
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

void print_public_key_as_address(uint8_t *in, char *out) {
    // See lcx_blake2.h and lcx_hash.h in Ledger sdk
    unsigned char blake2b_hash[32]; // the first 20 bytes of the hash will be the Nimiq address
    cx_blake2b_t blake2b_context;
    cx_blake2b_init(&blake2b_context, /* hash length in bits */ 256);
    cx_hash(&blake2b_context.header, CX_LAST, in, 32, blake2b_hash, 32);

    print_address(blake2b_hash, out);
}

void parse_amount(uint64_t amount, char *asset, char *out) {
    char buffer[AMOUNT_MAX_SIZE];
    uint64_t dVal = amount;
    int i, j;

    // If the amount can't be represented safely in JavaScript, signal an error
    if (MAX_SAFE_INTEGER < amount) THROW(0x6a80);

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

void parse_network_id(uint8_t *in, char *out) {
    if (42 == in[0]) {
        strcpy(out, "Main");
    } else if (1 == in[0]) {
        strcpy(out, "Test");
    } else if (2 == in[0]) {
        strcpy(out, "Development");
    } else if (3 == in[0]) {
        strcpy(out, "Bounty");
    } else {
        THROW(0x6a80);
    }
}

bool parse_normal_tx_data(uint8_t *data, uint16_t data_length, char *out) {
    // Make sure we don't get called with more data than we can fit on the extra data field.
    if (data_length > MAX_NORMAL_TX_DATA_LENGTH) THROW(0x6a80);

    if (data == NULL || data_length == 0) {
        // empty string
        strcpy(out, "");
        return false;
    }

    // Make sure that the string is always null-terminated
    out[MAX_NORMAL_TX_DATA_LENGTH] = '\0';

    // Check if there is any non-printable ASCII characters
    for (uint16_t i = 0; i < data_length; i++) {
        if ((data[i] < 32) || (data[i] > 126)) {
            if (data_length == CASHLINK_MAGIC_NUMBER_LENGTH
                && memcmp(data, CASHLINK_MAGIC_NUMBER, CASHLINK_MAGIC_NUMBER_LENGTH) == 0) {
                // Special Cashlink data which we do not want to display.
                strcpy(out, "");
                return true;
            } else {
                strcpy(out, "Binary data");
                return false;
            }
        }
    }

    // If there is not, copy the string to be displayed
    strncpy(out, (char *) data, data_length);
    return false;
}

void parse_htlc_creation_data(uint8_t *data, uint16_t data_length, uint8_t *sender, uint32_t validity_start_height,
    tx_data_htlc_creation_t *out) {
    if (data == NULL || (data_length != 78 && data_length != 110)) THROW(0x6a80); // invalid data

    // Process refund address
    print_address(data, out->refund_address);
    out->is_refund_address_sender_address = memcmp(data, sender, 20) == 0;
    data += 20;

    // Process redeem address
    print_address(data, out->redeem_address);
    data += 20;

    // Process hash algorithm
    hash_algorithm_t hash_algorithm = *data;
    data++;
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
            // Invalid hash algorithm. Notably, ARGON2D is blacklisted for HTLCs.
            THROW(0x6a80);
    }
    out->is_using_sha256 = hash_algorithm == HASH_ALGORITHM_SHA256;

    // Process hash root
    uint8_t hash_size = hash_algorithm == HASH_ALGORITHM_SHA512 ? 64 : 32;
    // Recheck data_length now that we know which size exactly it should have.
    if (data_length != 46 + hash_size) THROW(0x6a80);
    // Print the hash as hex. Note that %.*h is a non-standard format implemented by the ledger sdk for printing data
    // as hex (see os_printf.c).
    snprintf(out->hash_root, MIN(sizeof(out->hash_root), hash_size * 2 + 1), "%.*h", hash_size, data);
    data += hash_size;

    // Process hash count
    snprintf(out->hash_count, sizeof(out->hash_count), "%u", *data);
    data++;

    // Process timeout
    uint32_t timeout = readUInt32Block(data);
    data += 4;
    snprintf(out->timeout, sizeof(out->timeout), "%u", timeout);
    out->is_timing_out_soon = timeout < validity_start_height
        || timeout - validity_start_height < HTLC_TIMEOUT_SOON_THRESHOLD;
}

uint16_t readUInt16Block(uint8_t *buffer) {
    return buffer[1] + (buffer[0] << 8);
}

uint32_t readUInt32Block(uint8_t *buffer) {
    return buffer[3] + (buffer[2] << 8) + (buffer[1] <<  16) + (buffer[0] << 24);
}

uint64_t readUInt64Block(uint8_t *buffer) {
    uint64_t i1 = buffer[3] + (buffer[2] << 8) + (buffer[1] <<  16) + (buffer[0] << 24);
    buffer += 4;
    uint32_t i2 = buffer[3] + (buffer[2] << 8) + (buffer[1] <<  16) + (buffer[0] << 24);
    return i2 | (i1 << 32);
}

void parseTx(uint8_t *buffer, txContent_t *txContent) {
    // Read the extra data
    uint16_t data_length = readUInt16Block(buffer);
    PRINTF("data length: %u\n", data_length);
    buffer += 2;
    uint8_t *data = data_length != 0 ? buffer : NULL;
    buffer += data_length;

    // Read the sender
    uint8_t *sender = buffer;
    buffer += 20;
    uint8_t sender_type = buffer[0];
    buffer++;
    if (sender_type != ACCOUNT_TYPE_BASIC) THROW(0x6a80); // We only support basic accounts

    // Read the recipient
    uint8_t *recipient = buffer;
    buffer += 20;
    uint8_t recipient_type = buffer[0];
    buffer++;

    // Process the value field
    uint64_t value = readUInt64Block(buffer);
    PRINTF("value: %lu\n", value);
    parse_amount(value, "NIM", txContent->value);
    PRINTF("amount: %s\n", txContent->value);
    buffer += 8;

    // Process the fee field
    uint64_t fee = readUInt64Block(buffer);
    PRINTF("fee: %lu\n", fee);
    parse_amount(fee, "NIM", txContent->fee);
    PRINTF("fee amount: %s\n", txContent->fee);
    buffer += 8;

    // Read the validity start height
    uint32_t validity_start_height = readUInt32Block(buffer);
    buffer += 4;

    // Process the network field
    parse_network_id(buffer, txContent->network);
    buffer++;

    // Process the flags field
    uint8_t flags = buffer[0];
    PRINTF("flags: %u\n", flags);

    if (flags == 0) {
        // Normal transaction
        if (recipient_type != ACCOUNT_TYPE_BASIC) THROW(0x6a80);

        txContent->transaction_type = TRANSACTION_TYPE_NORMAL;

        bool is_cashlink = parse_normal_tx_data(data, data_length, txContent->type_specific.normal_tx.extra_data);
        PRINTF("data: %s - is Cashlink: %d\n", txContent->type_specific.normal_tx.extra_data, is_cashlink);

        strcpy(txContent->transaction_type_label, is_cashlink ? "Cashlink" : "Transaction");

        // Print the recipient address
        // We're ignoring the sender, as it's not too relevant where the funds are coming from.
        print_address(recipient, txContent->type_specific.normal_tx.recipient);
    } else if (flags == TX_FLAG_CONTRACT_CREATION) {
        // Note that we're ignoring the recipient for contract creation transactions as it must be the deterministically
        // calculated contract address, otherwise it's an invalid transaction which is rejected by the network nodes.
        if (recipient_type == ACCOUNT_TYPE_HTLC) {
            txContent->transaction_type = TRANSACTION_TYPE_HTLC_CREATION;
            strcpy(txContent->transaction_type_label, "HTLC / Swap");
            parse_htlc_creation_data(data, data_length, sender, validity_start_height,
                &txContent->type_specific.htlc_creation_tx);
        } else {
            // Unsupported recipient type
            THROW(0x6a80);
        }
    } else {
        // Unsupported flag
        THROW(0x6a80);
    }
}
