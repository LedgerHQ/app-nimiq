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
void on_rejected();
void on_address_approved();
void on_transaction_approved();
void on_message_approved();

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
        on_address_approved(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_VERIFIED, ui_menu_main);
    } else {
        on_rejected(),
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

// NBGL review utils

// The review with the most potential entries is the vesting creation flow. The maximum entries it can display are:
// amount, owner address, start, period, step count, step duration, first step duration, step amount, first step amount,
// last step amount, fee, network. Note that the single-block block entry and the multi-block vesting entries can not
// appear at the same time, same for the entry for the pre-vested amount and first step amount.
#define REVIEW_ENTRIES_MAX_COUNT 12
static struct {
    nbgl_contentTagValue_t entries[REVIEW_ENTRIES_MAX_COUNT];
    uint8_t count;
} review_entries;

static void review_entries_initialize() {
    // Initialize the structure with zeroes, including setting the count to 0, and the entries' .forcePageStart (don't
    // enforce a new page by default), .centeredInfo (don't center entry vertically) and .aliasValue (display full
    // values and no alias) to 0 / false.
    memset(&review_entries, 0, sizeof(review_entries));
}

WARN_UNUSED_RESULT
static error_t review_entries_add(const char *item, const char *value) {
    RETURN_ON_ERROR(
        review_entries.count >= REVIEW_ENTRIES_MAX_COUNT,
        ERROR_UNEXPECTED,
        "Too many nbgl ui entries\n"
    );
    review_entries.entries[review_entries.count].item = item;
    review_entries.entries[review_entries.count].value = value;
    review_entries.count++;
    return ERROR_NONE;
}
#undef REVIEW_ENTRIES_MAX_COUNT

WARN_UNUSED_RESULT
static error_t review_entries_add_optional(const char *item, const char *value, bool condition) {
    if (!condition) return ERROR_NONE;
    return review_entries_add(item, value);
}

static void review_entries_launch_use_case_review(
    nbgl_operationType_t operation_type,
    const nbgl_icon_details_t *icon,
    const char *review_title,
    const char *review_subtitle,
    const char *finish_title,
    nbgl_choiceCallback_t choice_callback,
    bool use_small_font
) {
    // The tagValueList gets copied in nbgl_useCaseReview, therefore it's sufficient to initialize it only in temporary
    // memory. We use G_io_apdu buffer as temporary buffer, to save some stack space, and check that it can fit the data
    // with a compile time assertion. Note that the entries are not copied, and we therefore have them in global memory.
    _Static_assert(
        sizeof(nbgl_contentTagValueList_t) <= sizeof(G_io_apdu_buffer),
        "G_io_apdu_buffer can't fit review list\n"
    );
    nbgl_contentTagValueList_t *temporary_tag_value_list_pointer = (nbgl_contentTagValueList_t*) G_io_apdu_buffer;
    // Initialize list with zeroes, including .nbMaxLinesForValue (no limit of lines to display).
    memset(temporary_tag_value_list_pointer, 0, sizeof(nbgl_contentTagValueList_t));
    temporary_tag_value_list_pointer->wrapping = true; // Prefer wrapping on spaces, e.g. for nicer address formatting.
    temporary_tag_value_list_pointer->smallCaseForValue = use_small_font;
    temporary_tag_value_list_pointer->pairs = review_entries.entries;
    temporary_tag_value_list_pointer->nbPairs = review_entries.count;

    nbgl_useCaseReview(
        operation_type,
        temporary_tag_value_list_pointer,
        icon,
        review_title,
        review_subtitle,
        finish_title,
        choice_callback
    );

    // Make sure that the list was in fact copied, to detect if that ever changes internally in nbgl_useCaseReview.
    // Unfortunately, we can't easily memcmp via a pointer to where the data is copied to, as the data is copied to a
    // static variable in nbgl_use_case.c and there's also no non-static method that exposes pointers to it. Instead, we
    // reset the temporary buffer, to make it immediately noticeable via Ragger end-to-end tests if the data was not
    // copied. Use explicit_bzero instead of memset to avoid this being optimized away.
    explicit_bzero(temporary_tag_value_list_pointer, sizeof(nbgl_contentTagValueList_t));
}

//////////////////////////////////////////////////////////////////////

// Transaction signing UI

WARN_UNUSED_RESULT
static error_t ui_transaction_prepare_review_entries_normal_or_staking_outgoing() {
    review_entries_initialize();
    RETURN_ON_ERROR(
        review_entries_add_optional(
            "Amount",
            PARSED_TX.value,
            ux_transaction_generic_has_amount_entry()
        )
        || review_entries_add(
            "Recipient",
            PARSED_TX_NORMAL_OR_STAKING_OUTGOING.recipient
        )
        || review_entries_add_optional(
            PARSED_TX_NORMAL_OR_STAKING_OUTGOING.extra_data_label,
            PARSED_TX_NORMAL_OR_STAKING_OUTGOING.extra_data,
            ux_transaction_normal_or_staking_outgoing_has_data_entry()
        )
        || review_entries_add_optional(
            "Fee",
            PARSED_TX.fee,
            ux_transaction_generic_has_fee_entry()
        )
        || review_entries_add(
            "Network",
            PARSED_TX.network
        ),
        ERROR_UNEXPECTED,
    );
    return ERROR_NONE;
}

WARN_UNUSED_RESULT
static error_t ui_transaction_prepare_review_entries_staking_incoming() {
    review_entries_initialize();
    RETURN_ON_ERROR(
        // Amount for non-signaling transactions
        review_entries_add_optional(
            "Amount",
            PARSED_TX.value,
            ux_transaction_generic_has_amount_entry()
        )
        // Amount in incoming staking data for signaling transactions
        || review_entries_add_optional(
            "Amount",
            PARSED_TX_STAKING_INCOMING.set_active_stake_or_retire_stake.amount,
            ux_transaction_staking_incoming_has_set_active_stake_or_retire_stake_amount_entry()
        )
        || review_entries_add_optional(
            "Staker",
            PARSED_TX_STAKING_INCOMING.validator_or_staker_address,
            ux_transaction_staking_incoming_has_staker_address_entry()
        )
        || review_entries_add_optional(
            "Delegation",
            PARSED_TX_STAKING_INCOMING.create_staker_or_update_staker.delegation,
            ux_transaction_staking_incoming_has_create_staker_or_update_staker_delegation_entry()
        )
        || review_entries_add_optional(
            "Reactivate all Stake",
            PARSED_TX_STAKING_INCOMING.create_staker_or_update_staker
                .update_staker_reactivate_all_stake,
            ux_transaction_staking_incoming_has_update_staker_reactivate_all_stake_entry()
        )
        || review_entries_add_optional(
            "Fee",
            PARSED_TX.fee,
            ux_transaction_generic_has_fee_entry()
        )
        || review_entries_add(
            "Network",
            PARSED_TX.network
        ),
        ERROR_UNEXPECTED
    );
    return ERROR_NONE;
}

static void on_transaction_reviewed(bool approved) {
    if (approved) {
        on_transaction_approved(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
        on_rejected(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main);
    }
}

WARN_UNUSED_RESULT
error_t ui_transaction_signing() {
    // Pointers to pre-existing const strings in read-only data segment / flash memory. Not meant to be written to.
    // While the strings are a bit repetitive, and thus somewhat wasteful in flash memory, we still prefer this approach
    // over assembling the string in RAM memory, because that is the much more limited resource. Manual newlines are set
    // for a nicer layout of the texts.
    const char *review_title;
    const char *finish_title;
    const char *review_subtitle = NULL; // none by default
    switch (PARSED_TX.transaction_label_type) {
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
            // This should not happen, as the transaction parser should have set a valid transaction label type.
            RETURN_ERROR(
                ERROR_UNEXPECTED,
                "Invalid transaction label type\n"
            );
    }

    switch (PARSED_TX.transaction_type) {
        case TRANSACTION_TYPE_NORMAL:
        case TRANSACTION_TYPE_STAKING_OUTGOING:
            RETURN_ON_ERROR(
                ui_transaction_prepare_review_entries_normal_or_staking_outgoing()
            );
            break;
        case TRANSACTION_TYPE_STAKING_INCOMING:
            RETURN_ON_ERROR(
                ui_transaction_prepare_review_entries_staking_incoming()
            );
            break;
        case TRANSACTION_TYPE_VESTING_CREATION:
        case TRANSACTION_TYPE_HTLC_CREATION:
        default:
            // This should not happen, as the transaction parser should have set a valid transaction type.
            RETURN_ERROR(
                ERROR_UNEXPECTED,
                "Invalid transaction type\n"
            );
    }

    review_entries_launch_use_case_review(
        /* operation_type */ TYPE_TRANSACTION,
        /* icon */ &C_app_nimiq_64px,
        /* review_title */ review_title,
        /* review_subtitle */ review_subtitle,
        /* finish_title */ finish_title,
        /* choice_callback */ on_transaction_reviewed,
        /* use_small_font */ false
    );
    return ERROR_NONE;
}

//////////////////////////////////////////////////////////////////////

// Message signing UI

WARN_UNUSED_RESULT
static error_t ui_message_prepare_review_entries(message_display_type_t messageDisplayType) {
    review_entries_initialize();
    ctx.req.msg.confirm.displayType = messageDisplayType; // used in ux_message_signing_prepare_printed_message
    RETURN_ON_ERROR(
        ux_message_signing_prepare_printed_message()
    );
    return review_entries_add(
        ctx.req.msg.confirm.printedMessageLabel,
        ctx.req.msg.confirm.printedMessage
    );
}

// Called when long press button on 3rd page is long-touched or when reject footer is touched.
static void on_message_reviewed(bool approved) {
    if (approved) {
        on_message_approved(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_menu_main);
    } else {
        on_rejected(),
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_menu_main);
    }
}

WARN_UNUSED_RESULT
error_t ui_message_signing(message_display_type_t messageDisplayType, bool startAtMessageDisplay) {
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
            RETURN_ERROR(
                ERROR_UNEXPECTED,
                "Invalid message display type\n"
            );
    }
    RETURN_ON_ERROR(
        ui_message_prepare_review_entries(messageDisplayType)
    );

    review_entries_launch_use_case_review(
        /* operation_type */ TYPE_MESSAGE,
        /* icon */ &C_Review_64px, // provided by ledger-secure-sdk
        /* review_title */ "Review message",
        /* review_subtitle */ review_subtitle,
        /* finish_title */ "Sign message",
        /* choice_callback */ on_message_reviewed,
        /* use_small_font */ true
    );
    return ERROR_NONE;
}

#endif // HAVE_NBGL
