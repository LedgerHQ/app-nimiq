#*******************************************************************************
#   Ledger Nimiq App
#   (c) 2018 Ledger
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#*******************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif
include $(BOLOS_SDK)/Makefile.defines

APPNAME = Nimiq
APP_LOAD_PARAMS=--appFlags 0x240 --path "44'/242'" --curve ed25519 $(COMMON_LOAD_PARAMS)

APPVERSION_M=1
APPVERSION_N=4
APPVERSION_P=6
APPVERSION=$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)

# Application icons following the guidelines:
# https://developers.ledger.com/docs/device-app/deliver/deliverables/icons
# Notes on how to create such icons in GIMP:
# - Open an SVG of the Nimiq logo and set the image width to the size of the safe area for the intended image size, e.g.
#   30px for the 32pxx32px image. Also import the paths.
# - Layer > Transparency > Remove Alpha Channel.
# - Edit > Fill the image with white.
# - Edit > Fill Path with black, or Select > From Path and fill the selection with a radial gradient, potentially with
#   an adequate offset set.
# - Set Image > Mode to indexed and for 1bit-per-pixel images (Nano devices) use a color palette with just black and
#   white, and for 4bit-per-pixel images (e-ink devices) use a color palette with 16 colors 0x00, 0x11, 0x22, ..., 0xff.
#   If desired, enable dithering and experiment with different modes. Mode "positioned" looks quite nice. For more
#   control, also Colors > Dither might be used.
# - Set the Layer > Layer Boundary Size to the intended size of the final image, centering the content. Then Image > Fit
#   Canvas to Layers.
# - Export the image as gif, which should be saved with the desired bits per pixel, if following these steps.
ifeq ($(TARGET_NAME), TARGET_NANOS)
ICONNAME=icons/app_nimiq_16px.gif
endif
ifeq ($(TARGET_NAME), $(filter $(TARGET_NAME), TARGET_NANOX TARGET_NANOS2))
ICONNAME=icons/app_nimiq_14px.gif
endif
ifeq ($(TARGET_NAME), TARGET_STAX)
ICONNAME=icons/app_nimiq_32px.gif
endif
ifeq ($(TARGET_NAME), TARGET_FLEX)
ICONNAME=icons/app_nimiq_40px.gif
endif

################
# Default rule #
################
all: default

############
# Platform #
############

DEFINES   += OS_IO_SEPROXYHAL
DEFINES   += HAVE_SPRINTF HAVE_SNPRINTF_FORMAT_U
DEFINES   += HAVE_IO_USB HAVE_L4_USBLIB IO_USB_MAX_ENDPOINTS=4 IO_HID_EP_LENGTH=64 HAVE_USB_APDU
DEFINES   += LEDGER_MAJOR_VERSION=$(APPVERSION_M) LEDGER_MINOR_VERSION=$(APPVERSION_N) LEDGER_PATCH_VERSION=$(APPVERSION_P)

# U2F
DEFINES   += HAVE_IO_U2F
DEFINES   += U2F_PROXY_MAGIC=\"w0w\"
DEFINES   += U2F_REQUEST_TIMEOUT=28000 # 28 seconds
DEFINES   += USB_SEGMENT_SIZE=64
DEFINES   += BLE_SEGMENT_SIZE=32 #max MTU, min 20
DEFINES   += UNUSED\(x\)=\(void\)x
DEFINES   += APPVERSION=\"$(APPVERSION)\"

#WEBUSB_URL     = www.ledgerwallet.com
#DEFINES       += HAVE_WEBUSB WEBUSB_URL_SIZE_B=$(shell echo -n $(WEBUSB_URL) | wc -c) WEBUSB_URL=$(shell echo -n $(WEBUSB_URL) | sed -e "s/./\\\'\0\\\',/g")
DEFINES   += HAVE_WEBUSB WEBUSB_URL_SIZE_B=0 WEBUSB_URL=""

ifeq ($(TARGET_NAME),TARGET_NANOS)
DEFINES		  += IO_SEPROXYHAL_BUFFER_SIZE_B=128
else
DEFINES		  += IO_SEPROXYHAL_BUFFER_SIZE_B=300
endif

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME), TARGET_NANOX TARGET_STAX TARGET_FLEX))
DEFINES       += HAVE_BLE BLE_COMMAND_TIMEOUT_MS=2000
DEFINES       += HAVE_BLE_APDU # basic ledger apdu transport over BLE
SDK_SOURCE_PATH  += lib_blewbxx lib_blewbxx_impl
endif

