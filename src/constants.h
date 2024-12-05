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

#include <stdint.h> // for uint8_t

#include "utility_macros.h" // for MAX and STRING_LENGTH_WITH_SUFFIX

// The maximum theoretically allowed NIM amount. Equal to JavaScript's Number.MAX_SAFE_INTEGER, see Coin::MAX_SAFE_VALUE
// in primitives/src/coin.rs in core-rs-albatross. Notably, this theoretical value is higher than the planned final
// total supply.
#define MAX_SAFE_LUNA_AMOUNT 9007199254740991

#define STRING_LENGTH_WITH_SUFFIX(length_a, suffix) (length_a - /* string terminator of string a */ 1 + sizeof(suffix))

// String lengths including the string terminator.
// Note that the measurement strings here don't end up in the app binary, only the measurement results of sizeof.
#define STRING_LENGTH_YES_NO MAX(sizeof("Yes"), sizeof("No"))
#define STRING_LENGTH_UINT8 sizeof("255") // "0" to "255" (any uint8)
#define STRING_LENGTH_UINT32 sizeof("4294967295") // "0" to "4294967295" (any uint32)
#define STRING_LENGTH_NIM_AMOUNT sizeof("90071992547.40991") // "0" to "90071992547.40991", MAX_SAFE_LUNA_AMOUNT in NIM
#define STRING_LENGTH_NIM_AMOUNT_WITH_TICKER STRING_LENGTH_WITH_SUFFIX(STRING_LENGTH_NIM_AMOUNT, " NIM")
#define STRING_LENGTH_USER_FRIENDLY_ADDRESS sizeof("NQ07 0000 0000 0000 0000 0000 0000 0000 0000")

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

/**
 * Error codes for methods that do not operate on the APDU protocol. They are meant to keep these methods somewhat
 * independent of Ledger specific code.
 */
typedef enum: uint8_t {
    /**
     * No error occurred. All good.
     */
    ERROR_NONE = 0x0,
    /**
     * Not further specified error of value 1/true, which is the automatic result of logical expressions, or variables
     * and return types of type bool that evaluate to true. This error is mostly defined to correctly classify such
     * automatic results.
     *
     * Instead of using this error, one of the more specific other errors should be used.
     */
    ERROR_TRUE = 0x1,
    /**
     * An unexpected error occurred that shouldn't have happened.
     *
     * Instead of using this error, preferably exit the app immediately, for example via a LEDGER_ASSERT, for less
     * overhead of error handling in functions that otherwise don't error.
     */
    ERROR_UNEXPECTED = 0x2,
    /**
     * Error reading data from a buffer, typically because the buffer is shorter than expected, but could also be
     * because of unexpected values being read, for example when reading structured data like signature proofs.
     */
    ERROR_READ = 0x3,
    /**
     * The length of some data is longer than allowed, or does not match the expected or advertised length.
     */
    ERROR_INVALID_LENGTH = 0x4,
    /**
     * The data is semantically incorrect. It might for example contain a disallowed, inconsistent or unexpected value.
     */
    ERROR_INCORRECT_DATA = 0x5,
    /**
     * Some operation or data is not supported by the ledger app or core-albatross.
     */
    ERROR_NOT_SUPPORTED = 0x6,
    /**
     * An error in a call of a cryptography method occurred, typically because of incorrect parameters passed, but also
     * for example if a method aborted because the device is locked.
     */
    ERROR_CRYPTOGRAPHY = 0x7,
} error_t;
#define ERROR_TRUE _Pragma("GCC warning \"Use of ERROR_TRUE is discouraged\"") ERROR_TRUE
#define ERROR_UNEXPECTED _Pragma("GCC warning \"Use of ERROR_UNEXPECTED is discouraged\"") ERROR_UNEXPECTED

