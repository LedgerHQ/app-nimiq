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
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "nimiq_utils.h"
#include "blake2b.h"
#include "base32.h"

static const char * captions[][5] = {
    {"Basic Tx", NULL, NULL, NULL, NULL},
    {"Extended Tx", "Data", "Sender", "Sender Type", "Recipient Type"} // For future use, not yet supported
};

static const uint8_t AMOUNT_MAX_SIZE = 17;

void iban_check(char in[36], char *check) {
    unsigned int counter = 0;
    unsigned int offset = 0;
    unsigned int modulo = 0;

    int partial_uint = 0;

    char total_number[71] = { 0 };
    char partial_number[10] = { 0 };

    // Convert the address to a number-only string
    for (unsigned int i = 0; i < 36; i++) {
        if (70 >= counter) {
            // XXX buffer overflow, signal error
        }
        if (in[i] >= 48 && in[i] <= 57) {
            total_number[counter++] = in[i];
        } else if (in[i] >= 65 && in[i] <= 90) {
            snprintf(&total_number[counter++], 3, "%d", in[i] - 55);
            // Letters convert to a two digit number, increase the counter one more time
            counter++;
        } else if (in[i] >= 97 && in[i] <= 122) {
            snprintf(&total_number[counter++], 3, "%d", in[i] - 87);
            // Letters convert to a two digit number, increase the counter one more time
            counter++;
        } else {
            // XXX unknown ascii code, signal error
        }
    }

    // Compute modulo-97 on the resulting number (do it in 32-bit pieces)
    counter = 0;
    for (unsigned int i = 0; i < 9; i++) {
        strncpy(&partial_number[offset], &total_number[counter], 9 - offset);
        // strncpy(check, partial_number, 10);
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
    char after_base32[36] = { 0 };

    base32_encode(in, 20, after_base32, 32);

    after_base32[32] = 'N';
    after_base32[33] = 'Q';
    after_base32[34] = '0';
    after_base32[35] = '0';
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

void print_public_key(uint8_t *in, char *out) {
    uint8_t after_blake[32] = { 0 };
    uint8_t short_blake[20] = { 0 };

    blake2b(after_blake, 32, NULL, 0, in, 32);
    memcpy(short_blake, after_blake, 20);

    print_address(short_blake, out);
}

void print_amount(uint64_t amount, char *asset, char *out) {
    char buffer[AMOUNT_MAX_SIZE];
    uint64_t dVal = amount;
    int i, j;

    memset(buffer, 0, AMOUNT_MAX_SIZE);
    for (i = 0; dVal > 0 || i < 9; i++) {
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

void print_int(uint32_t id, char *out) {
    char buffer[10];
    uint64_t dVal = id;
    int i, j;

    memset(buffer, 0, 10);
    for (i = 0; dVal > 0; i++) {
        buffer[i] = (dVal % 10) + '0';
        dVal /= 10;
        if (i >= 10) {
            THROW(0x6700);
        }
    }
    // reverse order
    for (i -= 1, j = 0; i >= 0 && j < 10-1; i--, j++) {
        out[j] = buffer[i];
    }
    if (j == 0) {
        out[0] = '0';
        j++;
    }
    out[j] = '\0';
}

void print_network_id(uint8_t *in, char *out) {
    if (0 == in[0]) {
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

void print_caption(uint8_t operationType, uint8_t captionType, char *out) {
    char *in = ((char*) PIC(captions[operationType][captionType]));
    if (in) {
        strcpy(out, in);
    }
}

uint16_t readUInt16Block(uint8_t *buffer) {
    return buffer[0] + (buffer[1] << 8);
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
    txContent->operationType = OPERATION_TYPE_BASIC_TX;
    uint16_t data_length = readUInt16Block(buffer);
    buffer += 2;
    if (0 != data_length) THROW(0x6a80);
    buffer += 20; // Ignore our own address
    uint8_t sender_type = buffer[0];
    buffer++;
    if (0 != sender_type) THROW(0x6a80);
    print_address(buffer, txContent->recipient);
    buffer += 20;
    uint8_t recipient_type = buffer[0];
    buffer++;
    if (0 != recipient_type) THROW(0x6a80);
    uint64_t value = readUInt64Block(buffer);
    print_amount(value, "NIM", txContent->value);
    buffer += 8;
    uint64_t fee = readUInt64Block(buffer);
    print_amount(fee, "NIM", txContent->fee);
    buffer += 8;
    uint32_t validity_start = readUInt32Block(buffer);
    print_int(validity_start, txContent->validity_start);
    buffer += 4;
    print_network_id(buffer, txContent->network);
    buffer++;
    uint8_t flags = buffer[0];
    buffer++;
    if (0 != flags) THROW(0x6a80);
}