from typing import Generator, Union
from contextlib import contextmanager
from ragger.backend.interface import BackendInterface

"""
A RawApduExchange specifies the input and output APDUs (excluding the status word) of a request, which can be exchanged
with a device backend, and be checked for the expected response apdu.
We use already encoded raw APDUs in our tests, instead of serializing them from input parameters of the requests, and
parsing them to the individual components of the response, because the serializing and parsing is already implemented in
Rust (Nimiq Core-albatross), C (Nimiq Ledger app), and Typescript (Nimiq Ledger API), which makes redoing all that work
again for the Python tests rather unnecessary. Instead we use encoded raw APDUs generated via the Nimiq Ledger API.
In the future, we could consider whether we actually want to serialize and parse the APDUs in the tests, by calling into
the Rust or Typescript implementation (not the C implementation, as that's the one we're testing) from python.
"""
class RawApduExchange:
    input_apdus: list[bytes]
    expected_response: bytes

    # Inputs and output are hex encoded raw apdus. The output does not contain the returned status word, e.g. 0x9000,
    # which is in response.status instead.
    def __init__(self, input_apdus: Union[str, list[str]], expected_response: str):
        self.input_apdus = list(map(bytes.fromhex, input_apdus if type(input_apdus) is list else [input_apdus]))
        if len(self.input_apdus) == 0:
            raise ValueError("Input is empty")
        self.expected_response = bytes.fromhex(expected_response)

    def exchange(self, backend: BackendInterface) -> None:
        for apdu in self.input_apdus[:-1]:
            response = backend.exchange_raw(apdu)
            assert response.status == 0x9000
            assert response.data == b""
        # Exchange last APDU and check the final result
        response = backend.exchange_raw(self.input_apdus[-1])
        assert response.status == 0x9000
        assert response.data == self.expected_response

    @contextmanager
    def exchange_async(self, backend: BackendInterface) -> Generator[None, None, None]:
        for apdu in self.input_apdus[:-1]:
            response = backend.exchange_raw(apdu)
            assert response.status == 0x9000
            assert response.data == b""
        # Asynchronously exchange last APDU. The result can be checked with check_async_response
        with backend.exchange_async_raw(self.input_apdus[-1]) as response:
            yield response

    def check_async_response(self, backend: BackendInterface) -> None:
        response = backend.last_async_response
        assert response is not None
        assert response.status == 0x9000
        assert response.data == self.expected_response
