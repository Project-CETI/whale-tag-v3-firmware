/**
 * @file   csv_spec_assert.h
 * @brief  Reusable helpers and Unity assertions for verifying CSV output
 *         against the data dictionary spec (via spec_reader.h).
 *
 * Include this header in any test file that validates CSV logging code
 * against the data dictionary.  It depends on Unity and spec_reader.h.
 *
 * Usage:
 *   #include "../../spec_reader.h"
 *   #include "../../csv_spec_assert.h"
 *
 *   // In tests:
 *   csv_assert_header_field_names(header_str, &spec);
 *   csv_assert_line_field_count(csv_line, len, &spec);
 */
#ifndef CSV_SPEC_ASSERT_H
#define CSV_SPEC_ASSERT_H

#include <unity.h>
#include <string.h>
#include <stdio.h>

/* ---- Helpers ------------------------------------------------------------ */

/// @brief Count occurrences of character c in string s.
static inline int csv_count_char(const char *s, char c) {
    int n = 0;
    while (*s) { if (*s == c) n++; s++; }
    return n;
}

/// @brief Split a delimited line into trimmed fields (modifies input in-place).
/// @param line  The line to split (will be modified — NUL-terminated tokens).
/// @param sep   The field separator character (e.g. ',' or ';').
/// @param fields  Array of char* to receive pointers into line.
/// @param max_fields  Maximum number of fields to parse.
/// @return Number of fields found.
static inline int csv_split_fields(char *line, char sep,
                                   char *fields[], int max_fields) {
    int n = 0;
    char *p = line;
    while (n < max_fields && *p) {
        while (*p == ' ') p++;
        fields[n++] = p;
        char *delim = strchr(p, sep);
        if (!delim) break;
        *delim = '\0';
        p = delim + 1;
    }
    /* trim trailing whitespace/newline from last field */
    if (n > 0) {
        char *last = fields[n - 1];
        size_t len = strlen(last);
        while (len > 0 && (last[len - 1] == '\n' || last[len - 1] == '\r'
                           || last[len - 1] == ' '))
            last[--len] = '\0';
    }
    return n;
}

/// @brief Find a SpecField by name.  Returns NULL if not found.
static inline const SpecField *csv_find_spec_field(const FileSpec *spec,
                                                   const char *name) {
    for (int i = 0; i < spec->num_fields; i++) {
        if (strcmp(spec->fields[i].name, name) == 0)
            return &spec->fields[i];
    }
    return NULL;
}

/// @brief Return pointer to the first non-comment line in the header.
static inline const char *csv_skip_comment_lines(const char *header,
                                                 const char *prefix) {
    size_t plen = strlen(prefix);
    const char *p = header;
    while (strncmp(p, prefix, plen) == 0) {
        const char *nl = strchr(p, '\n');
        if (!nl) return p;
        p = nl + 1;
    }
    return p;
}

/* ---- Assertions --------------------------------------------------------- */

/// @brief Assert the header string ends with a newline.
static inline void csv_assert_header_ends_with_newline(const char *header) {
    size_t len = strlen(header);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, len, "header is empty");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('\n', header[len - 1],
        "header must end with newline");
}

/// @brief Assert header starts with "# version: N\n" matching the spec.
static inline void csv_assert_version_comment(const char *header,
                                              const FileSpec *spec) {
    char expected[64];
    snprintf(expected, sizeof(expected), "%s version: %d\n",
             spec->comment_prefix, spec->format_version);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0,
        strncmp(header, expected, strlen(expected)),
        "header version comment does not match spec");
}

/// @brief Assert the number of columns in the header matches the spec.
static inline void csv_assert_header_field_count(const char *header,
                                                 const FileSpec *spec) {
    const char *columns = csv_skip_comment_lines(header, spec->comment_prefix);
    int delimiters = csv_count_char(columns, spec->separator[0]);
    /* one newline is counted only if separator == '\n', which it won't be */
    TEST_ASSERT_EQUAL_INT_MESSAGE(spec->num_fields, delimiters + 1,
        "header field count does not match spec");
}

/// @brief Assert each column name in the header matches the spec, in order.
static inline void csv_assert_header_field_names(const char *header,
                                                 const FileSpec *spec) {
    const char *columns = csv_skip_comment_lines(header, spec->comment_prefix);
    char copy[512];
    strncpy(copy, columns, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';

    char *fields[SPEC_MAX_FIELDS];
    int n = csv_split_fields(copy, spec->separator[0], fields, SPEC_MAX_FIELDS);

    TEST_ASSERT_EQUAL_INT_MESSAGE(spec->num_fields, n,
        "header field count does not match spec");

    for (int i = 0; i < n && i < spec->num_fields; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "field %d name mismatch", i);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(spec->fields[i].name, fields[i], msg);
    }
}

