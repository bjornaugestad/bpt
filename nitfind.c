/* Locate nits in source code. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>
#include <regex.h>
#include <assert.h>

/*
 * Struct contains a regular expression in text format, a
 * precompiled expression, and a handler function called whenever
 * regexec() finds 1..n matches.
 */
struct remap {
    const char *re;
    regex_t preg;
    const char *msg;
} map[] = {
    { .re = "^[ ]*return[ ]*\\([^\\(]*\\)[ ]*;.*", .msg = "Redundant parentheses" },
    { .re = "^([ ]*)(for|if|while|switch)(\\(.*)", .msg = "Missing space after keyword" },

#define OPERATORS "(==|!=|\\+=|-=|%=|<<|<<=|>>|>>=|\\|=|&=|\\|\\||&&)"

    { .re = "[^ ]" OPERATORS, .msg = "Missing space before operator" },
    { .re = OPERATORS "[A-Za-z_0-9]", .msg = "Missing space after operator" },
    { .re = "[ \t]$", .msg = "Trailing white-space" },
};

static void compile_re(void)
{
    size_t i, n = sizeof map / sizeof *map;
    int cflags = REG_EXTENDED;

    for (i = 0; i < n; i++) {
        if (regcomp(&map[i].preg, map[i].re, cflags) != 0) {
            fprintf(stderr, "Error compiling %s\n", map[i].re);
            exit(EXIT_FAILURE);
        }
    }
}

static void free_re(void)
{
    size_t i, n = sizeof map / sizeof *map;

    for (i = 0; i < n; i++)
        regfree(&map[i].preg);
}

static void exec_re(char *buf, const char *file, int line)
{
    size_t i, n = sizeof map / sizeof *map;
    regmatch_t matches[24];
    int eflags = 0;
    size_t nmatches = sizeof matches / sizeof *matches;

    for (i = 0; i < n; i++) {
        if (regexec(&map[i].preg, buf, nmatches, matches, eflags) == 0)
            fprintf(stderr, "%s(%d): %s\n", file, line, map[i].msg);
    }
}

static int end_of_file(FILE *f)
{
    int c = fgetc(f);
    if (c == EOF)
        return 1;

    ungetc(c, f);
    return 0;
}

static void process_file(const char *filename)
{
    FILE *in;
    char line[10240];
    size_t n, nblanklines = 0;
    int lineno = 0;

    if ( (in = fopen(filename, "r")) == NULL) {
        perror(filename);
        return;
    }
    while (fgets(line, sizeof line, in) != NULL) {
        lineno++;
        exec_re(line, filename, lineno);
        n = strlen(line);

        if (n == 0 && end_of_file(in))
            fprintf(stderr, "%s(%d):Blank line at end of file.\n", filename, lineno);

        // warn about too many blank lines.
        // by not printing more than 2 blank lines.
        if (n == 0)
            nblanklines++;
        else
            nblanklines = 0;

        if (nblanklines > 2)
            fprintf(stderr, "%s(%d):More than two consecutive blank lines\n", filename, lineno);
    }

    fclose(in);
}

int main(int argc, char *argv[])
{
    (void)argc;

    compile_re();

    while (*++argv != NULL)
        process_file(*argv);

    free_re();
    return 0;
}
