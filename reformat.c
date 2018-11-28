#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>


// Fold here, adjust with -w argument.
size_t g_width = 72;

// How much to indent after a newline
size_t g_indent = 4;

// What separates words in input lines?
// (note that this, plus getopt(), will fuck up quoted strings)
// We need a better parser than getopt.
#define SEPARATORS " \t\r\n\v"

// We're not nazis, so we accept some overflow
#define FUZZYLEN 5

static void rtrim(char *s)
{
    size_t n = strlen(s);

    while (n-- > 0 && isspace(s[n]))
        s[n] = '\0';
}

// Find first non-ws. Then memmove the rest of s to s[0]
static void ltrim(char *s)
{
    size_t off, len;
    
    if ((len = strlen(s)) == 0)
        return;

    if ((off = strspn(s, SEPARATORS)) == 0)
        return;

    // +1 to copy the \0 too.
    memmove(s, &s[off], len - off + 1);
}

static void trim(char *s)
{
    rtrim(s);
    ltrim(s);
}

__attribute__((format(printf,1,2)))
static void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

#if 0
__attribute__((format(printf,1,2)))
static void warning(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
#endif

static int print_verbose_stuff;
#if 0
__attribute__((format(printf,1,2)))
static void verbose(const char *fmt, ...)
{
    va_list ap;

    if (!print_verbose_stuff)
        return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

#endif
static void show_usage(void)
{
    static const char *text[] = {
        "usage: reformat file..."
        "A file is something with very long text lines, like"
        "compiler output or Makefile dependeny stuff.",
        "This program reformats a file's contents and prints the result to stdout.",
        "It's like the fold program, but tries to be context sensitive",
        "when folding. -I some/long/path goes on the same line.",
        "",
        "Also, reformat adds \\ at line ends when it breaks a line,",
        "so that e.g. a command is easily pastable and still runs.",
        "This is nifty when debugging makefiles",

    };

    size_t i, n = sizeof text / sizeof *text;
    for (i = 0; i < n; i++) {
        puts(text[i]);
    }
}

static void parse_commandline(int argc, char *argv[])
{
    int c;
    const char *options = "i:w:hv";

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch (c) {
            case 'h':
                show_usage();
                exit(0);

            case 'v':
                print_verbose_stuff = 1;
                break;

            case 'i':
                if ((g_indent = strtoul(optarg, NULL, 0)) == 0)
                    die("Invalid value for -i argument:%s\n", optarg);
                break;

            case 'w':
                if ((g_width = strtoul(optarg, NULL, 0)) == 0)
                    die("Invalid value for -w argument:%s\n", optarg);
                break;

            case '?':
            default:
                exit(1);
        }
    }
}

// Our patterns for recognizing compiler commands as well as
// finding good places to split stuff.
static const struct {
    unsigned score;

    // When splitting based on pattern, how many arguments
    // do we want to add? For example, -o and -I want 1 arg.
    // while gcc, -c and others want 0. We allow for some fuzziness,
    // where negative numbers have special meaning. (which? Don't know atm)
    int args;
    const char *pattern;
} compiler_map[] = {
    { 1, 0, " gcc " },
    { 1, 0, " g++ " },
    { 1, 0, "-gcc " },
    { 1, 0, "-g++ " },
    { 1, 1, "ccache " },
    { 1, 1, " -I " },
    { 1, 0, " -Werror " },
    { 1, 0, " -std=" },
    { 1, 0, " -c " },
    { 1, 1, " -o " },
};

// Try to locate s in compiler map, and see if it wants
// an argument or not. The match is a bit fuzzy due to
// whitespace.
static int word_wants_arg(const char *s)
{
    size_t i, n = sizeof compiler_map / sizeof *compiler_map;

    for (i = 0; i < n; i++) {
        char pattern[10240];
        strcpy(pattern, compiler_map[i].pattern);
        trim(pattern);

        if (strcmp(pattern, s) == 0)
            return compiler_map[i].args;
    }

    return 0;
}


// Is it a compiler command? How do we figure this out?
// It's simple, we use a couple of patterns to look for.
// We have a score too, to see how good a fit it was.
static int is_compiler_command(const char *s)
{
    unsigned score = 0;
    size_t i, n = sizeof compiler_map / sizeof *compiler_map;

    for (i = 0; i < n; i++) {
        if (strstr(s, compiler_map[i].pattern) != NULL)
            score += compiler_map[i].score;
    }

    return score > 3;
}

// Get one word from src and place it in dest. Die on errors.
// Skip leading ws.
// return number of bytes consumed from s, ws and all.
static size_t get_word(char *dest, size_t destsize, const char *src)
{
    size_t n = 0, ncopied = 0;

    while (isspace(*src)) {
        src++;
        n++;
    }

    // Now copy the characters
    while (*src != '\0' && !isspace(*src) && ncopied < destsize)
        dest[ncopied++] = *src++;

    if (ncopied == destsize)
        die("Destination buffer too small(%zu bytes) for string\n", destsize);

    dest[ncopied] = '\0';
    return n + ncopied;
}

static void indent(void)
{
    size_t i = g_indent;
    while (i--)
        putchar(' ');
}

// Right, so we want to split a compiler command, e. g. something like this:
// ccache /tools/buildroot_mack_929547afd1/bin/arm-buildroot-linux-gnueabihf-g++ -c -o applications/environment-control-daemon/.build/borg/src/mol/alarms/Alarm.o applications/environment-control-daemon/src/mol/alarms/Alarm.cpp -DBMAKE_BUILD=1 -DBMAKE_PROJECT_BORG -fPIC -Wno-psabi -I. -I =/usr/include/glib-2.0 -I =/usr/lib/glib-2.0/include -I =/usr/include/gio-unix-2.0 -I =/usr/include/gio-unix-2.0/ -I =/usr/include/glib-2.0 -I =/usr/lib/glib-2.0/include -I =/usr/include -pthread -I libraries/illumination-service/.build/borg/src -I libraries/system-identification-service/.build/borg/src -I libraries/application-framework/.build/borg/src -I libraries/configuration/src -I libraries/application-framework/src -I libraries/alarm-service/src -I libraries/console/src -I libraries/environment-control-service/.build/borg/src -I libraries/hal/.build/borg -I libraries/system-identification-service/src -I applications/environment-control-daemon/src -I libraries/environment-control-service/src -I libraries/hal/src -I libraries/eventloop/src -I libraries/notification-service/.build/borg/src -I libraries/patterns/src -I libraries/illumination-service/src -I libraries/jsonrpc-client/src -I libraries/notification-service/src -I libraries/utilities/src -I libraries/alarm-service/.build/borg/src
// How do we slay this beast? The important thing to start off with,
// is sequence. We cannot just split shit wherever we want. So figure
// out some initial segment which will be printable. Then keep consuming
// input.(What a bullshit comment...)
// Anyhow, we can start off by reading word by word, separated by ws.

static void split_compiler_command(const char *s)
{
    char word[10240], next[10240];
    size_t n, nnext, nprinted_on_this_line = 0;

    while ((n = get_word(word, sizeof word, s)) > 0) {
        if (nprinted_on_this_line + n > g_width) {
            nprinted_on_this_line = 0;
            puts("\\");
            indent();
        }

        if (word_wants_arg(word)) {
            nnext = get_word(next, sizeof next, s + n);
            size_t len = nprinted_on_this_line + n + nnext;
            ssize_t delta = g_width - len;
            if (delta < (FUZZYLEN * -1)) {
                // No room for argument. We need two lines here. 
                // if next word is long, we want word and next word on
                // a new line.
                nprinted_on_this_line = 0;
                puts("\\");
                indent();
            }

            nprinted_on_this_line += printf("%s %s ", word, next);
            s += n + nnext;
        }
        else {
            nprinted_on_this_line += printf("%s ", word);
            s += n;
        }
    }

}

// Disable until compiler commands are printed OK
static void split_any(const char *s)
{
    if (1) fputs(s, stdout);
}

static void split(const char *s)
{
    size_t slen;
    
    
    slen = strlen(s);

    if (slen < g_width) {
        fputs(s, stdout);
    }
    else if (is_compiler_command(s))
        split_compiler_command(s);
    else
        split_any(s);

    // Always end with a newline
    putchar('\n');
}

static void process(FILE *f)
{
    static char line[1024 * 1024];

    while (fgets(line, sizeof line, f)) {
        rtrim(line);
        split(line);
    }
}

static void sanity_check_self(void)
{
    char buf[100], buf2[sizeof buf];
    size_t i;

    strcpy(buf, "   hello");
    ltrim(buf);
    assert(strcmp(buf, "hello") == 0);

    strcpy(buf,"hello   \t");
    rtrim(buf);
    assert(strcmp(buf, "hello") == 0);

    strcpy(buf,"   \t\r\nhello   \t");
    trim(buf);
    assert(strcmp(buf, "hello") == 0);

    strcpy(buf, "  foo bar baz ");
    i = get_word(buf2, sizeof buf2, buf);
    assert(strcmp(buf2, "foo") == 0);
    assert(i == 5);

}


int main(int argc, char *argv[])
{
    FILE *f;

    sanity_check_self(); // integrated unit test...

    parse_commandline(argc, argv);

    if (optind == argc)
        process(stdin);
    else {
        while (argv[optind] != NULL) {
            if ((f = fopen(argv[optind], "r")) == NULL)
                die("%s:%s\n", argv[optind], strerror(errno));

            process(f);
            fclose(f);
            optind++;
        }
    }

    exit(0);
}
