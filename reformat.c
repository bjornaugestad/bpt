#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

__attribute__((format(printf,1,2)))
static void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

__attribute__((format(printf,1,2)))
static void warning(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static int print_verbose_stuff;
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

static void show_usage(void)
{
    static const char *text[] = {
        "usage: reformat file..."
        "A file is something with very long text lines, like"
        "compiler output or Makefile dependeny stuff.",
        "This program reformats a file's contents and prints the result to stdout.",
        "It's like the fold program, but tries to be context sensitive",
        "when folding. -I some/long/path goes on the same line.",
    };

    size_t i, n = sizeof text / sizeof *text;
    for (i = 0; i < n; i++) {
        puts(text[i]);
    }
}

static void parse_commandline(int argc, char *argv[])
{
    int c;
    const char *options = "w:hv";

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch (c) {
            case 'h':
                show_usage();
                exit(0);

            case 'v':
                print_verbose_stuff = 1;
                break;

            case 'w':
                // 'w' expects an argument. Remember to read optarg
                break;

            case '?':
            default:
                exit(1);
        }
    }
}

int main(int argc, char *argv[])
{

    parse_commandline(argc, argv);

    /* Add functionality here. */

    exit(0);
}
