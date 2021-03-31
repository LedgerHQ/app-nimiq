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

#include "os.h"
#include "cx.h"
#include <stdbool.h>
#include <limits.h>

#include "string.h"

#include "ux.h"

#include "nimiq_utils.h"
#include "ux_macros.h"

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

uint32_t set_result_get_publicKey(void);

#define MAX_BIP32_PATH 10

#define CLA 0xE0
#define INS_GET_PUBLIC_KEY 0x02
#define INS_SIGN_TX 0x04
#define INS_KEEP_ALIVE 0x08
#define P1_NO_SIGNATURE 0x00
#define P1_SIGNATURE 0x01
#define P2_NO_CONFIRM 0x00
#define P2_CONFIRM 0x01
#define P1_FIRST 0x00
#define P1_MORE 0x80
#define P2_LAST 0x00
#define P2_MORE 0x80

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_P2 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

#define MAX_RAW_TX 130

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    char address[45];
    uint8_t signature[64];
    bool returnSignature;
} publicKeyContext_t;

typedef struct transactionContext_t {
    uint8_t bip32PathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t rawTx[MAX_RAW_TX];
    uint32_t rawTxLength;
    txContent_t content;
} transactionContext_t;

typedef struct {
    union {
        publicKeyContext_t pk;
        transactionContext_t tx;
    } req;
    uint16_t u2fTimer;
} generalContext_t;

generalContext_t ctx;

unsigned int io_seproxyhal_touch_tx_ok();
unsigned int io_seproxyhal_touch_tx_cancel();
unsigned int io_seproxyhal_touch_address_ok();
unsigned int io_seproxyhal_touch_address_cancel();
void ui_idle(void);

ux_state_t G_ux;
bolos_ux_params_t G_ux_params;

typedef struct internalStorage_t {
    uint8_t fidoTransport;
    uint8_t initialized;
} internalStorage_t;

internalStorage_t const N_storage_real;
#define N_storage (*(internalStorage_t *)PIC(&N_storage_real))

//////////////////////////////////////////////////////////////////////

// Main Menu
UX_STEP_NOCB(
    ux_idle_flow_welcome_step,
    nn,
    {
        "Application",
        "is ready",
    });
UX_STEP_NOCB(
    ux_idle_flow_version_step,
    bn,
    {
        "Version",
        APPVERSION,
    });
UX_STEP_CB(
    ux_idle_flow_quit_step,
    pb,
    os_sched_exit(-1),
    {
        &C_icon_dashboard,
        "Quit",
    });
UX_FLOW(ux_idle_flow,
    &ux_idle_flow_welcome_step,
    &ux_idle_flow_version_step,
    &ux_idle_flow_quit_step
);

//////////////////////////////////////////////////////////////////////

// Transaction confirmation UI
UX_STEP_NOCB(ux_transaction_flow_transaction_type_step,
    pnn,
    {
        &C_icon_eye,
        "Confirm",
        ctx.req.tx.content.transaction_type_label,
    });
