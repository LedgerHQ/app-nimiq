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

#include <stdbool.h>
#include <string.h>

// From Ledger SDK
#include "os.h" // for os_* methods
#include "os_io_seproxyhal.h" // for SEPROXYHAL_TAG_* and G_io_app definitions
#include "cx.h" // for cx_* types and methods
#include "ux.h" // for UX_*_EVENT

#include "globals.h"
#include "nimiq_utils.h"
#include "nimiq_ux.h"

#define CLA 0xE0
// Defined instructions.
// Note that some values are disallowed as instructions (odd numbers, 6X, 9X) or predefined by ISO7816-3, see
// https://cardwerk.com/smart-card-standard-iso7816-4-section-5-basic-organizations/#:~:text=Table%2010%20%E2%80%93-,Invalid%20INS%20codes,-b8%20b7%20b6
// or https://de.wikipedia.org/wiki/Application_Protocol_Data_Unit#command_APDU
#define INS_GET_PUBLIC_KEY 0x02
#define INS_SIGN_TX 0x04
// #define INS_GET_APP_CONFIGURATION 0x06 // was removed, but still listing it here to avoid assigning the same value
#define INS_KEEP_ALIVE 0x08
#define INS_SIGN_MESSAGE 0x0A
#define P1_NO_SIGNATURE 0x00
#define P1_SIGNATURE 0x01
#define P2_NO_CONFIRM 0x00
#define P2_CONFIRM 0x01
#define P1_FIRST 0x00
#define P1_MORE 0x80
#define P2_LAST 0x00
#define P2_MORE 0x80

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_P2 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

unsigned int io_seproxyhal_on_address_approved();
unsigned int io_seproxyhal_on_address_rejected();
unsigned int io_seproxyhal_on_transaction_approved();
unsigned int io_seproxyhal_on_transaction_rejected();
unsigned int io_seproxyhal_on_message_approved();
unsigned int io_seproxyhal_on_message_rejected();
uint32_t set_result_get_publicKey();

