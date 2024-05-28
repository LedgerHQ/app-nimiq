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

#include "signature_proof.h"
#include "nimiq_utils.h"

/**
 * Read a signature proof from a buffer. Note that all pointers in the returned signature proof are to the original
 * buffer. No copy of the data is created.
 */
signature_proof_t read_signature_proof(uint8_t **in_out_buffer, uint16_t *in_out_buffer_length) {
    // See Serde::Serialize for SignatureProof in primitives/transaction/src/signature_proof.rs in core-rs-albatross.
    signature_proof_t signature_proof;
    signature_proof.type_and_flags = readUInt8(in_out_buffer, in_out_buffer_length);
    if (signature_proof.type_and_flags != 0) {
        PRINTF("Only ed25519 signature proofs without flags supported");
        THROW(0x6a80);
    }
    signature_proof.public_key = readSubBuffer(32, in_out_buffer, in_out_buffer_length);
    signature_proof.merkle_path_length = readUInt8(in_out_buffer, in_out_buffer_length);
    if (signature_proof.merkle_path_length) {
        PRINTF("Only signature proofs with empty merkle path supported");
        THROW(0x6a80);
    }
    signature_proof.signature = readSubBuffer(64, in_out_buffer, in_out_buffer_length);
    return signature_proof;
}

bool is_empty_default_signature_proof(signature_proof_t signature_proof) {
    // The empty default signature proof entirely consists of bytes of value 0, see Default for SignatureProof in
    // primitives/transaction/src/signature_proof.rs
    if (
        // type field (algorithm and flags), by default Ed25519 (PublicKey enum value 0) and no flags set
        signature_proof.type_and_flags != 0
        // Empty merkle path encoding u8 length 0, see Default and Serialize for MerklePath in utils/src/merkle/mod.rs
        || signature_proof.merkle_path_length != 0
    ) return false;
    // Ed25519PublicKey filled with 0s, see Default and Serialize in keys/src/public_key.rs
    for (uint8_t i = 0; i < 32; i++) {
        if (signature_proof.public_key[i] != 0) return false;
    }
    // Ed25519Signature filled with 0s, see Default and Serialize in keys/src/signature.rs
    for (uint8_t i = 0; i < 64; i++) {
        if (signature_proof.signature[i] != 0) return false;
    }
    return true;
}
