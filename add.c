#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

__attribute__((format(printf,1,2)))
static void warning(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

int main(void)
{
    char line[1024];
    long long result = 0;

    while (fgets(line, sizeof line, stdin) != NULL) {
        long long i = strtoll(line, NULL, 0);
        if (i != 0)
            result += i;
        else
            warning("Could not interpret %s\n", line);
    }

    printf("%lld\n", result);

    exit(0);
}
