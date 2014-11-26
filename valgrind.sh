#!/bin/bash -e
# See this doc on how to build the valgrind flavor beam:
# http://www.erlang.org/doc/installation_guide/INSTALL.html
export CFLAGS="$CFLAGS -g"
./rebar clean compile
rm -rf priv/erlbz2.so.dSYM
export VALGRIND_MISC_FLAGS="--suppressions=$ERL_TOP/erts/emulator/valgrind/suppress.standard --show-possibly-lost=yes --dsymutil=yes --leak-check=full --show-leak-kinds=all --trace-children=yes -v"
$ERL_TOP/bin/cerl -valgrind -pa ../erlbz2/ebin -smp enable