unsigned int io_seproxyhal_on_address_approved() {
    uint32_t tx = set_result_get_publicKey();
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_on_address_rejected() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_on_transaction_approved() {
    uint32_t tx = 0;

    // initialize private key
    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, ctx.req.tx.bip32Path, ctx.req.tx.bip32PathLength, privateKeyData, NULL, "ed25519 seed", 12);
    cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    // For incoming staking transactions which are meant to include a staker signature proof in their recipient data but
    // only include the empty default proof, we replace that empty signature proof with an actually signed staker proof.
    // For this, the same ledger account is used as staker and transaction sender, which is the most common case for
    // regular users. This way, no separate signature request needs to be sent to the ledger to first create the staker
    // signature proof, but both signatures are created in a single request for better UX. For advanced users, usage of
    // a different staker than the transaction sender is still supported by providing a ready pre-signed signature proof
    // in the request instead of an empty signature proof. Note that the staker signature proof is supposed to be signed
    // on the transaction data with the empty signature proof, see verify_transaction_signature for incoming set to true
    // in primitives/transaction/src/account/staking_contract/structs.rs, which is why we can conveniently use presence
    // of the empty signature proof to detect, that we should create the staker signature before signing the transaction
    // and directly sign it over the passed transaction with the empty proof.
    bool created_staker_signature = false;
    if (PARSED_TX.transaction_type == TRANSACTION_TYPE_STAKING_INCOMING
        && PARSED_TX_STAKING_INCOMING.has_validator_or_staker_signature_proof
        && is_empty_default_signature_proof(PARSED_TX_STAKING_INCOMING.validator_or_staker_signature_proof)
    ) {
        // Create the staker signature over the transaction with the empty signature proof in its data, which is exactly
        // what the staker signatue must sign. Write the signature to a temporary buffer, instead of directly to the
        // signature proof, to avoid writing to the data that is currently being signed. To save some stack space, we
        // use G_io_apdu_buffer as that temporary buffer.
        cx_eddsa_sign(&privateKey, CX_LAST, CX_SHA512, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, NULL, 0,
            G_io_apdu_buffer, 64, NULL);
        // Overwrite the signature in the signature proof in rawTx via pointers in validator_or_staker_signature_proof
        // which point to the original buffer.
        os_memmove(PARSED_TX_STAKING_INCOMING.validator_or_staker_signature_proof.signature, G_io_apdu_buffer, 64);
        created_staker_signature = true;

        // Similarly, overwrite the public key in the signature proof with the ledger account public key as staker, with
        // G_io_apdu_buffer as temporary buffer again. Check with a compile time assertion that it can fit the temp data
        _Static_assert(sizeof(cx_ecfp_public_key_t) <= IO_APDU_BUFFER_SIZE, "G_io_apdu_buffer does not fit public key");
        cx_ecfp_public_key_t *temporary_public_key_pointer = (cx_ecfp_public_key_t*) G_io_apdu_buffer;
        cx_ecfp_generate_pair(CX_CURVE_Ed25519, temporary_public_key_pointer, &privateKey, 1);
        // copy public key little endian to big endian
        for (uint8_t i = 0; i < 32; i++) {
            PARSED_TX_STAKING_INCOMING.validator_or_staker_signature_proof.public_key[i] =
                temporary_public_key_pointer->W[64 - i];
        }
        if (temporary_public_key_pointer->W[32] & 1) {
            PARSED_TX_STAKING_INCOMING.validator_or_staker_signature_proof.public_key[31] |= 0x80;
        }
    }

    // Create final transaction signature.
    // Note that we only generate the signature here. It's the calling library's responsibility to build an appropriate
    // signature proof or contract proof out of this signature, depending on the sender type.
#if CX_APILEVEL >= 8
    tx = cx_eddsa_sign(&privateKey, CX_LAST, CX_SHA512, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, NULL, 0, G_io_apdu_buffer, 64, NULL);
#else
    tx = cx_eddsa_sign(&privateKey, NULL, CX_LAST, CX_SHA512, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, G_io_apdu_buffer);
#endif
    os_memset(&privateKey, 0, sizeof(privateKey));

    if (created_staker_signature) {
        // Need to return the staker signature such that the caller can also update the staker signature proof in his tx
        os_memmove(G_io_apdu_buffer + tx, PARSED_TX_STAKING_INCOMING.validator_or_staker_signature_proof.signature, 64);
        tx += 64;
    }

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_on_transaction_rejected() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_on_message_approved() {
    uint32_t tx = 0;

    // initialize private key
    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    os_perso_derive_node_bip32_seed_key(/* mode */ HDW_ED25519_SLIP10, /* curve */ CX_CURVE_Ed25519,
        /* path */ ctx.req.msg.bip32Path, /* path length */ ctx.req.msg.bip32PathLength, /* out */ privateKeyData,
        /* chain */ NULL, /* seed key */ "ed25519 seed", /* seed key length */ 12);
    cx_ecfp_init_private_key(/* curve */ CX_CURVE_Ed25519, /* raw key */ privateKeyData,
        /* key length */ sizeof(privateKeyData), /* out */ &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    // Sign hashed message.
    // As specified in https://datatracker.ietf.org/doc/html/rfc8032#section-5.1.6, we're using CX_SHA512 as internal
    // hash algorithm for the ed25519 signature. Note that the passed message is a hash of length 32 against the
    // requirement "The data length must be lesser than the curve size." (which is 32 for ed25519) described in the sdk
    // documentation. But according to the specification, there are actually no length restrictions.
    // The signature has a fixed size of 64 bytes which fits a single apdu response.
    tx = cx_eddsa_sign(
        &privateKey,
        /* mode, unused for cx_eddsa_sign */ 0,
        /* internal hash algorithm */ CX_SHA512,
        /* input data */ ctx.req.msg.confirm.prefixedMessageHash,
        /* input length */ sizeof(ctx.req.msg.confirm.prefixedMessageHash),
        /* context, unused for cx_eddsa_sign */ NULL,
        /* context length */ 0,
        /* output */ G_io_apdu_buffer,
        /* max output length */ sizeof(G_io_apdu_buffer) - /* for sw */ 2,
        /* info, unused for cx_eddsa_sign */ NULL
    );
    os_memset(&privateKey, 0, sizeof(privateKey));

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_on_message_rejected() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    return 0; // do not redraw the widget
}

// delegate function for generic io_exchange, see ledger-nanos-secure-sdk
unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

uint32_t set_result_get_publicKey() {
    uint32_t tx = 0;

    uint8_t publicKey[32];
    // copy public key little endian to big endian
    uint8_t i;
    for (i = 0; i < 32; i++) {
        publicKey[i] = ctx.req.pk.publicKey.W[64 - i];
    }
    if ((ctx.req.pk.publicKey.W[32] & 1) != 0) {
        publicKey[31] |= 0x80;
    }

    os_memmove(G_io_apdu_buffer + tx, publicKey, 32);

    tx += 32;

    if (ctx.req.pk.returnSignature) {
        os_memmove(G_io_apdu_buffer + tx, ctx.req.pk.signature, 64);
        tx += 64;
    }

    return tx;
}

void handleGetPublicKey(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {

    if ((p1 != P1_SIGNATURE) && (p1 != P1_NO_SIGNATURE)) {
        PRINTF("Invalid P1");
        THROW(0x6B00);
    }
    if ((p2 != P2_CONFIRM) && (p2 != P2_NO_CONFIRM)) {
        PRINTF("Invalid P2");
        THROW(0x6B00);
    }
    ctx.req.pk.returnSignature = (p1 == P1_SIGNATURE);

    uint32_t bip32Path[MAX_BIP32_PATH_LENGTH];
    uint8_t bip32PathLength = readBip32Path(&dataBuffer, &dataLength, bip32Path);

    // Optionally create a signature with which the public key can be verified. We only allow signing messages up to 31
    // bytes, as we're blind signing here, and longer data could be Nimiq messages, which are 32 byte Sha256 digests, or
    // transactions, which have varying sizes but larger than 32 bytes.
    uint8_t msgLength;
    uint8_t *msg;
    if (ctx.req.pk.returnSignature) {
        if (dataLength > 31) {
            PRINTF("Verification message to sign must not exceed 31 bytes");
            THROW(0x6a80);
        }
        msgLength = (uint8_t) dataLength;
        msg = readSubBuffer(dataLength, &dataBuffer, &dataLength);
    }

    if (dataLength != 0) {
        PRINTF("INS_GET_PUBLIC_KEY instruction data too long");
        THROW(0x6700);
    }

    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, bip32Path, bip32PathLength, privateKeyData, NULL, "ed25519 seed", 12);

    cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    cx_ecfp_generate_pair(CX_CURVE_Ed25519, &ctx.req.pk.publicKey, &privateKey, 1);
    if (ctx.req.pk.returnSignature) {
#if CX_APILEVEL >= 8
        cx_eddsa_sign(&privateKey, CX_LAST, CX_SHA512, msg, msgLength, NULL, 0, ctx.req.pk.signature, sizeof(ctx.req.pk.signature), NULL);
#else
        cx_eddsa_sign(&privateKey, NULL, CX_LAST, CX_SHA512, msg, msgLength, ctx.req.pk.signature);
#endif
    }
    os_memset(&privateKey, 0, sizeof(privateKey));

    if (p2 & P2_CONFIRM) {
        uint8_t publicKey[32];
        // copy public key little endian to big endian
        uint8_t i;
        for (i = 0; i < 32; i++) {
            publicKey[i] = ctx.req.pk.publicKey.W[64 - i];
        }
        if ((ctx.req.pk.publicKey.W[32] & 1) != 0) {
            publicKey[31] |= 0x80;
        }
        print_public_key_as_address(publicKey, ctx.req.pk.address);
        ui_public_key();
        *flags |= IO_ASYNCH_REPLY;
    } else {
        *tx = set_result_get_publicKey();
        THROW(0x9000);
    }
}

void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {

    if ((p1 != P1_FIRST) && (p1 != P1_MORE)) {
        PRINTF("Invalid P1");
        THROW(0x6B00);
    }
    if ((p2 != P2_LAST) && (p2 != P2_MORE)) {
        PRINTF("Invalid P2");
        THROW(0x6B00);
    }

    if (p1 == P1_FIRST) {
        ctx.req.tx.bip32PathLength = readBip32Path(&dataBuffer, &dataLength, ctx.req.tx.bip32Path);
        ctx.req.tx.transactionVersion = readUInt8(&dataBuffer, &dataLength);

        // read raw tx data
        if (dataLength > MAX_RAW_TX) {
            PRINTF("Transaction too long");
            THROW(0x6700);
        }
        ctx.req.tx.rawTxLength = dataLength;
        os_memmove(ctx.req.tx.rawTx, dataBuffer, dataLength);
    } else {
        // read more raw tx data
        uint32_t offset = ctx.req.tx.rawTxLength;
        ctx.req.tx.rawTxLength += dataLength;
        if (ctx.req.tx.rawTxLength > MAX_RAW_TX) {
            PRINTF("Transaction too long");
            THROW(0x6700);
        }
        os_memmove(ctx.req.tx.rawTx+offset, dataBuffer, dataLength);
    }

    if (p2 == P2_MORE) {
        THROW(0x9000);
    }

    os_memset(&PARSED_TX, 0, sizeof(PARSED_TX));
    parseTx(ctx.req.tx.transactionVersion, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, &PARSED_TX);

    ui_transaction_signing();

    *flags |= IO_ASYNCH_REPLY;
}

void handleSignMessage(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags,
    volatile unsigned int *tx) {
    if ((p1 != P1_FIRST) && (p1 != P1_MORE)) {
        PRINTF("Invalid P1");
        THROW(0x6B00);
    }
    if ((p2 != P2_LAST) && (p2 != P2_MORE)) {
        PRINTF("Invalid P2");
        THROW(0x6B00);
    }

    if (p1 == P1_FIRST) {
        // Note: we expect the first chunk to at least contain the bip path, flags and encoded message length completely
        ctx.req.msg.bip32PathLength = readBip32Path(&dataBuffer, &dataLength, ctx.req.msg.bip32Path);
        ctx.req.msg.flags = readUInt8(&dataBuffer, &dataLength);
        ctx.req.msg.messageLength = readUInt32(&dataBuffer, &dataLength);

        ctx.req.msg.processedMessageLength = 0;
        ctx.req.msg.isPrintableAscii = ctx.req.msg.messageLength <= MAX_PRINTABLE_MESSAGE_LENGTH; // ascii-check later
        // See lcx_sha256.h and lcx_hash.h in Ledger sdk
        cx_sha256_init(&ctx.req.msg.prepare.messageHashContext);
        cx_sha256_init(&ctx.req.msg.prepare.prefixedMessageHashContext);

        // Nimiq signed messages add a prefix to the message and then hash both together.
        // This makes the calculated signature recognisable as a Nimiq specific signature and prevents signing arbitrary
        // data (e.g. a transaction). This implementation is equivalent to the handling in Key.signMessage in Nimiq's
        // Keyguard.
        cx_hash(&ctx.req.msg.prepare.prefixedMessageHashContext.header, /* flags */ 0,
            /* data */ MESSAGE_SIGNING_PREFIX, /* data length */ strlen(MESSAGE_SIGNING_PREFIX), /* output */ NULL,
            /* output length */ 0);
        // add data length printed as decimal number to the message prefix
        char decimalMessageLength[STRING_LENGTH_UINT32];
        // note: not %lu (for unsigned long int) because int is already 32bit on ledgers (see "Memory Alignment" in
        // Ledger docu), additionally Ledger's own implementation of sprintf does not support %lu (see os_printf.c)
        snprintf(decimalMessageLength, sizeof(decimalMessageLength), "%u", ctx.req.msg.messageLength);
        cx_hash(&ctx.req.msg.prepare.prefixedMessageHashContext.header, /* flags */ 0, /* data */ decimalMessageLength,
            /* data length */ strlen(decimalMessageLength), /* output */ NULL, /* output length */ 0);
    }

    if (dataLength != 0) {
        if ((unsigned long)ctx.req.msg.processedMessageLength + dataLength > ctx.req.msg.messageLength) {
            PRINTF("Message too long");
            THROW(0x6700);
        }
        // setup printable message
        if (ctx.req.msg.messageLength <= MAX_PRINTABLE_MESSAGE_LENGTH) {
            os_memmove(ctx.req.msg.printableMessage + ctx.req.msg.processedMessageLength, dataBuffer, dataLength);
            ctx.req.msg.isPrintableAscii = ctx.req.msg.isPrintableAscii && isPrintableAscii(dataBuffer, dataLength);
        }
        // hash message bytes
        cx_hash(&ctx.req.msg.prepare.messageHashContext.header, /* flags */ 0, /* data */ dataBuffer,
            /* data length */ dataLength, /* output */ NULL, /* output length */ 0);
        cx_hash(&ctx.req.msg.prepare.prefixedMessageHashContext.header, /* flags */ 0, /* data */ dataBuffer,
            /* data length */ dataLength, /* output */ NULL, /* output length */ 0);
        ctx.req.msg.processedMessageLength += dataLength; // guaranteed to not overflow due to the length check above
    }

    if (p2 == P2_MORE) {
        // Processing of current chunk finished; send success status word
        THROW(0x9000);
    }

    // Create hashes and request user to sign
    if (ctx.req.msg.processedMessageLength != ctx.req.msg.messageLength) {
        PRINTF("Invalid length to sign");
        THROW(0x6700);
    }
    // We're sharing the memory between the hash contexts and the created hashes to reduce memory usage. Make sure that
    // we're not overwriting the hash contexts while generating the hashes (which is also safe with the current cx_hash
    // implementation but could break anytime) by checking whether it fits the yet unused memory allocated in front of
    // the output messageHash and prefixedMessageHash. Note that this is a compile time assertion.
    _Static_assert(
        sizeof(ctx.req.msg.prepare.messageHashContext) + sizeof(ctx.req.msg.prepare.prefixedMessageHashContext)
            < sizeof(ctx.req.msg.confirm.printedMessageLabel) + sizeof(ctx.req.msg.confirm.printedMessage),
        "Hash context memory overlaps with output hash memory"
    );
    cx_hash(&ctx.req.msg.prepare.messageHashContext.header, /* flags */ CX_LAST, /* data */ NULL, /* data length */ 0,
        /* output */ ctx.req.msg.confirm.messageHash, /* output length */ sizeof(ctx.req.msg.confirm.messageHash));
    cx_hash(&ctx.req.msg.prepare.prefixedMessageHashContext.header, /* flags */ CX_LAST, /* data */ NULL,
        /* data length */ 0, /* output */ ctx.req.msg.confirm.prefixedMessageHash,
        /* output length */ sizeof(ctx.req.msg.confirm.prefixedMessageHash));
    ui_message_signing(
        // Depending on whether the data can be printed as ASCII or hex, default to ASCII, hex or hash display, unless
        // a specific preference was provided. The user can still switch the display type during the confirmation.
        ctx.req.msg.isPrintableAscii
            && !(ctx.req.msg.flags & (MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HEX | MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HASH))
            ? MESSAGE_DISPLAY_TYPE_ASCII
            : ctx.req.msg.messageLength <= MAX_PRINTABLE_MESSAGE_LENGTH
                && !(ctx.req.msg.flags & MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HASH)
                ? MESSAGE_DISPLAY_TYPE_HEX
                : MESSAGE_DISPLAY_TYPE_HASH,
        false
    );

    *flags |= IO_ASYNCH_REPLY;
}

void handleKeepAlive(volatile unsigned int *flags) {
    *flags |= IO_ASYNCH_REPLY;
}

void handleApdu(volatile unsigned int *flags, volatile unsigned int *tx) {
    unsigned short sw = 0;

    BEGIN_TRY {
        TRY {
            if (os_global_pin_is_validated() != BOLOS_UX_OK) {
                PRINTF("Device locked");
                THROW(0x6982);
            }
            if (G_io_apdu_buffer[OFFSET_CLA] != CLA) {
                PRINTF("Ivalid CLA");
                THROW(0x6e00);
            }

            ctx.u2fTimer = U2F_REQUEST_TIMEOUT;

            switch (G_io_apdu_buffer[OFFSET_INS]) {
            case INS_GET_PUBLIC_KEY:
                handleGetPublicKey(G_io_apdu_buffer[OFFSET_P1],
                                   G_io_apdu_buffer[OFFSET_P2],
                                   G_io_apdu_buffer + OFFSET_CDATA,
                                   G_io_apdu_buffer[OFFSET_LC],
                                   flags, tx);
                break;

            case INS_SIGN_TX:
                handleSignTx(G_io_apdu_buffer[OFFSET_P1],
                             G_io_apdu_buffer[OFFSET_P2],
                             G_io_apdu_buffer + OFFSET_CDATA,
                             G_io_apdu_buffer[OFFSET_LC],
                             flags, tx);
                break;

            case INS_KEEP_ALIVE:
                handleKeepAlive(flags);
                break;

            case INS_SIGN_MESSAGE:
                handleSignMessage(G_io_apdu_buffer[OFFSET_P1],
                                  G_io_apdu_buffer[OFFSET_P2],
                                  G_io_apdu_buffer + OFFSET_CDATA,
                                  G_io_apdu_buffer[OFFSET_LC],
                                  flags, tx);
                break;

            default:
                PRINTF("Invalid instruction");
                THROW(0x6D00);
                break;
            }
        }
        CATCH(EXCEPTION_IO_RESET) {
            THROW(EXCEPTION_IO_RESET);
        }
        CATCH_OTHER(e) {
            switch (e & 0xF000) {
            case 0x6000:
                // Wipe the transaction context and report the exception
                sw = e;
                os_memset(&PARSED_TX, 0, sizeof(PARSED_TX));
                break;
            case 0x9000:
                // All is well
                sw = e;
                break;
            default:
                // Internal error
                sw = 0x6800 | (e & 0x7FF);
                break;
            }
            // Unexpected exception => report
            G_io_apdu_buffer[*tx] = sw >> 8;
            G_io_apdu_buffer[*tx + 1] = sw;
            *tx += 2;
        }
        FINALLY {
        }
    }
    END_TRY;
}

void nimiq_main(void) {
    volatile unsigned int rx = 0; // length of apdu to exchange
    volatile unsigned int tx = 0; // length of our response
    volatile unsigned int flags = 0;

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the apdu is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        volatile unsigned short sw = 0;

        BEGIN_TRY {
            TRY {
                rx = tx;
                tx = 0; // ensure no race in catch_other if io_exchange throws
                        // an error
                // send current apdu response of length rx, read new apdu into G_io_apdu_buffer
                // and set rx to the length of the received apdu.
                rx = io_exchange(CHANNEL_APDU | flags, rx);
                flags = 0;

                // no or invalid length apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx < OFFSET_LC + 1 || rx != G_io_apdu_buffer[OFFSET_LC] + OFFSET_CDATA) {
                    PRINTF("No or invalid length APDU received");
                    THROW(0x6982);
                }

                PRINTF("New APDU received:\n%.*H\n", rx, G_io_apdu_buffer);

                handleApdu(&flags, &tx);
            }
            CATCH(EXCEPTION_IO_RESET) {
                THROW(EXCEPTION_IO_RESET);
            }
            CATCH_OTHER(e) {
                switch (e & 0xF000) {
                case 0x6000:
                    // Wipe the transaction context and report the exception
                    sw = e;
                    os_memset(&PARSED_TX, 0, sizeof(PARSED_TX));
                    break;
                case 0x9000:
                    // All is well
                    sw = e;
                    break;
                default:
                    // Internal error
                    sw = 0x6800 | (e & 0x7FF);
                    break;
                }
                // Unexpected exception => report
                G_io_apdu_buffer[tx] = sw >> 8;
                G_io_apdu_buffer[tx + 1] = sw;
                tx += 2;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

    // return_to_dashboard:
    return;
}

void u2fSendKeepAlive() {
    ctx.u2fTimer = 0;
    G_io_apdu_buffer[0] = 0x6e;
    G_io_apdu_buffer[1] = 0x02;
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_STATUS_EVENT:
        if (G_io_app.apdu_media == IO_APDU_MEDIA_USB_HID &&
            !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
              SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
            THROW(EXCEPTION_IO_RESET);
        }
    // no break is intentional

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
#ifdef HAVE_BAGL
        UX_DISPLAYED_EVENT({});
#endif // HAVE_BAGL
#ifdef HAVE_NBGL
        UX_DEFAULT_EVENT();
#endif // HAVE_NBGL
        break;

#ifdef HAVE_NBGL
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;
#endif // HAVE_NBGL

    case SEPROXYHAL_TAG_TICKER_EVENT:
        if (G_io_app.apdu_media == IO_APDU_MEDIA_U2F && ctx.u2fTimer > 0) {
            ctx.u2fTimer -= 100;
            if (ctx.u2fTimer <= 0) {
                u2fSendKeepAlive();
            }
        }
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {});
        break;

    default:
        UX_DEFAULT_EVENT();
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

void app_exit(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}


__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        os_memset(&PARSED_TX, 0, sizeof(PARSED_TX));

        UX_INIT();
        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

                if (N_storage.initialized != 0x01) {
                    internal_storage_t storage;
                    storage.fidoTransport = 0x01;
                    storage.initialized = 0x01;
                    nvm_write(&N_storage, (void *)&storage, sizeof(internal_storage_t));
                }

                // deactivate usb before activating
                USB_power(0);
                USB_power(1);

                ui_menu_main();

#ifdef HAVE_BLE
                BLE_power(0, NULL);
                BLE_power(1, "Nano X");
#endif // HAVE_BLE

                nimiq_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
                // reset IO and UX
                CLOSE_TRY;
                continue;
            }
            CATCH_ALL {
                CLOSE_TRY;
                break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }
    app_exit();

    return 0;
}
