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

#include "error_macros.h"

sw_t error_to_sw(error_t error) {
    switch (error) {
        case ERROR_READ: // Assume that the read error is due to too short data, which is the most common reason.
        case ERROR_INVALID_LENGTH:
            return SW_WRONG_DATA_LENGTH;
        case ERROR_INCORRECT_DATA:
            return SW_INCORRECT_DATA;
        case ERROR_NOT_SUPPORTED:
            return SW_NOT_SUPPORTED;
        case ERROR_CRYPTOGRAPHY:
            return SW_CRYPTOGRAPHY_FAIL;
        case ERROR_NONE:
            return SW_OK;
        default:
            // ERROR_TRUE, ERROR_UNEXPECTED or an error which is incorrectly missing here. Exit the app.
            LEDGER_ASSERT(
                false,
                "Unexpected error 0x%02x",
                error
            );
            return SW_BAD_STATE;
    }
}
