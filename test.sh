#!/bin/bash
mkdir -p obj
gcc test/parsertest.c src/nimiq_utils.c src/base32.c src/blake2b.c test/test_utils.c -o obj/parsertest -I src/ -I test/ -D TEST
./obj/parsertest test/basicTx.hex
gcc test/utilstest.c src/nimiq_utils.c src/base32.c src/blake2b.c test/test_utils.c -o obj/utilstest -I src/ -I test/ -D TEST
./obj/utilstest
