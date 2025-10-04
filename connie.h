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
#ifndef CONNIE_H
#define CONNIE_H

#include <stdint.h>
#include <stddef.h>

#define CTYPE_UNKNOWN     0
#define CTYPE_STRING      1
#define CTYPE_INT32       2
#define CTYPE_INT64       3
#define CTYPE_UINT32      4
#define CTYPE_UINT64      5
#define CTYPE_BOOL        6
#define CTYPE_NULL        7
#define CTYPE_MAP_OPEN    8
#define CTYPE_MAP_CLOSE   9
#define CTYPE_ARRAY_OPEN  10
#define CTYPE_ARRAY_CLOSE 11
#define CTYPE_BYTES       12
#define CTYPE_FP32        13
#define CTYPE_FP64        14

#define CKEY_UNKNOWN      0
#define CKEY_UINT         1
#define CKEY_STRING       2

#define CERR_OK                0
#define CERR_COMPLETE         (-1)
#define CERR_INVALID_ARGUMENT (-2)
#define CERR_INVALID_DATA     (-3)
#define CERR_INVALID_STATE    (-4)
#define CERR_INVALID_KEY      (-5)
#define CERR_DEPTH_OVERFLOW   (-6)
#define CERR_DEPTH_UNDERFLOW  (-7)
#define CERR_OUT_OF_BOUNDS    (-8)

#define CLIMITS_DEPTH 64

