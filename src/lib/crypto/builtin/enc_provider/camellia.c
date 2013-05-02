/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/enc_provider/camellia.c - Camellia enc provider */
/*
 * Copyright (C) 2009, 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "crypto_int.h"
#include "camellia.h"

/*
 * Private per-key data to cache after first generation.  We don't want to mess
 * with the imported Cemallia implementation too much, so we'll just use two
 * copies of its context, one for encryption and one for decryption, and use
 * the #rounds field as a flag for whether we've initialized each half.
 */
struct camellia_key_info_cache {
    camellia_ctx enc_ctx, dec_ctx;
};
#define CACHE(X) ((struct camellia_key_info_cache *)((X)->cache))

static inline void
enc(unsigned char *out, const unsigned char *in, camellia_ctx *ctx)
{
    if (camellia_enc_blk(in, out, ctx) != camellia_good)
        abort();
}

static inline void
dec(unsigned char *out, const unsigned char *in, camellia_ctx *ctx)
{
    if (camellia_dec_blk(in, out, ctx) != camellia_good)
        abort();
}

static void
xorblock(unsigned char *out, const unsigned char *in)
{
    int z;
    for (z = 0; z < BLOCK_SIZE/4; z++) {
        unsigned char *outptr = &out[z*4];
        const unsigned char *inptr = &in[z*4];
        /*
         * Use unaligned accesses.  On x86, this will probably still be faster
         * than multiple byte accesses for unaligned data, and for aligned data
         * should be far better.  (One test indicated about 2.4% faster
         * encryption for 1024-byte messages.)
         *
         * If some other CPU has really slow unaligned-word or byte accesses,
         * perhaps this function (or the load/store helpers?) should test for
         * alignment first.
         *
         * If byte accesses are faster than unaligned words, we may need to
         * conditionalize on CPU type, as that may be hard to determine
         * automatically.
         */
        store_32_n(load_32_n(outptr) ^ load_32_n(inptr), outptr);
    }
}

