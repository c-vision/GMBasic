/* FILE: native/gmb_runtime_c/tests/gmb_grep_selfcheck.c — esteso al Matcher SIMD/DFA. */
#include "gmb_grep.h"
#include "gmb_matcher.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); g_fail = 1; } } while (0)

static void write_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}
static gmb_val_t run(gmb_val_t opts, const char* path) { return gmb_grep_search(opts, path); }

int main(void) {
    CHECK(gmb_matcher_selftest() == 0, "matcher SIMD/DFA selftest");
    CHECK(gmb_grep_selftest() == 1, "selftest regex [0-9]+ su abc123");

    system("rm -rf /tmp/gmb_grep_sc && mkdir -p /tmp/gmb_grep_sc/sub");
    write_file("/tmp/gmb_grep_sc/a.txt", "hello world\nfoo BAR baz\n42 units\n");
    write_file("/tmp/gmb_grep_sc/b.c", "int main(void) { return 42; }\n// BAR comment\n");
    write_file("/tmp/gmb_grep_sc/sub/c.txt", "another line\nBAR again\n");

    /* 1. ignore-case ricorsivo su "bar" → 3 hit */
    {
        gmb_dict_t o = gmb_dict_new();
        gmb_dict_set(&o, "pattern", gmb_make_val_string_counted("bar"));
        gmb_dict_set(&o, "ignore_case", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "recursive", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "line_number", gmb_make_val_bool(-1));
        gmb_val_t hits = run(gmb_make_val_dict(o), "/tmp/gmb_grep_sc");
        CHECK(gmb_grep_count(hits) == 3, "ricorsivo ignore-case 'bar' = 3 hit");
    }
    /* 2. type "c" → solo b.c, pattern 42 → 1 hit */
    {
        gmb_dict_t o = gmb_dict_new();
        gmb_dict_set(&o, "pattern", gmb_make_val_string_counted("42"));
        gmb_dict_set(&o, "recursive", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "type", gmb_make_val_string_counted("c"));
        CHECK(gmb_grep_count(run(gmb_make_val_dict(o), "/tmp/gmb_grep_sc")) == 1, "type 'c' pattern 42 = 1 hit");
    }
    /* 3. exclude "*.c" pattern 42 → solo a.txt */
    {
        gmb_dict_t o = gmb_dict_new();
        gmb_dict_set(&o, "pattern", gmb_make_val_string_counted("42"));
        gmb_dict_set(&o, "recursive", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "exclude", gmb_make_val_string_counted("*.c"));
        CHECK(gmb_grep_count(run(gmb_make_val_dict(o), "/tmp/gmb_grep_sc")) == 1, "exclude '*.c' pattern 42 = 1 hit");
    }
    /* 4. files-with-matches */
    {
        gmb_dict_t o = gmb_dict_new();
        gmb_dict_set(&o, "pattern", gmb_make_val_string_counted("bar"));
        gmb_dict_set(&o, "ignore_case", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "recursive", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "files_with_matches", gmb_make_val_bool(-1));
        CHECK(gmb_grep_count(run(gmb_make_val_dict(o), "/tmp/gmb_grep_sc")) == 3, "files-with-matches 'bar' = 3 file");
    }
    /* 5. invert */
    {
        gmb_dict_t o = gmb_dict_new();
        gmb_dict_set(&o, "pattern", gmb_make_val_string_counted("BAR"));
        gmb_dict_set(&o, "ignore_case", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "invert", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "recursive", gmb_make_val_bool(-1));
        CHECK(gmb_grep_count(run(gmb_make_val_dict(o), "/tmp/gmb_grep_sc")) == 4, "invert 'BAR' = 4 righe non-matching");
    }
    /* 6. format file:line:text */
    {
        gmb_dict_t o = gmb_dict_new();
        gmb_dict_set(&o, "pattern", gmb_make_val_string_counted("hello"));
        gmb_val_t hits = run(gmb_make_val_dict(o), "/tmp/gmb_grep_sc/a.txt");
        gmb_string_t s = gmb_grep_format(hits, 1, 0);
        CHECK(strstr(s.data ? s.data : "", "a.txt:1:hello world") != NULL, "format file:line:text");
    }
    /* 7. fixed-string (fast path SIMD/DFA) su file */
    {
        gmb_dict_t o = gmb_dict_new();
        gmb_dict_set(&o, "pattern", gmb_make_val_string_counted("BAR"));
        gmb_dict_set(&o, "fixed", gmb_make_val_bool(-1));
        gmb_dict_set(&o, "recursive", gmb_make_val_bool(-1));
        CHECK(gmb_grep_count(run(gmb_make_val_dict(o), "/tmp/gmb_grep_sc")) == 3, "fixed 'BAR' ricorsivo = 3 hit");
    }

    system("rm -rf /tmp/gmb_grep_sc");
    if (g_fail) return 1;
    puts("gmb_grep_selfcheck OK");
    return 0;
}
