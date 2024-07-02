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
#include <stdio.h>
#include <string.h>
#include "nimiq_utils.h"
#include "test_utils.h"

int main(int argc, char *argv[]) {

    if(argc != 2 ) {
        printf("Expected argument missing\n");
        return 1;
    }

    char *filename = argv[1];
    uint8_t buffer[130];
    txContent_t txContent;

    char expected_expected_data[] = "f! \" # $ % & ' ( ) * + , - . / : ; < = > ? @ [  ] ^ _ ` { | } ~o";
    char expected_recipient[] = "NQ12 DHBY 842X 6RP1 Y94E K854 EL9P 77H6 0JJ1";
    char expected_amount[] = "0.001 NIM";
    char expected_fee[] = "0.00001 NIM";
    char expected_validity_start[] = "69517";

    int read = read_file(filename, buffer, 4096);
    if (read) {
        // printHexBlocks(buffer, read/2);
        parseTx(buffer, &txContent);

        if (strcmp(txContent.details1, expected_expected_data) != 0) {
            printf(
                "parseTx failed on extra data. Expected: %s; Actual: %s\n",
                expected_expected_data,
                txContent.details1
            );
        }
        if (strcmp(txContent.recipient, expected_recipient) != 0) {
            printf(
                "parseTx failed on recipient. Expected: %s; Actual: %s\n",
                expected_recipient,
                txContent.recipient
            );
        }
        if (strcmp(txContent.value, expected_amount) != 0) {
            printf(
                "parseTx failed on amount. Expected: %s; Actual: %s\n",
                expected_amount,
                txContent.value
            );
        }
        if (strcmp(txContent.fee, expected_fee) != 0) {
            printf(
                "parseTx failed on fee. Expected: %s; Actual: %s\n",
                expected_fee,
                txContent.fee
            );
        }
        if (strcmp(txContent.validity_start, expected_validity_start) != 0) {
            printf(
                "parseTx failed on validity start. Expected: %s; Actual: %s\n",
                expected_validity_start,
                txContent.validity_start
            );
        }
    }

    return 0;

}
