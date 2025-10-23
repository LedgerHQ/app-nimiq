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

#ifndef _NIMIQ_UX_H_
#define _NIMIQ_UX_H_

#include <stdbool.h>

#include "constants.h"

#if defined(TARGET_STAX) || defined(TARGET_FLEX)
#define ICON_APP_NIMIQ  C_app_nimiq_64px
#define ICON_APP_HOME   ICON_APP_NIMIQ
#define ICON_APP_REVIEW LARGE_REVIEW_ICON
#elif defined(TARGET_APEX_P)
#define ICON_APP_NIMIQ  C_app_nimiq_48px
#define ICON_APP_HOME   ICON_APP_NIMIQ
#define ICON_APP_REVIEW LARGE_REVIEW_ICON
#endif

void ui_menu_main();

void ui_public_key();

void ui_transaction_signing();

void ui_message_signing(message_display_type_t messageDisplayType, bool startAtMessageDisplay);

#endif // _NIMIQ_UX_H_
