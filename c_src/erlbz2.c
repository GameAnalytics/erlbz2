#include "erl_nif.h"
#include "bzlib.h"

#include <string.h>

#ifdef ERL_NIF_DIRTY_SCHEDULER_SUPPORT
#else
#warning "Dirty scheduler not supported by your Erlang distribution!"
#endif

/* See folly/FBVector.h for a motivation for this number */
#define GROW_FACTOR 1.5

#define MAX(a,b) (((a)>(b))?(a):(b))

/* Process a stream with the provided processor function, store results in
 * buffer. The buffer will be reallocated if it runs out of space.
 * The buffer is never freed, it is the callers responsibility to correctly
 * free the buffer regardless of the return value */
static int
process_stream(bz_stream* stream,
                int (*bz_processor)(bz_stream*),
                char** buffer,
                size_t buffer_size)
{
        int result;
        size_t new_size = 0;
        char* new_buffer = NULL;

        stream->next_out = *buffer;
        stream->avail_out = buffer_size;

        while (1)
        {
                result = bz_processor(stream);
                if (result == BZ_STREAM_END)
                        break;
                if (result != BZ_OK)
                        return result;

                if (stream->avail_out == 0)
                {
                        new_size = MAX((size_t)(buffer_size * GROW_FACTOR),
                                        buffer_size + 1);
                        new_buffer = realloc(*buffer, new_size);
                        if (!new_buffer)
                        {
                                return BZ_MEM_ERROR;
                        }
                        else
                        {
                                *buffer = new_buffer;
                        }
                        stream->next_out = (*buffer) + buffer_size;
                        stream->avail_out = new_size - buffer_size;
                        buffer_size = new_size;
                }
        }
        return BZ_OK;
}

/* Helper having the same api as BZ2_bzDecompress */
static int
bz_compress(bz_stream* stream)
{
        int result = BZ2_bzCompress(stream, BZ_FINISH);
        if (result == BZ_FINISH_OK)
                return BZ_OK;
        return result;
}

static size_t
bz_output_size(bz_stream* stream)
{
        return ((long)stream->total_out_hi32 << 32) + stream->total_out_lo32;
}

static ERL_NIF_TERM
compress(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
        ErlNifBinary in, out;
        bz_stream bzs;
        int block_size, work_factor, buffer_size;
        char* buffer = NULL;

        if (!enif_inspect_binary(env, argv[0], &in))
                return enif_make_badarg(env);

        if (!enif_get_int(env, argv[1], &block_size))
                return enif_make_badarg(env);
        if (block_size < 0 || block_size > 9)
                return enif_make_badarg(env);

        if (!enif_get_int(env, argv[2], &work_factor))
                return enif_make_badarg(env);
        if (!enif_get_int(env, argv[2], &work_factor))
                return enif_make_badarg(env);
        if (work_factor < 0 || work_factor > 250)
                return enif_make_badarg(env);

        if (!enif_get_int(env, argv[3], &buffer_size))
                return enif_make_badarg(env);
        if (buffer_size <= 0)
                return enif_make_badarg(env);

        memset(&bzs, 0, sizeof(bz_stream));
        bzs.next_in = (char*)in.data;
        bzs.avail_in = in.size;
        if (BZ2_bzCompressInit(&bzs, block_size, 0, work_factor) != BZ_OK)
                return enif_make_badarg(env);

        if (!(buffer = calloc(buffer_size, 1)))
        {
                BZ2_bzCompressEnd(&bzs);
                return BZ_MEM_ERROR;
        }

        if (process_stream(&bzs, bz_compress, &buffer, buffer_size) != BZ_OK)
        {
                free(buffer),
                BZ2_bzCompressEnd(&bzs);
                return enif_make_badarg(env);
        }

        if (!enif_alloc_binary(bz_output_size(&bzs), &out))
        {
                free(buffer),
                BZ2_bzCompressEnd(&bzs);
                return enif_make_badarg(env);
        }
        memcpy(out.data, buffer, out.size);

        free(buffer);
        if (BZ2_bzCompressEnd(&bzs) != BZ_OK)
                return enif_make_badarg(env);

        return enif_make_binary(env, &out);
}

static ERL_NIF_TERM
decompress(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
        ErlNifBinary in, out;
        bz_stream bzs;
        int buffer_size;
        char* buffer = NULL;

        if (!enif_inspect_binary(env, argv[0], &in))
                return enif_make_badarg(env);

        if (!enif_get_int(env, argv[1], &buffer_size))
                return enif_make_badarg(env);
        if (buffer_size <= 0)
                return enif_make_badarg(env);

        memset(&bzs, 0, sizeof(bz_stream));
        bzs.next_in = (char*)in.data;
        bzs.avail_in = in.size;
        if (BZ2_bzDecompressInit(&bzs, 0, 0) != BZ_OK)
                return enif_make_badarg(env);

        if (!(buffer = calloc(buffer_size, 1)))
        {
                BZ2_bzCompressEnd(&bzs);
                return BZ_MEM_ERROR;
        }

        if (process_stream(&bzs, BZ2_bzDecompress, &buffer, buffer_size) != BZ_OK)
        {
                free(buffer);
                BZ2_bzDecompressEnd(&bzs);
                return enif_make_badarg(env);
        }

        if (!enif_alloc_binary(bz_output_size(&bzs), &out))
        {
                free(buffer);
                BZ2_bzDecompressEnd(&bzs);
                return enif_make_badarg(env);
        }
        memcpy(out.data, buffer, out.size);

        free(buffer);
        if (BZ2_bzDecompressEnd(&bzs) != BZ_OK)
                return enif_make_badarg(env);

        return enif_make_binary(env, &out);
}

static ERL_NIF_TERM
compress_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
#ifdef ERL_NIF_DIRTY_SCHEDULER_SUPPORT
        return enif_schedule_nif(env, "compress_nif",
                        ERL_NIF_DIRTY_JOB_CPU_BOUND,
                        compress, argc, argv);
#else
        return compress(env, argc, argv);
#endif
}

static ERL_NIF_TERM
decompress_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
#ifdef ERL_NIF_DIRTY_SCHEDULER_SUPPORT
        return enif_schedule_nif(env, "decompress_nif",
                        ERL_NIF_DIRTY_JOB_CPU_BOUND,
                        decompress, argc, argv);
#else
        return decompress(env, argc, argv);
#endif
}

static ErlNifFunc nif_funcs[] = {
    {"compress", 4, compress_nif},
    {"decompress", 2, decompress_nif}
};

static int
upgrade(ErlNifEnv* env, void** priv_data, void** old_priv_data, ERL_NIF_TERM load_info)
{
        /* Support reloading the NIF, we have no global state so that's ok */
        return 0;
}

ERL_NIF_INIT(erlbz2, nif_funcs, NULL, NULL, upgrade, NULL);