UX_STEP_NOCB(
    ux_transaction_flow_amount_step,
    paging,
    {
        "Amount",
        ctx.req.tx.content.value,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_transaction_flow_fee_step,
    paging,
    strcmp(ctx.req.tx.content.fee, "0 NIM") != 0,
    {
        "Fee",
        ctx.req.tx.content.fee,
    });
UX_STEP_NOCB(
    ux_transaction_flow_recipient_step,
    paging,
    {
        "Recipient",
        ctx.req.tx.content.recipient,
    });
UX_OPTIONAL_STEP_NOCB(
    ux_transaction_flow_data_step,
    paging,
    ctx.req.tx.content.transaction_type != TRANSACTION_TYPE_CASHLINK && strlen(ctx.req.tx.content.extra_data),
    {
        "Data",
        ctx.req.tx.content.extra_data,
    });
UX_STEP_NOCB(
    ux_transaction_flow_network_step,
    paging,
    {
        "Network",
        ctx.req.tx.content.network,
    });
UX_STEP_CB(
    ux_transaction_flow_approve_step,
    pbb,
    io_seproxyhal_touch_tx_ok(),
    {
        &C_icon_validate_14,
        "Accept",
        "and send",
    });
UX_STEP_CB(
    ux_transaction_flow_reject_step,
    pb,
    io_seproxyhal_touch_tx_cancel(),
    {
        &C_icon_crossmark,
        "Reject",
    });
UX_FLOW(ux_transaction_flow,
    &ux_transaction_flow_transaction_type_step,
    &ux_transaction_flow_amount_step,
    &ux_transaction_flow_fee_step, // optional
    &ux_transaction_flow_recipient_step,
    &ux_transaction_flow_data_step, // optional
    &ux_transaction_flow_network_step,
    &ux_transaction_flow_approve_step,
    &ux_transaction_flow_reject_step
);

//////////////////////////////////////////////////////////////////////

// Address confirmation UI
UX_STEP_NOCB(
    ux_public_key_flow_address_step,
    paging,
    {
        "Address",
        ctx.req.pk.address,
    });
UX_STEP_CB(
    ux_public_key_flow_approve_step,
    pb,
    io_seproxyhal_touch_address_ok(),
    {
        &C_icon_validate_14,
        "Approve",
    });
UX_STEP_CB(
    ux_public_key_flow_reject_step,
    pb,
    io_seproxyhal_touch_address_cancel(),
    {
        &C_icon_crossmark,
        "Reject",
    });

UX_FLOW(ux_public_key_flow,
    &ux_public_key_flow_address_step,
    &ux_public_key_flow_approve_step,
    &ux_public_key_flow_reject_step
);


//////////////////////////////////////////////////////////////////////

void ui_idle(void) {
    // reserve a display stack slot if none yet.
    // The stack is for stacking UIs like the app UI, lock screen, screen saver, battery level warning, etc.
    if(G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
}

unsigned int io_seproxyhal_touch_address_ok() {
    uint32_t tx = set_result_get_publicKey();
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_address_cancel() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_ok() {
    uint32_t tx = 0;

    // initialize private key
    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, ctx.req.tx.bip32Path, ctx.req.tx.bip32PathLength, privateKeyData, NULL, "ed25519 seed", 12);
    cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    // sign hash
#if CX_APILEVEL >= 8
    tx = cx_eddsa_sign(&privateKey, CX_LAST, CX_SHA512, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, NULL, 0, G_io_apdu_buffer, sizeof(G_io_apdu_buffer), NULL);
#else
    tx = cx_eddsa_sign(&privateKey, NULL, CX_LAST, CX_SHA512, ctx.req.tx.rawTx, ctx.req.tx.rawTxLength, G_io_apdu_buffer);
#endif
    os_memset(&privateKey, 0, sizeof(privateKey));

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

    // Display back the original UX
    ui_idle();

    return 0; // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_cancel() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

// delegate function for generic io_exchange, see ledger-nanos-secure-sdk
unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

uint32_t set_result_get_publicKey() {
    uint32_t tx = 0;

    uint8_t publicKey[32];
    // copy public key little endian to big endian
    uint8_t i;
    for (i = 0; i < 32; i++) {
        publicKey[i] = ctx.req.pk.publicKey.W[64 - i];
    }
    if ((ctx.req.pk.publicKey.W[32] & 1) != 0) {
        publicKey[31] |= 0x80;
    }

    os_memmove(G_io_apdu_buffer + tx, publicKey, 32);

    tx += 32;

    if (ctx.req.pk.returnSignature) {
        os_memmove(G_io_apdu_buffer + tx, ctx.req.pk.signature, 64);
        tx += 64;
    }

    return tx;
}

uint8_t readBip32Path(uint8_t *dataBuffer, uint32_t *bip32Path) {
    uint8_t bip32PathLength = dataBuffer[0];
    dataBuffer += 1;
    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH)) {
        THROW(0x6a80);
    }
    uint8_t i;
    for (i = 0; i < bip32PathLength; i++) {
        bip32Path[i] = (dataBuffer[0] << 24) | (dataBuffer[1] << 16) |
                       (dataBuffer[2] << 8) | (dataBuffer[3]);
        dataBuffer += 4;
    }
    return bip32PathLength;
}

void handleGetPublicKey(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {

    if ((p1 != P1_SIGNATURE) && (p1 != P1_NO_SIGNATURE)) {
        THROW(0x6B00);
    }
    if ((p2 != P2_CONFIRM) && (p2 != P2_NO_CONFIRM)) {
        THROW(0x6B00);
    }
    ctx.req.pk.returnSignature = (p1 == P1_SIGNATURE);

    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t bip32PathLength = readBip32Path(dataBuffer, bip32Path);
    dataBuffer += 1 + bip32PathLength * 4;
    dataLength -= 1 + bip32PathLength * 4;

    uint16_t msgLength;
    uint8_t msg[32];
    if (ctx.req.pk.returnSignature) {
        msgLength = dataLength;
        if (msgLength > 32) {
            THROW(0x6a80);
        }
        os_memmove(msg, dataBuffer, msgLength);
    }

    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, bip32Path, bip32PathLength, privateKeyData, NULL, "ed25519 seed", 12);

    cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, &privateKey);
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    cx_ecfp_generate_pair(CX_CURVE_Ed25519, &ctx.req.pk.publicKey, &privateKey, 1);
    if (ctx.req.pk.returnSignature) {
#if CX_APILEVEL >= 8
        cx_eddsa_sign(&privateKey, CX_LAST, CX_SHA512, msg, msgLength, NULL, 0, ctx.req.pk.signature, sizeof(ctx.req.pk.signature), NULL);
#else
        cx_eddsa_sign(&privateKey, NULL, CX_LAST, CX_SHA512, msg, msgLength, ctx.req.pk.signature);
#endif
    }
    os_memset(&privateKey, 0, sizeof(privateKey));

    if (p2 & P2_CONFIRM) {
        uint8_t publicKey[32];
        // copy public key little endian to big endian
        uint8_t i;
        for (i = 0; i < 32; i++) {
            publicKey[i] = ctx.req.pk.publicKey.W[64 - i];
        }
        if ((ctx.req.pk.publicKey.W[32] & 1) != 0) {
            publicKey[31] |= 0x80;
        }
        print_public_key_as_address(publicKey, ctx.req.pk.address);
        ux_flow_init(0, ux_public_key_flow, NULL);
        *flags |= IO_ASYNCH_REPLY;
    } else {
        *tx = set_result_get_publicKey();
        THROW(0x9000);
    }
}

