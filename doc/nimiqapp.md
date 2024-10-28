# Nimiq application : Common Technical Specifications

## About

This document describes the APDU messages interface to communicate with the Nimiq application.

The application covers the following functionalities:

  - Retrieve a Nimiq public key
  - Sign Nimiq transaction
  - Sign messages in the Nimiq format

## Transport protocol

### General transport description

Ledger APDU requests and responses are encapsulated using a flexible protocol allowing to fragment large payloads over
different underlying transport mechanisms.

The common transport header is defined as follows:

| *Description*                         | *Length* |
|---------------------------------------|----------|
| Communication channel ID (big endian) | 2        |
| Command tag                           | 1        |
| Packet sequence index (big endian)    | 2        |
| Payload                               | variable |

The Communication channel ID allows command multiplexing over the same physical link. It is not used for the time
being, and should be set to 0101 to avoid compatibility issues with implementations ignoring a leading 00 byte.

The Command tag describes the message content. Use TAG_APDU (0x05) for standard APDU payloads, or TAG_PING (0x02) for a
simple link test.

The Packet sequence index describes the current sequence for fragmented payloads. The first fragment index is 0x00.

Note that the handling of the low level transport protocol is handled by BOLOS / the SDK. The Nimiq app operates on the
APDU protocol on top of the low level transport protocol, see next section.

### Application Protocol Data Unit (APDU) protocol

