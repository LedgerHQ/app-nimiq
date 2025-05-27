import pytest
from ledgered.devices import Device
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavInsID

from raw_apdu_exchange import RawApduExchange
from errors import Errors

APDUS = {
    # Message (ascii): 'Hello world.'
    "ascii": RawApduExchange(
        "e00a000022048000002c800000f28000000080000000000000000c48656c6c6f20776f726c642e",
        "6f59528bdad4c07c54c49c350b219978c8fd27d2b93e6c5c28dee3f1843679a3f71638c55935d239647bcdd794fce8e65e66f8464e7ff5"
            "63ce8c5ee84509c00b",
    ),
    # Message (ascii): 'Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt
    # ut labore et dolore magna aliquyam erat, sed diam voluptua. End.'
    "ascii_long": RawApduExchange(
        "e00a0000b6048000002c800000f2800000008000000000000000a04c6f72656d20697073756d20646f6c6f722073697420616d65742c20"
            "636f6e736574657475722073616469707363696e6720656c6974722c20736564206469616d206e6f6e756d79206569726d6f642074"
            "656d706f7220696e766964756e74207574206c61626f726520657420646f6c6f7265206d61676e6120616c69717579616d20657261"
            "742c20736564206469616d20766f6c75707475612e20456e642e",
        "e8eccbdcf7fa33d031269ec16ba53ce9d28eee91773a864dceca077980bd15e7f9543543cce06bf33525db01a49eddaca4227d17890434"
            "97a444f8399a774f0b",
    ),
    # Message (ascii): 'Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt
    # ut labore et dolore magna aliquyam erat, sed diam voluptua. End...'
    "ascii_overlong": RawApduExchange(
        "e00a0000b8048000002c800000f2800000008000000000000000a24c6f72656d20697073756d20646f6c6f722073697420616d65742c20"
            "636f6e736574657475722073616469707363696e6720656c6974722c20736564206469616d206e6f6e756d79206569726d6f642074"
            "656d706f7220696e766964756e74207574206c61626f726520657420646f6c6f7265206d61676e6120616c69717579616d20657261"
            "742c20736564206469616d20766f6c75707475612e20456e642e2e2e",
        "f01eb0776c988edc723a563acc1351789de922866d769f780c68cd4f88e739281ee0d74dc97f268ee065ccea45870104884d44156bf001"
            "63796dd9d153f21a03",
    ),
    # Message (hex): 'cafecafe'
    "hex": RawApduExchange(
        "e00a00001a048000002c800000f280000000800000000000000004cafecafe",
        "e19c8220c0565482aab9fdab7744b7d6ac632592422c7826e31e3682be01434e32d170a34e533ba8bfa64ff0d94875c9d887eb14bc3098"
            "d7521aae156ed5c90f",
    ),
}

def test_sign_message_approve(device: Device, backend, navigator, default_screenshot_path, test_name):
    for name, apdus in APDUS.items():
        screenshot_folder = test_name + f"_{name}"
        with apdus.exchange_async(backend):
            if device.is_nano:
                # Manually skip the first page which also contains the word "Sign"
                navigator.navigate([NavInsID.RIGHT_CLICK])
                navigator.navigate_until_text_and_compare(
                    NavInsID.RIGHT_CLICK,
                    [NavInsID.BOTH_CLICK],
                    "Sign",
                    default_screenshot_path,
                    screenshot_folder,
                    screen_change_before_first_instruction = False,
                )
            else:
                navigator.navigate_until_text_and_compare(
                    NavInsID.USE_CASE_REVIEW_TAP,
                    [NavInsID.USE_CASE_REVIEW_CONFIRM, NavInsID.USE_CASE_STATUS_DISMISS],
                    "Hold to sign",
                    default_screenshot_path,
                    screenshot_folder,
                )
        apdus.check_async_response(backend)

def test_sign_message_reject(device: Device, backend, navigator, default_screenshot_path, test_name):
    # As the reject flow is the same for all different message types, and is handled mostly by the SDK, we test it only
    # for one of the messages, the basic ascii message.
    apdus = APDUS["ascii"]
    if device.is_nano:
        with pytest.raises(ExceptionRAPDU) as e, apdus.exchange_async(backend):
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
        # Test the reject button on the three pages of the ascii message flow.
        for i in range(3):
            instructions = [NavInsID.USE_CASE_REVIEW_TAP] * i
            instructions += [
                NavInsID.USE_CASE_REVIEW_REJECT,
                NavInsID.USE_CASE_CHOICE_CONFIRM,
                NavInsID.USE_CASE_STATUS_DISMISS,
            ]
            with pytest.raises(ExceptionRAPDU) as e, apdus.exchange_async(backend):
                navigator.navigate_and_compare(
                    default_screenshot_path,
                    test_name + f"/on_page_{i}",
                    instructions,
                )
            assert e.value.status == Errors.SW_DENY
            assert len(e.value.data) == 0