#ifdef _cplusplus
extern "C" {
#endif

typedef void doc_commit_callback(const uint8_t *buffer, size_t size, void *data);

struct connie_writer_params {
    // Pointer to the memory buffer where CBOR data will be written.
    uint8_t *buffer;
    // Size of the buffer in bytes.
    size_t buffer_size;
    uint8_t key_type;
    // Callback to deliver serialized data.
    doc_commit_callback *callback;
};

struct connie_writer {
    uint8_t scope[CLIMITS_DEPTH]; // CTYPE_MAP or CTYPE_ARRAY
    struct connie_writer_params params;
    uint8_t *begin;
    uint8_t *end;
    uint8_t *ptr;
    uint32_t size;
    uint8_t scope_index;
    uint8_t flags;
    uint8_t key_type;
};

struct cbor_iter {
    const uint8_t *ptr;
    const uint8_t *end;
};

struct connie_reader {
    struct cbor_iter iter;
    uint8_t scope[CLIMITS_DEPTH]; // CTYPE_MAP or CTYPE_ARRAY
    uint16_t scope_count : 6;
    uint16_t invalid : 1;
    uint16_t complete : 1;
    uint16_t key_type : 2;
    uint16_t reserved : 6;
};

struct connie_output {
    union {
        const char *key_string;
        uint32_t key_uint32;
    };
    union {
        const char *value_string;
        const uint8_t *value_bytes;
        int32_t value_boolean;
        int32_t value_int32;
        int64_t value_int64;
        uint32_t value_uint32;
        uint64_t value_uint64;
        float value_fp32;
        double value_fp64;
    };
    // Length for strings and bytes
    size_t length;
    uint8_t type : 4;
    uint8_t key_type : 2;
    uint8_t reserved : 2;
};

/**
 * Initializes a document writer with a user-provided buffer.
 *
 * @param writer Pointer to the connie_writer structure to initialize.
 * @param buffer Pointer to the memory buffer where CBOR data will be written.
 * @param size   Size of the buffer in bytes.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_init(struct connie_writer *writer, uint8_t *buffer, size_t size, uint8_t key_type );

/**
 * Finalizes the document and returns the encoded CBOR output.
 *
 * @param writer Pointer to the initialized doc_writer.
 * @param output Optional pointer to receive the address of the encoded CBOR data.
 * @param size   Optional pointer to receive the size of the encoded data.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_finish(struct connie_writer *writer, uint8_t **output, size_t *size);

/**
 * Writes a key-value pair with a UTF-8 string value.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param value  Value as a null-terminated UTF-8 string.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_string(struct connie_writer *writer, uint32_t key1, const char *key2, const char *value);

/**
 * Writes a key-value pair with a signed 32-bit integer value.
 *
 * Internally, integer values are encoded using the minimal number of bits possible, so
 * there's no practical difference between this function and its 64-bit variant.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param value  Signed 32-bit integer value.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_int32(struct connie_writer *writer, uint32_t key1, const char *key2, int32_t value);

/**
 * Writes a key-value pair with a signed 64-bit integer value.
 *
 * Internally, integer values are encoded using the minimal number of bits possible, so
 * there's no practical difference between this function and its 64-bit variant.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param value  Signed 64-bit integer value.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_int64(struct connie_writer *writer, uint32_t key1, const char *key2, int64_t value);

/**
 * Writes a key-value pair with an unsigned 32-bit integer value.
 *
 * Internally, integer values are encoded using the minimal number of bits possible, so
 * there's no practical difference between this function and its 32-bit variant.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param value  Unsigned 32-bit integer value.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_uint32(struct connie_writer *writer, uint32_t key1, const char *key2, uint32_t value);

/**
 * Writes a key-value pair with an unsigned 64-bit integer value.
 *
 * Internally, integer values are encoded using the minimal number of bits possible, so
 * there's no practical difference between this function and its 32-bit variant.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param value  Unsigned 64-bit integer value.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_uint64(struct connie_writer *writer, uint32_t key1, const char *key2, uint64_t value);

/**
 * Writes a key-value pair with an IEEE-754 32-bit floating point value.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param value  IEEE-754 32-bit floating point value.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_fp32(struct connie_writer *writer, uint32_t key1, const char *key2, float value);

/**
 * Writes a key-value pair with an IEEE-754 64-bit floating point value.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param value  IEEE-754 64-bit floating point value.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_fp64(struct connie_writer *writer, uint32_t key1, const char *key2, double value);

/**
 * Writes a key-value pair with a binary blob (CBOR byte string).
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param data   Pointer to the binary data.
 * @param length Length of the binary data in bytes.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_bytes(struct connie_writer *writer, uint32_t key1, const char *key2, const uint8_t *data, size_t length);

/**
 * Writes a key with a null value.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_null(struct connie_writer *writer, uint32_t key1, const char *key2);

/**
 * Writes a key-value pair with a boolean value.
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @param value  Boolean value (0 = false, non-zero = true).
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_put_boolean(struct connie_writer *writer, uint32_t key1, const char *key2, int value);

/**
 * Opens a nested map under the given key.
 * Must be closed later with connie_close_map().
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_open_map(struct connie_writer *writer, uint32_t key1, const char *key2);

/**
 * Closes the most recently opened map.
 *
 * @param writer Pointer to the connie_writer.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_close_map(struct connie_writer *writer);

/**
 * Opens an array under the given key.
 * Must be closed later with connie_close_array().
 *
 * @param writer Pointer to the connie_writer.
 * @param key1 Key number. Ignored while writing values in arrays or if string key is selected.
 * @param key2 Key name as a null-terminated string. Ignored while writing values in arrays or if integer key is selected.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_open_array(struct connie_writer *writer, uint32_t key1, const char *key2);

/**
 * Closes the most recently opened array.
 *
 * @param writer Pointer to the connie_writer.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_writer_close_array(struct connie_writer *writer);


/**
 * Initializes a document reader with a CBOR-encoded buffer.
 *
 * Prepares the reader to iterate over top-level key-value entries.
 *
 * @param reader Pointer to the connie_reader structure to initialize.
 * @param buffer Pointer to the CBOR-encoded data buffer.
 * @param size   Size of the buffer in bytes.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_reader_init(struct connie_reader *reader, const uint8_t *buffer, size_t size);

/**
 * Returns the next entry from the document.
 *
 * This function acts as an iterator over the CBOR-encoded data.
 * It returns one parsed item at a time, which may be key-value pair
 * or a structural marker indicating the beginning or end of a subdocument
 * (CBOR map) or array.
 *
 * The output structure will contain either a data value or a signal that
 * a nested structure has started or ended, allowing hierarchical traversal.
 *
 * @param reader Pointer to the initialized connie_reader.
 * @param output Pointer to a connie_output structure to receive the parsed entry.
 *               The structure will indicate the type of item and its associated data.
 * @return DOCERR_OK on success, DOCERR_COMPLETE if no more entries are available
 *     or a DOCERR_* code on failure.
 */
int connie_reader_next(struct connie_reader *reader, struct connie_output *output);

/**
 * Reinitializes the document reader, resetting its internal state.
 *
 * This function allows the reader to restart iteration from the beginning
 * of the document without modifying or reloading the input buffer.
 * It is useful when you need to traverse the same CBOR-encoded structure
 * multiple times without reinitializing the buffer externally.
 *
 * @param reader Pointer to an already initialized connie_reader.
 *               The input buffer and size must remain unchanged.
 * @return DOCERR_OK on success, or a DOCERR_* code on failure.
 */
int connie_reader_reset(struct connie_reader *reader);

int connie_diagnostic(const uint8_t *buffer, size_t size);

#ifdef _cplusplus
}
#endif

#endif // CONNIE_H
