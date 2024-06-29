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

#ifdef HAVE_NBGL

// From Ledger SDK
#include "ux.h"
#include "nbgl_use_case.h"

#include "nimiq_ux.h"
#include "globals.h"
#include "nimiq_ux_utils_transaction_signing.h"
#include "nimiq_ux_utils_message_signing.h"

// These are declared in main.c
void app_exit();
unsigned int io_seproxyhal_on_address_approved();
unsigned int io_seproxyhal_on_address_rejected();
unsigned int io_seproxyhal_on_transaction_approved();
unsigned int io_seproxyhal_on_transaction_rejected();
unsigned int io_seproxyhal_on_message_approved();
unsigned int io_seproxyhal_on_message_rejected();

// Main menu and about menu

#define ABOUT_MENU_INFO_COUNT 2
// These const globals are stored in the read-only data segment of the app, i.e. in the flash memory and not in RAM,
// therefore we don't need to do any memory optimizations here (e.g. storing it in a struct union with other data). Also
// note that we don't need to mind the usage of const pointers ABOUT_MENU_INFO_TYPES and ABOUT_MENU_INFO_CONTENTS in
// const ABOUT_MENU_INFO_LIST with regards to the link address of the pointers being different from the runtime address
// (see documentation) as ngbl_use_case.h takes care of this by using the PIC macro.
static const char* const ABOUT_MENU_INFO_TYPES[ABOUT_MENU_INFO_COUNT] = {"Version", "Developer"};
static const char* const ABOUT_MENU_INFO_CONTENTS[ABOUT_MENU_INFO_COUNT] = {APPVERSION, /* "Nimiq" */ APPNAME};
static const nbgl_contentInfoList_t ABOUT_MENU_INFO_LIST = {
    .nbInfos = ABOUT_MENU_INFO_COUNT,
    .infoTypes = ABOUT_MENU_INFO_TYPES,
    .infoContents = ABOUT_MENU_INFO_CONTENTS,
};
#undef ABOUT_MENU_INFO_COUNT

void ui_menu_main() {
    nbgl_useCaseHomeAndSettings(
        /* appName */ APPNAME,
        /* appIcon */ &C_app_nimiq_64px,
        /* tagline */ NULL, // use default description
        /* initSettingPage */ INIT_HOME_PAGE, // start at the first page
        /* settingContents */ NULL, // no settings
        /* infosList */ &ABOUT_MENU_INFO_LIST,
        /* action */ NULL, // no additional action button
        /* quitCallback */ app_exit
    );
}

//////////////////////////////////////////////////////////////////////

// Address confirmation UI

static void on_address_reviewed(bool approved) {
    if (approved) {
        io_seproxyhal_on_address_approved(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_VERIFIED, ui_menu_main);
    } else {
        io_seproxyhal_on_address_rejected(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_REJECTED, ui_menu_main);
    }
}

void ui_public_key() {
    // Format address into three blocks per line by manually replacing the appropriate spaces with newlines. Note that
    // this also affects the rendered QR code, which is fine though as the Nimiq address format is agnostic to the type
    // of whitespace used.
    ctx.req.pk.address[14] = '\n';
    ctx.req.pk.address[29] = '\n';
    nbgl_useCaseAddressReview(
        /* address */ ctx.req.pk.address,
        /* additionalTagValueList */ NULL, // no additional infos
        /* icon */ &C_app_nimiq_64px,
        /* reviewTitle */ "Verify NIM address",
        /* reviewSubtitle */ "After copying and pasting, or scanning the address, make sure that it matches what's "
            "shown on your Ledger.",
        /* choiceCallback */ on_address_reviewed
    );
}

//////////////////////////////////////////////////////////////////////

// Transaction signing UI

// NBGL specific transaction utils
// The flow with the most potential entries is the vesting creation flow. the maximum entries it can display are:
// amount, owner address, start, period, step count, step duration, first step duration, step amount, first step amount,
// last step amount, fee, network. Note that the single-block block entry and the multi-block vesting entries can not
// appear at the same time, same for the entry for the pre-vested amount and first step amount.
#define TRANSACTION_REVIEW_MAX_ENTRIES_COUNT 12
static struct {
    nbgl_contentTagValueList_t list;
    nbgl_contentTagValue_t entries[TRANSACTION_REVIEW_MAX_ENTRIES_COUNT];
} transaction_review_entries;

static void transaction_review_entries_initialize() {
    // Initialize the structure with zeroes, including the list's .nbPairs (empty list), .nbMaxLinesForValue (no limit
    // of lines to display) and .smallCaseForValue (use regular font), as well as the entries' .forcePageStart (don't
    // enforce a new page by default), .centeredInfo (don't center entry vertically) and .aliasValue (display full
    // values and no alias).
    memset(&transaction_review_entries, 0, sizeof(transaction_review_entries));
    transaction_review_entries.list.pairs = transaction_review_entries.entries;
    transaction_review_entries.list.wrapping = true; // Prefer wrapping on spaces, for example for nicer address format.
}

