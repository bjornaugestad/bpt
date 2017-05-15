/* nitpick source code. Good to use prior to commits */

/* Features:
 * a) Remove trailing ws at end-of-line
 * b) Remove trailing ws at end-of-file
 * c) Replace tabs with spaces at start of line (1 tab == 4 spaces)
 * d) Replace \r\n with \n
 * e) Replace [[:ws:]]\n with \n
 * f) More to come. (Maybe add regex support for e.g. s/return (\(.*\));/return \1;/  )
 * g) Never more than two consecutive blank lines.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>
#include <regex.h>
#include <assert.h>
#include <ctype.h>

static int fix_newlines = 0;
static int fix_return = 0;
static int fix_keywords = 0;
static int fix_tabs = 0;
static int fix_end_of_file = 0;
static int fix_ws = 0;

static void handle_return(char *buf, size_t bufsize, regmatch_t *pmatch, size_t nmatch)
{
    size_t n;
    char *tmp, *s;

    assert(nmatch > 3);
    assert(pmatch[1].rm_so != -1);
    assert(pmatch[2].rm_so != -1);
    assert(pmatch[3].rm_so != -1);

    if (!fix_return)
        return;

    if ( (s = tmp = malloc(bufsize)) == NULL)
        abort();

    n = pmatch[1].rm_eo - pmatch[1].rm_so;
    memcpy(s, buf + pmatch[1].rm_so, n);
    s += n;

    // Handle return(foo); by inserting a space if needed.
    if (!isspace(buf[pmatch[2].rm_so])
    && !isspace(buf[pmatch[1].rm_eo - pmatch[1].rm_so - 1]))
        *s++ = ' ';

    n = pmatch[2].rm_eo - pmatch[2].rm_so;
    memcpy(s, buf + pmatch[2].rm_so, n);
    s += n;

    n = pmatch[3].rm_eo - pmatch[3].rm_so;
    memcpy(s, buf + pmatch[3].rm_so, n);
    s += n;
    *s = '\0';

    memcpy(buf, tmp, bufsize);
    free(tmp);
}

static void handle_keyword_space(char *buf, size_t bufsize, regmatch_t *pmatch, size_t nmatch)
{
    size_t n;
    char *tmp, *s;

    assert(nmatch > 3);
    assert(pmatch[1].rm_so != -1);
    assert(pmatch[2].rm_so != -1);
    assert(pmatch[3].rm_so != -1);

    if (!fix_keywords)
        return;

    if ( (s = tmp = malloc(bufsize)) == NULL)
        abort();

    n = pmatch[1].rm_eo - pmatch[1].rm_so;
    memcpy(s, buf + pmatch[1].rm_so, n);
    s += n;

    n = pmatch[2].rm_eo - pmatch[2].rm_so;
    memcpy(s, buf + pmatch[2].rm_so, n);
    s += n;

    // Add the missing space.
    *s++ = ' ';

    n = pmatch[3].rm_eo - pmatch[3].rm_so;
    memcpy(s, buf + pmatch[3].rm_so, n);
    s += n;
    *s = '\0';

    memcpy(buf, tmp, bufsize);
    free(tmp);
}

/*
 * Struct contains a regular expression in text format, a
 * precompiled expression, and a handler function called whenever
 * regexec() finds 1..n matches.
 */
