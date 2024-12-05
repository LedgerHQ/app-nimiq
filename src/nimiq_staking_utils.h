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

#ifndef _NIMIQ_STAKING_UTILS_H_
#define _NIMIQ_STAKING_UTILS_H_

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "error_macros.h"
#include "signature_proof.h"

typedef struct {
    staking_incoming_data_type_t type;
    bool has_validator_or_staker_signature_proof;
    signature_proof_t validator_or_staker_signature_proof; // only used if has_validator_or_staker_signature_proof set
    // All data types have a validator or staker address. Empty string if equal to sender address.
    char validator_or_staker_address[STRING_LENGTH_USER_FRIENDLY_ADDRESS];
    union {
        // Note that validator transactions are not supported yet.
        struct {
            char delegation[STRING_LENGTH_USER_FRIENDLY_ADDRESS]; // Empty string if optional delegation address unset.
            char update_staker_reactivate_all_stake[STRING_LENGTH_YES_NO]; // only used for UPDATE_STAKER; "Yes" or "No"
        } create_staker_or_update_staker;
        struct {
            char amount[STRING_LENGTH_NIM_AMOUNT_WITH_TICKER];
        } set_active_stake_or_retire_stake;
    };
} tx_data_staking_incoming_t;

WARN_UNUSED_RESULT
error_t parse_staking_incoming_data(transaction_version_t version, uint8_t *data, uint16_t data_length, uint8_t *sender,
    tx_data_staking_incoming_t *out);

WARN_UNUSED_RESULT
error_t parse_staking_outgoing_data(transaction_version_t version, uint8_t *sender_data, uint16_t sender_data_length,
    staking_outgoing_data_type_t *out_staking_outgoing_type);

bool is_staking_contract(uint8_t *address_bytes);

bool is_signaling_transaction_data(staking_incoming_data_type_t type);

#endif // _NIMIQ_STAKING_UTILS_H_
