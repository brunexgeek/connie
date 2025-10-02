/*
 * connie (concise nested information encoder)
 * Copyright 2025 Bruno Costa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "connie.h"

#define CT_UINT    0
#define CT_INT     1
#define CT_BSTR    2
#define CT_TSTR    3
#define CT_ARRAY   4
#define CT_MAP     5
#define CT_TAG     6
#define CT_SIMPLE  7

#define DWF_DRY_RUN 1
#define DWF_INVALID 2

#define IS_VALID_TYPE(x) \
    ( ((x) >= TYPE_DOUBLE && (x) <= TYPE_NULL) || \
      (x) == TYPE_INT32 || \
      (x) == TYPE_INT64 || \
      (x) == TYPE_DECIMAL )

#define RETURN_ON_ERROR(expr) \
    do { int err = (expr); if (err != CERR_OK) return err; } while (0)

#define RETURN_IF_INVALID(writer) \
    do { if ((writer)->flags & DWF_INVALID) return CERR_INVALID_STATE; } while (0)

#define RETURN_WRITER_ERROR(writer, code) \
    do { (writer)->flags |= DWF_INVALID; return code; } while (0)

static const size_t EMPTY_DOC_SIZE = 3;

struct cbor_iter_output {
    // Position at which entry starts. If there's any payload, it starts
    // at (1 + ibytes) and have (value) bytes long.
    const uint8_t *ptr;
    // Interpreted additional info
    uint64_t value;
    // Raw additional info
    uint16_t info : 5;
    // Major type
    uint16_t type : 3;
    // Number of bytes that make up the additional information
    uint16_t ibytes : 4;
    uint16_t reserved : 4;
};

static int cbor_write_fp32(struct connie_writer *writer, float value)
{
    if (writer->flags & DWF_DRY_RUN)
    {
        writer->size += 5;
        return CERR_OK;
    }
    if (writer->cursor + 5 >= writer->end)
        return CERR_OUT_OF_BOUNDS;

    union {
        float fp;
        uint32_t uint;
    } convert;
    convert.fp = value;

    *writer->cursor++ = (7 << 5) | 26;
    #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    for (int i = 0; i < 4; i++)
        *writer->cursor++ = (uint8_t)(convert.uint >> (8 * i));
    #else
    for (int i = 3; i >= 0; i--)
        *writer->cursor++ = (uint8_t)(convert.uint >> (8 * i));
    #endif
    return CERR_OK;
}

static int cbor_write_fp64(struct connie_writer *writer, double value)
{
    if (writer->flags & DWF_DRY_RUN)
    {
        writer->size += 9;
        return CERR_OK;
    }
    if (writer->cursor + 9 >= writer->end)
        return CERR_OUT_OF_BOUNDS;

    union {
        double fp;
        uint64_t uint;
    } convert;
    convert.fp = value;

    *writer->cursor++ = (7 << 5) | 27;
    #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    for (int i = 0; i < 8; i++)
        *writer->cursor++ = (uint8_t)(convert.uint >> (8 * i));
    #else
    for (int i = 7; i >= 0; i--)
        *writer->cursor++ = (uint8_t)(convert.uint >> (8 * i));
    #endif
    return CERR_OK;
}

static int cbor_write_uint(struct connie_writer *writer, uint8_t type, uint64_t value)
{
    // make sure we have enough data to read
    uint8_t bytes = 0;
    if (value < 24)
        bytes = 1;
    else if (value <= 0xFF)
        bytes = 2;
    else if (value <= 0xFFFF)
        bytes = 3;
    else if (value <= 0xFFFFFFFF)
        bytes = 5;
    else
        bytes = 9;

    if (writer->flags & DWF_DRY_RUN)
    {
        writer->size += bytes;
        return CERR_OK;
    }
    else
    if (writer->cursor + bytes >= writer->end)
        return CERR_OUT_OF_BOUNDS;

    switch (bytes)
    {
        case 1:
            *writer->cursor++ = (type << 5) | (uint8_t)value;
            break;
        case 2:
            *writer->cursor++ = (type << 5) | 24;
            *writer->cursor++ = (uint8_t)value;
            break;
        case 3:
            *writer->cursor++ = (type << 5) | 25;
            #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            *writer->cursor++ = (uint8_t)(value & 0xFF);
            *writer->cursor++ = (uint8_t)(value >> 8);
            #else
            *writer->cursor++ = (uint8_t)(value >> 8);
            *writer->cursor++ = (uint8_t)(value & 0xFF);
            #endif
            break;
        case 5:
            *writer->cursor++ = (type << 5) | 26;
            #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            for (int i = 0; i < 4; i++)
                *writer->cursor++ = (uint8_t)(value >> (8 * i));
            #else
            for (int i = 3; i >= 0; i--)
                *writer->cursor++ = (uint8_t)(value >> (8 * i));
            #endif
            break;
        case 9:
            *writer->cursor++ = (type << 5) | 27;
            #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            for (int i = 0; i < 8; i++)
                *writer->cursor++ = (uint8_t)(value >> (8 * i));
            #else
            for (int i = 7; i >= 0; i--)
                *writer->cursor++ = (uint8_t)(value >> (8 * i));
            #endif
            break;
        default:
            return CERR_INVALID_DATA;
    }

    return CERR_OK;
}

static int cbor_write_string(struct connie_writer *writer, const char *str)
{
    RETURN_IF_INVALID(writer);

    size_t len = strlen(str) + 1;
    cbor_write_uint(writer, CT_TSTR, len); // definite-length text string
    if (writer->flags & DWF_DRY_RUN)
        writer->size += len;
    else
    {
        if (writer->cursor + len >= writer->end)
            RETURN_WRITER_ERROR(writer, CERR_OUT_OF_BOUNDS);
        memcpy(writer->cursor, str, len);
        writer->cursor += len;
    }
    return CERR_OK;
}

static int cbor_read_next(struct cbor_iter *iter, struct cbor_iter_output *out)
{
    if (iter->ptr >= iter->end)
        return CERR_OUT_OF_BOUNDS;

    out->ptr = iter->ptr;
    out->ibytes = 0;
    out->type = *iter->ptr >> 5;
    out->info = *iter->ptr++ & 0x1F;

    // we do not support tags
    if (out->type == CT_TAG)
        return CERR_INVALID_DATA;

    // accept indefinite-length only for arrays and maps
    if (out->info == 31 && (out->type == CT_ARRAY || out->type == CT_MAP || out->type == CT_SIMPLE))
        return CERR_OK;
    else if (out->info >= 28)
        return CERR_INVALID_DATA;

    // interpret additional info remaining bytes, if any
    if (out->info >= 24)
    {
        out->ibytes = 1 << (out->info - 24);
        switch (out->ibytes)
        {
            case 1:
                out->value = *iter->ptr++;
                break;
            case 2:
            {
                #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
                out->value = iter->ptr[0] | iter->ptr[1] << 8;
                #else
                out->value = (iter->ptr[0] << 8) | iter->ptr[1];
                #endif
                iter->ptr += 2;
                break;
            }
            case 4:
            {
                out->value = 0;
                #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
                for (int i = 0; i < 4; i--)
                    out->value |= (uint64_t) iter->ptr[i] << (8 * i);
                #else
                for (int i = 3; i >= 0; i--)
                    out->value |= (uint64_t) iter->ptr[3-i] << (8 * i);
                #endif
                iter->ptr += 4;
                break;
            }
            case 8:
            {
                out->value = 0;
                #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
                for (int i = 0; i < 8; i--)
                    out->value |= (uint64_t) iter->ptr[i] << (8 * i);
                #else
                for (int i = 7; i >= 0; i--)
                    out->value |= (uint64_t) iter->ptr[7-i] << (8 * i);
                #endif
                iter->ptr += 8;
                break;
            }
        }
    }
    else
        out->value = out->info;

    // check the presence of the payload
    if (out->type == CT_BSTR || out->type == CT_TSTR)
    {
        if (iter->ptr + out->value >= iter->end)
            return CERR_OUT_OF_BOUNDS;
        if (out->type == CT_TSTR && (iter->ptr[0] == 0 || iter->ptr[out->value-1] != 0))
            return CERR_INVALID_DATA;

        iter->ptr += out->value;
    }

    return CERR_OK;
}

static inline int cbor_write_key(struct connie_writer *writer, uint32_t key1, const char *key2)
{
    RETURN_IF_INVALID(writer);

    if (writer->scope[writer->scope_index] == CTYPE_ARRAY_OPEN)
        return CERR_OK;

    if (writer->key_type == CKEY_UINT)
        return cbor_write_uint(writer, CT_UINT, key1);
    else if (writer->key_type == CKEY_STRING && key2 != NULL)
        return cbor_write_string(writer, key2);
    else
    {
        writer->flags |= DWF_INVALID;
        return CERR_INVALID_KEY;
    }
}

int connie_writer_init(struct connie_writer *writer, uint8_t *buffer, size_t size, uint8_t key_type )
{
    if (writer == NULL || (key_type != CKEY_STRING && key_type != CKEY_UINT))
        return CERR_INVALID_ARGUMENT;
    //if (params->buffer == NULL || params->buffer_size == 0 && params->callback == NULL)
    //    return CERR_INVALID_ARGUMENT;

    memset(writer, 0, sizeof(struct connie_writer));
    writer->scope[0] = CTYPE_MAP_OPEN;
    writer->key_type = key_type;
    if (buffer == NULL || size == 0)
    {
        writer->flags |= DWF_DRY_RUN;
        writer->size += 2;
    }
    else
    {
        writer->begin = writer->cursor = buffer;
        writer->end = buffer + size;
        // empty metadata map (not used for now)
        *writer->cursor++ = (CT_MAP << 5);
        // open root document
        *writer->cursor++ = ((CT_MAP << 5) | 0x1F);
    }

    return CERR_OK;
}

int connie_writer_finish(struct connie_writer *writer, uint8_t **output, size_t *size)
{
    if (writer == NULL || size == NULL)
        return CERR_INVALID_ARGUMENT;
    RETURN_IF_INVALID(writer);

    if (writer->flags & DWF_DRY_RUN)
        *size = writer->size + 1;
    else
    {
        if (writer->cursor >= writer->end)
            RETURN_WRITER_ERROR(writer, CERR_OUT_OF_BOUNDS);
        *writer->cursor++ = 0xFF; // type 0x07, info 0x1F

        if (output != NULL)
            *output = writer->begin;
        if (size != NULL)
            *size = (size_t) (writer->cursor - writer->begin);
    }

    writer->flags |= DWF_INVALID;
    return CERR_OK;
}

int connie_writer_put_string(struct connie_writer *writer, uint32_t key1, const char *key2, const char *value)
{
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    return cbor_write_string(writer, value);
}

int connie_writer_put_int32(struct connie_writer *writer, uint32_t key1, const char *key2, int32_t value)
{
    return connie_writer_put_int64(writer, key1, key2, (int64_t) value);
}

int connie_writer_put_int64(struct connie_writer *writer, uint32_t key1, const char *key2, int64_t value)
{
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    if (value >= 0)
        return cbor_write_uint(writer, CT_UINT, (uint64_t) value);
    return cbor_write_uint(writer, CT_INT, (uint64_t) ((int64_t) (-1) - value));
}

int connie_writer_put_uint32(struct connie_writer *writer, uint32_t key1, const char *key2, uint32_t value)
{
    return connie_writer_put_uint64(writer, key1, key2, (uint64_t) value);
}

int connie_writer_put_uint64(struct connie_writer *writer, uint32_t key1, const char *key2, uint64_t value)
{
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    return cbor_write_uint(writer, CT_UINT, value);
}

int connie_writer_put_fp32(struct connie_writer *writer, uint32_t key1, const char *key2, float value)
{
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    return cbor_write_fp32(writer, value);
}

int connie_writer_put_fp64(struct connie_writer *writer, uint32_t key1, const char *key2, double value)
{
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    return cbor_write_fp64(writer, value);
}

int connie_writer_put_bytes(struct connie_writer *writer, uint32_t key1, const char *key2, const uint8_t *data, size_t length)
{
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    cbor_write_uint(writer, CT_BSTR, length); // definite-length byte string
    if (writer->cursor + length >= writer->end)
        RETURN_WRITER_ERROR(writer, CERR_OUT_OF_BOUNDS);
    memcpy(writer->cursor, data, length);
    writer->cursor += length;
    return CERR_OK;
}

int connie_writer_put_null(struct connie_writer *writer, uint32_t key1, const char *key2)
{
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    return cbor_write_uint(writer, CT_SIMPLE, 22); // simple value, null
}

int connie_writer_put_boolean(struct connie_writer *writer, uint32_t key1, const char *key2, int value)
{
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    return cbor_write_uint(writer, CT_SIMPLE, value ? 21 : 20); // simple value
}

int connie_writer_open_map(struct connie_writer *writer, uint32_t key1, const char *key2)
{
    if (writer->scope_index + 1 >= CLIMITS_DEPTH)
        RETURN_WRITER_ERROR(writer, CERR_DEPTH_OVERFLOW);
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    writer->scope[++writer->scope_index] = CTYPE_MAP_OPEN;
    if (writer->flags & DWF_DRY_RUN)
        ++writer->size;
    else
    {
        if (writer->cursor >= writer->end)
            RETURN_WRITER_ERROR(writer, CERR_OUT_OF_BOUNDS);
        *writer->cursor++ = (CT_MAP << 5) | 0x1F; // indefinite-length map
    }
    return CERR_OK;
}

int connie_writer_close_map(struct connie_writer *writer)
{
    if (writer->scope_index == 0)
        RETURN_WRITER_ERROR(writer, CERR_DEPTH_UNDERFLOW);
    if (writer->scope[writer->scope_index] != CTYPE_MAP_OPEN)
        RETURN_WRITER_ERROR(writer, CERR_INVALID_STATE);
    --writer->scope_index;
    if (writer->flags & DWF_DRY_RUN)
        ++writer->size;
    else
    {
        if (writer->cursor >= writer->end)
            RETURN_WRITER_ERROR(writer, CERR_OUT_OF_BOUNDS);
        *writer->cursor++ = 0xFF; // type 0x07, extra info 0x1F
    }
    return CERR_OK;
}

int connie_writer_open_array(struct connie_writer *writer, uint32_t key1, const char *key2)
{
    if (writer->scope_index + 1 >= CLIMITS_DEPTH)
        RETURN_WRITER_ERROR(writer, CERR_DEPTH_OVERFLOW);
    RETURN_ON_ERROR(cbor_write_key(writer, key1, key2));
    writer->scope[++writer->scope_index] = CTYPE_ARRAY_OPEN;
    if (writer->flags & DWF_DRY_RUN)
        ++writer->size;
    else
    {
        if (writer->cursor >= writer->end)
            RETURN_WRITER_ERROR(writer, CERR_OUT_OF_BOUNDS);
        *writer->cursor++ = (CT_ARRAY << 5) | 0x1F; // indefinite-length array
    }
    return CERR_OK;
}

int connie_writer_close_array(struct connie_writer *writer)
{
    if (writer->scope_index == 0)
        RETURN_WRITER_ERROR(writer, CERR_DEPTH_UNDERFLOW);
    if (writer->scope[writer->scope_index] != CTYPE_ARRAY_OPEN)
        RETURN_WRITER_ERROR(writer, CERR_INVALID_STATE);
    --writer->scope_index;
    if (writer->flags & DWF_DRY_RUN)
        ++writer->size;
    else
    {
        if (writer->cursor >= writer->end)
            RETURN_WRITER_ERROR(writer, CERR_OUT_OF_BOUNDS);
        *writer->cursor++ = 0xFF; // type 0x07, extra info 0x1F
    }
    return CERR_OK;
}

int connie_reader_init(struct connie_reader *reader, const uint8_t *buffer, size_t size)
{
    if (buffer == NULL || size < EMPTY_DOC_SIZE)
        return CERR_INVALID_ARGUMENT;
    reader->invalid = 1;

    // empty metadata map (not used for now)
    if (buffer[0] != (CT_MAP << 5))
        return CERR_INVALID_DATA;
    // root document
    if (buffer[1] != ((CT_MAP << 5) | 0x1F))
        return CERR_INVALID_DATA;

    memset(reader, 0, sizeof(struct connie_reader));
    reader->iter.ptr = buffer + 1;
    reader->iter.end = buffer + size;
    return CERR_OK;
}

static inline float uint_to_fp32(uint32_t value)
{
    union {
        float fp;
        uint32_t uint;
    } convert;
    convert.uint = (uint32_t) value;
    return convert.fp;
}

static inline double uint_to_fp64(uint64_t value)
{
    union {
        double fp;
        uint64_t uint;
    } convert;
    convert.uint = (uint64_t) value;
    return convert.fp;
}

static inline int connie_reader_iterate(struct connie_reader *reader, struct connie_output *output)
{
    if (reader == NULL || output == NULL)
        return CERR_INVALID_ARGUMENT;
    if (reader->complete)
        return CERR_COMPLETE;

    memset(output, 0, sizeof(struct connie_output));
    struct cbor_iter_output out;
    int result;
    RETURN_ON_ERROR(result = cbor_read_next(&reader->iter, &out));

    // close a map/array
    if (*out.ptr == 0xFF)
    {
        if (reader->scope_count == 0)
            return CERR_DEPTH_UNDERFLOW;
        output->type = reader->scope[--reader->scope_count];
        reader->complete = reader->scope_count == 0;
        return CERR_OK;
    }
    else
    // only extract keys from maps
    if (reader->scope_count > 0 && reader->scope[reader->scope_count - 1] == CTYPE_MAP_CLOSE)
    {
        // de we already selected a key type?
        if (reader->key_type == CKEY_UNKNOWN)
            reader->key_type = out.type == CT_UINT ? CKEY_UINT : CKEY_STRING;
        // use selected key, if present
        output->key_type = reader->key_type;
        if (reader->key_type == CKEY_STRING && out.type == CT_TSTR)
            output->key_string = (const char*) out.ptr + 1 + out.ibytes;
        else if (reader->key_type == CKEY_UINT && out.type == CT_UINT)
            output->key_uint32 = out.value;
        else
            return CERR_INVALID_KEY;
        RETURN_ON_ERROR(result = cbor_read_next(&reader->iter, &out));
    }

    if (result == CERR_OK)
    {
        switch (out.type)
        {
            case CT_UINT:
                output->type = CTYPE_UINT64;
                output->value_uint64 = out.value;
                return CERR_OK;
            case CT_INT:
                output->type = CTYPE_INT64;
                output->value_int64 = (int64_t) (-1) - (int64_t) out.value;
                return CERR_OK;
            case CT_BSTR:
            case CT_TSTR:
                output->type = out.type == CT_BSTR ? CTYPE_BYTES : CTYPE_STRING;
                output->value_bytes = out.ptr + 1 + out.ibytes;
                output->length = out.value;
                return CERR_OK;
            case CT_ARRAY:
                if (++reader->scope_count >= CLIMITS_DEPTH)
                    return CERR_DEPTH_OVERFLOW;
                output->type = CTYPE_ARRAY_OPEN;
                reader->scope[reader->scope_count - 1] = CTYPE_ARRAY_CLOSE;
                return CERR_OK;
            case CT_MAP:
                if (++reader->scope_count >= CLIMITS_DEPTH)
                    return CERR_DEPTH_OVERFLOW;
                output->type = CTYPE_MAP_OPEN;
                reader->scope[reader->scope_count - 1] = CTYPE_MAP_CLOSE;
                return CERR_OK;
            case CT_SIMPLE:
            {
                switch (out.info)
                {
                    case 20: // false
                    case 21: // true
                        output->type = CTYPE_BOOL;
                        output->value_boolean = out.info - 20;
                        return CERR_OK;
                    case 22: // null
                    case 23: // undefined
                        output->type = CTYPE_NULL;
                        return CERR_OK;
                    case 26: // float
                        output->type = CTYPE_FP32;
                        output->value_fp32 = uint_to_fp32((uint32_t) out.value);
                        return CERR_OK;
                    case 27: // double
                        output->type = CTYPE_FP64;
                        output->value_fp64 = uint_to_fp64(out.value);
                        return CERR_OK;
                    case 31:
                    default:
                        return CERR_INVALID_DATA;
                }
            }
        }
    }

    return result;
}

int connie_reader_next(struct connie_reader *reader, struct connie_output *output)
{
    int result = connie_reader_iterate(reader, output);
    if (result != CERR_OK && result != CERR_COMPLETE)
        reader->invalid = 1;
    return result;
}

static void hex_dump(const uint8_t *data, int size)
{
    for (int i = 0; i < size; i++)
        printf("%02X ", data[i]);
}

static const char *TYPES[] =
{
    "uint",
    "sint",
    "bstring",
    "tstring",
    "array",
    "map",
    "tag",
    "simple",
};

int connie_diagnostic(const uint8_t *buffer, size_t size)
{
    if (buffer == NULL || size < EMPTY_DOC_SIZE)
        return CERR_INVALID_ARGUMENT;
    if (buffer[0] != 0xA0) // empty definite-length map
        return CERR_INVALID_DATA;

    struct cbor_iter_output out;
    struct cbor_iter iter;
    iter.ptr = buffer + 1;
    iter.end = buffer + size;

    int result, indent = 0;
    while ((result = cbor_read_next(&iter, &out)) == CERR_OK)
    {
        const uint8_t *ptr = out.ptr;
        for (int i = 0; i < indent; ++i)
            fputs("  ", stdout);
        hex_dump(ptr, 1 + out.ibytes);
        printf(" # %s\n", TYPES[out.type]);
        if (out.info > 0 && (out.type == 2 || out.type == 3))
        {
            indent++;
            for (int i = 0; i < indent; ++i)
                fputs("  ", stdout);
            hex_dump(ptr + 1 + out.ibytes, out.info);
            putc('\n', stdout);
            indent--;
        }
        if (out.ptr[0] == 0xFF)
            indent--;
        else if (out.info == 31)
            indent++;
    }

    return result;
}