void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {

    if ((p1 != P1_FIRST) && (p1 != P1_MORE)) {
        THROW(0x6B00);
    }
    if ((p2 != P2_LAST) && (p2 != P2_MORE)) {
        THROW(0x6B00);
    }

    if (p1 == P1_FIRST) {
        // read the bip32 path
        ctx.req.tx.bip32PathLength = readBip32Path(dataBuffer, ctx.req.tx.bip32Path);
        dataBuffer += 1 + ctx.req.tx.bip32PathLength * 4;
        dataLength -= 1 + ctx.req.tx.bip32PathLength * 4;

        // read raw tx data
        ctx.req.tx.rawTxLength = dataLength;
        if (dataLength > MAX_RAW_TX) {
            THROW(0x6700);
        }
        os_memmove(ctx.req.tx.rawTx, dataBuffer, dataLength);
    } else {
        // read more raw tx data
        uint32_t offset = ctx.req.tx.rawTxLength;
        ctx.req.tx.rawTxLength += dataLength;
        if (ctx.req.tx.rawTxLength > MAX_RAW_TX) {
            THROW(0x6700);
        }
        os_memmove(ctx.req.tx.rawTx+offset, dataBuffer, dataLength);
    }

    if (p2 == P2_MORE) {
        THROW(0x9000);
    }

    os_memset(&ctx.req.tx.content, 0, sizeof(ctx.req.tx.content));
    parseTx(ctx.req.tx.rawTx, &ctx.req.tx.content);

    ux_flow_init(0, ux_transaction_flow, NULL);

    *flags |= IO_ASYNCH_REPLY;
}

void handleKeepAlive(volatile unsigned int *flags) {
    *flags |= IO_ASYNCH_REPLY;
}