/**
 * APDU status words for methods that operate on the APDU protocol.
 * The ones we're using are mostly based on app-bitcoin-new's sw.h, which itself is based on app-boilerplate's sw.h with
 * some additions from old Bitcoin app's btchip_apdu_constants, and should mostly also be supported by @ledgerhq/errors.
 * Note however, that the error codes for some of the errors have changed from btchip_apdu_constants, for example for
 * INCORRECT_P1_P2.
 * They should be generally following smart card APDU standard ISO-7816. If we were to define custom errors, they should
 * be in the range 0xB000-0xEFFF, see errors.h in ledger-secure-sdk.
 * Definition of other common status words:
 * - https://de.wikipedia.org/wiki/Application_Protocol_Data_Unit#Statusw%C3%B6rter
 * - https://github.com/LedgerHQ/ledger-secure-sdk/blob/master/include/errors.h
 * - https://github.com/LedgerHQ/app-bitcoin-new/blob/master/src/boilerplate/sw.h
 * - https://github.com/LedgerHQ/app-boilerplate/blob/master/src/sw.h
 * - https://github.com/LedgerHQ/ledger-live/blob/develop/libs/ledgerjs/packages/errors/src/index.ts
 * - https://github.com/LedgerHQ/app-bitcoin/blob/master/include/btchip_apdu_constants.h
 * - https://ledgerhq.github.io/btchip-doc/bitcoin-technical-beta.html#_status_words
 */
typedef enum: uint16_t {
    /**
     * Status word for success.
     */
    SW_OK = 0x9000,
    /**
     * Status word for denied by user.
     */
    SW_DENY = 0x6985,
    /**
     * Status word for incorrect data.
     */
    SW_INCORRECT_DATA = 0x6A80,
    /**
     * Status word for request not currently supported.
     */
    SW_NOT_SUPPORTED = 0x6A82,
    /**
     * Status word for incorrect P1 or P2.
     */
    SW_WRONG_P1P2 = 0x6A86,
    /**
     * Status word for either wrong Lc or length of APDU command less than 5.
     */
    SW_WRONG_DATA_LENGTH = 0x6A87,
    /**
     * Status word for unknown command with this INS.
     */
    SW_INS_NOT_SUPPORTED = 0x6D00,
    /**
     * Status word for instruction class is different than CLA.
     */
    SW_CLA_NOT_SUPPORTED = 0x6E00,
    /**
     * Status word for a heartbeat response to avoid U2F timeouts. The client is supposed to reply with instruction
     * INS_KEEP_ALIVE, to continue the request.
     */
    SW_KEEP_ALIVE = 0x6E02,
    /**
     * Status word for bad state.
     */
    SW_BAD_STATE = 0xB007,
    /**
     * Status word for failure of a cryptography related operation.
     * In app-bitcoin-new and app-boilerplate, this is called SW_SIGNATURE_FAIL, but we use it in a more general way not
     * only for failed signatures but all cryptographic operations like public key derivation.
     */
    SW_CRYPTOGRAPHY_FAIL = 0xB008,

    // Additional status words defined in app-bitcoin-new's sw.h, which we don't currently use:
    // /**
    //  * Status word for command not valid for security reasons (for example: device needs to be unlocked with PIN).
    //  */
    // SW_SECURITY_STATUS_NOT_SATISFIED = 0x6982,
    // /**
    //  * Status word for fail in Swap
    //  */
    // SW_FAIL_SWAP = 0x6B00,
    // /**
    //  * Status word for wrong response length (buffer too small or too big).
    //  */
    // SW_WRONG_RESPONSE_LENGTH = 0xB000,
    // /**
    //  * Status word for interrupted execution.
    //  */
    // SW_INTERRUPTED_EXECUTION = 0xE000,

    // Additional custom status words defined in app-boilerplate's sw.h, which we don't currently use:
    // /**
    // * Status word for fail to display BIP32 path.
    // */
    // SW_DISPLAY_BIP32_PATH_FAIL = 0xB001,
    // /**
    // * Status word for fail to display address.
    // */
    // SW_DISPLAY_ADDRESS_FAIL = 0xB002,
    // /**
    // * Status word for fail to display amount.
    // */
    // SW_DISPLAY_AMOUNT_FAIL = 0xB003,
    // /**
    // * Status word for wrong transaction length.
    // */
    // SW_WRONG_TX_LENGTH = 0xB004,
    // /**
    // * Status word for fail of transaction parsing.
    // */
    // SW_TX_PARSING_FAIL = 0xB005,
    // /**
    // * Status word for fail of transaction hash.
    // */
    // SW_TX_HASH_FAIL = 0xB006,
} sw_t;

#endif // _NIMIQ_CONSTANTS_H_
