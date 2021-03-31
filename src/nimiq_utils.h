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
#ifdef TEST
#include <stdio.h>
#define THROW(code) { printf("error: %d", code); return; }
#define PRINTF(msg, arg) printf(msg, arg)
#define PIC(code) code
#define TARGET_NANOS 1
#else
#include "os.h"
#endif // TEST

#define MAX_DATA_LENGTH 64
#define MAX_DATA_STRING_LENGTH (MAX_DATA_LENGTH + 1) // One more byte for the NULL string terminator

#define CASHLINK_MAGIC_NUMBER "\x00\x82\x80\x92\x87"
#define CASHLINK_MAGIC_NUMBER_LENGTH 5

typedef enum {
    TRANSACTION_TYPE_BASIC,
    TRANSACTION_TYPE_CASHLINK, // Basic transaction that funds a cashlink
} transaction_type_t;

typedef struct txContent_t {
    transaction_type_t transaction_type;
    char transaction_type_label[12];
    char value[25];
    char fee[25];
    char recipient[45];
    char network[12];
    char extra_data[MAX_DATA_STRING_LENGTH];
} txContent_t;

void parseTx(uint8_t *buffer, txContent_t *txContent);

void print_address(uint8_t *in, char *out);

void print_public_key_as_address(uint8_t *in, char *out);

void print_amount(uint64_t amount, char *asset, char *out);

void print_network_id(uint8_t *in, char *out);

void print_transaction_type(transaction_type_t transaction_type, char *out);

#endif // _NIMIQ_UTILS_H_
