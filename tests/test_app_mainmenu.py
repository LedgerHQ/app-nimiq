from ledgered.devices import Device
from ragger.navigator import NavInsID, NavIns

def test_app_mainmenu(device: Device, navigator, test_name, default_screenshot_path):
    # Navigate in the main menu
    if device.is_nano:
        instructions = [
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,
        ]
    else:
        instructions = [
            NavInsID.USE_CASE_HOME_INFO,
        ]
    navigator.navigate_and_compare(
        default_screenshot_path,
        test_name,
        instructions,
        screen_change_before_first_instruction = False,
    )
