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

#include "os.h"
#include "cx.h"
#include <stdbool.h>
#include <limits.h>

#include "string.h"

#include "ux.h"

#include "nimiq_utils.h"
#include "ux_macros.h"

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

#define MAX_BIP32_PATH 10

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

// Max length for a transaction's serialized content (66 bytes excluding extra data) with max allowed data which is 64
// bytes for a normal transaction, 44 bytes for a vesting contract creation or 110 bytes for a htlc creation (with hash
// algorithm Sha512)
#define MAX_RAW_TX 176
// Limit printable message length as Nano S has only about 4kB of ram total, used for global vars and stack.
// Additionally, the paging ui displays only ~16 chars per page on Nano S. Printed message buffer length dimension
// chosen such that it can hold the printed uint32 message length (STRING_LENGTH_UINT32 (11) bytes), the message printed
// as hash (32 byte hash as hex + string terminator = 65 bytes), ascii (1 char per byte + string terminator) or hex
// (2 char per byte + string terminator).
#define MAX_PRINTABLE_MESSAGE_LENGTH 160 // 10+ pages ascii or 20 pages hex
#define PRINTED_MESSAGE_BUFFER_LENGTH (MAX_PRINTABLE_MESSAGE_LENGTH * 2 + 1)

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    char address[45];
    uint8_t signature[64];
    bool returnSignature;
} publicKeyContext_t;

typedef struct transactionContext_t {
    uint8_t bip32PathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t rawTx[MAX_RAW_TX];
    uint32_t rawTxLength;
    txContent_t content;
} transactionContext_t;

