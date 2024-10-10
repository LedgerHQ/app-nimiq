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

#include <string.h>

#include "nimiq_staking_utils.h"
#include "nimiq_utils.h"
#include "signature_proof.h"

// Staking transactions
// Incoming staking transactions are to the staking contract and encode their data in the recipient data, outgoing
// staking transaction are from the staking contract and encode their data in the sender data. Staking has been
// introduced / is only supported for Albatross, i.e. core-rs-albatross. For serialization format see:
// - IncomingStakingTransactionData in primitives/transaction/src/account/staking_contract/structs.rs for recipient
// - OutgoingStakingTransactionData in primitives/transaction/src/account/staking_contract/structs.rs for sender
// - rust types supported by serde: https://serde.rs/data-model.html#types
// - which are serialized to bytes in postcard format: https://postcard.jamesmunns.com/wire-format.html, see data() in
//   transaction-builder/src/recipient/mod.rs and there used serialize_to_vec

WARN_UNUSED_RESULT
error_t parse_staking_incoming_data(transaction_version_t version, uint8_t *data, uint16_t data_length, uint8_t *sender,
    tx_data_staking_incoming_t *out) {
    RETURN_ON_ERROR(
        version == TRANSACTION_VERSION_LEGACY,
        ERROR_INCORRECT_DATA,
        "Staking is not supported for legacy transactions\n"
    );

    uint8_t validator_or_staker_address_buffer[20];
    // NULL means not specified, which is then the same as sender, as we create the staker signature proof with the
    // sender account, if the empty signature proof was provided, see transaction signing in main.c
    uint8_t *effective_validator_or_staker_address = NULL;

    _Static_assert(
        sizeof(out->type) == 1,
        "out->type has more than one byte. Need to take endianness into account when reading into a u8 pointer.\n"
    );
    RETURN_ON_ERROR(
        !read_u8(&data, &data_length, &out->type),
        ERROR_READ
    );
    switch (out->type) {
        case CREATE_STAKER:
        case UPDATE_STAKER: {
            bool hasDelegation;
            RETURN_ON_ERROR(
                !read_bool(&data, &data_length, &hasDelegation),
                ERROR_READ
            );
            if (hasDelegation) {
                uint8_t *delegation_address_pointer;
                RETURN_ON_ERROR(
                    !read_sub_buffer(20, &data, &data_length, &delegation_address_pointer),
                    ERROR_READ
                );
                RETURN_ON_ERROR(
                    print_address(delegation_address_pointer, out->create_staker_or_update_staker.delegation)
                );
            } else {
                strcpy(out->create_staker_or_update_staker.delegation, "");
            }
            if (out->type == UPDATE_STAKER) {
                bool reactivate_all_stake;
                RETURN_ON_ERROR(
                    !read_bool(&data, &data_length, &reactivate_all_stake),
                    ERROR_READ
                );
                strcpy(out->create_staker_or_update_staker.update_staker_reactivate_all_stake,
                    reactivate_all_stake ? "Yes" : "No");
            }
            break;
        }

        case ADD_STAKE: {
            RETURN_ON_ERROR(
                !read_sub_buffer(20, &data, &data_length, &effective_validator_or_staker_address),
                ERROR_READ
            );
            break;
        }

        case SET_ACTIVE_STAKE:
        case RETIRE_STAKE: {
            uint64_t amount;
            RETURN_ON_ERROR(
                !read_u64(&data, &data_length, &amount),
                ERROR_READ
            );
            RETURN_ON_ERROR(
                parse_amount(amount, "NIM", out->set_active_stake_or_retire_stake.amount)
            );
            break;
        }

        default:
            // Note that validator transactions are not supported yet.
            RETURN_ERROR(
                ERROR_NOT_SUPPORTED,
                "Invalid incoming staking transaction data type\n"
            );
    }

    if (out->type != ADD_STAKE) {
        // All types but ADD_STAKE encode a validator or staker signature proof at the end of the data.
        out->has_validator_or_staker_signature_proof = true;
        RETURN_ON_ERROR(
            !read_signature_proof(&data, &data_length, &out->validator_or_staker_signature_proof),
            ERROR_READ
        );
        RETURN_ON_ERROR(
            // Currently only ed25519 and empty merkle paths are supported.
            out->validator_or_staker_signature_proof.type_and_flags != 0
            || out->validator_or_staker_signature_proof.merkle_path_length,
            ERROR_NOT_SUPPORTED,
            "Only ed25519 signature proofs without flags and merkle paths supported\n"
        );
        if (!is_empty_default_signature_proof(out->validator_or_staker_signature_proof)) {
            RETURN_ON_ERROR(
                public_key_to_address(out->validator_or_staker_signature_proof.public_key,
                    validator_or_staker_address_buffer)
            );
            effective_validator_or_staker_address = validator_or_staker_address_buffer;
        }
    } else {
        out->has_validator_or_staker_signature_proof = false;
    }

    RETURN_ON_ERROR(
        data_length != 0,
        ERROR_INVALID_LENGTH,
        "Incoming staking data too long\n"
    );

    // Print the validator or staker address if it is different to the sender address.
    // Other parts of the signature proofs don't need to be displayed or verified as they're verified by network nodes.
    if (effective_validator_or_staker_address && memcmp(effective_validator_or_staker_address, sender, 20)) {
        RETURN_ON_ERROR(
            print_address(effective_validator_or_staker_address, out->validator_or_staker_address)
        );
    } else {
        // The staker address is the same as the sender address. Note that different to parse_htlc_creation_data or
        // parse_vesting_creation_data we don't block non-basic sender types for staker creation here, because contract
        // sender addresses would not be able to create a valid signature proof anyway as no signing key is known for
        // the contract address.
        strcpy(out->validator_or_staker_address, "");
    }

    return ERROR_NONE;
}

