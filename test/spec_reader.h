/**
 * @file   spec_reader.h
 * @brief  Parse tag_data_formats.yaml and expose file/field specs for tests.
 *
 * This is a minimal, purpose-built YAML subset parser — it only handles the
 * specific structure used in tag_data_formats.yaml.  It is NOT a
 * general-purpose YAML library.
 *
 * Usage:
 *   FileSpec spec;
 *   if (spec_parse("data_pressure.csv", &spec) != 0) { ... error ... }
 *   // spec.num_fields, spec.fields[i].name, etc. are now populated
 */
#ifndef SPEC_READER_H
#define SPEC_READER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Override SPEC_FILE before including this header to change the path */
#ifndef SPEC_FILE
#define SPEC_FILE "data_dictionary.yaml"
#endif

#define SPEC_MAX_FIELDS       16
#define SPEC_MAX_FIELD_NAME   64
#define SPEC_MAX_FORMAT       16
#define SPEC_MAX_NOTE_VALUES  8
#define SPEC_MAX_NOTE_LEN     32

typedef struct {
    char name[SPEC_MAX_FIELD_NAME];
    char format[SPEC_MAX_FORMAT];
    double range_min;
    double range_max;
    int has_range;
    char possible_values[SPEC_MAX_NOTE_VALUES][SPEC_MAX_NOTE_LEN];
    int num_possible_values;
} SpecField;

#define SPEC_MAX_COMMENT_PREFIX 8
#define SPEC_MAX_SEPARATOR      4

typedef struct {
    int format_version;
    char comment_prefix[SPEC_MAX_COMMENT_PREFIX];
    char separator[SPEC_MAX_SEPARATOR];
    int num_fields;
    SpecField fields[SPEC_MAX_FIELDS];
} FileSpec;

/* ---- internal helpers --------------------------------------------------- */

static inline int spec__indent(const char *line) {
    int n = 0;
    while (line[n] == ' ') n++;
    return n;
}

static inline int spec__extract_quoted(const char *line, const char *key,
                                       char *out, size_t out_sz) {
    const char *k = strstr(line, key);
    if (!k) return 0;
    const char *q1 = strchr(k, '"');
    if (!q1) return 0;
    const char *q2 = strchr(q1 + 1, '"');
    if (!q2) return 0;
    size_t len = (size_t)(q2 - q1 - 1);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, q1 + 1, len);
    out[len] = '\0';
    return 1;
}

static inline int spec__extract_int(const char *line, const char *key, int *out) {
    const char *k = strstr(line, key);
    if (!k) return 0;
    const char *colon = strchr(k, ':');
    if (!colon) return 0;
    *out = atoi(colon + 1);
    return 1;
}

static inline int spec__extract_double(const char *line, const char *key, double *out) {
    const char *k = strstr(line, key);
    if (!k) return 0;
    const char *colon = strchr(k, ':');
    if (!colon) return 0;
    *out = atof(colon + 1);
    return 1;
}

/* ---- public API --------------------------------------------------------- */

/// @brief Parse SPEC_FILE and populate a FileSpec for the named file entry.
/// @param filename  The "name" value to search for (e.g. "data_pressure.csv")
/// @param spec      Output struct — zeroed and populated on success
/// @return 0 on success, -1 if the file could not be opened or the entry was
///         not found
static int spec_parse(const char *filename, FileSpec *spec) {
    memset(spec, 0, sizeof(*spec));
    spec->separator[0] = ',';  /* default separator */

    FILE *f = fopen(SPEC_FILE, "r");
    if (!f) {
        fprintf(stderr, "spec_parse: could not open %s\n", SPEC_FILE);
        return -1;
    }

    char line[512];
    int found_file = 0;
    int in_fields = 0;
    int in_range = 0;
    int in_possible_values = 0;
    int file_indent = -1;
    int field_indent = -1;
    SpecField *cur_field = NULL;

    while (fgets(line, sizeof(line), f)) {
        int indent = spec__indent(line);
        char *trimmed = line + indent;

        /* skip blanks / comments */
        if (trimmed[0] == '\n' || trimmed[0] == '#' || trimmed[0] == '\0')
            continue;

        /* --- locate the file entry by name --- */
        if (!found_file) {
            char name_buf[128];
            if (strstr(trimmed, "- name:") &&
                spec__extract_quoted(trimmed, "name:", name_buf, sizeof(name_buf))) {
                if (strcmp(name_buf, filename) == 0) {
                    found_file = 1;
                    file_indent = indent;
                }
            }
            continue;
        }

        /* Left this file entry — either a new list item or a top-level key
           (e.g. "types:") at or below the file's indent level. */
        if (indent <= file_indent)
            break;

        /* --- file-level keys --- */
        if (!in_fields && !in_range && !in_possible_values) {
            spec__extract_int(trimmed, "file_format_version:", &spec->format_version);
            spec__extract_quoted(trimmed, "comment_prefix:", spec->comment_prefix,
                                 sizeof(spec->comment_prefix));
            spec__extract_quoted(trimmed, "separator:", spec->separator,
                                 sizeof(spec->separator));
            if (strstr(trimmed, "fields:")) {
                in_fields = 1;
                continue;
            }
            continue;
        }

        /* --- inside fields list --- */
        if (in_fields) {
            /* new field item */
            if (strstr(trimmed, "- name:")) {
                in_range = 0;
                in_possible_values = 0;
                field_indent = indent;
                if (spec->num_fields < SPEC_MAX_FIELDS) {
                    cur_field = &spec->fields[spec->num_fields++];
                    memset(cur_field, 0, sizeof(*cur_field));
                    spec__extract_quoted(trimmed, "name:", cur_field->name,
                                         sizeof(cur_field->name));
                }
                continue;
            }

            /* exited the fields list? */
            if (field_indent >= 0 && indent <= file_indent + 4 &&
                trimmed[0] != '-') {
                in_fields = 0;
                in_range = 0;
                in_possible_values = 0;
                continue;
            }

            if (!cur_field) continue;

            /* field-level keys */
            if (strstr(trimmed, "format:")) {
                spec__extract_quoted(trimmed, "format:", cur_field->format,
                                     sizeof(cur_field->format));
                continue;
            }
            if (strstr(trimmed, "range:")) {
                in_range = 1;
                in_possible_values = 0;
                continue;
            }
            if (strstr(trimmed, "possible_values:")) {
                in_possible_values = 1;
                in_range = 0;
                continue;
            }

            if (in_range) {
                spec__extract_double(trimmed, "min:", &cur_field->range_min);
                if (spec__extract_double(trimmed, "max:", &cur_field->range_max))
                    cur_field->has_range = 1;
                continue;
            }

            if (in_possible_values) {
                if (strstr(trimmed, "value:")) {
                    if (cur_field->num_possible_values < SPEC_MAX_NOTE_VALUES) {
                        char *dest = cur_field->possible_values[cur_field->num_possible_values++];
                        if (!spec__extract_quoted(trimmed, "value:", dest, SPEC_MAX_NOTE_LEN))
                            dest[0] = '\0'; /* empty string value */
                    }
                }
                continue;
            }
        }
    }

    fclose(f);
    return found_file ? 0 : -1;
}

#endif /* SPEC_READER_H */
