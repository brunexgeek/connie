#include "connie.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <ctype.h>

#define MIN_VALUE(X, Y) (((X) < (Y)) ? (X) : (Y))

static void hex_dump(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (i > 0 && (i % 16) == 0)
            puts("");
        printf("%02X ", data[i]);
    }
    printf("\n");
}

static int print_document(const uint8_t *buffer, size_t size)
{
    int result = 0;
    int indent = 0;
    struct connie_output output;
    struct connie_reader reader;
    connie_reader_init(&reader, buffer, size);
    while ((result = connie_reader_next(&reader, &output)) == CERR_OK)
    {
        for (int i = 0; i < indent; ++i)
            fputs("  ", stdout);

        if (output.key_type == CKEY_STRING)
            printf("%s =", output.key_string);
        if (output.key_type == CKEY_UINT)
            printf("%d = ", output.key_uint32);

        switch (output.type)
        {
            case CTYPE_STRING:
                printf("'%s'\n", output.value_string);
                break;
            case CTYPE_MAP_OPEN:
                ++indent;
                puts("{");
                break;
            case CTYPE_MAP_CLOSE:
                --indent;
                puts("}");
                break;
            case CTYPE_ARRAY_OPEN:
                ++indent;
                puts("[");
                break;
            case CTYPE_ARRAY_CLOSE:
                --indent;
                puts("]");
                break;
            case CTYPE_BYTES:
                puts("<binary data>\n");
                break;
            case CTYPE_BOOL:
                printf("%s\n", output.value_boolean != 0 ? "true" : "false");
                break;
            case CTYPE_NULL:
                puts("null\n");
                break;
            case CTYPE_INT32:
                printf("%d\n", output.value_int32);
                break;
            case CTYPE_UINT32:
                printf("%u\n", output.value_uint32);
                break;
            case CTYPE_INT64:
                printf("%ld\n", output.value_int64);
                break;
            case CTYPE_UINT64:
                printf("%lu\n", output.value_uint64);
                break;
            case CTYPE_FP32:
                printf("%f\n", output.value_fp32);
                break;
            case CTYPE_FP64:
                printf("%f\n",  output.value_fp64);
                break;
        }
    }

    return result;
}

#define DETECT_ERROR(x) \
    do  { int err = (x); \
         if (err != CERR_OK) { \
             printf("Error %d at %s:%d\n", err, __FILE__, __LINE__); \
             return 1; \
        }} while (0)

static int write_to_file(const uint8_t *buffer, size_t size, void *data)
{
    fwrite(buffer, size, 1, (FILE*)data);
    return CERR_OK;
}

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    uint8_t buffer[128];
    FILE *out = NULL;
    if (argc == 2)
        out = fopen(argv[1], "wb");

    struct connie_writer_params params = {
        .buffer = buffer,
        .buffer_size = sizeof(buffer),
        .callback = out != NULL ? write_to_file : NULL,
        .data = out != NULL ? out : NULL,
        .key_type = CKEY_UINT,
    };

    struct connie_writer writer;
    uint32_t key = 0;
    DETECT_ERROR(connie_writer_init(&writer, &params));

    DETECT_ERROR(connie_writer_put_string(&writer, ++key, "name", "Alice"));
    DETECT_ERROR(connie_writer_put_int32(&writer, ++key, "age", -30));
    DETECT_ERROR(connie_writer_put_int32(&writer, ++key, "age", 70000));
    DETECT_ERROR(connie_writer_put_boolean(&writer, ++key, "isStudent", 1));
    DETECT_ERROR(connie_writer_put_fp32(&writer, ++key, "weight", 234.23F));
    DETECT_ERROR(connie_writer_put_fp64(&writer, ++key, "salary", 150236.999));

    // Nested map
    DETECT_ERROR(connie_writer_open_map(&writer, ++key, "address"));
    DETECT_ERROR(connie_writer_put_string(&writer, ++key, "city", "São Paulo"));
    DETECT_ERROR(connie_writer_put_string(&writer, ++key, "zip", "13140-000"));
    DETECT_ERROR(connie_writer_close_map(&writer));

    // Array
    DETECT_ERROR(connie_writer_open_array(&writer, ++key, "skills"));
    DETECT_ERROR(connie_writer_put_string(&writer, 0, NULL, "C"));
    DETECT_ERROR(connie_writer_put_string(&writer, 0, NULL, "Python"));
    DETECT_ERROR(connie_writer_put_string(&writer, 0, NULL, "Go"));
    DETECT_ERROR(connie_writer_close_array(&writer));

    size_t size = 0;
    DETECT_ERROR(connie_writer_finish(&writer, NULL, &size));

    if (out != NULL)
        fclose(out);
    else
    {
        hex_dump(buffer, size);

        if (print_document(buffer, size) != CERR_COMPLETE)
            return 1;

        puts("\nDiagnotisc output:");
        if (connie_diagnostic(buffer, size) != CERR_COMPLETE)
            return 1;
    }

    return 0;
}

