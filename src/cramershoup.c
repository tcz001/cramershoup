/* -*- mode: c; c-file-style: "openbsd" -*- */
/* TODO:5002 You may want to change the copyright of all files. This is the
 * TODO:5002 ISC license. Choose another one if you want.
 */
/*
 * Copyright (c) 2014 Fan Jiang <fan.torchz@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "cramershoup.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include "decaf_crypto.h"
#include "randombytes.h"

extern const char *__progname;

static void
usage(void)
{
    /* TODO:3002 Don't forget to update the usage block with the most
     * TODO:3002 important options. */
    fprintf(stderr, "Usage: %s [OPTIONS]\n",
            __progname);
    fprintf(stderr, "Version: %s\n", PACKAGE_STRING);
    fprintf(stderr, "\n");
    fprintf(stderr, " -d, --debug        be more verbose.\n");
    fprintf(stderr, " -h, --help         display help and exit\n");
    fprintf(stderr, " -v, --version      print version and exit\n");
    fprintf(stderr, " -g, --generator    print generator\n");
    fprintf(stderr, " -G, --keygen       generate a keypair\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "see manual page " PACKAGE "(8) for more information\n");
}

char *
sprint_encoded_scalar(decaf_448_scalar_t s)
{
    char *buf;
    buf = malloc(sizeof(char)*DECAF_448_SCALAR_BYTES*2);
    unsigned char ser[DECAF_448_SCALAR_BYTES];
    decaf_448_scalar_encode(ser,s);
    for (int i = 0; i < DECAF_448_SCALAR_BYTES; i++){
        sprintf(&buf[i*2], "%02x", (uint8_t) ser[i]);
    }
    return buf;
}

char *
sprint_encoded_point(decaf_448_point_t p)
{
    char *buf;
    buf = malloc(sizeof(char)*DECAF_448_SER_BYTES*2);
    unsigned char ser[DECAF_448_SER_BYTES];
    decaf_448_point_encode(ser,p);
    for (int i = 0; i < DECAF_448_SER_BYTES; i++){
        sprintf(&buf[i*2], "%02x", (uint8_t) ser[i]);
    }
    return buf;
}

void
find_g2(decaf_448_point_t p)
{
    unsigned char encoded_scalar[DECAF_448_SCALAR_BYTES+8];
    const char *magic = "otrv4_g2";
    keccak_sponge_t sponge;
    shake256_init(sponge);
    shake256_update(sponge, (const unsigned char *)magic, strlen(magic));
    shake256_final(sponge, encoded_scalar, sizeof(encoded_scalar));
    shake256_destroy(sponge);

    decaf_448_scalar_t scalar;
    decaf_bool_t valid = decaf_448_scalar_decode(scalar, encoded_scalar);
    decaf_bzero(encoded_scalar, sizeof(encoded_scalar));
    if (!valid){
        log_warnx("main", "decode failure\n");
    }

    decaf_448_precomputed_scalarmul(p, decaf_448_precomputed_base, scalar);
}

void
print_generators()
{
    decaf_448_point_t g1, g2;

    decaf_448_point_copy(g1, decaf_448_point_base);
    find_g2(g2);
    printf("g1: %s\n", sprint_encoded_point(g1));
    printf("g2: %s\n", sprint_encoded_point(g2));
}

void random_scalar(decaf_448_scalar_t secret_scalar)
{
    decaf_448_symmetric_key_t proto;
    randombytes(proto,sizeof(proto));

    const char *magic = "cramershoup_secret";
    uint8_t encoded_scalar[DECAF_448_SCALAR_BYTES+8];

    keccak_sponge_t sponge;
    shake256_init(sponge);
    shake256_update(sponge, proto, sizeof(decaf_448_symmetric_key_t));
    shake256_update(sponge, (const unsigned char *)magic, strlen(magic));
    shake256_final(sponge, encoded_scalar, sizeof(encoded_scalar));
    shake256_destroy(sponge);

    decaf_448_scalar_decode_long(secret_scalar, encoded_scalar, sizeof(encoded_scalar));
    decaf_bzero(encoded_scalar, sizeof(encoded_scalar));
}

/* carmershoup_private_key_t */
void
generate_keys()
{

    decaf_448_point_t g1, g2;
    decaf_448_point_copy(g1, decaf_448_point_base);
    find_g2(g2);

    decaf_448_scalar_t x1, x2, y1, y2, z;

    random_scalar(x1);
    random_scalar(x2);
    random_scalar(y1);
    random_scalar(y2);
    random_scalar(z);

    log_info("generate_keys", "x1: %s", sprint_encoded_scalar(x1));
    log_info("generate_keys", "x2: %s", sprint_encoded_scalar(x2));
    log_info("generate_keys", "y1: %s", sprint_encoded_scalar(y1));
    log_info("generate_keys", "y2: %s", sprint_encoded_scalar(y2));
    log_info("generate_keys", "z: %s", sprint_encoded_scalar(z));

    decaf_448_point_t c, d, h;
    decaf_448_point_double_scalarmul(c, g1, x1, g2, x2);
    decaf_448_point_double_scalarmul(d, g1, y1, g2, y2);
    decaf_448_point_scalarmul(h, g1, z);

    log_info("generate_keys", "c: %s", sprint_encoded_point(c));
    log_info("generate_keys", "d: %s", sprint_encoded_point(d));
    log_info("generate_keys", "h: %s", sprint_encoded_point(h));
}

int
main(int argc, char *argv[])
{
    int debug = 1;
    int keygen = 0;
    int ch;

    static struct option long_options[] = {
        { "debug", no_argument, 0, 'd' },
        { "help",  no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { "generator", no_argument, 0, 'g' },
        { "keygen", no_argument, 0, 'G' },
        { 0 }
    };
    while (1) {
        int option_index = 0;
        ch = getopt_long(argc, argv, "hvdgGD:",
                long_options, &option_index);
        if (ch == -1) break;
        switch (ch) {
            case 'h':
                usage();
                exit(0);
                break;
            case 'v':
                fprintf(stdout, "%s\n", PACKAGE_VERSION);
                exit(0);
                break;
            case 'g':
                print_generators();
                exit(0);
                break;
            case 'G':
                keygen++;
                break;
            case 'd':
                debug++;
                break;
            case 'D':
                log_accept(optarg);
                break;
            default:
                fprintf(stderr, "unknown option `%c'\n", ch);
                usage();
                exit(1);
        }
    }

    log_init(debug, __progname);
    if (keygen) generate_keys();

    return EXIT_SUCCESS;
}
