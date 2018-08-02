/*
 * genutil - generate commandline utility skeleton.
 *
 * Rationale: Over and over again, I write small utility programs.
 * These programs tend to be very similar, so I can save a lot of 
 * time generating a skeleton implementation. This program does
 * that.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>


static int configfile_support;
static const char *name;
static int commandline_parsing;
static const char *options;

static void show_usage(void)
{
    static const char *text[] = {
        "usage: genutil [opts] name",
        "opts can be",
        "    c      add configuration file support",
        "    o opts add option parsing. opts contains the options we want to support",
        "",
        "name is the name of the utility, and will create a file named name.c."
        "",
        "The -h and -v options will always be added.",
        "",
        "example: genutil -a -o xz: foo",
        "example: genutil -o xz: foo",
    };
    size_t i, n = sizeof text / sizeof *text;
    for (i = 0; i < n; i++) {
        puts(text[i]);
    }
}

static void parse_commandline(int argc, char *argv[])
{
    int c;
    const char *opts = "o:c";

    while ((c = getopt(argc, argv, opts)) != EOF) {
        switch(c) {
            case 'c':
                configfile_support = 1;
                break;

            case 'o':
                commandline_parsing = 1;
                options = optarg;
                break;

            case '?':
            default:
                show_usage();
                exit(1);
                
        }
    }

    if (optind < argc)
        name = argv[optind];
}

static void check_options(void)
{
    int ok = 1;
    const char *s;

    if (name == NULL) {
        fputs("name cannot be NULL\n", stderr);
        exit(1);
    }

    if (strlen(name) > 30) {
        fputs("name is too long\n", stderr);
        ok = 0;
    }

    // Check for legal chars. We accept only [A-z0-9_]
    s = name;
    while (*s != '\0') {
        if (!isalnum(*s) && *s != '_') {
            fprintf(stderr, "Illegal char in name: %c\n", *s);
            ok = 0;
            break;
        }

        s++;
    }

    if (!ok)
        exit(1);
}

static const char *includes[] = {
    "stdarg.h",
    "stdio.h",
    "string.h",
    "stdint.h",
    "stdlib.h",
    "ctype.h",
    "unistd.h",
    "errno.h",
};
    
static void add_includes(FILE *f)
{
    size_t i, n;

    n = sizeof includes / sizeof *includes;
    for (i = 0; i < n; i++)
        fprintf(f, "#include <%s>\n", includes[i]);

    fputc('\n', f);
}

static void add_die_and_warning(FILE *f)
{
    size_t i, n;

    static const char *die[] = {
	"__attribute__((format(printf,1,2)))",
        "static void die(const char *fmt, ...)",
        "{",
        "    va_list ap;",
        "",
        "    va_start(ap, fmt);",
        "    vfprintf(stderr, fmt, ap);",
        "    va_end(ap);",
        "    exit(1);",
        "}",
        "",
    };

    n = sizeof die / sizeof *die;
    for (i = 0; i < n; i++)
        fprintf(f, "%s\n", die[i]);
    
    static const char *warning[] = {
	"__attribute__((format(printf,1,2)))",
        "static void warning(const char *fmt, ...)",
        "{",
        "    va_list ap;",
        "",
        "    va_start(ap, fmt);",
        "    vfprintf(stderr, fmt, ap);",
        "    va_end(ap);",
        "}",
        "",
    };

    n = sizeof warning / sizeof *warning;
    for (i = 0; i < n; i++)
        fprintf(f, "%s\n", warning[i]);
}

static void add_verbose(FILE *f)
{
    static const char *verbose[] = {
        "static int print_verbose_stuff;",
	"__attribute__((format(printf,1,2)))",
        "static void verbose(const char *fmt, ...)",
        "{",
        "    va_list ap;",
        "",
        "    if (!print_verbose_stuff)",
        "        return;",
        "",
        "    va_start(ap, fmt);",
        "    vfprintf(stderr, fmt, ap);",
        "    va_end(ap);",
        "}",
        "",
    };
    
    size_t i, n = sizeof verbose / sizeof *verbose;
    for (i = 0; i < n; i++)
        fprintf(f, "%s\n", verbose[i]);
}

static void add_show_usage(FILE *f)
{
    static const char *src[] = {
        "static void show_usage(void)",
        "{",
        "    static const char *text[] = {",
        "        \"usage: add your own text here\",",
        "        \"\",",
        "    };",
        "",
        "    size_t i, n = sizeof text / sizeof *text;",
        "    for (i = 0; i < n; i++) {",
        "        puts(text[i]);",
        "    }",
        "}",
        "",
    };

    size_t i, n = sizeof src / sizeof *src;
    for (i = 0; i < n; i++) 
        fprintf(f, "%s\n", src[i]);
}

static void add_commandline_parsing(FILE *f)
{
    char buf[1024];

    add_show_usage(f);

    fprintf(f, "static void parse_commandline(int argc, char *argv[])\n");
    fprintf(f, "{\n");
    fprintf(f, "    int c;\n");

    // Create a set of options which includes the user supplied options
    // and v+h in case the user didn't specify these two.
    strcpy(buf, options);
    if (strchr(options, 'h') == NULL)
        strcat(buf, "h");

    if (strchr(options, 'v') == NULL)
        strcat(buf, "v");

    fprintf(f, "    const char *options = \"%s\";\n", buf);
    fprintf(f, "\n");
    fprintf(f, "    while ((c = getopt(argc, argv, options)) != EOF) {\n");
    fprintf(f, "        switch (c) {\n");
    fprintf(f, "            case 'h':\n");
    fprintf(f, "                show_usage();\n");
    fprintf(f, "                exit(0);\n");
    fprintf(f, "\n");
    fprintf(f, "            case 'v':\n");
    fprintf(f, "                print_verbose_stuff = 1;\n");
    fprintf(f, "                break;\n");
    fprintf(f, "\n");

    // Generate one case entry for each option.
    const char *s = buf;
    while (*s != '\0') {
        char c = *s++;

        // already handled
        if (c == 'h' || c == 'v')
            continue; 

        fprintf(f, "            case '%c':\n", c);
        if (*s == ':') {
            s++;
            fprintf(f, "                // '%c' expects an argument. Remember to read optarg\n", c);
        }

        fprintf(f, "                break;\n");
        fprintf(f, "\n");
    }

    fprintf(f, "            case '?':\n");
    fprintf(f, "            default:\n");
    fprintf(f, "                exit(1);\n");
    fprintf(f, "        }\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");
}

static void add_main(FILE *f)
{
    fprintf(f, "int main(int argc, char *argv[])\n");
    fprintf(f, "{\n");
    fprintf(f, "\n");

    if (commandline_parsing) {
        fprintf(f, "    parse_commandline(argc, argv);\n");
    }
    if (configfile_support) {
        fprintf(f, "    read_configfile();\n");
    }
    fprintf(f, "\n");
    fprintf(f, "    /* Add functionality here. */\n");
    fprintf(f, "\n");
    fprintf(f, "    exit(0);\n");
    fprintf(f, "}\n");
}

