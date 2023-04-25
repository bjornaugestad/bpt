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

__attribute__((format(printf,1,2)))
static void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

static void process(const char *filename)
{
    FILE *f;
    char line[4096];

    if ((f = fopen(filename, "a")) == NULL)
        die("%s:%s\n", filename, strerror(errno));

    while (fgets(line, sizeof line, stdin) != NULL) {
        if (fputs(line, f) == EOF)
            die("append: %s\n", strerror(errno));
    }

    fclose(f);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        die("USAGE: append filename\n");

    process(argv[1]);
    exit(0);
}
