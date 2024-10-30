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

#ifndef _NIMIQ_SIGNATURE_PROOF_H_
#define _NIMIQ_SIGNATURE_PROOF_H_

#include <stdint.h>
#include <stdbool.h>

#include "error_macros.h"

typedef struct {
    // Currently only ed25519 without flags and only empty merkle paths are supported, therefore:
    // - the public key is an ed25519 public key and the signature an ed25519 signature
    // - no WebauthnExtraFields are present
    // - no merkle path compressed vector or node hashes are present.
    uint8_t *public_key; // pointer to a 32 byte ed25519 public key
    // uint8_t *merkle_path_compressed; // NULL or a pointer to a VecU8
    // uint8_t *merkle_path_node_hashes; // NULL or a pointer to a VecU8
    uint8_t *signature; // pointer to a 64 byte ed25519 signature
    uint8_t type_and_flags;
    uint8_t merkle_path_length;
} signature_proof_t;

WARN_UNUSED_RESULT
bool read_signature_proof(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length,
    signature_proof_t *out_signature_proof);

bool is_empty_default_signature_proof(signature_proof_t signature_proof);

#endif // _NIMIQ_SIGNATURE_PROOF_H_
