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

#include "nimiq_ux_utils_transaction_signing.h"
#include "globals.h"

bool ux_transaction_generic_has_amount_entry() {
    // The transaction amount can be 0 for signaling transactions, in which case we want to show the amount given in the
    // IncomingStakingTransactionData instead, if applicable.
    return strcmp(PARSED_TX.value, "0 NIM") != 0;
}

bool ux_transaction_generic_has_fee_entry() {
    return strcmp(PARSED_TX.fee, "0 NIM") != 0;
}

bool ux_transaction_normal_or_staking_outgoing_has_data_entry() {
    return strlen(PARSED_TX_NORMAL_OR_STAKING_OUTGOING.extra_data);
}

// HTLC creation specific UI steps and flow
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

bool ux_transaction_htlc_creation_has_refund_address_entry() {
    return !PARSED_TX_HTLC_CREATION.is_refund_address_sender_address;
}

bool ux_transaction_htlc_creation_has_hash_algorithm_entry() {
    return !PARSED_TX_HTLC_CREATION.is_refund_address_sender_address
        || !PARSED_TX_HTLC_CREATION.is_timing_out_soon
        || !PARSED_TX_HTLC_CREATION.is_using_sha256;
}

bool ux_transaction_htlc_creation_has_hash_count_entry() {
    return strcmp(PARSED_TX_HTLC_CREATION.hash_count, "1") != 0
        && (!PARSED_TX_HTLC_CREATION.is_refund_address_sender_address || !PARSED_TX_HTLC_CREATION.is_timing_out_soon);
}

bool ux_transaction_htlc_creation_has_timeout_entry() {
    return !PARSED_TX_HTLC_CREATION.is_refund_address_sender_address
        || !PARSED_TX_HTLC_CREATION.is_timing_out_soon;
}

// Vesting Contract Creation specific UI steps and flow
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

bool ux_transaction_vesting_creation_has_owner_address_entry() {
    return !PARSED_TX_VESTING_CREATION.is_owner_address_sender_address;
}

bool ux_transaction_vesting_creation_has_single_vesting_block_entry() {
    // simplified ui for step_count == 1 case
    return !PARSED_TX_VESTING_CREATION.is_multi_step;
}

bool ux_transaction_vesting_creation_has_start_and_period_and_step_count_and_step_duration_entries() {
    return PARSED_TX_VESTING_CREATION.is_multi_step;
}

bool ux_transaction_vesting_creation_has_first_step_duration_entry() {
    return PARSED_TX_VESTING_CREATION.is_multi_step
        // The first step duration is different from the regular step duration.
        && strcmp(PARSED_TX_VESTING_CREATION.first_step_block_count, PARSED_TX_VESTING_CREATION.step_block_count) != 0;
}

bool ux_transaction_vesting_creation_has_step_amount_entry() {
    return PARSED_TX_VESTING_CREATION.is_multi_step
        // Skip if step_count == 2 and both steps differ from what would be the regular step amount.
        && !(
            strcmp(PARSED_TX_VESTING_CREATION.step_count, "2") == 0
                && strcmp(PARSED_TX_VESTING_CREATION.first_step_amount, PARSED_TX_VESTING_CREATION.step_amount) != 0
                && strcmp(PARSED_TX_VESTING_CREATION.last_step_amount, PARSED_TX_VESTING_CREATION.step_amount) != 0
        );
}

bool ux_transaction_vesting_creation_has_first_step_amount_entry() {
    return PARSED_TX_VESTING_CREATION.is_multi_step
        // The first step amount is different from the regular step amount.
        && strcmp(PARSED_TX_VESTING_CREATION.first_step_amount, PARSED_TX_VESTING_CREATION.step_amount) != 0;
}

bool ux_transaction_vesting_creation_has_last_step_amount_entry() {
    return PARSED_TX_VESTING_CREATION.is_multi_step
        // The last step amount is different from the regular step amount.
        && strcmp(PARSED_TX_VESTING_CREATION.last_step_amount, PARSED_TX_VESTING_CREATION.step_amount) != 0;
}