static void add_configfile(FILE *f)
{
    char filename[1024];

    sprintf(filename, "%s.conf", name);

    fprintf(f, "static void read_configfile(void)\n");
    fprintf(f, "{\n");
    fprintf(f, "    const char *filename = \"%s\";\n", filename);
    fprintf(f, "    FILE *f;\n");
    fprintf(f, "    char line[1024];\n");
    fprintf(f, "\n");
    fprintf(f, "    if ((f = fopen(filename, \"r\")) == NULL) {\n");
    fprintf(f, "        verbose(\"%%s : %%s\\n\", filename, strerror(errno));\n");
    fprintf(f, "        return;\n");
    fprintf(f, "    }\n");
    fprintf(f, "\n");
    fprintf(f, "    while (fgets(line, sizeof line, f)) {\n");
    fprintf(f, "        /* process line. */\n");
    fprintf(f, "    }\n");
    fprintf(f, "\n");
    fprintf(f, "    fclose(f);\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");
}

static void generate_code(void)
{
    FILE *f;
    char buf[1024];

    sprintf(buf, "%s.c", name);

    if ((f = fopen(buf, "w")) == NULL) {
        perror(buf);
        exit(1);
    }

    add_includes(f);
    add_die_and_warning(f);
    add_verbose(f);
    if (commandline_parsing)
        add_commandline_parsing(f);

    if (configfile_support)
        add_configfile(f);

    add_main(f);

    fclose(f);
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        show_usage();
        exit(0);
    }

    parse_commandline(argc, argv);

    check_options();
    generate_code();
    return 1;
}

