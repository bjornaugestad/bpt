#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>


// What's considered a long line? Default is 80, override with -n 
static long g_nchars = 80;

__attribute__((format(printf,1,2)))
static void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

static void show_usage(void)
{
    // TODO: add usage text
    static const char *text[] = {
        "usage: add your own text here",
        "",
    };

    size_t i, n = sizeof text / sizeof *text;
    for (i = 0; i < n; i++) {
        puts(text[i]);
    }
}

static void parse_commandline(int argc, char *argv[])
{
    int c;
    const char *options = "n:hv";

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch (c) {
            case 'h':
                show_usage();
                exit(0);

            case 'n':
                g_nchars = strtol(optarg, NULL, 10);
                break;

            case '?':
            default:
                exit(1);
        }
    }

    // Sanity check
    if (g_nchars == 0 || g_nchars == LONG_MIN || g_nchars == LONG_MAX)
        die("Illegal value for -n");
}


static unsigned long g_lineno;
static void process(const char *filename)
{
    FILE *f;
    static char line[8192 * 1024]; // 8MB linelen to avoid chopping up lines
    long len;

    if (strcmp(filename, "-") == 0)
        f = stdin;
    else if ((f = fopen(filename, "r")) == NULL)
        die("%s:%s\n", filename, strerror(errno));

    while (fgets(line, sizeof line, f) != NULL) {
        g_lineno++;
        len = strlen(line);
        if (len > g_nchars)
            printf("%zu: %s\n", g_lineno, line);
    }

    if (f != stdin)
        fclose(f);
}

int main(int argc, char *argv[])
{
    parse_commandline(argc, argv);

    if (optind == argc) {
        process("-"); // stdin
    }
    else {
        while (optind < argc)
            process(argv[optind++]);
    }

    exit(0);
}
