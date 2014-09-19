-module(erlbz2).
-on_load(init/0).

-export([compress/1, decompress/1, compress/4, decompress/2]).

compress(Binary) ->
    compress(Binary, 9, 30, 4096).

decompress(Binary) ->
    decompress(Binary, 4096).

compress(_Binary, _BlockSize, _WorkFactor, _BufferSize) ->
     exit({not_loaded, [{module, ?MODULE}, {line, ?LINE}]}).

decompress(_Binary, _BufferSize) ->
     exit({not_loaded, [{module, ?MODULE}, {line, ?LINE}]}).

init() ->
    SoName = filename:join([code:priv_dir(?MODULE), ?MODULE]),
    erlang:load_nif(SoName, 0).
