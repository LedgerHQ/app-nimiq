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

#include "constants.h"
#include "globals.h"
#include "error_macros.h"
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

void on_rejected();
void on_address_approved();
void on_transaction_approved();
void on_message_approved();
static error_t set_result_get_public_key(uint8_t *destination, uint16_t destination_length, uint16_t *out_data_length);

/**
 * Send off a response APDU for an async request previously initiated in an io_exchange with flag IO_ASYNCH_REPLY. For
 * this purpose, the reply is sent with the IO_RETURN_AFTER_TX flag.
 */
static void io_send_async_response_apdu(uint8_t *data, uint16_t data_length, sw_t sw) {
    if (data_length > sizeof(G_io_apdu_buffer) - /* for sw */ 2) {
        sw = SW_WRONG_DATA_LENGTH;
    }
    if (sw != SW_OK) {
        // Enforce only sending an error code.
        data_length = 0;
    }
    if (data_length && data != G_io_apdu_buffer) {
        memmove(G_io_apdu_buffer, data, data_length);
    }
    // Append status word
    G_io_apdu_buffer[data_length] = (uint8_t) (sw >> 8);
    G_io_apdu_buffer[data_length + 1] = (uint8_t) (sw);
    // Send APDU with flag IO_RETURN_AFTER_TX, in response to a previous call with IO_ASYNCH_REPLY. Note that an
    // io_exchange call with IO_ASYNCH_REPLY skips sending any data, which we now make up for by submitting the data at
    // a later time. On the other hand, IO_RETURN_AFTER_TX returns after the data transmission, and skips waiting for a
    // new command APDU, which the previous io_exchange call with IO_ASYNCH_REPLY is already waiting for. The command
    // loop will continue at the previous IO_ASYNCH_REPLY io_exchange call, once a new command APDU is received. This
    // way, both io_exchange calls work nicely hand in hand.
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, data_length + /* for sw */ 2);
}

void on_rejected() {
    PRINTF("User rejected the request.\n");
    io_send_async_response_apdu(NULL, 0, SW_DENY);
}

void on_address_approved() {
    sw_t sw = SW_OK;
    uint16_t data_length = 0;
    ON_ERROR(
        set_result_get_public_key(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), &data_length),
        { sw = ERROR_TO_SW(); }
    );
    io_send_async_response_apdu(G_io_apdu_buffer, data_length, sw);
}