static void transaction_review_entries_add(const char *item, const char *value) {
    if (transaction_review_entries.list.nbPairs >= TRANSACTION_REVIEW_MAX_ENTRIES_COUNT) {
        PRINTF("Too many nbgl transaction ui entries");
        THROW(0x6700);
    }
    transaction_review_entries.entries[transaction_review_entries.list.nbPairs].item = item;
    transaction_review_entries.entries[transaction_review_entries.list.nbPairs].value = value;
    transaction_review_entries.list.nbPairs++;
}
#undef TRANSACTION_REVIEW_MAX_ENTRIES_COUNT

static void transaction_review_entries_add_optional(const char *item, const char *value, bool condition) {
    if (!condition) return;
    transaction_review_entries_add(item, value);
}

static void transaction_review_entries_prepare_normal_or_staking_outgoing() {
    transaction_review_entries_initialize();
    transaction_review_entries_add_optional(
        "Amount",
        ctx.req.tx.content.value,
        ux_transaction_generic_has_amount_entry()
    );
    transaction_review_entries_add(
        "Recipient",
        ctx.req.tx.content.type_specific.normal_or_staking_outgoing_tx.recipient
    );
    transaction_review_entries_add_optional(
        ctx.req.tx.content.type_specific.normal_or_staking_outgoing_tx.extra_data_label,
        ctx.req.tx.content.type_specific.normal_or_staking_outgoing_tx.extra_data,
        ux_transaction_normal_or_staking_outgoing_has_data_entry()
    );
    transaction_review_entries_add_optional(
        "Fee",
        ctx.req.tx.content.fee,
        ux_transaction_generic_has_fee_entry()
    );
    transaction_review_entries_add(
        "Network",
        ctx.req.tx.content.network
    );
}

static void transaction_review_entries_prepare_staking_incoming() {
    transaction_review_entries_initialize();
    // Amount for non-signaling transactions
    transaction_review_entries_add_optional(
        "Amount",
        ctx.req.tx.content.value,
        ux_transaction_generic_has_amount_entry()
    );
    // Amount in incoming staking data for signaling transactions
    transaction_review_entries_add_optional(
        "Amount",
        ctx.req.tx.content.type_specific.staking_incoming_tx.set_active_stake_or_retire_stake.amount,
        ux_transaction_staking_incoming_has_set_active_stake_or_retire_stake_amount_entry()
    );
    transaction_review_entries_add_optional(
        "Staker",
        ctx.req.tx.content.type_specific.staking_incoming_tx.validator_or_staker_address,
        ux_transaction_staking_incoming_has_staker_address_entry()
    );
    transaction_review_entries_add_optional(
        "Delegation",
        ctx.req.tx.content.type_specific.staking_incoming_tx.create_staker_or_update_staker.delegation,
        ux_transaction_staking_incoming_has_create_staker_or_update_staker_delegation_entry()
    );
    transaction_review_entries_add_optional(
        "Reactivate all Stake",
        ctx.req.tx.content.type_specific.staking_incoming_tx.create_staker_or_update_staker
            .update_staker_reactivate_all_stake,
        ux_transaction_staking_incoming_has_update_staker_reactivate_all_stake_entry()
    );
    transaction_review_entries_add_optional(
        "Fee",
        ctx.req.tx.content.fee,
        ux_transaction_generic_has_fee_entry()
    );
    transaction_review_entries_add(
        "Network",
        ctx.req.tx.content.network
    );
}