bool ux_transaction_vesting_creation_has_pre_vested_amount_entry() {
    return strcmp(PARSED_TX_VESTING_CREATION.pre_vested_amount, "0 NIM") != 0;
}

// Incoming staking transaction (transactions to the staking contract) specific UI steps and flow
// Considerations for which data can be safely skipped under which circumstances:
// - 0 NIM transaction amount for signaling transactions. If the incoming staking data includes an amount, that is shown
//   instead.
// - transaction recipient address as this must be the staking contract.
// - validator address (from validator signature proof): is always displayed as validator management is considered an
//   advanced feature, where the user probably wants to have a complete overview of the transaction data. It's also not
//   a very regularly occurring / common transaction.
// - staker address (from staker signature proof): If the staker address equals the transaction sender, we omit display
//   because then the staker is under control of the user / this Ledger (in case of multi-sig, partial control). This is
//   also the most common case. We don't need to check that the sender address actually belongs to the Ledger account as
//   a signature for a wrong sender address will be rejected by the network. This also covers contracts as sender as the
//   staker address can't really be the contract address because for the deterministic contract address, no signing key
//   is known which could create the valid staker signature proof. Also, multi-sig sender addresses are theoretically
//   covered, i.e. the mutli-sig signed transactions also assume the multi-sig as staker owner by default, however multi
//   sig staker signature proofs are not currently supported as they'd require a non-empty merkle path. Note that any
//   other address under the control of this Ledger could also be whitelisted, but currently the staker address being
//   equal to the transaction sender is the normal case in our current use cases and whitelisting other Ledger addresses
//   would require transmitting the staker address key path with the request, such that we can verify that the address
//   is one under control of this Ledger.
// - no other parts of the signature proof need to be displayed to the user as invalid signature proofs will be rejected
//   by the network nodes.
// - delegation addresses: are always displayed as the assumption is that common, non-advanced users would not delegate
//   to themselves, and advanced users would like to see this information, even if delegating to themselves.

bool ux_transaction_staking_incoming_has_set_active_stake_or_retire_stake_amount_entry() {
    // Show if it's a data type that specifies an amount in the data. Note that these are signaling transactions. I.e.
    // the regular transaction amount is 0, and not displayed.
    return PARSED_TX_STAKING_INCOMING.type == SET_ACTIVE_STAKE
        || PARSED_TX_STAKING_INCOMING.type == RETIRE_STAKE;
}

bool ux_transaction_staking_incoming_has_staker_address_entry() {
    // Show if it's a staker tx, as opposed to a validator tx, and staker address is set (i.e. it's different to sender)
    return (PARSED_TX_STAKING_INCOMING.type == CREATE_STAKER
        || PARSED_TX_STAKING_INCOMING.type == ADD_STAKE
        || PARSED_TX_STAKING_INCOMING.type == UPDATE_STAKER
        || PARSED_TX_STAKING_INCOMING.type == SET_ACTIVE_STAKE
        || PARSED_TX_STAKING_INCOMING.type == RETIRE_STAKE
    ) && strlen(PARSED_TX_STAKING_INCOMING.validator_or_staker_address);
}

bool ux_transaction_staking_incoming_has_create_staker_or_update_staker_delegation_entry() {
    // Show if it's a data type that potentially specifies a delegation address, and it is set.
    return (PARSED_TX_STAKING_INCOMING.type == CREATE_STAKER || PARSED_TX_STAKING_INCOMING.type == UPDATE_STAKER)
        && strlen(PARSED_TX_STAKING_INCOMING.create_staker_or_update_staker.delegation);
}

bool ux_transaction_staking_incoming_has_update_staker_reactivate_all_stake_entry() {
    // Show reactivate_all_stake for data type UPDATE_STAKER.
    return PARSED_TX_STAKING_INCOMING.type == UPDATE_STAKER;
}