void on_transaction_approved() {
    sw_t sw = SW_OK;
    uint16_t data_length = 0;

    // initialize private key
    uint8_t privateKeyData[64]; // the private key is only 32 bytes, but os_derive_bip32_with_seed_no_throw expects 64
    cx_ecfp_private_key_t privateKey;
    GOTO_ON_ERROR(
        os_derive_bip32_with_seed_no_throw(
            /* derivation mode */ HDW_ED25519_SLIP10,
            /* curve */ CX_CURVE_Ed25519,
            /* path */ ctx.req.tx.bip32Path,
            /* path length */ ctx.req.tx.bip32PathLength,
            /* out */ privateKeyData,
            /* chain code */ NULL,
            /* seed key */ "ed25519 seed",
            /* seed key length */ 12
        )
        || cx_ecfp_init_private_key_no_throw(
            /* curve */ CX_CURVE_Ed25519,
            /* raw key */ privateKeyData,
            /* key length */ 32,
            /* out */ &privateKey
        ),
        end,
        sw,
        SW_CRYPTOGRAPHY_FAIL,
        "Failed to derive private key\n"
    );
    // The private key data is also cleared at the end, but for extra paranoia, clear it as soon as possible.
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
        GOTO_ON_ERROR(
            // As specified in datatracker.ietf.org/doc/html/rfc8032#section-5.1.6, we're using CX_SHA512 as internal
            // hash algorithm for the ed25519 signature. According to the specification, there is no length restriction
            // for the data. The signature has a fixed size of 64 bytes.
            cx_eddsa_sign_no_throw(
                /* private key */ &privateKey,
                /* hash id */ CX_SHA512,
                /* hash */ ctx.req.tx.rawTx,
                /* hash length */ ctx.req.tx.rawTxLength,
                /* out */ G_io_apdu_buffer,
                /* out length */ 64
            ),
            end,
            sw,
            SW_CRYPTOGRAPHY_FAIL,
            "Failed to sign\n"
        );
        // Overwrite the signature in the signature proof in rawTx via pointers in validator_or_staker_signature_proof
        // which point to the original buffer.
        os_memmove(PARSED_TX_STAKING_INCOMING.validator_or_staker_signature_proof.signature, G_io_apdu_buffer, 64);
        created_staker_signature = true;

        // Similarly, overwrite the public key in the signature proof with the ledger account public key as staker, with
        // G_io_apdu_buffer as temporary buffer again. Check with a compile time assertion that it can fit the temp data
        _Static_assert(
            sizeof(cx_ecfp_public_key_t) <= sizeof(G_io_apdu_buffer),
            "G_io_apdu_buffer does not fit public key\n"
        );
        cx_ecfp_public_key_t *temporary_public_key_pointer = (cx_ecfp_public_key_t*) G_io_apdu_buffer;
        GOTO_ON_ERROR(
            cx_ecfp_generate_pair_no_throw(
                /* curve */ CX_CURVE_Ed25519,
                /* out */ temporary_public_key_pointer,
                /* private key */ &privateKey,
                /* keep private key */ true
            ),
            end,
            sw,
            SW_CRYPTOGRAPHY_FAIL,
            "Failed to generate public key\n"
        );
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
    GOTO_ON_ERROR(
        // As specified in datatracker.ietf.org/doc/html/rfc8032#section-5.1.6, we're using CX_SHA512 as internal
        // hash algorithm for the ed25519 signature. According to the specification, there is no length restriction
        // for the data. The signature has a fixed size of 64 bytes.
        cx_eddsa_sign_no_throw(
            /* private key */ &privateKey,
            /* hash id */ CX_SHA512,
            /* hash */ ctx.req.tx.rawTx,
            /* hash length */ ctx.req.tx.rawTxLength,
            /* out */ G_io_apdu_buffer,
            /* out length */ 64
        ),
        end,
        sw,
        SW_CRYPTOGRAPHY_FAIL,
        "Failed to sign\n"
    );
    data_length = 64;

    if (created_staker_signature) {
        // Need to return the staker signature such that the caller can also update the staker signature proof in his tx
        os_memmove(
            G_io_apdu_buffer + data_length,
            PARSED_TX_STAKING_INCOMING.validator_or_staker_signature_proof.signature,
            64
        );
        data_length += 64;
    }

end:
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    os_memset(&privateKey, 0, sizeof(privateKey));
    io_send_async_response_apdu(G_io_apdu_buffer, data_length, sw);
}

void on_message_approved() {
    sw_t sw = SW_OK;
    uint16_t data_length = 64; // Response is a single signature, which fits a single APDU response.

    // initialize private key
    uint8_t privateKeyData[64]; // the private key is only 32 bytes, but os_derive_bip32_with_seed_no_throw expects 64
    cx_ecfp_private_key_t privateKey;
    ON_ERROR(
        os_derive_bip32_with_seed_no_throw(
            /* derivation mode */ HDW_ED25519_SLIP10,
            /* curve */ CX_CURVE_Ed25519,
            /* path */ ctx.req.msg.bip32Path,
            /* path length */ ctx.req.msg.bip32PathLength,
            /* out */ privateKeyData,
            /* chain code */ NULL,
            /* seed key */ "ed25519 seed",
            /* seed key length */ 12
        )
        || cx_ecfp_init_private_key_no_throw(
            /* curve */ CX_CURVE_Ed25519,
            /* raw key */ privateKeyData,
            /* key length */ 32,
            /* out */ &privateKey
        )
        // Sign hashed message.
        // As specified in datatracker.ietf.org/doc/html/rfc8032#section-5.1.6, we're using CX_SHA512 as internal hash
        // algorithm for the ed25519 signature. According to the specification, there is no length restriction for the
        // data. The signature has a fixed size of 64 bytes.
        || cx_eddsa_sign_no_throw(
            /* private key */ &privateKey,
            /* hash id */ CX_SHA512,
            /* hash */ ctx.req.msg.confirm.prefixedMessageHash,
            /* hash length */ sizeof(ctx.req.msg.confirm.prefixedMessageHash),
            /* out */ G_io_apdu_buffer,
            /* out length */ 64
        ),
        {
            sw = error;
            data_length = 0;
        },
        SW_CRYPTOGRAPHY_FAIL,
        "Failed to derive private key or to sign\n"
    );
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    os_memset(&privateKey, 0, sizeof(privateKey));

    io_send_async_response_apdu(G_io_apdu_buffer, data_length, sw);
}