void handleApdu(volatile unsigned int *flags, volatile unsigned int *tx) {
    unsigned short sw = 0;

    BEGIN_TRY {
        TRY {
            if (os_global_pin_is_validated() != BOLOS_UX_OK) {
                THROW(0x6982);
            }
            if (G_io_apdu_buffer[OFFSET_CLA] != CLA) {
                THROW(0x6e00);
            }

            ctx.u2fTimer = U2F_REQUEST_TIMEOUT;

            switch (G_io_apdu_buffer[OFFSET_INS]) {
            case INS_GET_PUBLIC_KEY:
                handleGetPublicKey(G_io_apdu_buffer[OFFSET_P1],
                                   G_io_apdu_buffer[OFFSET_P2],
                                   G_io_apdu_buffer + OFFSET_CDATA,
                                   G_io_apdu_buffer[OFFSET_LC],
                                   flags, tx);
                break;

            case INS_SIGN_TX:
                handleSignTx(G_io_apdu_buffer[OFFSET_P1],
                             G_io_apdu_buffer[OFFSET_P2],
                             G_io_apdu_buffer + OFFSET_CDATA,
                             G_io_apdu_buffer[OFFSET_LC], flags, tx);
                break;

            case INS_KEEP_ALIVE:
                handleKeepAlive(flags);
                break;

            default:
                THROW(0x6D00);
                break;
            }
        }
        CATCH(EXCEPTION_IO_RESET) {
            THROW(EXCEPTION_IO_RESET);
        }
        CATCH_OTHER(e) {
            switch (e & 0xF000) {
            case 0x6000:
                // Wipe the transaction context and report the exception
                sw = e;
                os_memset(&ctx.req.tx.content, 0, sizeof(ctx.req.tx.content));
                break;
            case 0x9000:
                // All is well
                sw = e;
                break;
            default:
                // Internal error
                sw = 0x6800 | (e & 0x7FF);
                break;
            }
            // Unexpected exception => report
            G_io_apdu_buffer[*tx] = sw >> 8;
            G_io_apdu_buffer[*tx + 1] = sw;
            *tx += 2;
        }
        FINALLY {
        }
    }
    END_TRY;
}

void nimiq_main(void) {
    volatile unsigned int rx = 0; // length of apdu to exchange
    volatile unsigned int tx = 0; // length of our response
    volatile unsigned int flags = 0;

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the apdu is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        volatile unsigned short sw = 0;

        BEGIN_TRY {
            TRY {
                rx = tx;
                tx = 0; // ensure no race in catch_other if io_exchange throws
                        // an error
                // send current apdu response of length rx, read new apdu into G_io_apdu_buffer
                // and set rx to the length of the received apdu.
                rx = io_exchange(CHANNEL_APDU | flags, rx);
                flags = 0;

                // no apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx == 0) {
                    THROW(0x6982);
                }

                PRINTF("New APDU received:\n%.*H\n", rx, G_io_apdu_buffer);

                handleApdu(&flags, &tx);
            }
            CATCH(EXCEPTION_IO_RESET) {
                THROW(EXCEPTION_IO_RESET);
            }
            CATCH_OTHER(e) {
                switch (e & 0xF000) {
                case 0x6000:
                    // Wipe the transaction context and report the exception
                    sw = e;
                    os_memset(&ctx.req.tx.content, 0, sizeof(ctx.req.tx.content));
                    break;
                case 0x9000:
                    // All is well
                    sw = e;
                    break;
                default:
                    // Internal error
                    sw = 0x6800 | (e & 0x7FF);
                    break;
                }
                // Unexpected exception => report
                G_io_apdu_buffer[tx] = sw >> 8;
                G_io_apdu_buffer[tx + 1] = sw;
                tx += 2;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

    // return_to_dashboard:
    return;
}

// resolve io_seproxyhal_display as io_seproxyhal_display_default
void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *)element);
}

void u2fSendKeepAlive() {
    ctx.u2fTimer = 0;
    G_io_apdu_buffer[0] = 0x6e;
    G_io_apdu_buffer[1] = 0x02;
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_STATUS_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
            !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
              SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
            THROW(EXCEPTION_IO_RESET);
        }
    // no break is intentional
    default:
        UX_DEFAULT_EVENT();
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_U2F && ctx.u2fTimer > 0) {
            ctx.u2fTimer -= 100;
            if (ctx.u2fTimer <= 0) {
                u2fSendKeepAlive();
            }
        }
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

void app_exit(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}


__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        os_memset(&ctx.req.tx.content, 0, sizeof(ctx.req.tx.content));

        UX_INIT();
        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

#ifdef TARGET_NANOX
                // grab the current plane mode setting
                G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif // TARGET_NANOX

                if (N_storage.initialized != 0x01) {
                    internalStorage_t storage;
                    storage.fidoTransport = 0x01;
                    storage.initialized = 0x01;
                    nvm_write(&N_storage, (void *)&storage,
                              sizeof(internalStorage_t));
                }

                // deactivate usb before activating
                USB_power(0);
                USB_power(1);

                ui_idle();

#ifdef HAVE_BLE
                BLE_power(0, NULL);
                BLE_power(1, "Nano X");
#endif // HAVE_BLE

                nimiq_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
                // reset IO and UX
                CLOSE_TRY;
                continue;
            }
            CATCH_ALL {
                CLOSE_TRY;
                break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }
    app_exit();

    return 0;
}
