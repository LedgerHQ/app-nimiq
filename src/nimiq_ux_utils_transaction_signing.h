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

#ifndef _NIMIQ_UX_UTILS_TRANSACTION_SIGNING_H_
#define _NIMIQ_UX_UTILS_TRANSACTION_SIGNING_H_

#include <stdbool.h>

bool ux_transaction_generic_has_amount_entry();
bool ux_transaction_generic_has_fee_entry();

bool ux_transaction_normal_or_staking_outgoing_has_data_entry();

bool ux_transaction_htlc_creation_has_refund_address_entry();
bool ux_transaction_htlc_creation_has_hash_algorithm_entry();
bool ux_transaction_htlc_creation_has_hash_count_entry();
bool ux_transaction_htlc_creation_has_timeout_entry();

bool ux_transaction_vesting_creation_has_owner_address_entry();
bool ux_transaction_vesting_creation_has_single_vesting_block_entry();
bool ux_transaction_vesting_creation_has_start_and_period_and_step_count_and_step_duration_entries();
bool ux_transaction_vesting_creation_has_first_step_duration_entry();
bool ux_transaction_vesting_creation_has_step_amount_entry();
bool ux_transaction_vesting_creation_has_first_step_amount_entry();
bool ux_transaction_vesting_creation_has_last_step_amount_entry();
bool ux_transaction_vesting_creation_has_pre_vested_amount_entry();

bool ux_transaction_staking_incoming_has_set_active_stake_or_retire_stake_amount_entry();
bool ux_transaction_staking_incoming_has_staker_address_entry();
bool ux_transaction_staking_incoming_has_create_staker_or_update_staker_delegation_entry();
bool ux_transaction_staking_incoming_has_update_staker_reactivate_all_stake_entry();

#endif // _NIMIQ_UX_UTILS_TRANSACTION_SIGNING_H_