void u2f_send_keep_alive() {
    PRINTF("Send U2F heartbeat\n");
    ctx.u2fTimer = 0;
    io_send_async_response_apdu(NULL, 0, SW_KEEP_ALIVE);
}

WARN_UNUSED_RESULT
static error_t set_result_get_public_key(uint8_t *destination, uint16_t destination_length, uint16_t *out_data_length) {
    *out_data_length = 0;
    RETURN_ON_ERROR(
        destination_length < /* public key */ 32 + /* verification signature */ (ctx.req.pk.returnSignature ? 64 : 0),
        ERROR_INVALID_LENGTH,
        "Buffer too short to fit public key or verification signature\n"
    );

    // Copy public key little endian to big endian
    for (uint8_t i = 0; i < 32; i++) {
        destination[i] = ctx.req.pk.publicKey.W[64 - i];
    }
    if ((ctx.req.pk.publicKey.W[32] & 1) != 0) {
        destination[31] |= 0x80;
    }
    *out_data_length += 32;

    // Add verification signature
    if (ctx.req.pk.returnSignature) {
        memmove(destination + *out_data_length, ctx.req.pk.signature, 64);
        *out_data_length += 64;
    }

    return ERROR_NONE;
}

WARN_UNUSED_RESULT
sw_t handle_get_public_key(uint8_t p1, uint8_t p2, uint8_t *data_buffer, uint16_t data_length, unsigned int *out_flags,
    uint16_t *out_apdu_length) {
    *out_flags = 0;
    *out_apdu_length = 0;
    sw_t sw = SW_OK;

    uint8_t privateKeyData[64]; // the private key is only 32 bytes, but os_derive_bip32_with_seed_no_throw expects 64
    cx_ecfp_private_key_t privateKey;

    GOTO_ON_ERROR(
        ((p1 != P1_SIGNATURE) && (p1 != P1_NO_SIGNATURE))
        || ((p2 != P2_CONFIRM) && (p2 != P2_NO_CONFIRM)),
        end,
        sw,
        SW_WRONG_P1P2,
        "Invalid P1 or P2\n"
    );

    ctx.req.pk.returnSignature = (p1 == P1_SIGNATURE);

    uint32_t bip32Path[MAX_BIP32_PATH_LENGTH];
    uint8_t bip32PathLength;
    GOTO_ON_ERROR(
        !read_bip32_path(&data_buffer, &data_length, bip32Path, &bip32PathLength),
        end,
        sw,
        SW_WRONG_DATA_LENGTH
    );

    // Optionally create a signature with which the public key can be verified. We only allow signing messages up to 31
    // bytes, as we're blind signing here, and longer data could be Nimiq messages, which are 32 byte Sha256 digests, or
    // transactions, which have varying sizes but larger than 32 bytes.
    uint8_t msgLength;
    uint8_t *msg;
    if (ctx.req.pk.returnSignature) {
        GOTO_ON_ERROR(
            data_length > 31,
            end,
            sw,
            SW_INCORRECT_DATA,
            "Verification message to sign must not exceed 31 bytes\n"
        );
        msgLength = (uint8_t) data_length;
        GOTO_ON_ERROR(
            !read_sub_buffer(data_length, &data_buffer, &data_length, &msg),
            end,
            sw,
            SW_WRONG_DATA_LENGTH
        );
    }

    GOTO_ON_ERROR(
        data_length != 0,
        end,
        sw,
        SW_WRONG_DATA_LENGTH,
        "INS_GET_PUBLIC_KEY instruction data too long\n"
    );

    GOTO_ON_ERROR(
        os_derive_bip32_with_seed_no_throw(
            /* derivation mode */ HDW_ED25519_SLIP10,
            /* curve */ CX_CURVE_Ed25519,
            /* path */ bip32Path,
            /* path length */ bip32PathLength,
            /* out */ privateKeyData,
            /* chain code */ NULL,
            /* seed key */ "ed25519 seed",
            /* seed key length */ 12
        )
        || cx_ecfp_init_private_key_no_throw(
            /* curve */ CX_CURVE_Ed25519,
            /* raw key */ privateKeyData,
            /* key length */ 32,
            /* out */ &privateKey
        ),
        end,
        sw,
        SW_CRYPTOGRAPHY_FAIL,
        "Failed to derive private key\n"
    );
    // The private key data is also cleared at the end, but for extra paranoia, clear it as soon as possible.
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    GOTO_ON_ERROR(
        cx_ecfp_generate_pair_no_throw(
            /* curve */ CX_CURVE_Ed25519,
            /* out */ &ctx.req.pk.publicKey,
            /* private key */ &privateKey,
            /* keep private key */ true
        ),
        end,
        sw,
        SW_CRYPTOGRAPHY_FAIL,
        "Failed to generate public key\n"
    );
    if (ctx.req.pk.returnSignature) {
        GOTO_ON_ERROR(
            cx_eddsa_sign_no_throw(
                /* private key */ &privateKey,
                /* hash id */ CX_SHA512,
                /* hash */ msg,
                /* hash length */ msgLength,
                /* out */ ctx.req.pk.signature,
                /* out length */ sizeof(ctx.req.pk.signature)
            ),
            end,
            sw,
            SW_CRYPTOGRAPHY_FAIL,
            "Failed to sign\n"
        );
    }
    // The private key is also cleared at the end, but for extra paranoia, clear it as soon as possible.
    os_memset(&privateKey, 0, sizeof(privateKey));

    if (p2 & P2_CONFIRM) {
        // Async request, in which we display the address and ask the user to confirm.
        uint8_t publicKey[32];
        // copy public key little endian to big endian
        uint8_t i;
        for (i = 0; i < 32; i++) {
            publicKey[i] = ctx.req.pk.publicKey.W[64 - i];
        }
        if ((ctx.req.pk.publicKey.W[32] & 1) != 0) {
            publicKey[31] |= 0x80;
        }
        GOTO_ON_ERROR(
            print_public_key_as_address(publicKey, ctx.req.pk.address),
            end,
            sw,
            ERROR_TO_SW(),
            "Failed to print public key\n"
        );

        ui_public_key();
        *out_flags = IO_ASYNCH_REPLY;
    } else {
        // Sync request, in which the public key is returned without any user interaction.
        GOTO_ON_ERROR(
            set_result_get_public_key(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), out_apdu_length),
            end,
            sw,
            ERROR_TO_SW()
        );
    }

