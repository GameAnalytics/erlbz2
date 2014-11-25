#!/bin/bash -e
export CFLAGS="$CFLAGS -g"
./rebar clean compile
export VALGRIND_MISC_FLAGS="--suppressions=$ERL_TOP/erts/emulator/valgrind/suppress.standard --show-possibly-lost=no --dsymutil=yes --leak-check=full"
$ERL_TOP/bin/cerl -valgrind