static void on_transaction_reviewed(bool approved) {
    if (approved) {
        io_seproxyhal_on_transaction_approved(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
        io_seproxyhal_on_transaction_rejected(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main);
    }
}

void ui_transaction_signing() {
    // Pointers to pre-existing const strings in read-only data segment / flash memory. Not meant to be written to.
    // While the strings are a bit repetitive, and thus somewhat wasteful in flash memory, we still prefer this approach
    // over assembling the string in RAM memory, because that is the much more limited resource. Manual newlines are set
    // for a nicer layout of the texts.
    const char *review_title;
    const char *finish_title;
    const char *review_subtitle = NULL; // none by default
    switch (ctx.req.tx.content.transaction_label_type) {
        case TRANSACTION_LABEL_TYPE_REGULAR_TRANSACTION:
            review_title = "Review transaction\nto send NIM";
            finish_title = "Sign transaction\nto send NIM";
            break;
        case TRANSACTION_LABEL_TYPE_CASHLINK:
            review_title = "Review\nCashlink creation";
            finish_title = "Sign\nCashlink creation";
            break;
        case TRANSACTION_LABEL_TYPE_VESTING_CREATION:
            review_title = "Review\nVesting Contract creation";
            finish_title = "Sign\nVesting Contract creation";
            break;
        case TRANSACTION_LABEL_TYPE_HTLC_CREATION:
            review_title = "Review\nHTLC creation / Swap";
            finish_title = "Sign\nHTLC creation / Swap";
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_CREATE_STAKER:
            review_title = "Review staking setup\nto start staking";
            finish_title = "Sign staking setup\nto start staking";
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_ADD_STAKE:
            review_title = "Review\naddition of stake";
            finish_title = "Sign\naddition of stake";
            review_subtitle = "This transaction increases your amount of NIM assigned for staking.";
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_UPDATE_STAKER:
            review_title = "Review update\nof staking setup";
            finish_title = "Sign update\nof staking setup";
            review_subtitle = "This transaction updates your current staking setup with the following changes.";
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_SET_ACTIVE_STAKE:
            review_title = "Review setting\nactive stake amount";
            finish_title = "Sign setting\nactive stake amount";
            review_subtitle = "This transaction sets the NIM amount that will be actively used for staking. Set it to "
                "0, if you want to change the validator you're delegating to.";
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_RETIRE_STAKE:
            review_title = "Review retiring\nstaked NIM";
            finish_title = "Sign retiring\nstaked NIM";
            review_subtitle = "This transaction will disable the given amount of NIM from staking. This needs to be "
                "done first, if you want to later withdraw them.";
            break;
        case TRANSACTION_LABEL_TYPE_STAKING_REMOVE_STAKE:
            review_title = "Review withrawal\nof previously staked NIM";
            finish_title = "Sign withdrawal\nof previously staked NIM";
            break;
        default:
            PRINTF("Invalid transaction label type");
            THROW(0x6a80);
    }

    switch (ctx.req.tx.content.transaction_type) {
        case TRANSACTION_TYPE_NORMAL:
        case TRANSACTION_TYPE_STAKING_OUTGOING:
            transaction_review_entries_prepare_normal_or_staking_outgoing();
            break;
        case TRANSACTION_TYPE_STAKING_INCOMING:
            transaction_review_entries_prepare_staking_incoming();
            break;
        case TRANSACTION_TYPE_VESTING_CREATION:
        case TRANSACTION_TYPE_HTLC_CREATION:
        default:
            PRINTF("Invalid transaction type");
            THROW(0x6a80);
    }

    nbgl_useCaseReview(
        /* operationType */ TYPE_TRANSACTION,
        /* tagValueList */ &transaction_review_entries.list,
        /* icon */ &C_app_nimiq_64px,
        /* reviewTitle */ review_title,
        /* reviewSubTitle */ review_subtitle,
        /* finishTitle */ finish_title,
        /* choiceCallback */ on_transaction_reviewed
    );
}

//////////////////////////////////////////////////////////////////////

// Message signing UI

// Although we only have a single entry with fixed content, these are currenlty not const globals stored in the read-
// only data segment of the app, i.e. in the flash memory and not in RAM, as this errors with a segmentation faul. It
// seems like the PIC macro is missing in some places in nbgl_use_case.h to convert the link addresses of pointers in
// the structures to their runtime addresses for pointers encoded in const data (see documentation). For this reason, we
// currently have them in RAM, which is not ideal as the RAM is very limited.
nbgl_contentTagValue_t message_review_entry;
nbgl_contentTagValueList_t message_review_list;

// Called when long press button on 3rd page is long-touched or when reject footer is touched.
static void on_message_reviewed(bool approved) {
    if (approved) {
        io_seproxyhal_on_message_approved(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_menu_main);
    } else {
        io_seproxyhal_on_message_rejected(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_menu_main);
    }
}

void ui_message_signing(message_display_type_t messageDisplayType, bool startAtMessageDisplay) {
    ctx.req.msg.confirm.displayType = messageDisplayType;
    UNUSED(startAtMessageDisplay);

    // Pointer to pre-existing const strings in read-only data segment / flash memory. Not meant to be written to.
    const char *review_subtitle;
    switch (messageDisplayType) {
        case MESSAGE_DISPLAY_TYPE_ASCII:
            review_subtitle = NULL; // none
            break;
        case MESSAGE_DISPLAY_TYPE_HEX:
            review_subtitle = "The message is displayed in HEX format.";
            break;
        case MESSAGE_DISPLAY_TYPE_HASH:
            review_subtitle = "The message is displayed as SHA-256 hash.";
            break;
        default:
            PRINTF("Invalid message display type");
            THROW(0x6a80);
    }
    ux_message_signing_prepare_printed_message();

    message_review_entry.item = ctx.req.msg.confirm.printedMessageLabel,
    message_review_entry.value = ctx.req.msg.confirm.printedMessage,
    message_review_list.nbPairs = 1,
    message_review_list.pairs = &message_review_entry,
    message_review_list.smallCaseForValue = true, // use a smaller font
    message_review_list.wrapping = true, // prefer wrapping on spaces
    nbgl_useCaseReview(
        /* operationType */ TYPE_MESSAGE,
        /* tagValueList */ &message_review_list,
        /* icon */ &C_Review_64px, // provided by ledger-secure-sdk
        /* reviewTitle */ "Review message",
        /* reviewSubTitle */ review_subtitle,
        /* finishTitle */ "Sign message",
        /* choiceCallback */ on_message_reviewed
    );
}

#endif // HAVE_NBGL
