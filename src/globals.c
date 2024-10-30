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

// From Ledger SDK
#include "os_io_seproxyhal.h"
#include "ux.h"

#include "globals.h"

// Specified in os_io_seproxyhal.h
unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

// Specified in ux_bagl.h / ux_nbgl.h, included via ux.h
ux_state_t G_ux;
bolos_ux_params_t G_ux_params;

// Specified in globals.h
const internal_storage_t N_storage_real; // const variable in flash storage; writable via nvm_write
generalContext_t ctx;
