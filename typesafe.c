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
    const char *options = "ht:v";

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch (c) {
            case 'h':
                show_usage();
                exit(0);

            case 'v':
                print_verbose_stuff = 1;
                break;

            case 't':
                // TODO: 't' expects an argument. read optarg
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

    // TODO: Add functionality here.

    exit(0);
}