static krb5_error_code
krb5int_camellia_encrypt(krb5_key key, const krb5_data *ivec,
                         krb5_crypto_iov *data, size_t num_data)
{
    unsigned char tmp[BLOCK_SIZE], tmp2[BLOCK_SIZE];
    int nblocks = 0, blockno;
    size_t input_length, i;
    struct iov_block_state input_pos, output_pos;

    if (key->cache == NULL) {
        key->cache = malloc(sizeof(struct camellia_key_info_cache));
        if (key->cache == NULL)
            return ENOMEM;
        CACHE(key)->enc_ctx.keybitlen = CACHE(key)->dec_ctx.keybitlen = 0;
    }
    if (CACHE(key)->enc_ctx.keybitlen == 0) {
        if (camellia_enc_key(key->keyblock.contents, key->keyblock.length,
                             &CACHE(key)->enc_ctx) != camellia_good)
            abort();
    }
    if (ivec != NULL)
        memcpy(tmp, ivec->data, BLOCK_SIZE);
    else
        memset(tmp, 0, BLOCK_SIZE);

    for (i = 0, input_length = 0; i < num_data; i++) {
        krb5_crypto_iov *iov = &data[i];

        if (ENCRYPT_IOV(iov))
            input_length += iov->data.length;
    }

    IOV_BLOCK_STATE_INIT(&input_pos);
    IOV_BLOCK_STATE_INIT(&output_pos);

    nblocks = (input_length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (nblocks == 1) {
        krb5int_c_iov_get_block(tmp, BLOCK_SIZE, data, num_data, &input_pos);
        enc(tmp2, tmp, &CACHE(key)->enc_ctx);
        krb5int_c_iov_put_block(data, num_data, tmp2, BLOCK_SIZE, &output_pos);
    } else if (nblocks > 1) {
        unsigned char blockN2[BLOCK_SIZE];   /* second last */
        unsigned char blockN1[BLOCK_SIZE];   /* last block */

        for (blockno = 0; blockno < nblocks - 2; blockno++) {
            unsigned char blockN[BLOCK_SIZE], *block;

            krb5int_c_iov_get_block_nocopy(blockN, BLOCK_SIZE,
                                           data, num_data, &input_pos, &block);
            xorblock(tmp, block);
            enc(block, tmp, &CACHE(key)->enc_ctx);
            krb5int_c_iov_put_block_nocopy(data, num_data, blockN, BLOCK_SIZE,
                                           &output_pos, block);

            /* Set up for next block.  */
            memcpy(tmp, block, BLOCK_SIZE);
        }

        /* Do final CTS step for last two blocks (the second of which
           may or may not be incomplete).  */

        /* First, get the last two blocks */
        memset(blockN1, 0, sizeof(blockN1)); /* pad last block with zeros */
        krb5int_c_iov_get_block(blockN2, BLOCK_SIZE, data, num_data,
                                &input_pos);
        krb5int_c_iov_get_block(blockN1, BLOCK_SIZE, data, num_data,
                                &input_pos);

        /* Encrypt second last block */
        xorblock(tmp, blockN2);
        enc(tmp2, tmp, &CACHE(key)->enc_ctx);
        memcpy(blockN2, tmp2, BLOCK_SIZE); /* blockN2 now contains first block */
        memcpy(tmp, tmp2, BLOCK_SIZE);

        /* Encrypt last block */
        xorblock(tmp, blockN1);
        enc(tmp2, tmp, &CACHE(key)->enc_ctx);
        memcpy(blockN1, tmp2, BLOCK_SIZE);

        /* Put the last two blocks back into the iovec (reverse order) */
        krb5int_c_iov_put_block(data, num_data, blockN1, BLOCK_SIZE,
                                &output_pos);
        krb5int_c_iov_put_block(data, num_data, blockN2, BLOCK_SIZE,
                                &output_pos);

        if (ivec != NULL)
            memcpy(ivec->data, blockN1, BLOCK_SIZE);
    }

    return 0;
}

static krb5_error_code
krb5int_camellia_decrypt(krb5_key key, const krb5_data *ivec,
                         krb5_crypto_iov *data, size_t num_data)
{
    unsigned char tmp[BLOCK_SIZE], tmp2[BLOCK_SIZE], tmp3[BLOCK_SIZE];
    int nblocks = 0, blockno;
    unsigned int i;
    size_t input_length;
    struct iov_block_state input_pos, output_pos;

    if (key->cache == NULL) {
        key->cache = malloc(sizeof(struct camellia_key_info_cache));
        if (key->cache == NULL)
            return ENOMEM;
        CACHE(key)->enc_ctx.keybitlen = CACHE(key)->dec_ctx.keybitlen = 0;
    }
    if (CACHE(key)->dec_ctx.keybitlen == 0) {
        if (camellia_dec_key(key->keyblock.contents, key->keyblock.length,
                             &CACHE(key)->dec_ctx) != camellia_good)
            abort();
    }

    if (ivec != NULL)
        memcpy(tmp, ivec->data, BLOCK_SIZE);
    else
        memset(tmp, 0, BLOCK_SIZE);

    for (i = 0, input_length = 0; i < num_data; i++) {
        krb5_crypto_iov *iov = &data[i];

        if (ENCRYPT_IOV(iov))
            input_length += iov->data.length;
    }

    IOV_BLOCK_STATE_INIT(&input_pos);
    IOV_BLOCK_STATE_INIT(&output_pos);

    nblocks = (input_length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (nblocks == 1) {
        krb5int_c_iov_get_block(tmp, BLOCK_SIZE, data, num_data, &input_pos);
        dec(tmp2, tmp, &CACHE(key)->dec_ctx);
        krb5int_c_iov_put_block(data, num_data, tmp2, BLOCK_SIZE, &output_pos);
    } else if (nblocks > 1) {
        unsigned char blockN2[BLOCK_SIZE];   /* second last */
        unsigned char blockN1[BLOCK_SIZE];   /* last block */

        for (blockno = 0; blockno < nblocks - 2; blockno++) {
            unsigned char blockN[BLOCK_SIZE], *block;

            krb5int_c_iov_get_block_nocopy(blockN, BLOCK_SIZE,
                                           data, num_data, &input_pos, &block);
            memcpy(tmp2, block, BLOCK_SIZE);
            dec(block, block, &CACHE(key)->dec_ctx);
            xorblock(block, tmp);
            memcpy(tmp, tmp2, BLOCK_SIZE);
            krb5int_c_iov_put_block_nocopy(data, num_data, blockN, BLOCK_SIZE,
                                           &output_pos, block);
        }

        /* Do last two blocks, the second of which (next-to-last block
           of plaintext) may be incomplete.  */

        /* First, get the last two encrypted blocks */
        memset(blockN1, 0, sizeof(blockN1)); /* pad last block with zeros */
        krb5int_c_iov_get_block(blockN2, BLOCK_SIZE, data, num_data,
                                &input_pos);
        krb5int_c_iov_get_block(blockN1, BLOCK_SIZE, data, num_data,
                                &input_pos);

        if (ivec != NULL)
            memcpy(ivec->data, blockN2, BLOCK_SIZE);

        /* Decrypt second last block */
        dec(tmp2, blockN2, &CACHE(key)->dec_ctx);
        /* Set tmp2 to last (possibly partial) plaintext block, and
           save it.  */
        xorblock(tmp2, blockN1);
        memcpy(blockN2, tmp2, BLOCK_SIZE);

        /* Maybe keep the trailing part, and copy in the last
           ciphertext block.  */
        input_length %= BLOCK_SIZE;
        memcpy(tmp2, blockN1, input_length ? input_length : BLOCK_SIZE);
        dec(tmp3, tmp2, &CACHE(key)->dec_ctx);
        xorblock(tmp3, tmp);
        memcpy(blockN1, tmp3, BLOCK_SIZE);

        /* Put the last two blocks back into the iovec */
        krb5int_c_iov_put_block(data, num_data, blockN1, BLOCK_SIZE,
                                &output_pos);
        krb5int_c_iov_put_block(data, num_data, blockN2, BLOCK_SIZE,
                                &output_pos);
    }

    return 0;
}

krb5_error_code
krb5int_camellia_cbc_mac(krb5_key key, const krb5_crypto_iov *data,
                         size_t num_data, const krb5_data *iv,
                         krb5_data *output)
{
    camellia_ctx ctx;
    unsigned char blockY[BLOCK_SIZE];
    struct iov_block_state iov_state;

    if (output->length < BLOCK_SIZE)
        return KRB5_BAD_MSIZE;

    if (camellia_enc_key(key->keyblock.contents,
                         key->keyblock.length, &ctx) != camellia_good)
        abort();

    if (iv != NULL)
        memcpy(blockY, iv->data, BLOCK_SIZE);
    else
        memset(blockY, 0, BLOCK_SIZE);

    IOV_BLOCK_STATE_INIT(&iov_state);
    for (;;) {
        unsigned char blockB[BLOCK_SIZE];

        if (!krb5int_c_iov_get_block(blockB, BLOCK_SIZE, data, num_data,
                                     &iov_state))
            break;

        xorblock(blockB, blockY);
        if (camellia_enc_blk(blockB, blockY, &ctx) != camellia_good)
            abort();
    }

    output->length = BLOCK_SIZE;
    memcpy(output->data, blockY, BLOCK_SIZE);

    return 0;
}

static krb5_error_code
camellia_init_state(const krb5_keyblock *key, krb5_keyusage usage,
                    krb5_data *state)
{
    state->length = 16;
    state->data = malloc(16);
    if (state->data == NULL)
        return ENOMEM;
    memset(state->data, 0, state->length);
    return 0;
}

static void
camellia_key_cleanup(krb5_key key)
{
    zapfree(key->cache, sizeof(struct camellia_key_info_cache));
}

const struct krb5_enc_provider krb5int_enc_camellia128 = {
    16,
    16, 16,
    krb5int_camellia_encrypt,
    krb5int_camellia_decrypt,
    krb5int_camellia_cbc_mac,
    camellia_init_state,
    krb5int_default_free_state,
    camellia_key_cleanup
};

const struct krb5_enc_provider krb5int_enc_camellia256 = {
    16,
    32, 32,
    krb5int_camellia_encrypt,
    krb5int_camellia_decrypt,
    krb5int_camellia_cbc_mac,
    camellia_init_state,
    krb5int_default_free_state,
    camellia_key_cleanup
};
