#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include "decaf_crypto.h"
#include "randombytes.h"
#include "cramershoup.h"
#include "log.h"

extern const char *__progname;

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n",
            __progname);
    fprintf(stderr, "Version: %s\n", PACKAGE_STRING);
    fprintf(stderr, "\n");
    fprintf(stderr, " -d, --debug        be more verbose.\n");
    fprintf(stderr, " -h, --help         display help and exit\n");
    fprintf(stderr, " -v, --version      print version and exit\n");
    fprintf(stderr, " -G, --keygen       generate a keypair\n");
    fprintf(stderr, " -E, --enc          encrypt\n");
    fprintf(stderr, " -D, --dec          decrypt\n");
    fprintf(stderr, " -T, --test         end to end test\n");
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
save_key(FILE *file, private_key_t *priv, public_key_t *pub)
{
    // private_key
    unsigned char *x1,*x2,*y1,*y2,*z;
    x1 = malloc(sizeof(unsigned char)*DECAF_448_SCALAR_BYTES);
    x2 = malloc(sizeof(unsigned char)*DECAF_448_SCALAR_BYTES);
    y1 = malloc(sizeof(unsigned char)*DECAF_448_SCALAR_BYTES);
    y2 = malloc(sizeof(unsigned char)*DECAF_448_SCALAR_BYTES);
    z  = malloc(sizeof(unsigned char)*DECAF_448_SCALAR_BYTES);
    decaf_448_scalar_encode(x1,priv->x1);
    decaf_448_scalar_encode(x2,priv->x2);
    decaf_448_scalar_encode(y1,priv->y1);
    decaf_448_scalar_encode(y2,priv->y2);
    decaf_448_scalar_encode(z ,priv->z);
    fwrite(x1, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    fwrite(x2, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    fwrite(y1, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    fwrite(y2, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    fwrite(z , sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);

    // public_key
    unsigned char *c,*d,*h;
    c = malloc(sizeof(unsigned char)*DECAF_448_SER_BYTES);
    d = malloc(sizeof(unsigned char)*DECAF_448_SER_BYTES);
    h = malloc(sizeof(unsigned char)*DECAF_448_SER_BYTES);
    decaf_448_point_encode(c,pub->c);
    decaf_448_point_encode(d,pub->d);
    decaf_448_point_encode(h,pub->h);
    fwrite(c, sizeof(unsigned char), DECAF_448_SER_BYTES, file);
    fwrite(d, sizeof(unsigned char), DECAF_448_SER_BYTES, file);
    fwrite(h, sizeof(unsigned char), DECAF_448_SER_BYTES, file);
}

void
parse_key_file(private_key_t *priv, public_key_t *pub, FILE* file)
{
    if(!file){
        fatal("parse_key_file", "file read failure\n");
    }
    decaf_bool_t valid;
    size_t size;
    unsigned char buffer[DECAF_448_SCALAR_BYTES];

    size = fread(buffer, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    valid = decaf_448_scalar_decode(priv->x1, (const unsigned char *)buffer);
    if (!valid || size != DECAF_448_SCALAR_BYTES){
        fatal("parse_key_file", "x1 decode failure\n");
    }

    size = fread(buffer, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    valid = decaf_448_scalar_decode(priv->x2, (const unsigned char *)buffer);
    if (!valid || size != DECAF_448_SCALAR_BYTES){
        fatal("parse_key_file", "x2 decode failure\n");
    }

    size = fread(buffer, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    valid = decaf_448_scalar_decode(priv->y1, (const unsigned char *)buffer);
    if (!valid || size != DECAF_448_SCALAR_BYTES){
        fatal("parse_key_file", "y1 decode failure\n");
    }

    size = fread(buffer, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    valid = decaf_448_scalar_decode(priv->y2, (const unsigned char *)buffer);
    if (!valid || size != DECAF_448_SCALAR_BYTES){
        fatal("parse_key_file", "y2 decode failure\n");
    }

    size = fread(buffer, sizeof(unsigned char), DECAF_448_SCALAR_BYTES, file);
    valid = decaf_448_scalar_decode(priv->z , (const unsigned char *)buffer);
    if (!valid || size != DECAF_448_SCALAR_BYTES){
        fatal("parse_key_file", "z decode failure\n");
    }

    size = fread(buffer, sizeof(unsigned char), DECAF_448_SER_BYTES, file);
    valid = decaf_448_point_decode(pub->c, (const unsigned char *)buffer, DECAF_FALSE);
    if (!valid || size != DECAF_448_SER_BYTES){
        fatal("parse_key_file", "c decode failure\n");
    }

    size = fread(buffer, sizeof(unsigned char), DECAF_448_SER_BYTES, file);
    valid = decaf_448_point_decode(pub->d, (const unsigned char *)buffer, DECAF_FALSE);
    if (!valid || size != DECAF_448_SER_BYTES){
        fatal("parse_key_file", "d decode failure\n");
    }

    size = fread(buffer, sizeof(unsigned char), DECAF_448_SER_BYTES, file);
    valid = decaf_448_point_decode(pub->h, (const unsigned char *)buffer, DECAF_FALSE);
    if (!valid || size != DECAF_448_SER_BYTES){
        fatal("parse_key_file", "h decode failure\n");
    }
}

// TODO: need a better way to do this in test
void
test_random_scalar(decaf_448_scalar_t secret_scalar)
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


int
main(int argc, char *argv[])
{
    int debug = 1;
    int keygen = 0;
    int enc = 0;
    int dec = 0;
    int end_to_end = 0;
    int ch;

    static struct option long_options[] = {
        { "debug", optional_argument, 0, 'd' },
        { "help",  no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { "keygen", required_argument, 0, 'G' },
        { "enc", required_argument, 0, 'E' },
        { "dec", required_argument, 0, 'D' },
        { "test", required_argument, 0, 'T' },
        { 0 }
    };

    struct globalArgs_t {
        const char *keyFileName;    /* -o option */
    } globalArgs;

    while (1) {
        int option_index = 0;
        ch = getopt_long(argc, argv, "hvgdT:G:E:D:",
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
            case 'G':
                globalArgs.keyFileName = optarg;
                keygen++;
                break;
            case 'E':
                globalArgs.keyFileName = optarg;
                enc++;
                break;
            case 'D':
                globalArgs.keyFileName = optarg;
                dec++;
                break;
            case 'T':
                globalArgs.keyFileName = optarg;
                end_to_end++;
                break;
            case 'd':
                log_accept(optarg);
                debug++;
                break;
            default:
                fprintf(stderr, "unknown option `%c'\n", ch);
                usage();
                exit(1);
        }
    }

    log_init(debug, __progname);

    if (keygen)
    {
        private_key_t private_key;
        public_key_t public_key;
        cramershoup_448_derive_keys(&private_key, &public_key);
        FILE *fp = fopen(globalArgs.keyFileName, "w"); // Open file for writing
        save_key(fp, &private_key, &public_key);
        fclose(fp);
    }

    if (end_to_end)
    {
        FILE *fp = fopen(globalArgs.keyFileName, "r"); // Open file for reading
        private_key_t private_key;
        public_key_t public_key;
        parse_key_file(&private_key, &public_key, fp);
        fclose(fp);

        unsigned char *plaintext = malloc(sizeof(unsigned char)*DECAF_448_SER_BYTES);
        unsigned char *ciphertext = malloc(sizeof(unsigned char)*(DECAF_448_SER_BYTES*4));

        decaf_448_scalar_t r;
        test_random_scalar(r);
        decaf_bool_t valid = decaf_448_scalar_decode(r, (const unsigned char *)"hello world");
        if (!valid) {
        }
        decaf_448_point_t g1r;
        decaf_448_point_scalarmul(g1r, decaf_448_point_base, r);
        printf("plaintext: \n");
        decaf_448_point_encode(plaintext,g1r);
        for (int i = 0; i < DECAF_448_SER_BYTES; i++){
            printf("%02x", plaintext[i]);
        }
        printf("\n");
        cramershoup_448_enc(ciphertext, plaintext, &public_key);
        printf("ciphertext:\n");
        for (int j = 0; j < 4; j++){
            for (int i = 0; i < DECAF_448_SER_BYTES; i++){
                printf("%02x", ciphertext[i+DECAF_448_SER_BYTES*j]);
            }
            printf("\n");
        }

        unsigned char *decrypted = malloc(sizeof(unsigned char)*DECAF_448_SER_BYTES);
        cramershoup_448_dec(decrypted, ciphertext, &private_key);
        printf("decrypted: \n");
        for (int i = 0; i < DECAF_448_SER_BYTES; i++){
            printf("%02x", decrypted[i]);
        }
        printf("\n");
    }

    return EXIT_SUCCESS;
}