The communication protocol used by [BOLOS](https://ledger.readthedocs.io/en/latest/bolos/overview.html) to exchange
[APDUs](https://en.wikipedia.org/wiki/Smart_card_application_protocol_data_unit) is very close to
[ISO 7816-4](https://www.iso.org/standard/77180.html) with a few differences:

- `Lc` length is always exactly 1 byte, i.e. payload length is limited to 255 bytes.
- No `Le` field in APDU command
- The maximum size of APDU commands and responses, including payload and encoding, is defined by IO_APDU_BUFFER_SIZE in
  the [SDK](https://github.com/LedgerHQ/ledger-secure-sdk/blob/master/include/os_io.h).

### Command APDU encoding

| *Name* | *Description*                                                         | *Length* |
|--------|-----------------------------------------------------------------------|----------|
| CLA    | Instruction class - indicates the type of command                     | 1        |
| INS    | Instruction code - indicates the specific command                     | 1        |
| P1     | Instruction parameter 1 for the command                               | 1        |
| P2     | Instruction parameter 2 for the command                               | 1        |
| Lc     | The number of bytes of command data to follow (a value from 0 to 255) | 1        |
| CData  | Command data with `Lc` bytes                                          | variable |

### Response APDU encoding

| *Name* | *Description*                | *Length* |
|--------|------------------------------|----------|
| RData  | Response data (can be empty) | variable |
| SW     | [Status word](#status-words) | 2        |


## Supported APDUs

### Get Public Key

#### Description

This command returns the public key for the given BIP 32 path. An optional message can be sent to sign to verify
the validity of the generated keypair. Optionally, the address can also be shown to the user for confirmation.

#### Encoding

**Command**

| *CLA* | *INS* | *P1*                        | *P2*                            |
|-------|-------|-----------------------------|---------------------------------|
| E0    | 02    | 00 : don't return signature | 00 : don't ask for confirmation |
|       |       | 01 : return signature       | 01 : ask for confirmation       |

**Input data**

| *Description*                                                       | *Length*          |
|---------------------------------------------------------------------|-------------------|
| Bip32 path length (max 10)                                          | 1                 |
| First Bip32 path entry (big endian)                                 | 4                 |
| ...                                                                 | 4                 |
| Last Bip32 path entry (big endian)                                  | 4                 |
| Message to sign, if requested. Must start with prefix "dummy-data:" | variable (max 31) |

**Output data**

| *Description*           | *Length* |
|-------------------------|----------|
| Public Key              | 32       |
| Signature, if requested | 64       |


### Sign Transaction

#### Description

This command shows the details of a transaction to the user and signs it upon confirmation.

#### Encoding

**Command**

| *CLA* | *INS* | *P1*               | *P2*              |
|-------|-------|--------------------|-------------------|
| E0    | 04    | 00: first apdu     | 00: last apdu     |
|       |       | 80: not first apdu | 80: not last apdu |

**Input data (first transaction data chunk)**

| *Description*                                   | *Length* |
|-------------------------------------------------|----------|
| Bip32 path length (max 10)                      | 1        |
| First Bip32 path entry (big endian)             | 4        |
| ...                                             | 4        |
| Last Bip32 path entry (big endian)              | 4        |
| Transaction version (00: Legacy, 01: Albatross) | 1        |
| Serialized transaction chunk                    | variable |

**Input data (other transaction data chunk)**

| *Description*                | *Length* |
|------------------------------|----------|
| Serialized transaction chunk | variable |

**Serialized transaction format**

The combined transaction chunks form the serialized transaction to sign. They are encoded as follows:
- [Legacy transactions](https://nimiq-network.github.io/developer-reference/chapters/transactions.html#extended-transaction)
- [Albatross transactions](https://www.nimiq.com/developers/learn/protocol/transactions)
- Note that the data to sign is always in extended transaction format, regardless of whether the transaction might be
  stored in reduced basic transaction format in the blockchain.

**Output data**

| *Description*                                     | *Length* |
|---------------------------------------------------|----------|
| EDDSA encoded transaction signature (ed25519)     | 64       |
| Optional EDDSA encoded staker signature (ed25519) | 64       |

The staker signature is returned only for staking transactions for which an empty signature proof was provided in the
transaction data, instead of a pre-signed staker signature proof. In this case, the Nimiq app creates the staker
signature proof automatically, with the same key as staker as the transaction sender, instead of the user having to
create the staker signature proof separately, for this case which is the most common case.


### Sign Message

#### Description

This command shows a message to the user and upon confirmation signs it in Nimiq format, see below.

#### Encoding

**Command**

| *CLA* | *INS* | *P1*               | *P2*              |
|-------|-------|--------------------|-------------------|
| E0    | 0A    | 00: first apdu     | 00: last apdu     |
|       |       | 80: not first apdu | 80: not last apdu |

**Input data (first message data chunk)**

| *Description*                                                     | *Length* |
|-------------------------------------------------------------------|----------|
| Bip32 path length (max 10)                                        | 1        |
| First Bip32 path entry (big endian)                               | 4        |
| ...                                                               | 4        |
| Last Bip32 path entry (big endian)                                | 4        |
| Flags (00: none, 01: prefer hex display, 02: prefer hash display) | 1        |
| Message length (big endian)                                       | 4        |
| Serialized message chunk                                          | variable |

**Input data (other message data chunk)**

| *Description*                                                                     | *Length* |
|-----------------------------------------------------------------------------------|----------|
| Serialized transaction chunk                                                      | variable |

**Serialized message format**

The combined message chunks form the plain binary message to sign. The message can always be displayed as message hash.
If it is at most [MAX_PRINTABLE_MESSAGE_LENGTH](https://github.com/nimiq/ledger-app-nimiq/blob/master/src/constants.h)
bytes long, it can be displayed as hex. If it additionally consists of ASCII characters only, it can be displayed as
ASCII text.

The actually signed message is in Nimiq message format, which is the sha256 hash of prefix "\x16Nimiq Signed Message:\n"
(where \x16, decimal 22, is the prefix length), of the message length rendered as decimal string, and of the plain
message. This is calculated automatically by the Nimiq app. Signing in Nimiq message format makes the calculated
signature recognisable as a Nimiq specific signature and prevents signing arbitrary data, e.g. a transaction.

**Output data**

| *Description*                     | *Length* |
|-----------------------------------|----------|
| Nimia message signature (ed25519) | 64       |


### Keep Alive

#### Description

This command can be sent to continue an async request previously interrupted by the app by a SW_KEEP_ALIVE heartbeat.
This can be used to work around timeouts of WebAuthn and U2F transport types, by renewing the request, but provides no
real value for other transport types, which do not time out and for which the app doesn't send heartbeats.

#### Encoding

**Command**

| *CLA* | *INS* | *P1*   | *P2*   |
|-------|-------|--------|--------|
| E0    | 08    | unused | unused |

**Input and output data**

This request has no own input and output data. It is used to extend a previous request.


## Status Words 

Status words tend to be similar to common
[APDU responses](https://www.eftlab.com/knowledge-base/complete-list-of-apdu-responses/) in the industry.

| *Name*               | *Description*                               | *SW* |
|----------------------|---------------------------------------------|------|
| SW_OK                | Normal ending of the command                | 9000 |
| SW_DENY              | Request denied by the user                  | 6985 |
| SW_INCORRECT_DATA    | Incorrect data                              | 6A80 |
| SW_NOT_SUPPORTED     | Request not currently supported             | 6A82 |
| SW_WRONG_P1P2        | Incorrect P1 or P2                          | 6A86 |
| SW_WRONG_DATA_LENGTH | Incorrect length                            | 6A87 |
| SW_INS_NOT_SUPPORTED | Unexpected INS                              | 6D00 |
| SW_CLA_NOT_SUPPORTED | Unexpected CLA                              | 6E00 |
| SW_KEEP_ALIVE        | Heartbeat response to avoid U2F timeouts    | 6E02 |
| SW_BAD_STATE         | Bad state                                   | B007 |
| SW_CRYPTOGRAPHY_FAIL | Failure of a cryptography related operation | B008 |