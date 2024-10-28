# Nimiq app for Ledger devices

## Introduction

This is a device app for the [Ledger Hardware Wallets](https://www.ledger.com/) which makes it possible to store your
[Nimiq token (NIM)](https://nimiq.com/) on those devices (more precisely use the associated private key for NIM related
operations).

A companion [Javascript library](https://github.com/nimiq/ledger-api) is available to communicate with this app.

## Development Setup

For build and installation instructions, please refer to:
- The README of the [Ledger Boilerplate App](https://github.com/LedgerHQ/app-boilerplate)
- [Usage instructions of Ledger's VSCode extension](https://marketplace.visualstudio.com/items?itemName=LedgerHQ.ledger-dev-tools)
  and the associated [quickstart guide](https://developers.ledger.com/docs/device-app/beginner/vscode-extension),
  or the underlying [ledger-app-builder docker images](https://github.com/LedgerHQ/ledger-app-builder/), if you prefer
  to use them directly, without the VSCode extension. Here at Nimiq, mainly the docker images have been used directly
  for building and testing the application via the [Speculos](https://github.com/LedgerHQ/speculos) emulator, on real
  devices and via [Ragger](https://github.com/LedgerHQ/ragger) functional tests.

## Testing

The project contains functional tests powered by [Ragger](https://github.com/LedgerHQ/ragger). They can be launched via
the VSCode extension or docker images as described in the section [Development Setup](#development-setup).

Additionally, the project contains [unit-tests](https://github.com/nimiq/ledger-app-nimiq/tree/master/unit-tests).
