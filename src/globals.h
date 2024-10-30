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

#ifndef _NIMIQ_GLOBALS_H_
#define _NIMIQ_GLOBALS_H_

#include <stdint.h>
#include <stdbool.h>

// From Ledger SDK
#include "cx.h"

#include "constants.h"
#include "utility_macros.h"
#include "nimiq_utils.h"

/**
 * Global structure for NVM data storage.
 */
typedef struct internal_storage_t {
    uint8_t fidoTransport; // for u2f; currently unused; enabled by default
    uint8_t initialized;
} internal_storage_t;

// extern variable, shared across .c files. Declared in globals.c
extern const internal_storage_t N_storage_real;
#define N_storage (*(volatile internal_storage_t *) PIC(&N_storage_real))

typedef struct publicKeyContext_t {
    cx_ecfp_256_public_key_t publicKey;
    char address[STRING_LENGTH_USER_FRIENDLY_ADDRESS];
    uint8_t signature[64];
    bool returnSignature;
} publicKeyContext_t;

typedef struct transactionContext_t {
    uint8_t bip32PathLength;
    uint32_t bip32Path[MAX_BIP32_PATH_LENGTH];
    transaction_version_t transactionVersion;
    uint8_t rawTx[MAX_RAW_TX];
    uint32_t rawTxLength;
    parsed_tx_t parsed;
} transactionContext_t;

// Printed message buffer length dimension chosen such that it can hold the printed uint32 message length
// (STRING_LENGTH_UINT32 (11) bytes), the message printed as hash (32 byte hash as hex + string terminator = 65 bytes),
// ascii (1 char per byte + string terminator) or hex (2 char per byte + string terminator).
#define PRINTED_MESSAGE_BUFFER_LENGTH (MAX_PRINTABLE_MESSAGE_LENGTH * 2 + 1)
typedef struct messageSigningContext_t {
    uint32_t bip32Path[MAX_BIP32_PATH_LENGTH];
    // nimiq supports signing data of arbitrary length, but for now we restrict the length in the ledger app to uint32_t
    uint32_t messageLength;
    uint32_t processedMessageLength;
    union {
        // the memory is specifically aligned such that messageHashContext and prefixedMessageHashContext do not overlap
        // with messageHash and prefxedMessageHash, see _Static_assert in handleSignMessage.
        struct {
            cx_sha256_t messageHashContext;
            cx_sha256_t prefixedMessageHashContext;
        } prepare;
        struct {
            message_display_type_t displayType;
            char printedMessageLabel[MAX(sizeof("Message"), MAX(sizeof("Message Hex"), sizeof("Message Hash")))];
            char printedMessage[PRINTED_MESSAGE_BUFFER_LENGTH];
            uint8_t messageHash[32];
            uint8_t prefixedMessageHash[32];
        } confirm;
    };
    uint8_t printableMessage[MAX_PRINTABLE_MESSAGE_LENGTH];
    bool isPrintableAscii;
    uint8_t bip32PathLength;
    uint8_t flags;
} messageSigningContext_t;

typedef struct {
    union {
        publicKeyContext_t pk;
        transactionContext_t tx;
        messageSigningContext_t msg;
    } req;
    uint16_t u2fTimer;
} generalContext_t;

// extern variable, shared across .c files. Declared in globals.c
extern generalContext_t ctx;

// Shortcuts for parsed transaction data
#define PARSED_TX (ctx.req.tx.parsed)
#define PARSED_TX_NORMAL_OR_STAKING_OUTGOING (PARSED_TX.type_specific.normal_or_staking_outgoing_tx)
#define PARSED_TX_VESTING_CREATION (PARSED_TX.type_specific.vesting_creation_tx)
#define PARSED_TX_HTLC_CREATION (PARSED_TX.type_specific.htlc_creation_tx)
#define PARSED_TX_STAKING_INCOMING (PARSED_TX.type_specific.staking_incoming_tx)

#endif // _NIMIQ_GLOBALS_H_
