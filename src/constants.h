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

#ifndef _NIMIQ_CONSTANTS_H_
#define _NIMIQ_CONSTANTS_H_

#define STRING_LENGTH_YES_NO 4 // "Yes" or "No"
#define STRING_LENGTH_UINT8 4 // "0" to "255" (any uint8)
#define STRING_LENGTH_UINT32 11 // "0" to "4294967295" (any uint32)
#define STRING_LENGTH_NIM_AMOUNT 22 // "0" to "90071992547.40991 NIM" (MAX_SAFE_INTEGER Luna in NIM)
#define STRING_LENGTH_USER_FRIENDLY_ADDRESS 45

#define MAX_BIP32_PATH_LENGTH 10

#define CASHLINK_MAGIC_NUMBER "\x00\x82\x80\x92\x87"
#define CASHLINK_MAGIC_NUMBER_LENGTH 5

#define MESSAGE_SIGNING_PREFIX "\x16Nimiq Signed Message:\n" // 0x16 (decimal 22) is the prefix length

#define TX_FLAG_CONTRACT_CREATION (0x1 << 0)
#define TX_FLAG_SIGNALING (0x1 << 1)
#define MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HEX (0x1 << 0)
#define MESSAGE_FLAG_PREFER_DISPLAY_TYPE_HASH (0x1 << 1)

// Max supported length for a transaction's serialized content, based on Albatross, where data is generally longer due
// to added sender data and uint64 timestamps instead of uint32 block counts in vesting and htlc contracts. Sum of:
// - Minimum of 67 bytes common to all transactions for recipient data length, sender address, sender type, recipient
//   address, recipient type, value, fee, validity start height, network id, flags, sender data length (assuming sender
//   data length being encoded in a single byte varint, i.e. sender data up to 127 bytes length), recipient data length.
// - Sender or recipient data of up to 121 bytes. Note that currently, we don't support sender data and recipient data
//   to be set at the same time. This number is the maximum of:
//   1 byte sender data for OutgoingStakingTransactionData.
//   64 bytes recipient data limit we set for basic transactions.
//   52 bytes recipient data for fully specified VestingContract CreationTransactionData.
//   114 bytes recipient data for HashedTimeLockedContract CreationTransactionData with Sha512 hash root.
//   Staking recipient data for IncomingStakingTransactionData (including enum index), which can be:
//     120 bytes for fully specified CreateStaker with ed25519 signature proof with empty merkle path.
//     21 bytes for AddStake.
//     121 bytes for fully specified UpdateStaker with ed25519 signature proof with empty merkle path.
//     107 bytes for SetActiveStake.
//     107 bytes for RetireStake.
//     (Validator transactions are not covered yet, as not supported yet.)
#define MAX_RAW_TX 188
// Limit printable message length as Nano S has only about 4kB of RAM total, used for global vars and stack, and on top
// of the message buffer, there is the buffer for the printed message, which is twice as large, see messageSigningContext_t
// in globals.h Additionally, the paging ui displays only ~16 chars per page on Nano S.
#define MAX_PRINTABLE_MESSAGE_LENGTH 160 // 10+ pages ascii or 20 pages hex on Nano S

typedef enum {
    TRANSACTION_VERSION_LEGACY,
    TRANSACTION_VERSION_ALBATROSS,
} transaction_version_t;

typedef enum {
    ACCOUNT_TYPE_BASIC = 0,
    ACCOUNT_TYPE_VESTING = 1,
    ACCOUNT_TYPE_HTLC = 2,
    ACCOUNT_TYPE_STAKING = 3,
} account_type_t;

typedef enum {
    HASH_ALGORITHM_BLAKE2B = 1,
    HASH_ALGORITHM_ARGON2D = 2,
    HASH_ALGORITHM_SHA256 = 3,
    HASH_ALGORITHM_SHA512 = 4,
} hash_algorithm_t;

typedef enum {
    TRANSACTION_TYPE_NORMAL,
    TRANSACTION_TYPE_VESTING_CREATION,
    TRANSACTION_TYPE_HTLC_CREATION,
    TRANSACTION_TYPE_STAKING_INCOMING,
    TRANSACTION_TYPE_STAKING_OUTGOING,
} transaction_type_t;

typedef enum {
    TRANSACTION_LABEL_TYPE_REGULAR_TRANSACTION,
    TRANSACTION_LABEL_TYPE_CASHLINK,
    TRANSACTION_LABEL_TYPE_VESTING_CREATION,
    TRANSACTION_LABEL_TYPE_HTLC_CREATION,
    TRANSACTION_LABEL_TYPE_STAKING_CREATE_STAKER,
    TRANSACTION_LABEL_TYPE_STAKING_ADD_STAKE,
    TRANSACTION_LABEL_TYPE_STAKING_UPDATE_STAKER,
    TRANSACTION_LABEL_TYPE_STAKING_SET_ACTIVE_STAKE,
    TRANSACTION_LABEL_TYPE_STAKING_RETIRE_STAKE,
    TRANSACTION_LABEL_TYPE_STAKING_REMOVE_STAKE,
} transaction_label_type_t;

// Recipient data type for incoming transactions to the staking contract.
typedef enum {
    CREATE_VALIDATOR,
    UPDATE_VALIDATOR,
    DEACTIVATE_VALIDATOR,
    REACTIVATE_VALIDATOR,
    RETIRE_VALIDATOR,
    CREATE_STAKER,
    ADD_STAKE,
    UPDATE_STAKER,
    SET_ACTIVE_STAKE,
    RETIRE_STAKE,
} staking_incoming_data_type_t;
// Sender data type for outgoing transactions from the staking contract.
typedef enum {
    DELETE_VALIDATOR,
    REMOVE_STAKE,
} staking_outgoing_data_type_t;

typedef enum {
    MESSAGE_DISPLAY_TYPE_ASCII,
    MESSAGE_DISPLAY_TYPE_HEX,
    MESSAGE_DISPLAY_TYPE_HASH,
} message_display_type_t;

#endif // _NIMIQ_CONSTANTS_H_