end:
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    os_memset(&privateKey, 0, sizeof(privateKey));
    return sw;
}

WARN_UNUSED_RESULT
sw_t handle_sign_transaction(uint8_t p1, uint8_t p2, uint8_t *data_buffer, uint16_t data_length,
    unsigned int *out_flags) {
    *out_flags = 0;

    RETURN_ON_ERROR(
        ((p1 != P1_FIRST) && (p1 != P1_MORE))
        || ((p2 != P2_LAST) && (p2 != P2_MORE)),
        SW_WRONG_P1P2,
        "Invalid P1 or P2\n"
    );

    if (p1 == P1_FIRST) {
        _Static_assert(
            sizeof(ctx.req.tx.transactionVersion) == 1,
            "transactionVersion has more than one byte. Need to take endianness into account when reading into a u8 "
                "pointer.\n"
        );
        RETURN_ON_ERROR(
            !read_bip32_path(&data_buffer, &data_length, ctx.req.tx.bip32Path, &ctx.req.tx.bip32PathLength)
            || !read_u8(&data_buffer, &data_length, &ctx.req.tx.transactionVersion),
            SW_WRONG_DATA_LENGTH
        );

        // read raw tx data
        RETURN_ON_ERROR(
            data_length > MAX_RAW_TX,
            SW_WRONG_DATA_LENGTH,
            "Transaction too long\n"
        );
        ctx.req.tx.rawTxLength = data_length;
        os_memmove(ctx.req.tx.rawTx, data_buffer, data_length);
    } else {
        // read more raw tx data
        uint32_t offset = ctx.req.tx.rawTxLength;
        ctx.req.tx.rawTxLength += data_length;
        RETURN_ON_ERROR(
            ctx.req.tx.rawTxLength > MAX_RAW_TX,
            SW_WRONG_DATA_LENGTH,
            "Transaction too long\n"
        );
        os_memmove(ctx.req.tx.rawTx+offset, data_buffer, data_length);
    }

    if (p2 == P2_MORE) {
        // Processing of current chunk finished; send success status word and let the caller continue with more chunks.
        return SW_OK;
    }

    os_memset(&PARSED_TX, 0, sizeof(PARSED_TX));
    RETURN_ON_ERROR(
        parse_tx(ctx.req.tx.transactionVersion, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, &PARSED_TX),
        ERROR_TO_SW(),
        "Failed to parse transaction\n"
    );

    ui_transaction_signing();
    *out_flags = IO_ASYNCH_REPLY;
    return SW_OK;
}