WARN_UNUSED_RESULT
error_t parse_staking_outgoing_data(transaction_version_t version, uint8_t *sender_data, uint16_t sender_data_length,
    staking_outgoing_data_type_t *out_staking_outgoing_type) {
    RETURN_ON_ERROR(
        version == TRANSACTION_VERSION_LEGACY,
        ERROR_INCORRECT_DATA,
        "Staking is not supported for legacy transactions\n"
    );

    _Static_assert(
        sizeof(*out_staking_outgoing_type) == 1,
        "*out_staking_outgoing_type has more than one byte. Need to take endianness into account when reading into a "
            "u8 pointer.\n"
    );
    RETURN_ON_ERROR(
        !read_u8(&sender_data, &sender_data_length, out_staking_outgoing_type),
        ERROR_READ
    );
    RETURN_ON_ERROR(
        *out_staking_outgoing_type != DELETE_VALIDATOR && *out_staking_outgoing_type != REMOVE_STAKE,
        ERROR_INCORRECT_DATA,
        "Invalid outgoing staking type\n"
    );

    RETURN_ON_ERROR(
        sender_data_length != 0,
        ERROR_INVALID_LENGTH,
        "Outgoing staking data too long\n"
    );

    return ERROR_NONE;
}

bool is_staking_contract(uint8_t *address) {
    // Staking contract address consists of 19 value 0 bytes followed by 1 value 1 byte, see STAKING_CONTRACT_ADDRESS in
    // primitives/src/policy.rs in core-rs-albatross
    for (uint8_t i = 0; i < 19; i++) {
        if (address[i] != 0) return false;
    }
    return address[19] == 1;
}

bool is_signaling_transaction_data(staking_incoming_data_type_t type) {
    // See is_signaling in primitives/transaction/src/account/staking_contract/structs.rs in core-rs-albatross
    return type == UPDATE_VALIDATOR
        || type == DEACTIVATE_VALIDATOR
        || type == REACTIVATE_VALIDATOR
        || type == RETIRE_VALIDATOR
        || type == UPDATE_STAKER
        || type == SET_ACTIVE_STAKE
        || type == RETIRE_STAKE;
}
