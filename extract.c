#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include <meta_common.h>
#include <meta_regex.h>

static void show_usage(void)
{
    // TODO: add usage text
    static const char *text[] = {
        "extract: Extract patterns from files."
        "USAGE: extract pattern file...",
        "USAGE: extract -e pattern -e pattern... file...",
        "USAGE: extract pattern - (for stdin)",
        "",
    };

    size_t i, n = sizeof text / sizeof *text;
    for (i = 0; i < n; i++) {
        puts(text[i]);
    }
}

#define  MAXPATTERNS  1000
static size_t npatterns = 0;
static const char* pattern[MAXPATTERNS];
static regex regexes[MAXPATTERNS];

static void add_entry(const char *s)
{
    assert(s != NULL);
    if (npatterns >= sizeof pattern / sizeof *pattern)
        die("Too many patterns\n");

    pattern[npatterns++] = s;
}

static void compile_patterns(void)
{
    size_t i;

    for (i = 0; i < npatterns; i++) {
        if ((regexes[i] = regex_new()) == NULL)
            die("Out of memory");

        if (!regex_comp(regexes[i], pattern[i]))
            die("Unable to compile pattern %s\n", pattern[i]);
    }
}

static void parse_commandline(int argc, char *argv[])
{
    int c;
    const char *options = "e:h";

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch (c) {
            case 'h':
                show_usage();
                exit(0);

            case 'e':
                add_entry(optarg);
                break;

            case '?':
            default:
                exit(1);
        }
    }
}


// Tricky fact: "foo foo foo" is found once. It should've been found trice.
//
static void extract_if_matching(const char *src)
{
    size_t i, j;
    char line[10 *1024];
    const char *orgsrc = src;

    for (i = 0; i < npatterns; i++) {
        src = orgsrc;
        for (;;) { // In case we have more than one match per line
            int rc = regex_exec(regexes[i], src);
            if (rc == -1)
                die("Some error occurred");
            else if (rc == 0)
                break;

            // Now extract all matching patterns. We have to loop, chatgpt says. 
            for (j = 0; j < (size_t)rc; j++) {
                memset(line, 0, sizeof line);
                if (!regex_get_match(regexes[i], j, src, line, sizeof line))
                    die("Internal error");

                src += strlen(line);
                puts(line);
                break; // We skip subexpressions
            }
        }
    }
}

static void process(const char *filename)
{
    char line[10 *1024];

    FILE *f;

    if (strcmp(filename, "-") == 0)
        f = stdin;
    else
        f = fopen(filename, "r");
    if (f == NULL) 
        die_perror("%s", filename);

    while (fgets(line, sizeof line, f))
        extract_if_matching(line);

    if (strcmp(filename, "-") != 0)
        fclose(f);
}


int main(int argc, char *argv[])
{
    parse_commandline(argc, argv);

    // support extract pattern file... usage
    if (npatterns == 0 && argc >= 2)
        add_entry(argv[optind++]);

    if (npatterns == 0)
        die("No patterns to extract\n");

    compile_patterns();

    for (int i = optind; i < argc; i++)
        process(argv[i]);

    exit(0);
}
