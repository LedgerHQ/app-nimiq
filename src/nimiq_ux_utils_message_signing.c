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

#include "nimiq_ux_utils_message_signing.h"
#include "globals.h"

void ux_message_signing_prepare_printed_message() {
    // No errors are expected here, as all data has already been verified in handleSignMessage
    switch (ctx.req.msg.confirm.displayType) {
        case MESSAGE_DISPLAY_TYPE_ASCII:
            strcpy(ctx.req.msg.confirm.printedMessageLabel, "Message");
            os_memmove(ctx.req.msg.confirm.printedMessage, ctx.req.msg.printableMessage, ctx.req.msg.messageLength);
            ctx.req.msg.confirm.printedMessage[ctx.req.msg.messageLength] = '\0'; // string terminator
            break;
        case MESSAGE_DISPLAY_TYPE_HEX:
            strcpy(ctx.req.msg.confirm.printedMessageLabel, "Message Hex");
            LEDGER_ASSERT(
                print_hex(ctx.req.msg.printableMessage, ctx.req.msg.messageLength, ctx.req.msg.confirm.printedMessage,
                    sizeof(ctx.req.msg.confirm.printedMessage)) == ERROR_NONE,
                "Failed to print message hex"
            );
            break;
        case MESSAGE_DISPLAY_TYPE_HASH:
            strcpy(ctx.req.msg.confirm.printedMessageLabel, "Message Hash");
            LEDGER_ASSERT(
                print_hex(ctx.req.msg.confirm.messageHash, sizeof(ctx.req.msg.confirm.messageHash),
                    ctx.req.msg.confirm.printedMessage, sizeof(ctx.req.msg.confirm.printedMessage)) == ERROR_NONE,
                "Failed to print message hash"
            );
            break;
    }
}