struct remap {
    const char *re;
    regex_t preg;
    void (*handler)(char *buf, size_t bufsize, regmatch_t *pmatch, size_t nmatch);
} map[] = {
    { .re = "^([ \t]*return)[ ]*\\(([^\\(]*)\\)([ ]*;.*)", .handler = handle_return },
    { .re = "^([ \t]*)(for|if|while|switch)(\\(.*)", .handler = handle_keyword_space },
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

static void exec_re(char *buf, size_t bufsize)
{
    size_t i, n = sizeof map / sizeof *map;
    regmatch_t matches[24];
    int eflags = 0;
    size_t nmatches = sizeof matches / sizeof *matches;

    for (i = 0; i < n; i++) {
        if (regexec(&map[i].preg, buf, nmatches, matches, eflags) == 0)
            map[i].handler(buf, bufsize, matches, nmatches);
    }
}

static void remove_trailing_ws(char *s)
{
    size_t i;

    i = strlen(s);

    if (!fix_ws) {
        /* Just remove the \n so that the later fprintf() doesn't 
         * print two newlines. */
        if (i-- && s[i] == '\n')
            s[i] = '\0';
    }
    else {
        while (i-- && isspace(s[i]))
            s[i] = '\0';
    }
}

static void handle_tabs(char *buf, size_t bufsize)
{
    size_t n;
    char *s = buf;

    if (!fix_tabs)
        return;

    while (*s != '\0' && isspace(*s)) {
        if (*s == '\t') {
            // Replace the tab with 4 spaces.
            // First, check for available space.
            if (strlen(buf) + 4 >= bufsize)
                break;

            // move rest of string 4 bytes to the right
            // Note that we also copy the zero-terminator.
            n = strlen(s);
            memmove(s + 4, s + 1, n);

            // Then insert 4 spaces
            *s++ = ' ';
            *s++ = ' ';
            *s++ = ' ';
            *s   = ' ';
        }

        s++;
    }
}

static void process_line(char *buf, size_t bufsize)
{
    remove_trailing_ws(buf);
    handle_tabs(buf, bufsize);
    exec_re(buf, bufsize);
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
    FILE *in, *out;
    const char *tempfile = tempnam(".", "nitp");

    char line[10240];
    size_t n, nblanklines = 0;

    if ( (in = fopen(filename, "r")) == NULL) {
        perror(filename);
        return;
    }
    else if ( (out = fopen(tempfile, "w")) == NULL) {
        perror(tempfile);
        fclose(in);
        return;
    }

    while (fgets(line, sizeof line, in) != NULL) {
        process_line(line, sizeof line);
        n = strlen(line) + 1; /* for the \n */

        // Don't print last line if empty.
        if (n == 1 && end_of_file(in) && fix_end_of_file)
            break;

        if (fix_newlines) {
            // print 3..n blank lines as two blank lines
            // by not printing more than 2 blank lines.
            if (n == 1)
                nblanklines++;
            else
                nblanklines = 0;

            if (nblanklines > 2)
                continue;
        }

        if (fprintf(out, "%s\n", line) != (int)n) {
            perror("fprintf");
            fclose(in);
            fclose(out);
            unlink(tempfile);
            exit(EXIT_FAILURE);
        }
    }

    fclose(in);
    fclose(out);
    if (rename(tempfile, filename) == -1) {
        perror("rename");
        exit(EXIT_FAILURE);
    }
}

static void usage(const char *name)
{
    const char *text = "usage: %s opts... file...\n"
        "opts are [nrktea]\n"
        "-w Fix trailing whitespace\n"
        "-n Fix consecutive newlines\n"
        "-r Fix return-statements with ()\n"
        "-k Add spaces after keywords\n"
        "-t Replace one TAB with four spaces\n"
        "-e Fix blank line at end of file\n"
        "-a all of the above\n";

    printf(text, name);
}

static void parse_commandline(int argc, char *argv[])
{
    const char *opts = "wtknraeh";
    int c;

    while ( (c = getopt(argc, argv, opts)) != EOF) {
        switch (c) {
            case 'w':
                fix_ws = 1;
                break;

            case 'n':
                fix_newlines = 1;
                break;

            case 'r':
                fix_return = 1;
                break;

            case 'k':
                fix_keywords = 1;
                break;

            case 't':
                fix_tabs = 1;
                break;

            case 'h':
                usage(argv[0]);
                exit(0);

            case 'e':
                fix_end_of_file = 1;
                break;

            case 'a':
                fix_newlines = 1;
                fix_return = 1;
                fix_keywords = 1;
                fix_tabs = 1;
                fix_end_of_file = 1;
                fix_ws = 1;
                break;

            default:
                usage(argv[0]);
                exit(1);
        }
    }

    if (!fix_newlines
    && !fix_return
    && !fix_keywords
    && !fix_tabs
    && !fix_ws
    && !fix_end_of_file) {
        usage(argv[0]);
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    (void)argc;

    compile_re();
    parse_commandline(argc, argv);

    while (argv[optind] != NULL)
        process_file(argv[optind++]);

    free_re();
    return 0;
}