ifeq ($(TARGET_NAME),TARGET_NANOS)
DEFINES       += HAVE_BAGL BAGL_WIDTH=128 BAGL_HEIGHT=32
DEFINES       += HAVE_UX_FLOW
endif

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME), TARGET_NANOX TARGET_NANOS2))
DEFINES       += HAVE_BAGL BAGL_WIDTH=128 BAGL_HEIGHT=64
DEFINES       += HAVE_BAGL_ELLIPSIS # long label truncation feature
DEFINES       += HAVE_BAGL_FONT_OPEN_SANS_REGULAR_11PX
DEFINES       += HAVE_BAGL_FONT_OPEN_SANS_EXTRABOLD_11PX
DEFINES       += HAVE_BAGL_FONT_OPEN_SANS_LIGHT_16PX
DEFINES       += HAVE_UX_FLOW
endif

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME), TARGET_STAX TARGET_FLEX))
# For Stax or Flex NBGL must be used
USE_NBGL = 1
DEFINES += HAVE_INAPP_BLE_PAIRING
DEFINES += HAVE_NBGL
DEFINES += HAVE_PIEZO_SOUND
DEFINES += HAVE_SE_TOUCH
DEFINES += HAVE_SE_EINK_DISPLAY
DEFINES += NBGL_PAGE
DEFINES += NBGL_USE_CASE
DEFINES += SCREEN_SIZE_WALLET
DEFINES += NBGL_QRCODE
SDK_SOURCE_PATH += qrcode
ifeq ($(TARGET_NAME),TARGET_STAX)
DEFINES += HAVE_BAGL_FONT_INTER_REGULAR_24PX
DEFINES += HAVE_BAGL_FONT_INTER_SEMIBOLD_24PX
DEFINES += HAVE_BAGL_FONT_INTER_MEDIUM_32PX
endif
ifeq ($(TARGET_NAME),TARGET_FLEX)
DEFINES += HAVE_BAGL_FONT_INTER_REGULAR_28PX
DEFINES += HAVE_BAGL_FONT_INTER_SEMIBOLD_28PX
DEFINES += HAVE_BAGL_FONT_INTER_MEDIUM_36PX
DEFINES += HAVE_FAST_HOLD_TO_APPROVE
endif
endif

# Enabling debug PRINTF
DEBUG = 0
ifneq ($(DEBUG),0)
        ifeq ($(TARGET_NAME),TARGET_NANOS)
                DEFINES   += HAVE_PRINTF PRINTF=screen_printf
        else
                DEFINES   += HAVE_PRINTF PRINTF=mcu_usb_printf
        endif
else
        DEFINES   += PRINTF\(...\)=
endif



##############
# Compiler #
##############
ifneq ($(BOLOS_ENV),)
$(info BOLOS_ENV=$(BOLOS_ENV))
CLANGPATH := $(BOLOS_ENV)/clang-arm-fropi/bin/
GCCPATH := $(BOLOS_ENV)/gcc-arm-none-eabi-5_3-2016q1/bin/
else
$(info BOLOS_ENV is not set: falling back to CLANGPATH and GCCPATH)
endif
ifeq ($(CLANGPATH),)
$(info CLANGPATH is not set: clang will be used from PATH)
endif
ifeq ($(GCCPATH),)
$(info GCCPATH is not set: arm-none-eabi-* will be used from PATH)
endif

CC       := $(CLANGPATH)clang

#CFLAGS   += -O0
CFLAGS   += -O3 -Os

AS     := $(GCCPATH)arm-none-eabi-gcc

LD       := $(GCCPATH)arm-none-eabi-gcc
LDFLAGS  += -O3 -Os
LDLIBS   += -lm -lgcc -lc

# import rules to compile glyphs(/pone)
include $(BOLOS_SDK)/Makefile.glyphs

### computed variables
APP_SOURCE_PATH  += src
SDK_SOURCE_PATH  += lib_stusb
SDK_SOURCE_PATH  += lib_stusb_impl
SDK_SOURCE_PATH  += lib_u2f

load: all
	python -m ledgerblue.loadApp $(APP_LOAD_PARAMS)

delete:
	python -m ledgerblue.deleteApp $(COMMON_DELETE_PARAMS)

# import generic rules from the sdk
include $(BOLOS_SDK)/Makefile.rules

#add dependency on custom makefile filename
dep/%.d: %.c Makefile.genericwallet

listvariants:
	@echo VARIANTS COIN nimiq
