/*
 * genman: generate man page source.
 * The syntax of man pages are notoriously hard to remember.
 * This program generates the necessary text, or at least
 * a starting point.
 * USAGE: see genman -h.
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


static const char *usage[] = {
    "Usage: genman [options]",
    "options are:",
    "\t-r type	return type of function. Default is void.",
    "\t-a args	Function arguments on the form \"name=type;\".",
    "\t-g group Group number for the man page. Default is 3.",
    "\t-i includefile Which file to include, if any.",
    "\t-n function name. Required!",
    "\t-A author name. Required!",
    "\t-G Group name. Required!",
    "",
    "",
    "",
};

static void show_usage(void)
{
    size_t i, n;

    n = sizeof usage / sizeof *usage;
    for (i = 0; i < n; i++)
        puts(usage[i]);
}

static const char *g_return_type = NULL;
static const char *g_arguments = NULL;
static const char *g_includefile = NULL;
static const char *g_name = NULL;
static const char *g_author = NULL;
static const char *g_groupname = NULL;
static int g_group = 3;

static int sane_args(void)
{
    char name[1024], type[1024];
    const char *s = g_arguments;
    size_t i;

    /* No args is fine. */
    if (g_arguments == NULL)
        return 1;

    /* Now, parse the string and look for name=type; pairs.
     * We accept spaces since type names can be more than
     * one word, like in "name = const char *;"
     */

    for (;;) {
        i = 0;

        /* Copy name */
        while (*s != '\0' && *s != '=')
            name[i++] = *s++;

        if (*s++ != '=')
            return 0;

        name[i] = 0;

        /* copy type */
        i = 0;
        while (*s != '\0' && *s != ';')
            type[i++] = *s++;

        if (*s++ != ';')
            return 0;

        type[i] = '\0';

        fprintf(stderr, "name:%s type:%s\n", name, type);

        if (*s == '\0')
            return 1;
    }
}

static void parse_command_line(int argc, char *argv[])
{
    int c;
    const char *options = "hA:G:r:a:i:n:g:";

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch (c) {
            case 'A':
                g_author = optarg;
                break;

            case 'G':
                g_groupname = optarg;
                break;

            case 'r':
                g_return_type = optarg;
                break;

            case 'i':
                g_includefile = optarg;
                break;

            case 'a':
                g_arguments = optarg;
                break;

            case 'n':
                g_name = optarg;
                break;

            case 'g':
                g_group = atoi(optarg);
                break;

            case 'h':
                show_usage();
                exit(EXIT_SUCCESS);

            case '?':
                show_usage();
                exit(EXIT_FAILURE);
        }
    }

    if (g_name == NULL) {
        fprintf(stderr, "-n function_name is required\n");
        exit(EXIT_FAILURE);
    }

    if (g_author == NULL) {
        fprintf(stderr, "-A author is required\n");
        exit(EXIT_FAILURE);
    }

    if (g_groupname == NULL) {
        fprintf(stderr, "-G group name is required\n");
        exit(EXIT_FAILURE);
    }

    if (!sane_args()) {
        fprintf(stderr, "Syntax error in arguments. Syntax is name=type;...\n");

        exit(EXIT_FAILURE);
    }
}

static const char *upper(const char *s)
{
    static char buf[1024];
    size_t i = 0;

    while (*s != '\0')
        buf[i++] = toupper(*s++);

    buf[i] = 0;
    return buf;
}

static void print_args(void)
{
    char name[1024], type[1024];
    const char *s = g_arguments;
    size_t i;

    /* No args is fine. */
    if (g_arguments == NULL)
        return;

    /* Now, parse the string and look for name=type; pairs.
     * We accept spaces since type names can be more than
     * one word, like in "name = const char *;"
     */

    for (;;) {
        i = 0;

        /* Copy name */
        while (*s != '\0' && *s != '=')
            name[i++] = *s++;

        assert(*s == '=');
        s++;

        name[i] = 0;

        /* copy type */
        i = 0;
        while (*s != '\0' && *s != ';')
            type[i++] = *s++;

        assert(*s == ';');
        s++;

        type[i] = '\0';

        printf(".Fa \"%s %s\"\n", type, name);

        if (*s == '\0')
            return;
    }
}

static void print_date(void)
{
    char buf[1024];
    time_t now = time(NULL);

    strftime(buf, sizeof buf, ".Dd %b %d, %Y", gmtime(&now));
    puts(buf);
}

static void produce_man_page(void)
{

    assert(g_name != NULL);

    print_date();
    printf(".Os POSIX\n");
    printf(".Dt %s\n", upper(g_groupname));
    printf(".Th %s %d\n", g_name, g_group);
    printf(".Sh NAME\n");
    printf(".Nm %s\n", g_name);
    printf(".Nd Short desc here\n");
    printf(".Sh SYNOPSIS\n");
    if (g_includefile != NULL)
        printf(".Fd #include <%s>\n", g_includefile);
    printf(".Fo \"");
    if (g_return_type != NULL)
        printf("%s ", g_return_type);
    else
        printf("void ");
    printf("%s\"\n", g_name);
    print_args();
    printf(".Fc\n");

    printf(".Sh DESCRIPTION\n");
    printf(".Nm %s()\n", g_name);
    printf("long desc goes here.\n");

    printf(".Sh AUTHOR\n");
    printf(".An %s\n", g_author);
}

int main(int argc, char *argv[])
{
    parse_command_line(argc, argv);
    produce_man_page();

    return 0;
}