/// @brief Assert a CSV data line has the correct number of fields.
static inline void csv_assert_line_field_count(const char *csv_line,
                                               size_t len,
                                               const FileSpec *spec) {
    (void)len;
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        spec->num_fields - 1,
        csv_count_char(csv_line, spec->separator[0]),
        "CSV line field count does not match spec");
}

/// @brief Assert a CSV data line ends with exactly one newline.
static inline void csv_assert_line_ends_with_newline(const char *csv_line,
                                                     size_t len) {
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, len, "CSV line is empty");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('\n', csv_line[len - 1],
        "CSV line must end with newline");
}

/// @brief Assert a CSV data line contains no embedded newlines (only one at end).
static inline void csv_assert_line_no_embedded_newlines(const char *csv_line,
                                                        size_t len) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, csv_count_char(csv_line, '\n'),
        "CSV line must contain exactly one newline (at end)");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('\n', csv_line[len - 1],
        "the single newline must be at the end");
}

/// @brief Assert a formatted value appears in the CSV line.
///        Formats `value` with the spec field's format string
///        and checks that it appears as a substring.
static inline void csv_assert_field_format(const char *csv_line,
                                           const FileSpec *spec,
                                           const char *field_name,
                                           double value) {
    const SpecField *sf = csv_find_spec_field(spec, field_name);
    char msg[128];
    snprintf(msg, sizeof(msg), "'%s' field not found in spec", field_name);
    TEST_ASSERT_NOT_NULL_MESSAGE(sf, msg);

    snprintf(msg, sizeof(msg), "'%s' has no format in spec", field_name);
    TEST_ASSERT_TRUE_MESSAGE(strlen(sf->format) > 0, msg);

    char expected[64];
    snprintf(expected, sizeof(expected), sf->format, value);

    snprintf(msg, sizeof(msg),
             "'%s' value \"%s\" not found in CSV line", field_name, expected);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(csv_line, expected), msg);
}

/// @brief Assert a string token appears in the Notes field of a CSV line.
///        Also verifies the token is a known possible_value in the spec.
static inline void csv_assert_note_present(const char *csv_line,
                                           const FileSpec *spec,
                                           const char *note_token) {
    const SpecField *nf = csv_find_spec_field(spec, "Notes");
    TEST_ASSERT_NOT_NULL_MESSAGE(nf, "Notes field not found in spec");

    /* verify the token is a known possible_value */
    int found = 0;
    for (int i = 0; i < nf->num_possible_values; i++) {
        if (strcmp(nf->possible_values[i], note_token) == 0) {
            found = 1;
            break;
        }
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "'%s' not listed as a possible_value in spec",
             note_token);
    TEST_ASSERT_TRUE_MESSAGE(found, msg);

    snprintf(msg, sizeof(msg), "CSV line does not contain note '%s'",
             note_token);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(csv_line, note_token), msg);
}

/// @brief Assert the Notes field is empty for a clean sample.
///        Assumes Notes is the second field (index 1).
static inline void csv_assert_notes_empty(const char *csv_line,
                                          const FileSpec *spec) {
    char copy[512];
    strncpy(copy, csv_line, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';

    char *fields[SPEC_MAX_FIELDS];
    int n = csv_split_fields(copy, spec->separator[0], fields, SPEC_MAX_FIELDS);
    TEST_ASSERT_TRUE_MESSAGE(n >= 2, "CSV line has fewer than 2 fields");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", fields[1],
        "Notes field should be empty for a clean sample");
}

/// @brief Assert a field's value falls within the spec's declared range.
static inline void csv_assert_field_in_range(const FileSpec *spec,
                                             const char *field_name,
                                             double value,
                                             double tolerance) {
    const SpecField *sf = csv_find_spec_field(spec, field_name);
    char msg[128];
    snprintf(msg, sizeof(msg), "'%s' field not found in spec", field_name);
    TEST_ASSERT_NOT_NULL_MESSAGE(sf, msg);

    snprintf(msg, sizeof(msg), "'%s' has no range in spec", field_name);
    TEST_ASSERT_TRUE_MESSAGE(sf->has_range, msg);

    snprintf(msg, sizeof(msg), "'%s' value %f below range min %f",
             field_name, value, sf->range_min);
    TEST_ASSERT_TRUE_MESSAGE(value >= sf->range_min - tolerance, msg);

    snprintf(msg, sizeof(msg), "'%s' value %f above range max %f",
             field_name, value, sf->range_max);
    TEST_ASSERT_TRUE_MESSAGE(value <= sf->range_max + tolerance, msg);
}

#endif /* CSV_SPEC_ASSERT_H */