WARN_UNUSED_RESULT
sw_t handle_sign_message(uint8_t p1, uint8_t p2, uint8_t *data_buffer, uint16_t data_length, unsigned int *out_flags) {
    *out_flags = 0;

    RETURN_ON_ERROR(
        ((p1 != P1_FIRST) && (p1 != P1_MORE))
        || ((p2 != P2_LAST) && (p2 != P2_MORE)),
        SW_WRONG_P1P2,
        "Invalid P1 or P2\n"
    );

    if (p1 == P1_FIRST) {
        // Note: we expect the first chunk to at least contain the bip path, flags and encoded message length completely
        RETURN_ON_ERROR(
            !read_bip32_path(&data_buffer, &data_length, ctx.req.msg.bip32Path, &ctx.req.msg.bip32PathLength)
            || !read_u8(&data_buffer, &data_length, &ctx.req.msg.flags)
            || !read_u32(&data_buffer, &data_length, &ctx.req.msg.messageLength),
            SW_WRONG_DATA_LENGTH
        );

        ctx.req.msg.processedMessageLength = 0;
        ctx.req.msg.isPrintableAscii = ctx.req.msg.messageLength <= MAX_PRINTABLE_MESSAGE_LENGTH; // ascii-check later
        // Note that cx_sha256_init never throws and is not deprecated. See lcx_sha256.h and lcx_hash.h in Ledger sdk.
        cx_sha256_init(&ctx.req.msg.prepare.messageHashContext);
        cx_sha256_init(&ctx.req.msg.prepare.prefixedMessageHashContext);

        // Nimiq signed messages add a prefix to the message and then hash both together.
        // This makes the calculated signature recognisable as a Nimiq specific signature and prevents signing arbitrary
        // data (e.g. a transaction). This implementation is equivalent to the handling in Key.signMessage in Nimiq's
        // Keyguard.
        RETURN_ON_ERROR(
            cx_hash_update(
                /* hash context */ &ctx.req.msg.prepare.prefixedMessageHashContext.header,
                /* data */ MESSAGE_SIGNING_PREFIX,
                /* data length */ strlen(MESSAGE_SIGNING_PREFIX)
            ),
            SW_CRYPTOGRAPHY_FAIL,
            "Failed to update message hash\n"
        );
        // add data length printed as decimal number to the message prefix
        char decimalMessageLength[STRING_LENGTH_UINT32];
        // note: not %lu (for unsigned long int) because int is already 32bit on ledgers (see "Memory Alignment" in
        // Ledger docu), additionally Ledger's own implementation of sprintf does not support %lu (see os_printf.c)
        snprintf(decimalMessageLength, sizeof(decimalMessageLength), "%u", ctx.req.msg.messageLength);
        RETURN_ON_ERROR(
            cx_hash_update(
                /* hash context */ &ctx.req.msg.prepare.prefixedMessageHashContext.header,
                /* data */ decimalMessageLength,
                /* data length */ strlen(decimalMessageLength)
            ),
            SW_CRYPTOGRAPHY_FAIL,
            "Failed to update message hash\n"
        );
    }

    if (data_length != 0) {
        RETURN_ON_ERROR(
            (unsigned long)ctx.req.msg.processedMessageLength + data_length > ctx.req.msg.messageLength,
            SW_WRONG_DATA_LENGTH,
            "Message too long\n"
        );
        // setup printable message
        if (ctx.req.msg.messageLength <= MAX_PRINTABLE_MESSAGE_LENGTH) {
            os_memmove(ctx.req.msg.printableMessage + ctx.req.msg.processedMessageLength, data_buffer, data_length);
            ctx.req.msg.isPrintableAscii = ctx.req.msg.isPrintableAscii && is_printable_ascii(data_buffer, data_length);
        }
        // hash message bytes
        RETURN_ON_ERROR(
            cx_hash_update(
                /* has context */ &ctx.req.msg.prepare.messageHashContext.header,
                /* data */ data_buffer,
                /* data length */ data_length
            )
            || cx_hash_update(
                /* hash context */ &ctx.req.msg.prepare.prefixedMessageHashContext.header,
                /* data */ data_buffer,
                /* data length */ data_length
            ),
            SW_CRYPTOGRAPHY_FAIL,
            "Failed to update message hash\n"
        );
        ctx.req.msg.processedMessageLength += data_length; // guaranteed to not overflow due to the length check above
    }

    if (p2 == P2_MORE) {
        // Processing of current chunk finished; send success status word and let the caller continue with more chunks.
        return SW_OK;
    }

    // Create hashes and request user to sign
    RETURN_ON_ERROR(
        ctx.req.msg.processedMessageLength != ctx.req.msg.messageLength,
        SW_WRONG_DATA_LENGTH,
        "Invalid message length\n"
    );
    // We're sharing the memory between the hash contexts and the created hashes to reduce memory usage. Make sure that
    // we're not overwriting the hash contexts while generating the hashes (which is also safe with the current cx_hash
    // implementation but could break anytime) by checking whether it fits the yet unused memory allocated in front of
    // the output messageHash and prefixedMessageHash. Note that this is a compile time assertion.
    _Static_assert(
        sizeof(ctx.req.msg.prepare.messageHashContext) + sizeof(ctx.req.msg.prepare.prefixedMessageHashContext)
            < sizeof(ctx.req.msg.confirm.printedMessageLabel) + sizeof(ctx.req.msg.confirm.printedMessage),
        "Hash context memory overlaps with output hash memory\n"
    );
#if defined(NIMIQ_DEBUG) && NIMIQ_DEBUG
    // Check output size manually, because cx_hash_final doesn't check it, in contrast to cx_hash_no_throw.
    RETURN_ON_ERROR(
        cx_hash_get_size(&ctx.req.msg.prepare.messageHashContext.header)
            > sizeof(ctx.req.msg.confirm.messageHash)
        || cx_hash_get_size(&ctx.req.msg.prepare.prefixedMessageHashContext.header)
            > sizeof(ctx.req.msg.confirm.prefixedMessageHash),
        SW_CRYPTOGRAPHY_FAIL,
        "Invalid message hash output length\n"
    );
#endif
    RETURN_ON_ERROR(
        cx_hash_final(
            /* hash context */ &ctx.req.msg.prepare.messageHashContext.header,
            /* output */ ctx.req.msg.confirm.messageHash
        )
        || cx_hash_final(
            /* hash context */ &ctx.req.msg.prepare.prefixedMessageHashContext.header,
            /* output */ ctx.req.msg.confirm.prefixedMessageHash
        ),
        SW_CRYPTOGRAPHY_FAIL,
        "Failed to finalize message hash\n"
    );

    ui_message_signing(
        // Depending on whether the data can be printed as ASCII or hex, default to ASCII, hex or hash display, unless
        // a specific preference was provided. The user can still switch the display type during the confirmation (not
        // implemented yet for NBGL).
        ctx.req.msg.isPrintableAscii
            && !(ctx.req.msg.flags & (MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HEX | MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HASH))
            ? MESSAGE_DISPLAY_TYPE_ASCII
            : ctx.req.msg.messageLength <= MAX_PRINTABLE_MESSAGE_LENGTH
                && !(ctx.req.msg.flags & MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HASH)
                ? MESSAGE_DISPLAY_TYPE_HEX
                : MESSAGE_DISPLAY_TYPE_HASH,
        false
    );
    *out_flags = IO_ASYNCH_REPLY;
    return SW_OK;
}

