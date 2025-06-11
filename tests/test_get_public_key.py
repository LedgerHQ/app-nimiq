import pytest
from ledgered.devices import Device, DeviceType
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavInsID, NavIns

from raw_apdu_exchange import RawApduExchange
from errors import Errors

APDUS = {
    "no_confirm": RawApduExchange(
        "e002000011048000002c800000f28000000080000000", # confirm flag unset
        "6b20f8ca4ed445c2d666e80361cbc1b98d0d55ca44ad8c560755d528f8d3d71c",
    ),
    "confirm": RawApduExchange(
        "e002000111048000002c800000f28000000080000000", # confirm flag set
        "6b20f8ca4ed445c2d666e80361cbc1b98d0d55ca44ad8c560755d528f8d3d71c"
    ),
}

def test_get_public_key_no_confirm(backend):
    APDUS["no_confirm"].exchange(backend)

def test_get_public_key_confirm_approve(device: Device, backend, navigator, default_screenshot_path, test_name):
    with APDUS["confirm"].exchange_async(backend):
        if device.is_nano:
            navigator.navigate_until_text_and_compare(
                NavInsID.RIGHT_CLICK,
                [NavInsID.BOTH_CLICK],
                "Approve",
                default_screenshot_path,
                test_name,
            )
        else:
            instructions = [
                NavInsID.USE_CASE_REVIEW_TAP,
                NavIns(NavInsID.TOUCH, (64, 520) if device.type == DeviceType.STAX else (80, 435)), # QR button
                NavInsID.USE_CASE_ADDRESS_CONFIRMATION_EXIT_QR,
                NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CONFIRM,
                NavInsID.USE_CASE_STATUS_DISMISS
            ]
            navigator.navigate_and_compare(
                default_screenshot_path,
                test_name,
                instructions,
            )
    APDUS["confirm"].check_async_response(backend)

def test_get_public_key_confirm_reject(device: Device, backend, navigator, default_screenshot_path, test_name):
    if device.is_nano:
        with pytest.raises(ExceptionRAPDU) as e, APDUS["confirm"].exchange_async(backend):
            navigator.navigate_until_text_and_compare(
                NavInsID.RIGHT_CLICK,
                [NavInsID.BOTH_CLICK],
                "Reject",
                default_screenshot_path,
                test_name,
            )
        assert e.value.status == Errors.SW_DENY
        assert len(e.value.data) == 0
    else:
        instructions_set = [
            # Test the reject button on both pages
            [
                NavInsID.USE_CASE_REVIEW_REJECT,
                NavInsID.USE_CASE_STATUS_DISMISS
            ],
            [
                NavInsID.USE_CASE_REVIEW_TAP,
                NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CANCEL,
                NavInsID.USE_CASE_STATUS_DISMISS
            ]
        ]
        for i, instructions in enumerate(instructions_set):
            with pytest.raises(ExceptionRAPDU) as e, APDUS["confirm"].exchange_async(backend):
                navigator.navigate_and_compare(
                    default_screenshot_path,
                    test_name + f"/on_page_{i}",
                    instructions,
                )
            assert e.value.status == Errors.SW_DENY
            assert len(e.value.data) == 0
