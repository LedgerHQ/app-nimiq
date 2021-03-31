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

void print_amount(uint64_t amount, char *asset, char *out) {
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

void print_network_id(uint8_t *in, char *out) {
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

void print_extra_data(uint8_t *in, char *out, uint16_t data_size) {
    // Extra safety check: make sure we don't get called with more data than
    // we can fit on the extra data field.
    if (MAX_DATA_LENGTH < data_size) THROW(0x6a80);

    // Make sure that the string is always null-terminated
    out[MAX_DATA_LENGTH] = '\0';

    // Check if there is any non-printable ASCII characters
    for (uint16_t i = 0; i < data_size; i++) {
        if ((32 > in[i]) || (126 < in[i])) {
            strcpy(out, "Binary data");
            return;
        }
    }

    // If there is not, copy the string to be displayed
    strncpy(out, (char *) in, data_size);
}

void print_transaction_type(transaction_type_t transaction_type, char *out) {
    switch (transaction_type) {
        case TRANSACTION_TYPE_BASIC:
            strcpy(out, "Transaction");
            break;
        case TRANSACTION_TYPE_CASHLINK:
            strcpy(out, "Cashlink");
            break;
        default:
            THROW(0x6a80);
    }
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
    // Process the data length field
    uint16_t data_length = readUInt16Block(buffer);
    PRINTF("data length: %u\n", data_length);
    buffer += 2;

    // Process the extra data field
    if (0 == data_length) {
        txContent->transaction_type = TRANSACTION_TYPE_BASIC;
    } else if ((CASHLINK_MAGIC_NUMBER_LENGTH == data_length) &&
     (0 == memcmp(buffer, CASHLINK_MAGIC_NUMBER, CASHLINK_MAGIC_NUMBER_LENGTH))) {
        txContent->transaction_type = TRANSACTION_TYPE_CASHLINK;
        buffer += CASHLINK_MAGIC_NUMBER_LENGTH;
    } else if (MAX_DATA_LENGTH >= data_length) {
        txContent->transaction_type = TRANSACTION_TYPE_BASIC;

        print_extra_data(buffer, txContent->extra_data, data_length);
        PRINTF("data: %s\n", txContent->extra_data);
        buffer += data_length;
    } else {
        THROW(0x6a80);
    }

    print_transaction_type(txContent->transaction_type, txContent->transaction_type_label);

    // Process the sender field
    buffer += 20; // Ignore our own address for Basic Tx (even with data)

    // Process the sender account type field
    uint8_t sender_type = buffer[0];
    buffer++;
    if (0 != sender_type) THROW(0x6a80); // We only support basic accounts

    // Proccess the recipient field
    print_address(buffer, txContent->recipient);
    PRINTF("recipient: %s\n", txContent->recipient);
    buffer += 20;

    // Proccess the recipient account type field
    uint8_t recipient_type = buffer[0];
    buffer++;
    if (0 != recipient_type) THROW(0x6a80); // We only support basic accounts

    // Proccess the value field
    uint64_t value = readUInt64Block(buffer);
    PRINTF("value: %lu\n", value);
    print_amount(value, "NIM", txContent->value);
    PRINTF("amount: %s\n", txContent->value);
    buffer += 8;

    // Proccess the fee field
    uint64_t fee = readUInt64Block(buffer);
    PRINTF("fee: %lu\n", fee);
    print_amount(fee, "NIM", txContent->fee);
    PRINTF("fee amount: %s\n", txContent->fee);
    buffer += 8;

    // Skip the validity start field
    buffer += 4;

    // Proccess the validity start field
    print_network_id(buffer, txContent->network);
    buffer++;

    // Proccess the flags field
    uint8_t flags = buffer[0];
    PRINTF("flags: %u\n", flags);
    if (0 != flags) THROW(0x6a80); // No flags are supported yet
}
