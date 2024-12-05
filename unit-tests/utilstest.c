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

void test_print_amount(uint64_t amount, char *expected) {
    char *asset = "NIM";
    char printed[24];
    print_amount(amount, asset, printed);

    if (strcmp(printed, expected) != 0) {
        printf("test_print_amount failed. Expected: %s; Actual: %s\n", expected, printed);
    }
}

void test_print_caption(uint8_t operationType, uint8_t captionType, char *expected) {
    char s[15];
    print_caption(operationType, captionType, s);
    if (strcmp(s, expected) != 0) {
        printf("test_print_caption failed. Expected: %s; Actual: %s\n", expected, s);
    }
}

int main(int argc, char *argv[]) {

    test_print_amount(1, "0.00001 NIM");
    test_print_amount(10000000, "100 NIM");
    test_print_amount(100000000000001, "1000000000.00001 NIM");
    test_print_amount(100000001, "1000.00001 NIM");
    test_print_amount(1000000010000, "10000000.1 NIM");

    test_print_caption(0, 0, "Basic Tx");

    return 0;
}
