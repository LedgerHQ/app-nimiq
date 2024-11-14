from utils import pop_sized_buf_from_buffer, pop_size_prefixed_buf_from_buf

def test_get_app_and_version(backend):
    # Note that this is a request handled by BOLOS, see io_process_default_apdus in ledger-secure-sdk.
    response = backend.exchange(
        cla=0xB0, # specific CLA for BOLOS
        ins=0x01, # specific INS for get_app_and_version
        p1=0x00,
        p2=0x00,
        data=b"",
    ).data
    # response = format_id (1)
    #            app_name_raw_len (1)
    #            app_name_raw (var)
    #            version_raw_len (1)
    #            version_raw (var)
    #            unused_len (1)
    #            unused (var)
    response, _ = pop_sized_buf_from_buffer(response, 1)
    response, _, app_name_raw = pop_size_prefixed_buf_from_buf(response)
    response, _, version_raw = pop_size_prefixed_buf_from_buf(response)
    response, _, _ = pop_size_prefixed_buf_from_buf(response)

    assert len(response) == 0

    app_name, version = app_name_raw.decode("ascii"), version_raw.decode("ascii")

    assert app_name == "Nimiq"
    assert version == "2.0.1"