typedef struct messageSigningContext_t {
    uint32_t bip32Path[MAX_BIP32_PATH];
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
            char printedMessageLabel[13]; // "Message", "Message Hex" or "Message Hash" + string terminator
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

generalContext_t ctx;

unsigned int io_seproxyhal_touch_tx_ok();
unsigned int io_seproxyhal_touch_tx_cancel();
unsigned int io_seproxyhal_touch_message_ok();
unsigned int io_seproxyhal_touch_message_cancel();
unsigned int io_seproxyhal_touch_address_ok();
unsigned int io_seproxyhal_touch_address_cancel();
void ui_idle();
void ui_message_signing(message_display_type_t messageDisplayType, bool startAtMessageDisplay);
uint32_t set_result_get_publicKey();


ux_state_t G_ux;
bolos_ux_params_t G_ux_params;

typedef struct internalStorage_t {
    uint8_t fidoTransport;
    uint8_t initialized;
} internalStorage_t;

internalStorage_t const N_storage_real;
#define N_storage (*(internalStorage_t *)PIC(&N_storage_real))

//////////////////////////////////////////////////////////////////////

// Main Menu
UX_STEP_NOCB(
    ux_idle_flow_welcome_step,
    nn,
    {
        "Application",
        "is ready",
    });
UX_STEP_NOCB(
    ux_idle_flow_version_step,
    bn,
    {
        "Version",
        APPVERSION,
    });
UX_STEP_CB(
    ux_idle_flow_quit_step,
    pb,
    os_sched_exit(-1),
    {
        &C_icon_dashboard,
        "Quit",
    });
UX_FLOW(ux_idle_flow,
    &ux_idle_flow_welcome_step,
    &ux_idle_flow_version_step,
    &ux_idle_flow_quit_step,
    FLOW_LOOP
);

//////////////////////////////////////////////////////////////////////

// Generic transaction confirmation UI steps
UX_STEP_NOCB(
    ux_transaction_generic_flow_transaction_type_step,
    pnn,
    {
        &C_icon_eye,
        "Confirm",
        ctx.req.tx.content.transaction_type_label,
    });
UX_STEP_NOCB(
    ux_transaction_generic_flow_amount_step,
    paging,
    {
        "Amount",
        ctx.req.tx.content.value,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_transaction_generic_flow_fee_step,
    paging,
    strcmp(ctx.req.tx.content.fee, "0 NIM") != 0,
    {
        "Fee",
        ctx.req.tx.content.fee,
    });
UX_STEP_NOCB(
    ux_transaction_generic_flow_network_step,
    paging,
    {
        "Network",
        ctx.req.tx.content.network,
    });
UX_STEP_CB(
    ux_transaction_generic_flow_approve_step,
    pbb,
    io_seproxyhal_touch_tx_ok(),
    {
        &C_icon_validate_14,
        "Accept",
        "and send",
    });
UX_STEP_CB(
    ux_transaction_generic_flow_reject_step,
    pb,
    io_seproxyhal_touch_tx_cancel(),
    {
        &C_icon_crossmark,
        "Reject",
    });

//////////////////////////////////////////////////////////////////////

// Normal, non contract creation transaction specific UI steps
UX_STEP_NOCB(
    ux_transaction_normal_flow_recipient_step,
    paging,
    {
        "Recipient",
        ctx.req.tx.content.type_specific.normal_tx.recipient,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_transaction_normal_flow_data_step,
    paging,
    strlen(ctx.req.tx.content.type_specific.normal_tx.extra_data),
    {
        ctx.req.tx.content.type_specific.normal_tx.extra_data_label,
        ctx.req.tx.content.type_specific.normal_tx.extra_data,
    });

UX_FLOW(ux_transaction_normal_flow,
    &ux_transaction_generic_flow_transaction_type_step,
    &ux_transaction_generic_flow_amount_step,
    &ux_transaction_generic_flow_fee_step, // optional
    &ux_transaction_normal_flow_recipient_step,
    &ux_transaction_normal_flow_data_step, // optional
    &ux_transaction_generic_flow_network_step,
    &ux_transaction_generic_flow_approve_step,
    &ux_transaction_generic_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// HTLC creation specific UI
// As HTLCs are quite technical, we try to not display the less relevant information to the user.
// Considerations for which data can be safely skipped under which circumstances:
// - transaction recipient address (not to be confused with the htlc recipient address, also called redeem address):
//   The recipient address for the contract creation must be the contract address which is deterministically calculated
//   from the other transaction parameters. Any transaction with a different recipient address than the expected address
//   is rejected by the Nimiq network which does therefore not need to be displayed or checked.
// - htlc refund address (also called htlc sender; not to be confused with the transaction sender):
//   If the refund address equals the transaction sender, we omit display because then the funds can be refunded to
//   where they came from, which is an address of a BasicAccount or MultiSig under (partial) control of this Ledger.
//   Note that any other address under the control of this Ledger could also be whitelisted, but currently the refund
//   address being equal to the transaction sender is the normal case in our current use cases and whitelisting other
//   Ledger addresses would require transmitting the refund address key path with the request, such that we can verify
//   that the address is one under control of this Ledger.
// - hash algorithm:
//   As the user confirms the hash root, an attacker trying to let the user create a htlc with the wrong hash algorithm
//   would need to know the pre-image for the hash root for the specified algorithm to be able to gain access to the
//   funds which is close to impossible unless he's the legitimate recipient anyways who specified the confirmed hash
//   root. The worst that can happen, is that the funds are locked until the timeout at which point they can be redeemed
//   by the refund address owner. To avoid that the refund address owner as attacker could take advantage of making it
//   impossible for the redeem address owner to redeem the funds, we skip the hash algorithm display only if the refund
//   address is our address (see above). As an additional restriction, we also skip the display of the hash algorithm
//   only if it's sha256 which is the commonly used hash algorithm and if the funds are not locked for a long time.
// - hash count (here called hash steps):
//   Specifying a lower hash count than the actual intended hash count allows the htlc redeem address owner as attacker
//   to redeem more funds per pre-image step than intended. However, if the user's machine creating the manipulated
//   transaction to be signed on the Ledger is compromised, also usually the htlc secret (unhashed pre-image) which is
//   usually on the same machine is compromised such that the hash count doesn't matter anymore. I.e. if the htlc secret
//   is compromised, the hash count yields no protection of the funds anymore.
//   Specifying a higher hash count than the intended one effectively locks part of the funds for the redeem address
//   owner until they become available to the refund address owner after the timeout. To avoid that the refund address
//   owner as attacker could take advantage of blocking funds to the redeem address owner, we skip the display of the
//   hash count only if the refund address is our address (see above). Additionally, to avoid that funds are potentially
//   locked via a higher hash count for a long time, we only skip the display for short timeouts. However, as blocking
//   the funds requires a higher hash count than the actual one, for 1, the lowest possible hash count, we never have to
//   display it.
// - timeout (here called htlc expiry block):
//   The timeout specifies for how long funds will be locked if not redeemed until they are refundable. As short
//   timeouts (which should be the case for most practically used htlcs) are favorable for the user if the refund
//   address is his, we don't display short timeouts. To avoid that another refund address owner as attacker could take
//   advantage of a short or already passed timeout, we skip the timeout display only if the refund address is our
//   address (see above).
UX_STEP_NOCB(
    ux_htlc_creation_flow_redeem_address_step,
    paging,
    {
        "HTLC Recipient",
        ctx.req.tx.content.type_specific.htlc_creation_tx.redeem_address,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_htlc_creation_flow_refund_address_step,
    paging,
    !ctx.req.tx.content.type_specific.htlc_creation_tx.is_refund_address_sender_address,
    {
        "Refund to",
        ctx.req.tx.content.type_specific.htlc_creation_tx.refund_address,
    });
UX_STEP_NOCB(
    ux_htlc_creation_flow_hash_root_step,
    paging,
    {
        "Hashed Secret", // more user friendly label for hash root
        ctx.req.tx.content.type_specific.htlc_creation_tx.hash_root,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_htlc_creation_flow_hash_algorithm_step,
    paging,
    !ctx.req.tx.content.type_specific.htlc_creation_tx.is_refund_address_sender_address
        || !ctx.req.tx.content.type_specific.htlc_creation_tx.is_timing_out_soon
        || !ctx.req.tx.content.type_specific.htlc_creation_tx.is_using_sha256,
    {
        "Hash Algorithm",
        ctx.req.tx.content.type_specific.htlc_creation_tx.hash_algorithm,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_htlc_creation_flow_hash_count_step,
    paging,
    strcmp(ctx.req.tx.content.type_specific.htlc_creation_tx.hash_count, "1") != 0
        && (!ctx.req.tx.content.type_specific.htlc_creation_tx.is_refund_address_sender_address
        || !ctx.req.tx.content.type_specific.htlc_creation_tx.is_timing_out_soon),
    {
        "Hash Steps", // more user friendly label for hash count
        ctx.req.tx.content.type_specific.htlc_creation_tx.hash_count,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_htlc_creation_flow_timeout_step,
    paging,
    !ctx.req.tx.content.type_specific.htlc_creation_tx.is_refund_address_sender_address
        || !ctx.req.tx.content.type_specific.htlc_creation_tx.is_timing_out_soon,
    {
        "HTLC Expiry Block", // more user friendly label for timeout
        ctx.req.tx.content.type_specific.htlc_creation_tx.timeout,
    });

UX_FLOW(ux_transaction_htlc_creation_flow,
    &ux_transaction_generic_flow_transaction_type_step,
    &ux_transaction_generic_flow_amount_step,
    &ux_transaction_generic_flow_fee_step, // optional
    &ux_htlc_creation_flow_redeem_address_step,
    &ux_htlc_creation_flow_refund_address_step, // optional
    &ux_htlc_creation_flow_hash_root_step,
    &ux_htlc_creation_flow_hash_algorithm_step, // optional
    &ux_htlc_creation_flow_hash_count_step, // optional
    &ux_htlc_creation_flow_timeout_step, // optional
    &ux_transaction_generic_flow_network_step,
    &ux_transaction_generic_flow_approve_step,
    &ux_transaction_generic_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// Vesting Contract Creation specific UI
// Other than for HTLCs we generally do not try to skip less relevant data as vesting contracts are only rarely created
// and all parameters are similarly important. However, depending on the specific vesting contract parameters, some data
// is redundant. Specifically, we have the following optimizations:
// - vesting owner:
//   Display of the vesting owner address is skipped if it equals the transaction sender address.
// - for 0 steps (all funds are pre-vested):
//   We only show the info about the pre-vested amount and skip all other data.
// - for 1 step (all funds unlock at a specific block):
//   We show a special entry with the vesting block. Additionally, the step for a pre-vested amount might be shown. All
//   other info is redundant and skipped.
// - for 2 steps:
//   If first step amount and last step amount differ from regular step amount, do not display what would be the regular
//   step amount as all steps differ from that.
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_owner_address_step,
    paging,
    !ctx.req.tx.content.type_specific.vesting_creation_tx.is_owner_address_sender_address,
    {
        "Vesting Owner",
        ctx.req.tx.content.type_specific.vesting_creation_tx.owner_address,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_single_vesting_block_step, // simplified ui for step_count == 1 case
    paging,
    strcmp(ctx.req.tx.content.type_specific.vesting_creation_tx.step_count, "1") == 0,
    {
        "Vested at Block",
        ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_block,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_start_block_step,
    paging,
    ctx.req.tx.content.type_specific.vesting_creation_tx.is_multi_step,
    {
        "Vesting Start Block",
        ctx.req.tx.content.type_specific.vesting_creation_tx.start_block,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_period_step,
    paging,
    ctx.req.tx.content.type_specific.vesting_creation_tx.is_multi_step,
    {
        "Vesting Period",
        ctx.req.tx.content.type_specific.vesting_creation_tx.period,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_step_count_step,
    paging,
    ctx.req.tx.content.type_specific.vesting_creation_tx.is_multi_step,
    {
        "Vesting Steps",
        ctx.req.tx.content.type_specific.vesting_creation_tx.step_count,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_step_block_count_step,
    paging,
    ctx.req.tx.content.type_specific.vesting_creation_tx.is_multi_step,
    {
        "Blocks Per Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.step_block_count,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_first_step_block_count_step,
    paging,
    ctx.req.tx.content.type_specific.vesting_creation_tx.is_multi_step
        // is different from regular step block count
        && strcmp(ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_block_count,
            ctx.req.tx.content.type_specific.vesting_creation_tx.step_block_count) != 0,
    {
        "First Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_block_count,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_step_amount_step,
    paging,
    ctx.req.tx.content.type_specific.vesting_creation_tx.is_multi_step
        // skip if step_count == 2 and both steps differ from what would be the regular step amount
        && !(
            strcmp(ctx.req.tx.content.type_specific.vesting_creation_tx.step_count, "2") == 0
                && strcmp(ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_amount,
                    ctx.req.tx.content.type_specific.vesting_creation_tx.step_amount) != 0
                && strcmp(ctx.req.tx.content.type_specific.vesting_creation_tx.last_step_amount,
                    ctx.req.tx.content.type_specific.vesting_creation_tx.step_amount) != 0
        ),
    {
        "Vested per Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.step_amount,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_first_step_amount_step,
    paging,
    ctx.req.tx.content.type_specific.vesting_creation_tx.is_multi_step
        // is different from regular step amount
        && strcmp(ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_amount,
            ctx.req.tx.content.type_specific.vesting_creation_tx.step_amount) != 0,
    {
        "First Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_amount,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_last_step_amount_step,
    paging,
    ctx.req.tx.content.type_specific.vesting_creation_tx.is_multi_step
        // is different from regular step amount
        && strcmp(ctx.req.tx.content.type_specific.vesting_creation_tx.last_step_amount,
            ctx.req.tx.content.type_specific.vesting_creation_tx.step_amount) != 0,
    {
        "Last Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.last_step_amount,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_pre_vested_amount_step,
    paging,
    strcmp(ctx.req.tx.content.type_specific.vesting_creation_tx.pre_vested_amount, "0 NIM") != 0,
    {
        "Pre-Vested",
        ctx.req.tx.content.type_specific.vesting_creation_tx.pre_vested_amount,
    });

UX_FLOW(ux_transaction_vesting_creation_flow,
    &ux_transaction_generic_flow_transaction_type_step,
    &ux_transaction_generic_flow_amount_step,
    &ux_transaction_generic_flow_fee_step, // optional
    &ux_vesting_creation_flow_owner_address_step, // optional
    &ux_vesting_creation_flow_single_vesting_block_step, // optional
    &ux_vesting_creation_flow_start_block_step, // optional
    &ux_vesting_creation_flow_period_step, // optional
    &ux_vesting_creation_flow_step_count_step, // optional
    &ux_vesting_creation_flow_step_block_count_step, // optional
    &ux_vesting_creation_flow_first_step_block_count_step, // optional
    &ux_vesting_creation_flow_step_amount_step, // optional
    &ux_vesting_creation_flow_first_step_amount_step, // optional
    &ux_vesting_creation_flow_last_step_amount_step, // optional
    &ux_vesting_creation_flow_pre_vested_amount_step, // optional
    &ux_transaction_generic_flow_network_step,
    &ux_transaction_generic_flow_approve_step,
    &ux_transaction_generic_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// Message signing UI
UX_STEP_NOCB(
    ux_message_flow_intro_step,
    pnn,
    {
        &C_icon_certificate,
        "Sign",
        "message",
    });
UX_STEP_NOCB_INIT(
    ux_message_flow_message_length_step,
    paging,
    {
        // note: not %lu (for unsigned long int) because int is already 32bit on ledgers (see "Memory Alignment" in
        // Ledger docu), additionally Ledger's own implementation of sprintf does not support %lu (see os_printf.c)
        snprintf(ctx.req.msg.confirm.printedMessage, sizeof(ctx.req.msg.confirm.printedMessage), "%u",
            ctx.req.msg.messageLength);
    },
    {
        "Message Length",
        ctx.req.msg.confirm.printedMessage,
    });
UX_STEP_NOCB_INIT(
    ux_message_flow_message_step,
    paging,
    {
        // No error handling needed here. All data has already been verified in handleSignMessage
        switch (ctx.req.msg.confirm.displayType) {
            case MESSAGE_DISPLAY_TYPE_ASCII:
                strcpy(ctx.req.msg.confirm.printedMessageLabel, "Message");
                os_memmove(ctx.req.msg.confirm.printedMessage, ctx.req.msg.printableMessage, ctx.req.msg.messageLength);
                ctx.req.msg.confirm.printedMessage[ctx.req.msg.messageLength] = '\0'; // string terminator
                break;
            case MESSAGE_DISPLAY_TYPE_HEX:
                strcpy(ctx.req.msg.confirm.printedMessageLabel, "Message Hex");
                print_hex(ctx.req.msg.printableMessage, ctx.req.msg.messageLength, ctx.req.msg.confirm.printedMessage,
                    sizeof(ctx.req.msg.confirm.printedMessage));
                break;
            case MESSAGE_DISPLAY_TYPE_HASH:
                strcpy(ctx.req.msg.confirm.printedMessageLabel, "Message Hash");
                print_hex(ctx.req.msg.confirm.messageHash, sizeof(ctx.req.msg.confirm.messageHash),
                    ctx.req.msg.confirm.printedMessage, sizeof(ctx.req.msg.confirm.printedMessage));
                break;
        }
    },
    {
        ctx.req.msg.confirm.printedMessageLabel,
        ctx.req.msg.confirm.printedMessage,
    });
UX_OPTIONAL_STEP_CB(
    ux_message_flow_display_ascii_step,
    pbb,
    ctx.req.msg.isPrintableAscii && ctx.req.msg.confirm.displayType != MESSAGE_DISPLAY_TYPE_ASCII,
    {
        ui_message_signing(MESSAGE_DISPLAY_TYPE_ASCII, true);
    },
    {
        &C_icon_certificate,
        "Display",
        "as Text",
    });
UX_OPTIONAL_STEP_CB(
    ux_message_flow_display_hex_step,
    pbb,
    ctx.req.msg.messageLength <= MAX_PRINTABLE_MESSAGE_LENGTH
        && ctx.req.msg.confirm.displayType != MESSAGE_DISPLAY_TYPE_HEX,
    {
        ui_message_signing(MESSAGE_DISPLAY_TYPE_HEX, true);
    },
    {
        &C_icon_certificate,
        "Display",
        "as Hex",
    });
UX_OPTIONAL_STEP_CB(
    ux_message_flow_display_hash_step,
    pbb,
    ctx.req.msg.confirm.displayType != MESSAGE_DISPLAY_TYPE_HASH,
    {
        ui_message_signing(MESSAGE_DISPLAY_TYPE_HASH, true);
    },
    {
        &C_icon_certificate,
        "Display",
        "as Hash",
    });
UX_STEP_CB(
    ux_message_flow_approve_step,
    pbb,
    io_seproxyhal_touch_message_ok(),
    {
        &C_icon_validate_14,
        "Sign",
        "message",
    });
UX_STEP_CB(
    ux_message_flow_reject_step,
    pb,
    io_seproxyhal_touch_message_cancel(),
    {
        &C_icon_crossmark,
        "Reject",
    });

UX_FLOW(ux_message_flow,
    &ux_message_flow_intro_step,
    &ux_message_flow_message_length_step,
    &ux_message_flow_message_step,
    &ux_message_flow_display_ascii_step,
    &ux_message_flow_display_hex_step,
    &ux_message_flow_display_hash_step,
    &ux_message_flow_approve_step,
    &ux_message_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// Address confirmation UI
UX_STEP_NOCB(
    ux_public_key_flow_address_step,
    paging,
    {
        "Address",
        ctx.req.pk.address,
    });
UX_STEP_CB(
    ux_public_key_flow_approve_step,
    pb,
    io_seproxyhal_touch_address_ok(),
    {
        &C_icon_validate_14,
        "Approve",
    });
UX_STEP_CB(
    ux_public_key_flow_reject_step,
    pb,
    io_seproxyhal_touch_address_cancel(),
    {
        &C_icon_crossmark,
        "Reject",
    });

UX_FLOW(ux_public_key_flow,
    &ux_public_key_flow_address_step,
    &ux_public_key_flow_approve_step,
    &ux_public_key_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

void ui_idle(void) {
    // reserve a display stack slot if none yet.
    // The stack is for stacking UIs like the app UI, lock screen, screen saver, battery level warning, etc.
    if(G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
}

void ui_message_signing(message_display_type_t messageDisplayType, bool startAtMessageDisplay) {
    ctx.req.msg.confirm.displayType = messageDisplayType;
    ux_flow_init(0, ux_message_flow, startAtMessageDisplay ? &ux_message_flow_message_step : NULL);
}

unsigned int io_seproxyhal_touch_address_ok() {
    uint32_t tx = set_result_get_publicKey();
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_address_cancel() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_ok() {
    uint32_t tx = 0;

    // initialize private key
    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, ctx.req.tx.bip32Path, ctx.req.tx.bip32PathLength, privateKeyData, NULL, "ed25519 seed", 12);
    cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    // Sign transaction.
    // Note that we only generate the signature here. It's the calling library's responsibility to build an appropriate
    // signature proof or contract proof out of this signature, depending on the sender type.
#if CX_APILEVEL >= 8
    tx = cx_eddsa_sign(&privateKey, CX_LAST, CX_SHA512, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, NULL, 0, G_io_apdu_buffer, sizeof(G_io_apdu_buffer), NULL);
#else
    tx = cx_eddsa_sign(&privateKey, NULL, CX_LAST, CX_SHA512, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, G_io_apdu_buffer);
#endif
    os_memset(&privateKey, 0, sizeof(privateKey));

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

    // Display back the original UX
    ui_idle();

    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_cancel() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_message_ok() {
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

    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_message_cancel() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    // Display back the original UX
    ui_idle();
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

uint8_t readBip32Path(uint8_t *dataBuffer, uint32_t *bip32Path) {
    uint8_t bip32PathLength = dataBuffer[0];
    dataBuffer += 1;
    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH)) {
        PRINTF("Invalid bip32 path length");
        THROW(0x6a80);
    }
    uint8_t i;
    for (i = 0; i < bip32PathLength; i++) {
        bip32Path[i] = (dataBuffer[0] << 24) | (dataBuffer[1] << 16) |
                       (dataBuffer[2] << 8) | (dataBuffer[3]);
        dataBuffer += 4;
    }
    return bip32PathLength;
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

    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t bip32PathLength = readBip32Path(dataBuffer, bip32Path);
    dataBuffer += 1 + bip32PathLength * 4;
    dataLength -= 1 + bip32PathLength * 4;

    // Optionally create a signature with which the public key can be verified. We only allow signing messages up to 31
    // bytes, as we're blind signing here, and longer data could be Nimiq messages, which are 32 byte Sha256 digests, or
    // transactions, which have varying sizes but larger than 32 bytes.
    uint8_t msgLength;
    uint8_t msg[31];
    if (ctx.req.pk.returnSignature) {
        if (dataLength >= 32) {
            PRINTF("Verification message to sign must not exceed 31 bytes");
            THROW(0x6a80);
        }
        msgLength = (uint8_t) dataLength;
        os_memmove(msg, dataBuffer, msgLength);
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
        ux_flow_init(0, ux_public_key_flow, NULL);
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
        // read the bip32 path
        ctx.req.tx.bip32PathLength = readBip32Path(dataBuffer, ctx.req.tx.bip32Path);
        dataBuffer += 1 + ctx.req.tx.bip32PathLength * 4;
        dataLength -= 1 + ctx.req.tx.bip32PathLength * 4;

        // read raw tx data
        ctx.req.tx.rawTxLength = dataLength;
        if (dataLength > MAX_RAW_TX) {
            PRINTF("Transaction too long");
            THROW(0x6700);
        }
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

    os_memset(&ctx.req.tx.content, 0, sizeof(ctx.req.tx.content));
    parseTx(ctx.req.tx.rawTx, &ctx.req.tx.content);

    const ux_flow_step_t* const * transaction_flow;
    if (ctx.req.tx.content.transaction_type == TRANSACTION_TYPE_NORMAL) {
        transaction_flow = ux_transaction_normal_flow;
    } else if (ctx.req.tx.content.transaction_type == TRANSACTION_TYPE_HTLC_CREATION) {
        transaction_flow = ux_transaction_htlc_creation_flow;
    } else if (ctx.req.tx.content.transaction_type == TRANSACTION_TYPE_VESTING_CREATION) {
        transaction_flow = ux_transaction_vesting_creation_flow;
    } else {
        PRINTF("Invalid transaction type");
        THROW(0x6a80);
    }
    ux_flow_init(0, transaction_flow, NULL);

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
        // read the bip32 path
        ctx.req.msg.bip32PathLength = readBip32Path(dataBuffer, ctx.req.msg.bip32Path);
        const uint8_t bip32PathEncodedLength = /* bip path length encoded in uint8 */ 1
            + /* bip path entries, each uint32 */ ctx.req.msg.bip32PathLength * 4;
        if (dataLength < bip32PathEncodedLength + /* flags */ 1 + /* message length encoded in uint32 */ 4) {
            // we expect the first chunk to at least contain the bip path, flags and encoded message length completely
            PRINTF("First message signing chunk too short");
            THROW(0x6700);
        }
        dataBuffer += bip32PathEncodedLength;
        dataLength -= bip32PathEncodedLength;

        ctx.req.msg.flags = dataBuffer[0];
        dataBuffer++;
        dataLength--;

        ctx.req.msg.messageLength = readUInt32Block(dataBuffer);
        dataBuffer += 4;
        dataLength -= 4;

        ctx.req.msg.processedMessageLength = 0;
        ctx.req.msg.isPrintableAscii = ctx.req.msg.messageLength <= MAX_PRINTABLE_MESSAGE_LENGTH; // ascii-check later
        // See lcx_sha256.h and lcx_hash.h in Ledger sdk
        cx_sha256_init(&ctx.req.msg.prepare.messageHashContext);
        cx_sha256_init(&ctx.req.msg.prepare.prefixedMessageHashContext);

        // Nimiq signed messages add a prefix to the message and then hash both together.
        // This makes the calculated signature recognisable as a Nimiq specific signature and prevents signing arbitrary
        // arbitrary data (e.g. a transaction). This implementation is equivalent to the handling in Key.signMessage in
        // Nimiq's Keyguard.
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
        dataBuffer += dataLength;
        dataLength -= dataLength;
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
                THROW(0x6e00);
                PRINTF("Ivalid CLA");
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
                os_memset(&ctx.req.tx.content, 0, sizeof(ctx.req.tx.content));
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

                // no apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx == 0) {
                    PRINTF("No APDU received");
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
                    os_memset(&ctx.req.tx.content, 0, sizeof(ctx.req.tx.content));
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

// resolve io_seproxyhal_display as io_seproxyhal_display_default
void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *)element);
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
        if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
            !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
              SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
            THROW(EXCEPTION_IO_RESET);
        }
    // no break is intentional
    default:
        UX_DEFAULT_EVENT();
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_U2F && ctx.u2fTimer > 0) {
            ctx.u2fTimer -= 100;
            if (ctx.u2fTimer <= 0) {
                u2fSendKeepAlive();
            }
        }
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
        os_memset(&ctx.req.tx.content, 0, sizeof(ctx.req.tx.content));

        UX_INIT();
        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

#ifdef TARGET_NANOX
                // grab the current plane mode setting
                G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif // TARGET_NANOX

                if (N_storage.initialized != 0x01) {
                    internalStorage_t storage;
                    storage.fidoTransport = 0x01;
                    storage.initialized = 0x01;
                    nvm_write(&N_storage, (void *)&storage,
                              sizeof(internalStorage_t));
                }

                // deactivate usb before activating
                USB_power(0);
                USB_power(1);

                ui_idle();

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
