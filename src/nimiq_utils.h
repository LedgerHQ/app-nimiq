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

#define OPERATION_TYPE_BASIC_TX 0

#define CAPTION_TYPE_OPERATION 0
#define CAPTION_TYPE_DETAILS1 1
#define CAPTION_TYPE_DETAILS2 2
#define CAPTION_TYPE_DETAILS3 3
#define CAPTION_TYPE_DETAILS4 4

typedef struct txContent_t {
    char network[12];
    char recipient[45];
    char value[25];
    char fee[25];
    char validity_start[11];
    uint8_t operationType;
    char details1[80];
    char details2[45];
    char details3[15];
    char details4[15];
} txContent_t;

void parseTx(uint8_t *buffer, txContent_t *txContent);

void print_address(uint8_t *in, char *out);

void print_public_key(uint8_t *in, char *out);

void print_amount(uint64_t amount, char *asset, char *out);

void print_network_id(uint8_t *in, char *out);

void print_caption(uint8_t operationType, uint8_t captionType, char *out);

void print_int(uint32_t in, char *out);


#endif // _NIMIQ_UTILS_H_