WARN_UNUSED_RESULT
sw_t handle_keep_alive(unsigned int *out_flags) {
    // Renew an async reply interrupted by u2f_send_keep_alive, which was then followed by a client request of
    // INS_KEEP_ALIVE to continue the request, by starting a new async reply, which can eventually be resolved with the
    // actual reply, or another keep alive timeout.
    *out_flags = IO_ASYNCH_REPLY;
    return SW_OK;
}

WARN_UNUSED_RESULT
sw_t handle_apdu(unsigned int *out_flags, uint16_t *out_apdu_length) {
    *out_flags = 0;
    *out_apdu_length = 0;

    // Don't need to check for os_global_pin_is_validated(), as in newer SDK versions it's already done in io_exchange.

    RETURN_ON_ERROR(
        G_io_apdu_buffer[OFFSET_CLA] != CLA,
        SW_CLA_NOT_SUPPORTED,
        "Invalid CLA\n"
    );

    ctx.u2fTimer = U2F_REQUEST_TIMEOUT;

    switch (G_io_apdu_buffer[OFFSET_INS]) {
        case INS_GET_PUBLIC_KEY:
            PRINTF("Handle INS_GET_PUBLIC_KEY\n");
            return handle_get_public_key(
                G_io_apdu_buffer[OFFSET_P1],
                G_io_apdu_buffer[OFFSET_P2],
                G_io_apdu_buffer + OFFSET_CDATA,
                G_io_apdu_buffer[OFFSET_LC],
                out_flags,
                out_apdu_length
            );
        case INS_SIGN_TX:
            PRINTF("Handle INS_SIGN_TX\n");
            return handle_sign_transaction(
                G_io_apdu_buffer[OFFSET_P1],
                G_io_apdu_buffer[OFFSET_P2],
                G_io_apdu_buffer + OFFSET_CDATA,
                G_io_apdu_buffer[OFFSET_LC],
                out_flags
            );
        case INS_SIGN_MESSAGE:
            PRINTF("Handle INS_SIGN_MESSAGE\n");
            return handle_sign_message(
                G_io_apdu_buffer[OFFSET_P1],
                G_io_apdu_buffer[OFFSET_P2],
                G_io_apdu_buffer + OFFSET_CDATA,
                G_io_apdu_buffer[OFFSET_LC],
                out_flags
            );
        case INS_KEEP_ALIVE:
            PRINTF("Handle INS_KEEP_ALIVE\n");
            return handle_keep_alive(out_flags);
        default:
            RETURN_ERROR(
                SW_INS_NOT_SUPPORTED,
                "Invalid instruction\n"
            );
    }
}

