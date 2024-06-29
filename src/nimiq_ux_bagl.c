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

#ifdef HAVE_BAGL

// From Ledger SDK
#include "ux.h"

#include "nimiq_ux.h"
#include "globals.h"
#include "nimiq_ux_bagl_macros.h"
#include "nimiq_ux_utils_transaction_signing.h"
#include "nimiq_ux_utils_message_signing.h"

// These are declared in main.c
void app_exit();
unsigned int io_seproxyhal_on_address_approved();
unsigned int io_seproxyhal_on_address_rejected();
unsigned int io_seproxyhal_on_transaction_approved();
unsigned int io_seproxyhal_on_transaction_rejected();
unsigned int io_seproxyhal_on_message_approved();
unsigned int io_seproxyhal_on_message_rejected();

// Main menu UI steps and flow

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
    app_exit(),
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

// Address confirmation UI steps and flow

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
    {
        io_seproxyhal_on_address_approved();
        ui_menu_main();
    },
    {
        &C_icon_validate_14,
        "Approve",
    });
UX_STEP_CB(
    ux_public_key_flow_reject_step,
    pb,
    {
        io_seproxyhal_on_address_rejected();
        ui_menu_main();
    },
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

// Generic transaction confirmation UI steps

UX_STEP_NOCB(
    ux_transaction_generic_flow_transaction_type_step,
    pnn,
    {
        &C_icon_eye,
        "Confirm",
        ctx.req.tx.content.transaction_label,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_transaction_generic_flow_amount_step,
    paging,
    ux_transaction_generic_has_amount_entry(), // amount can be 0 for signaling transactions
    {
        "Amount",
        ctx.req.tx.content.value,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_transaction_generic_flow_fee_step,
    paging,
    ux_transaction_generic_has_fee_entry(),
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
    {
        io_seproxyhal_on_transaction_approved();
        ui_menu_main();
    },
    {
        &C_icon_validate_14,
        "Accept",
        "and send",
    });
UX_STEP_CB(
    ux_transaction_generic_flow_reject_step,
    pb,
    {
        io_seproxyhal_on_transaction_rejected();
        ui_menu_main();
    },
    {
        &C_icon_crossmark,
        "Reject",
    });

//////////////////////////////////////////////////////////////////////

// Normal, non contract creation transaction specific UI steps and flow

UX_STEP_NOCB(
    ux_transaction_normal_or_staking_outgoing_flow_recipient_step,
    paging,
    {
        "Recipient",
        ctx.req.tx.content.type_specific.normal_or_staking_outgoing_tx.recipient,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_transaction_normal_or_staking_outgoing_flow_data_step,
    paging,
    ux_transaction_normal_or_staking_outgoing_has_data_entry(),
    {
        ctx.req.tx.content.type_specific.normal_or_staking_outgoing_tx.extra_data_label,
        ctx.req.tx.content.type_specific.normal_or_staking_outgoing_tx.extra_data,
    });

UX_FLOW(ux_transaction_normal_or_staking_outgoing_flow,
    &ux_transaction_generic_flow_transaction_type_step,
    &ux_transaction_generic_flow_amount_step, // optional, but always displayed as this is not a signaling transaction
    &ux_transaction_normal_or_staking_outgoing_flow_recipient_step,
    &ux_transaction_normal_or_staking_outgoing_flow_data_step, // optional
    &ux_transaction_generic_flow_fee_step, // optional
    &ux_transaction_generic_flow_network_step,
    &ux_transaction_generic_flow_approve_step,
    &ux_transaction_generic_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// HTLC creation specific UI steps and flow

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
    ux_transaction_htlc_creation_has_refund_address_entry(),
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
    ux_transaction_htlc_creation_has_hash_algorithm_entry(),
    {
        "Hash Algorithm",
        ctx.req.tx.content.type_specific.htlc_creation_tx.hash_algorithm,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_htlc_creation_flow_hash_count_step,
    paging,
    ux_transaction_htlc_creation_has_hash_count_entry(),
    {
        "Hash Steps", // more user friendly label for hash count
        ctx.req.tx.content.type_specific.htlc_creation_tx.hash_count,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_htlc_creation_flow_timeout_step,
    paging,
    ux_transaction_htlc_creation_has_timeout_entry(),
    {
        "HTLC Expiry Block", // more user friendly label for timeout
        ctx.req.tx.content.type_specific.htlc_creation_tx.timeout,
    });

UX_FLOW(ux_transaction_htlc_creation_flow,
    &ux_transaction_generic_flow_transaction_type_step,
    &ux_transaction_generic_flow_amount_step, // optional, but always displayed as this is not a signaling transaction
    &ux_htlc_creation_flow_redeem_address_step,
    &ux_htlc_creation_flow_refund_address_step, // optional
    &ux_htlc_creation_flow_hash_root_step,
    &ux_htlc_creation_flow_hash_algorithm_step, // optional
    &ux_htlc_creation_flow_hash_count_step, // optional
    &ux_htlc_creation_flow_timeout_step, // optional
    &ux_transaction_generic_flow_fee_step, // optional
    &ux_transaction_generic_flow_network_step,
    &ux_transaction_generic_flow_approve_step,
    &ux_transaction_generic_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// Vesting Contract Creation specific UI steps and flow

UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_owner_address_step,
    paging,
    ux_transaction_vesting_creation_has_owner_address_entry(),
    {
        "Vesting Owner",
        ctx.req.tx.content.type_specific.vesting_creation_tx.owner_address,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_single_vesting_block_step, // simplified ui for step_count == 1 case
    paging,
    ux_transaction_vesting_creation_has_single_vesting_block_entry(),
    {
        "Vested at Block",
        ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_block,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_start_block_step,
    paging,
    ux_transaction_vesting_creation_has_start_and_period_and_step_count_and_step_duration_entries(),
    {
        "Vesting Start Block",
        ctx.req.tx.content.type_specific.vesting_creation_tx.start_block,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_period_step,
    paging,
    ux_transaction_vesting_creation_has_start_and_period_and_step_count_and_step_duration_entries(),
    {
        "Vesting Period",
        ctx.req.tx.content.type_specific.vesting_creation_tx.period,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_step_count_step,
    paging,
    ux_transaction_vesting_creation_has_start_and_period_and_step_count_and_step_duration_entries(),
    {
        "Vesting Steps",
        ctx.req.tx.content.type_specific.vesting_creation_tx.step_count,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_step_block_count_step,
    paging,
    ux_transaction_vesting_creation_has_start_and_period_and_step_count_and_step_duration_entries(),
    {
        "Blocks Per Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.step_block_count,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_first_step_block_count_step,
    paging,
    ux_transaction_vesting_creation_has_first_step_duration_entry(),
    {
        "First Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_block_count,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_step_amount_step,
    paging,
    ux_transaction_vesting_creation_has_step_amount_entry(),
    {
        "Vested per Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.step_amount,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_first_step_amount_step,
    paging,
    ux_transaction_vesting_creation_has_first_step_amount_entry(),
    {
        "First Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.first_step_amount,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_last_step_amount_step,
    paging,
    ux_transaction_vesting_creation_has_last_step_amount_entry(),
    {
        "Last Step",
        ctx.req.tx.content.type_specific.vesting_creation_tx.last_step_amount,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_vesting_creation_flow_pre_vested_amount_step,
    paging,
    ux_transaction_vesting_creation_has_pre_vested_amount_entry(),
    {
        "Pre-Vested",
        ctx.req.tx.content.type_specific.vesting_creation_tx.pre_vested_amount,
    });

UX_FLOW(ux_transaction_vesting_creation_flow,
    &ux_transaction_generic_flow_transaction_type_step,
    &ux_transaction_generic_flow_amount_step, // optional, but always displayed as this is not a signaling transaction
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
    &ux_transaction_generic_flow_fee_step, // optional
    &ux_transaction_generic_flow_network_step,
    &ux_transaction_generic_flow_approve_step,
    &ux_transaction_generic_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// Incoming staking transaction (transactions to the staking contract) specific UI steps and flow

UX_OPTIONAL_STEP_NOCB(
    ux_staking_incoming_flow_set_active_stake_or_retire_stake_amount_step,
    paging,
    ux_transaction_staking_incoming_has_set_active_stake_or_retire_stake_amount_entry(),
    {
        "Amount",
        ctx.req.tx.content.type_specific.staking_incoming_tx.set_active_stake_or_retire_stake.amount,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_staking_incoming_flow_staker_address_step,
    paging,
    ux_transaction_staking_incoming_has_staker_address_entry(),
    {
        "Staker",
        ctx.req.tx.content.type_specific.staking_incoming_tx.validator_or_staker_address,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_staking_incoming_flow_create_staker_or_update_staker_delegation_step,
    paging,
    ux_transaction_staking_incoming_has_create_staker_or_update_staker_delegation_entry(),
    {
        "Delegation",
        ctx.req.tx.content.type_specific.staking_incoming_tx.create_staker_or_update_staker.delegation,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_staking_incoming_flow_update_staker_reactivate_all_stake_step,
    paging,
    ux_transaction_staking_incoming_has_update_staker_reactivate_all_stake_entry(),
    {
        "Reactivate all Stake",
        ctx.req.tx.content.type_specific.staking_incoming_tx.create_staker_or_update_staker
            .update_staker_reactivate_all_stake,
    });

UX_FLOW(ux_transaction_staking_incoming_flow,
    &ux_transaction_generic_flow_transaction_type_step,
    &ux_transaction_generic_flow_amount_step, // optional, not shown for signaling tx
    &ux_staking_incoming_flow_set_active_stake_or_retire_stake_amount_step, // optional, for amount in signaling data
    &ux_staking_incoming_flow_staker_address_step, // optional
    &ux_staking_incoming_flow_create_staker_or_update_staker_delegation_step, // optional
    &ux_staking_incoming_flow_update_staker_reactivate_all_stake_step, // optional
    &ux_transaction_generic_flow_fee_step, // optional
    &ux_transaction_generic_flow_network_step,
    &ux_transaction_generic_flow_approve_step,
    &ux_transaction_generic_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// Message signing UI steps and flow

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
    ux_message_signing_prepare_printed_message(),
    {
        ctx.req.msg.confirm.printedMessageLabel,
        ctx.req.msg.confirm.printedMessage,
    });
UX_OPTIONAL_STEP_CB(
    ux_message_flow_display_ascii_step,
    pbb,
    ctx.req.msg.isPrintableAscii && ctx.req.msg.confirm.displayType != MESSAGE_DISPLAY_TYPE_ASCII,
    ui_message_signing(MESSAGE_DISPLAY_TYPE_ASCII, true),
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
    ui_message_signing(MESSAGE_DISPLAY_TYPE_HEX, true),
    {
        &C_icon_certificate,
        "Display",
        "as Hex",
    });
UX_OPTIONAL_STEP_CB(
    ux_message_flow_display_hash_step,
    pbb,
    ctx.req.msg.confirm.displayType != MESSAGE_DISPLAY_TYPE_HASH,
    ui_message_signing(MESSAGE_DISPLAY_TYPE_HASH, true),
    {
        &C_icon_certificate,
        "Display",
        "as Hash",
    });
UX_STEP_CB(
    ux_message_flow_approve_step,
    pbb,
    {
        io_seproxyhal_on_message_approved();
        ui_menu_main();
    },
    {
        &C_icon_validate_14,
        "Sign",
        "message",
    });
UX_STEP_CB(
    ux_message_flow_reject_step,
    pb,
    {
        io_seproxyhal_on_message_rejected();
        ui_menu_main();
    },
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

void ui_menu_main() {
    // reserve a display stack slot if none yet.
    // The stack is for stacking UIs like the app UI, lock screen, screen saver, battery level warning, etc.
    if(G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
}

void ui_public_key() {
    ux_flow_init(0, ux_public_key_flow, NULL);
}

void ui_transaction_signing() {
    // The complete title will be "Confirm <ctx.req.tx.content.transaction_label>"
    switch (ctx.req.tx.content.transaction_label_type) {
        case TRANSACTION_LABEL_TYPE_REGULAR_TRANSACTION:
            strcpy(ctx.req.tx.content.transaction_label, "Transaction");
            break;
        case TRANSACTION_LABEL_TYPE_CASHLINK:
            strcpy(ctx.req.tx.content.transaction_label, "Cashlink");
            break;
        case TRANSACTION_LABEL_TYPE_VESTING_CREATION:
            strcpy(ctx.req.tx.content.transaction_label, "Vesting");
            break;
        case TRANSACTION_LABEL_TYPE_HTLC_CREATION:
            strcpy(ctx.req.tx.content.transaction_label, "HTLC / Swap");
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_CREATE_STAKER:
            strcpy(ctx.req.tx.content.transaction_label, "Create Staker");
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_ADD_STAKE:
            strcpy(ctx.req.tx.content.transaction_label, "Add Stake");
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_UPDATE_STAKER:
            strcpy(ctx.req.tx.content.transaction_label, "Update Staker");
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_SET_ACTIVE_STAKE:
            strcpy(ctx.req.tx.content.transaction_label, "Set Active Stake");
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_RETIRE_STAKE:
            strcpy(ctx.req.tx.content.transaction_label, "Retire Stake");
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_REMOVE_STAKE:
            strcpy(ctx.req.tx.content.transaction_label, "Unstake");
            break;
        default:
            PRINTF("Invalid transaction label type");
            THROW(0x6a80);
    }

    const ux_flow_step_t* const * transaction_flow;
    switch (ctx.req.tx.content.transaction_type) {
        case TRANSACTION_TYPE_NORMAL:
        case TRANSACTION_TYPE_STAKING_OUTGOING:
            transaction_flow = ux_transaction_normal_or_staking_outgoing_flow;
            break;
        case TRANSACTION_TYPE_VESTING_CREATION:
            transaction_flow = ux_transaction_vesting_creation_flow;
            break;
        case TRANSACTION_TYPE_HTLC_CREATION:
            transaction_flow = ux_transaction_htlc_creation_flow;
            break;
        case TRANSACTION_TYPE_STAKING_INCOMING:
            transaction_flow = ux_transaction_staking_incoming_flow;
            break;
        default:
            PRINTF("Invalid transaction type");
            THROW(0x6a80);
    }

    ux_flow_init(0, transaction_flow, NULL);
}

void ui_message_signing(message_display_type_t messageDisplayType, bool startAtMessageDisplay) {
    ctx.req.msg.confirm.displayType = messageDisplayType;
    ux_flow_init(0, ux_message_flow, startAtMessageDisplay ? &ux_message_flow_message_step : NULL);
}

// resolve io_seproxyhal_display as io_seproxyhal_display_default
void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *)element);
}

#endif // HAVE_BAGL
