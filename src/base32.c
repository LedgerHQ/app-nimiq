/*******************************************************************************
*   Ledger Nimiq App
*   (c) 2018 Ledger
*
*  adapted from https://stash.forgerock.org/projects/OPENAM/repos/forgerock-authenticator-ios/browse/ForgeRock-Authenticator/base32.c
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

#include "base32.h"

WARN_UNUSED_RESULT
error_t base32_encode(const uint8_t *data, int length, char *result, int buf_size) {
    int count = 0;
    int quantum = 8;

    RETURN_ON_ERROR(
        length < 0 || length > (1 << 28),
        ERROR_INVALID_LENGTH
    );

    if (length > 0) {
        int buffer = data[0];
        int next = 1;
        int bitsLeft = 8;

        while (count < buf_size && (bitsLeft > 0 || next < length)) {
            if (bitsLeft < 5) {
                if (next < length) {
                    buffer <<= 8;
                    buffer |= data[next++] & 0xFF;
                    bitsLeft += 8;
                } else {
                    int pad = 5 - bitsLeft;
                    buffer <<= pad;
                    bitsLeft += pad;
                }
            }

            int index = 0x1F & (buffer >> (bitsLeft - 5));
            bitsLeft -= 5;
            result[count++] = "0123456789ABCDEFGHJKLMNPQRSTUVXY"[index];

            // Track the characters which make up a single quantum of 8 characters
            quantum--;
            if (quantum == 0) {
                quantum = 8;
            }
        }

        // If the number of encoded characters does not make a full quantum, insert padding
        if (quantum != 8) {
            while (quantum > 0 && count < buf_size) {
                result[count++] = '=';
                quantum--;
            }
        }
    }

    // Finally check if we exceeded buffer size.
    RETURN_ON_ERROR(
        count >= buf_size,
        ERROR_INVALID_LENGTH
    );

    result[count] = '\000';
    return ERROR_NONE;
}