void nimiq_main() {
    unsigned int flags = 0;
    uint16_t command_apdu_length = 0; // length of a received APDU
    uint16_t response_apdu_length = 0; // length of our response APDU

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the APDU is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        command_apdu_length = io_exchange(CHANNEL_APDU | flags, response_apdu_length);

        sw_t sw;
        if (command_apdu_length < OFFSET_LC + 1
            || command_apdu_length != G_io_apdu_buffer[OFFSET_LC] + OFFSET_CDATA) {
            PRINTF("No or invalid length APDU received\n");
            flags = 0;
            response_apdu_length = 0;
            sw = SW_WRONG_DATA_LENGTH;
        } else {
            PRINTF("New APDU received:\n%.*H\n", command_apdu_length, G_io_apdu_buffer);
            sw = handle_apdu(&flags, &response_apdu_length);
        }

        if (sw != SW_OK) {
            // Wipe global data to ensure it can't be continued to be used or misinterpreted.
            os_memset(&ctx, 0, sizeof(ctx));
            flags = 0;
            response_apdu_length = 0;
        }

        // Finalize the APDU response by appending the status word. The final response will be sent out with io_exchange
        // in the next loop iteration.
        G_io_apdu_buffer[response_apdu_length++] = (uint8_t) (sw >> 8);
        G_io_apdu_buffer[response_apdu_length++] = (uint8_t) (sw);
    }
}

// Handler that io_exchange delegates to for channels other than CHANNEL_APDU, to handle data exchange via the hal
// (hardware abstaction layer), see os_io_seproxyhal.c in ledger-secure-sdk.
unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
        case CHANNEL_KEYBOARD:
            break;

        case CHANNEL_SPI:
            // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
            if (tx_len) {
                io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

                if (channel & IO_RESET_AFTER_REPLIED) {
                    reset();
                }
                return 0; // nothing received from the master so far (it's a tx transaction)
            } else {
                return io_seproxyhal_spi_recv(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), 0);
            }

        default:
            THROW(INVALID_PARAMETER);
    }
    return 0;
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if needed

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
                if (ctx.u2fTimer == 0) {
                    u2f_send_keep_alive();
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
    os_sched_exit(-1);
}


__attribute__((section(".boot"))) int main() {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        os_memset(&ctx, 0, sizeof(ctx));

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
                BLE_power(1, NULL);
#endif // HAVE_BLE

                nimiq_main();
            }
            CATCH_OTHER(e) {
                // Note even though we're jumping out of the try block, we don't need to call CLOSE_TRY here, because
                // CATCH_OTHER already does so automatically.
                if (e == EXCEPTION_IO_RESET || e == INVALID_STATE) continue; // Reset IO and UX and restart.
                else break; // On other exceptions terminate the application.
            }
            FINALLY {
            }
        }
        END_TRY;
    }
    app_exit();

    return 0;
}
