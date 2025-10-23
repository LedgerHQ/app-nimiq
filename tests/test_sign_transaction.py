import pytest
from ledgered.devices import Device
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavInsID

from .raw_apdu_exchange import RawApduExchange
from .errors import Errors

APDUS = {
    # Basic transactions

    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    "basic": RawApduExchange(
        "e004000055048000002c800000f28000000080000000010000e677d153553b84db141148ec9d7e77bb55983a2900000000000000000000"
            "00000000000000000000000000000000009896800000000000000000000004d2050000",
        "e5c55becb7c0873a23ad79c2000038475b14d95a9e49619de6b91e158e2593658758acd5f30693c36c8f9a5edd79668aaf07d01256ab31"
            "9ab2c4fa65e6da9d09",
    ),
    # Version: 'legacy',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    "basic_legacy": RawApduExchange(
        "e004000054048000002c800000f28000000080000000000000e677d153553b84db141148ec9d7e77bb55983a2900000000000000000000"
            "00000000000000000000000000000000009896800000000000000000000004d20100",
        "9687e14d6149acbcf4e400d170b43e4537088c3f32f6f0affe2e861a0ca9924026d175f67c7b3e11e63a0c116bff4e68f99560d41601fe"
            "412475f0488cb8180d",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0.12345',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    "basic_fee": RawApduExchange(
        "e004000055048000002c800000f28000000080000000010000e677d153553b84db141148ec9d7e77bb55983a2900000000000000000000"
            "00000000000000000000000000000000009896800000000000003039000004d2050000",
        "528b7e3f04e07811ba9c9a6fa282217fdd6993c849b581ddbb565b47af29f17ce2ef23bf9a525d7b679bdf5f1a0330fea8d883c48322c7"
            "a3a7db7012597a9a0d",
    ),
    # Version: 'legacy',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0.12345',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    "basic_fee_legacy": RawApduExchange(
        "e004000054048000002c800000f28000000080000000000000e677d153553b84db141148ec9d7e77bb55983a2900000000000000000000"
            "00000000000000000000000000000000009896800000000000003039000004d20100",
        "a3551d26679bf81fb2105411e1482b2a0ed0c578751fce3d69098c60d17d4ec8e972d3fb1f65b314a801b04ee892a7881d7c2e532134ce"
            "9107d5e1166f8d9605",
    ),

    # Transactions with simple data

    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Text):  Sender Data Text: '', Recipient Data Text: 'Hello world.'
    "data_ascii": RawApduExchange(
        "e004000061048000002c800000f2800000008000000001000c48656c6c6f20776f726c642ee677d153553b84db141148ec9d7e77bb5598"
            "3a290000000000000000000000000000000000000000000000000000009896800000000000000000000004d2050000",
        "f173897b7984d1fff0122c3daba02acea9bc9f331dd131b404cea3c498c6d7f8bc89b8852247874e4e3065d3566b2d70e45ea918c7bd05"
            "8a645e52e4db8bc70f",
    ),
    # Version: 'legacy',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Text):  Sender Data Text: '', Recipient Data Text: 'Hello world.'
    "data_ascii_legacy": RawApduExchange(
        "e004000060048000002c800000f2800000008000000000000c48656c6c6f20776f726c642ee677d153553b84db141148ec9d7e77bb5598"
            "3a290000000000000000000000000000000000000000000000000000009896800000000000000000000004d20100",
        "fc61daa6869fdd3dd99e8c02f4010082409993f424efed841ceb60644353abb78736f3a76b7a3f1a6864dfb2a5fc6f7627e7f2e0ca853c"
            "f99baf3ec119969c0e",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Hex):  Sender Data Hex: '', Recipient Data Hex: 'cafecafe'
    "data_binary": RawApduExchange(
        "e004000059048000002c800000f28000000080000000010004cafecafee677d153553b84db141148ec9d7e77bb55983a29000000000000"
            "0000000000000000000000000000000000000000009896800000000000000000000004d2050000",
        "6c30daa99f36d0b17bc77314bac352ea0b6ed4fd06983729686c5058841a7b719542593a37a3b7d5b82e7ddcb71852cb2eec40a76d8ec2"
            "3896d64fba3012db0f",
    ),
    # Version: 'legacy',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Hex):  Sender Data Hex: '', Recipient Data Hex: 'cafecafe'
    "data_binary_legacy": RawApduExchange(
        "e004000058048000002c800000f28000000080000000000004cafecafee677d153553b84db141148ec9d7e77bb55983a29000000000000"
            "0000000000000000000000000000000000000000009896800000000000000000000004d20100",
        "355e819e876b93e8b2b25fee1c88ee69bb971d4801ce582bd19220934df7a5de2dabb8363fd9ea0c00e280ad01a8bfbb12006f374e3c02"
            "2d78484c2739e7e10a",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Hex):  Sender Data Hex: '', Recipient Data Hex: '0082809287' (Cashlink magic number)
    "data_cashlink": RawApduExchange(
        "e00400005a048000002c800000f280000000800000000100050082809287e677d153553b84db141148ec9d7e77bb55983a290000000000"
            "000000000000000000000000000000000000000000009896800000000000000000000004d2050000",
        "24b0ad96ec7abf6485c4d91b3deda03a98b2c8a178e5e0dccc19c096ecc15a99a0d15df55095ebd1436bbf1292f0c19633b1be2fa23129"
            "2dc514dccf1000d40f",
    ),
    # Version: 'legacy',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Hex):  Sender Data Hex: '', Recipient Data Hex: '0082809287' (Cashlink magic number)
    "data_cashlink_legacy": RawApduExchange(
        "e004000059048000002c800000f280000000800000000000050082809287e677d153553b84db141148ec9d7e77bb55983a290000000000"
            "000000000000000000000000000000000000000000009896800000000000000000000004d20100",
        "52327394e8e41d033ebd6a278a1cdb579d9672cfd1c316b4f19a907fd56f7fe83abb68b5d06ddec61f3a5a254d62ff992d7888bb01a153"
            "d37e5a174d814ca607",
    ),

    # Staking Transactions
    # Note that these do not exist for legacy transactions.

    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Recipient Type: '3',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Create Staker):  Delegation: '', Signature Proof: ''
    "staking_create_staker": RawApduExchange(
        "e0040000b9048000002c800000f28000000080000000010064050000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000e677d153553b84db141148ec9d7e77bb55983a2900000000000000000000000000000000"
            "00000000010300000000009896800000000000000000000004d2050000",
        "27dccfd7c0fe6c820944ec5cd23969ef1b6d0294c1b0c06531f949178b215177e027be855562665ff4d4cac381176770423b65728092c4"
            "c509ab24f7c214f70372ca20775e73e6cb8cc32218347949264630f198ddc6ef31d574c8cef68d43ed9743d00005297a52758190cc"
            "fcdce22cb7d2a6d4dc34b152e685fdb50ba1f505",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Recipient Type: '3',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Create Staker):  Delegation: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000', Signature Proof: ''
    "staking_create_staker_delegation": RawApduExchange(
        "e0040000cd048000002c800000f28000000080000000010078050100000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "00000000000000000000000000000000000000000000000000000000000000000000000000e677d153553b84db141148ec9d7e77bb"
            "55983a290000000000000000000000000000000000000000010300000000009896800000000000000000000004d2050000",
        "27240bd71d0a553990eca0cb5b919f183d86371b8d717d2189a5b88ab7203bc24b916968adc936912012d0d231c29a3002b212936c6f76"
            "bad2114d0b91c1d80000cb4fc956631d2e169dc8f1dd90431f1d09ea708964fcf027cde04e6db09656a6bb70c8704f68ddfd43f459"
            "718593eb64de4011853c04aa1bd89fc57e4d150d",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Recipient Type: '3',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Add Stake):  Staker: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9'
    "staking_add_stake_sender_staker": RawApduExchange(
        "e00400006a048000002c800000f2800000008000000001001506e677d153553b84db141148ec9d7e77bb55983a29e677d153553b84db14"
            "1148ec9d7e77bb55983a290000000000000000000000000000000000000000010300000000009896800000000000000000000004d2"
            "050000",
        "2a38ab898921853eea9855d40e3af98c2839093acf52d91172a5a67d4cb351ff74c3c877ac1cb885295580031b90e56bce942fb5e1ed84"
            "2003db45863a08890f",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Recipient Type: '3',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Add Stake):  Staker: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000'
    "staking_add_stake_different_staker": RawApduExchange(
        "e00400006a048000002c800000f28000000080000000010015060000000000000000000000000000000000000000e677d153553b84db14"
            "1148ec9d7e77bb55983a290000000000000000000000000000000000000000010300000000009896800000000000000000000004d2"
            "050000",
        "5b209d11c1bb84a615ece37731076d46ea6d5175e02d076cef1dd691d12cb77cee45156463e7e3fbf22ed379450b002e9767a58f0fe12a"
            "7a6794ad27f2467702",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Recipient Type: '3',
    # Amount: '0', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '2'
    # Transaction Data (Update Staker):  New Delegation: '', Reactivate All Stake: 'true', Signature Proof: ''
    "staking_update_staker_no_new_delegation_and_with_reactivate": RawApduExchange(
        "e0040000ba048000002c800000f28000000080000000010065070001000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "000000000000000000000000000000000000e677d153553b84db141148ec9d7e77bb55983a29000000000000000000000000000000"
            "0000000000010300000000000000000000000000000000000004d2050200",
        "17f06c9fc8e08b18d35877dc50588c34e4b50dcc70f5a6d4623f91b8a2f262d17b760e748b799c2cbaa9d044630a3d0e95ba99c0c9641a"
            "eaab91e7674239d302b49e288ef94ec8b7c17280750ae4d1914aa33efd4ad1addadf6e2c9e64927e3d5c7e785bf23ec7bd352d9d31"
            "6d278a91d8c87cf1871489d7f90485a65969880b",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Recipient Type: '3',
    # Amount: '0', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '2'
    # Transaction Data (Update Staker):  New Delegation: 'NQ07 0000 0000 0000 0000 0000 0000 0000 0000',
    # Reactivate All Stake: 'false', Signature Proof: ''
    "staking_update_staker_with_new_delegation_and_no_reactivate": RawApduExchange(
        "e0040000ce048000002c800000f28000000080000000010079070100000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000000000000000e677d153553b84db141148ec9d7e77"
            "bb55983a290000000000000000000000000000000000000000010300000000000000000000000000000000000004d2050200",
        "0088883312ee7ad7cc7d726f497b43083c91046ed80d3f560ec311f2449f765415151e9166f6417727244a9a559643ce689e57bc2a4579"
            "bd384eae0128109e018ce3a252b72e4862b54c3935b7b29f38a0b4cc720ffeddfca80295e04bf56ee4b301da4368dedca74df2d665"
            "c1f9ed044226874ef47a85d156dc98ceff5d7200",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Recipient Type: '3',
    # Amount: '0', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '2'
    # Transaction Data (Set Active State):  Amount: '100', Signature Proof: ''
    "staking_set_active_stake": RawApduExchange(
        "e0040000c0048000002c800000f2800000008000000001006b080000000000989680000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "000000000000000000000000000000000000000000000000e677d153553b84db141148ec9d7e77bb55983a29000000000000000000"
            "0000000000000000000000010300000000000000000000000000000000000004d2050200",
        "ccf6531465a4ac4cdcf0df7b25312741b82d8acb7a612e49d16ae4fa80f842ec958fa6793c7c3cf725ca8b58af612eebcc7ee2067e3316"
            "942e9905de7d41ce07d1641885a21321cf06692374aa4525e82d6f172388a2125ecdff2f2c1df30c2f1299fb0ce82e9bea7d81b2e3"
            "5f27b172dbf73136a463736ab99a495b9bc8270d",
    ),
    # Version: 'albatross',
    # Sender: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Sender Type: '0',
    # Recipient: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Recipient Type: '3',
    # Amount: '0', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '2'
    # Transaction Data (Retire State):  Amount: '100', Signature Proof: ''
    "staking_retire_stake": RawApduExchange(
        "e0040000c0048000002c800000f2800000008000000001006b090000000000989680000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "000000000000000000000000000000000000000000000000e677d153553b84db141148ec9d7e77bb55983a29000000000000000000"
            "0000000000000000000000010300000000000000000000000000000000000004d2050200",
        "bac243fb66b0769b119596885673d97551532ede3a905601e7225e306a675c31bd06e5c9af8d1f1d4c203853de9e00e57aa3ccb8b65344"
            "1cc24d7bf33622f007be9d4a8fc608bf83cee0bec70110cb7c15c92124c2554c49bafd9fa319ac3d7db4ee1fca6aaed0b12b39ba77"
            "538160361407d27f51f06383ae8b5ebc77a4b303",
    ),
    # Version: 'albatross',
    # Sender: 'NQ77 0000 0000 0000 0000 0000 0000 0000 0001', Sender Type: '3',
    # Recipient: 'NQ13 URTV 2LSM 7E2D N50H 93N9 SYKP PDAR GEH9', Recipient Type: '0',
    # Amount: '100', Fee: '0',
    # Validity Start Height: '1234', Network: 'test', Flags: '0'
    # Transaction Data (Remove Stake):  No inputs
    "staking_remove_stake": RawApduExchange(
        "e004000056048000002c800000f28000000080000000010000000000000000000000000000000000000000000103e677d153553b84db14"
            "1148ec9d7e77bb55983a290000000000009896800000000000000000000004d205000101",
        "8a2b1cb0557d274366d05735b627d9a273a7d742d0607f6da31d01e53353c5474759aad5d65bbb925a775903ef15eea058c7f2d8cbcc52"
            "c34df2962ff9b8440b",
    ),
}

def test_sign_transaction_approve(device: Device, backend, navigator, default_screenshot_path, test_name):
    for name, apdus in APDUS.items():
        name = name.removesuffix("_legacy") # UI / screenshots are the same for legacy transactions
        screenshot_folder = test_name + f"_{name}"
        with apdus.exchange_async(backend):
            if device.is_nano:
                navigator.navigate_until_text_and_compare(
                    NavInsID.RIGHT_CLICK,
                    [NavInsID.BOTH_CLICK],
                    "Accept",
                    default_screenshot_path,
                    screenshot_folder,
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

def test_sign_transaction_reject(device: Device, backend, navigator, default_screenshot_path, test_name):
    # As the reject flow is the same for all different transaction types, and is handled mostly by the SDK, we test it
    # only for one of the transactions, the basic transaction.
    apdus = APDUS["basic"]
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
        # Test the reject button on the three pages of the basic tx flow.
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
