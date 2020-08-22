#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if 0
__attribute__((format(printf,1,2)))
static void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

#endif

__attribute__((format(printf,1,2)))
static void warning(const char *fmt, ...)
{
    va_list ap;

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

static unsigned long long g_factor = 1;
static unsigned long long g_size = 0;
static const char *suffix = "";

static void parse_commandline(int argc, char *argv[])
{
    int c;
    const char *options = "bkmgh";

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch (c) {
            case 'h':
                show_usage();
                exit(0);

            case 'b':
                g_factor = 1;
                break;

            case 'k':
                g_factor = 1024;
                suffix = " KB";
                break;

            case 'm':
                suffix = " MB";
                g_factor = 1024 * 1024;
                break;

            case 'g':
                suffix = " GB";
                g_factor = 1024 * 1024 * 1024;
                break;

            case '?':
            default:
                exit(1);
        }
    }
}

static void printsize(void)
{
    printf("%llu%s\n", g_size / g_factor, suffix);
}

static void rtrim(char *s)
{
    size_t end = strlen(s);
    while (end > 0 && isspace(s[--end]))
        s[end] = '\0';
}


static void addsize(const char *path)
{
    struct stat st;
    char *s = strdup(path);

    rtrim(s);
    if (stat(s, &st))
        perror(s);
    else if ((st.st_mode & S_IFMT) == S_IFREG)
        g_size += st.st_size;
    else
        warning("%s: not a regular file\n", s);


    free(s);
}


int main(int argc, char *argv[])
{

    parse_commandline(argc, argv);

    if (optind == argc) {
        char path[10240];
        while (fgets(path, sizeof path, stdin) != NULL) {
            addsize(path);
        }
    }
    else {
        while (argv[optind] != NULL) {
            addsize(argv[optind++]);
        }
    }

    printsize();


    exit(0);
}
