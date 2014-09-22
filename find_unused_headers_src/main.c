#include "file.h"
#include "misc.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/*
 * Configuration files can be stored in different places.
 * We search them all but require none.
 */
static const char *configfiles[] = {
    "/etc/find_unused_headers.conf",
    "/usr/local/etc/find_unused_headers.conf", 
    "./find_unused_headers.conf",
    "./test.conf"
};

/* Users may add config file on the command line.  */
const char * optional_config_file = NULL;
static int use_config_files = 1;

static int line_is_comment(const char *s)
{
    assert(s != NULL);
    return *s == '#';
}

static int line_is_empty(const char *s)
{
    assert(s != NULL);
    while(*s) {
        if(!isspace((unsigned char)*s))
            return 0;
        s++;
    }

    return 1;
}

static void rstrip(char *s)
{
    size_t i;

    assert(s != NULL);
    i = strlen(s);
    if(i-- == 0)
        return;

    while(i > 0 && isspace((unsigned char)s[i]))
        s[i--] = '\0';
}

/* Legal characters in addition to isalnum()'s definition */
const char *filechars = "._/";
const char *fnchars = ":";

/* A line is a filename if it consists of [A-z0-9_]*
 * We dislike trailing ws so rstrip the line first. */
static int line_is_filename(const char *s)
{

    assert(s != NULL);
    while(*s) {
        if(!isalnum(*s) && strchr(filechars, *s) == NULL)
            return 0;
        s++;
    }

    return 1;
}

static int is_legal_char(char c)
{
    if(isalnum(c) || strchr(filechars, c) != NULL || strchr(fnchars, c) != NULL)
        return 1;
    return 0;
}


/* Parse the line of 1..n symbols and add them to the file object. */
static void add_symbols(struct file *p, const char *s)
{
    /* We copy characters from s to buf to make it possible
     * to zero-terminate the strings prior to calling file_add_symbol().
     */
    char buf[1024];
    size_t i;

    assert(p != NULL);
    assert(s != NULL);

    /* Skip leading ws, if any. */
    while(*s && isspace((unsigned char)*s))
        s++;

    i = 0;
    while(*s) {
        if(isspace((unsigned char)*s)) {
            buf[i] = '\0';
            if(i > 0) {
                file_add_symbol(p, buf);
                i = 0;
                /* Skip more ws */
                while(*s && isspace((unsigned char)*s))
                    s++;
            }
        }
        else if(is_legal_char(*s))
            buf[i++] = *s++;
        else {
            fprintf(stderr, "Found garbage in configfile: %c\n", *s);
            i = 0;
            s++;
        }
    }

    /* We may have a symbol in buf. Add it */
    if(i > 0) {
        buf[i] = '\0';
        file_add_symbol(p, buf);
    }
}

static void read_config_file(struct file *head, const char *filename)
{
    FILE *f;
    char line[2048];
    struct file *curr = NULL;

    assert(filename != NULL);

    if( (f = fopen(filename, "r")) == NULL)
        return; 

    while(fgets(line, sizeof line, f) != NULL) {
        rstrip(line);
        if(line_is_comment(line) || line_is_empty(line))
            ;
        else if(line_is_filename(line)) {
            curr = file_new_tail(head, line);
        }
        else if(curr != NULL) {
            add_symbols(curr, line);
        }
        else {
            fprintf(stderr, "%s: Could not make sense of input: %s\n", 
                filename, line);
        }
    }

    fclose(f);
}

static void read_config_files(struct file *head)
{
    size_t i, n = sizeof configfiles / sizeof *configfiles;

    for(i = 0; i < n; i++) 
        read_config_file(head, configfiles[i]);

    if(optional_config_file != NULL)
        read_config_file(head, optional_config_file);
}

static void dump_tree(struct file *p)
{
    while( (p = file_next(p)) != NULL) {
        size_t i, n;
        
        printf("file:%s\n", file_name(p));
        n = file_nsymbols(p);
        for(i = 0; i < n; i++) 
            printf(" %s", file_symbol(p, i));
        puts("\n");

        printf("files where %s is unneeded:\n", file_name(p));
        n = file_nunused(p);
        for(i = 0; i < n; i++) 
            printf(" %s", file_unused(p, i));
        puts("\n");
    }

}

static void show_help(void)
{
    printf("no help yet.\n");
}

static void parse_options(int argc, char *argv[])
{
    const char *options = "xhc:";
    int c;

    while( (c = getopt(argc, argv, options)) != EOF) {
        switch(c) {
            case 'h':
                show_help();
                exit(0);

            case 'c': 
                optional_config_file = optarg;
                break;

            case 'x':
                use_config_files = 0;
                break;

            case '?':
            default:
                show_help();
                exit(1);
        }
    }
}

/* 
 * Some header files define stuff with the same name as the basename
 * of the header file. For example, assert.h defines assert. This causes
 * problems for us, as we search for assert and find assert.h. This function
 * checks if the line contains an include-statement. If so, return 1, else 0.
 */
static int include_statement(const char *s)
{
    return strstr(s, "#include") != NULL;
}

/* Does the file pointed to by filename use one or more symbols defined to 
 * belong to the p-file? If so, return 1. Else return 0. */
static int file_uses_symbol(struct file *p, const char *filename)
{
    FILE *f;
    char buf[2048];
    int found = 0;
    size_t i, n = file_nsymbols(p);
    regex_t *pre;
    regmatch_t match;

    assert(filename != NULL);
    assert(strlen(filename) > 0);

    if( (f = fopen(filename, "r")) == NULL) 
        abort();

    /* Read each line. If one of the file's symbols are found,
     * we can stop reading. */
    while(fgets(buf, sizeof buf, f) != NULL) {
        for(i = 0; i < n; i++) {
            pre = file_symbol_regex(p, i);
            if(regexec(pre, buf, 1, &match, 0) == 0
            && !include_statement(buf)) {
                found = 1;
                goto done;
            }
        }
    }

done:
    fclose(f);
    return found;
}

static void check_symbol_usage(struct file *p, const char *filename)
{
    if(!file_uses_symbol(p, filename)) 
        file_add_unused(p, filename);
}




/* Process file, what does that mean? Basically we search for 
 * include-statements for each file in the linked list. If we find
 * such a statement, we see if any of the symbols are used. If not,
 * we have found a redundant include-statement. Now, what do we do?
 * We want to separate the processing and the reporting step. How
 * can that be achieved best? Maybe we should just add the filename
 * to the file object? Can't be that stupid, can it? */
static void find_unused_headers(struct file *head, const char *filename)
{
    FILE *f;
    char buf[2048];
    int rc;
    regmatch_t matches[1];

    assert(filename != NULL);
    assert(strlen(filename) > 0);

    if( (f = fopen(filename, "r")) == NULL) 
        return;

    /*
     * It's a pita we cannot mmap the source file. regexec()
     * requires a zero-terminated buffer, but our files don't have it.
     */

    while( (head = file_next(head)) != NULL) {
        regex_t *pre = file_include_regex(head);
        rewind(f);
        while(fgets(buf, sizeof buf, f) != NULL) {
            /* Look for include-statement */
            rc = regexec(pre, buf, sizeof matches / sizeof *matches, matches, 0);
            if(rc == 0) {
                // WE have a match. Now check all symbols. To do that, we must
                // re-read the file. Use a separate function.
                check_symbol_usage(head, filename);
            }
        }
    }

    fclose(f);
}

/* Does the file include a headerfile with a specific name? If so, return 1.
 * Else return 0 */
static int file_includes_header(struct file *head, const char *filename)
{
    FILE *f;
    char buf[2048];
    int rc;
    int found = 0;
    regmatch_t matches[1];
    regex_t *pre;

    assert(filename != NULL);
    assert(strlen(filename) > 0);

    if( (f = fopen(filename, "r")) == NULL) 
        return 0;


    pre = file_include_regex(head);
    while(fgets(buf, sizeof buf, f) != NULL) {
        /* Look for include-statement */
        rc = regexec(pre, buf, sizeof matches / sizeof *matches, matches, 0);
        if(rc == 0) {
            found = 1;
            break;
        }
    }

    fclose(f);
    return found;
}



/*
 * To locate missing headers, we must first check the source file for symbols
 * from * each header file. Then, if we do find a symbol, we must check for 
 * an include statement.
 * If no statement is found, we must register the missing filename somewhere. 
 */
static void find_missing_headers(struct file *head, const char *filename)
{
    FILE *f;

    assert(filename != NULL);
    assert(strlen(filename) > 0);

    if( (f = fopen(filename, "r")) == NULL) 
        return;

    while( (head = file_next(head)) != NULL) {
        if (!file_uses_symbol(head, filename))
            continue;

        if (file_includes_header(head, filename))
            continue;

        printf("%s: Uses symbols from %s, but does not include it itself\n",
            filename, file_name(head));
    }

    fclose(f);
}


static void compile_all_symbols(struct file *head)
{
    while( (head = file_next(head)))
        file_compile_symbols(head);
}

// some include statements contain paths with slash.
// sed does not like such paths, so we must escape 
// the slashes.
static const char *escape(const char *s)
{
    const char *chars = "/";
    static char buf[10240];
    size_t i = 0;

    assert(strlen(s) < sizeof buf);

    while (*s != '\0') {
        if (strchr(chars, *s) != NULL)
            buf[i++] = '\\';

        buf[i++] = *s++;
    }

    buf[i] = '\0';
    return buf;
}

/* Let's default to something suitable for sed */
static void report_unused_headers(struct file *p)
{
    size_t i, n;

    assert(p != NULL);

    while ( (p = file_next(p)) != NULL) {
        n = file_nunused(p);
        if(n > 0) {
            printf("sed -ie '/#include[ ]*[<\"]%s[>\"]/d' ", escape(file_name(p)));
            for(i = 0; i < n; i++) 
                printf(" %s", file_unused(p, i));
            puts("\n");
        }
    }
}

int main(int argc, char *argv[])
{
    int i;
    struct file *head;

    if(argc == 0) {
        /* No options or filenames. */
        return 0;
    }

    parse_options(argc, argv);
    if(optind == argc) {
        fprintf(stderr, "%s: No input files\n", argv[0]);
        exit(1);
    }

    head = file_new("head");
    add_standard_headers(head);

    if (strstr(argv[0], "find_missing_headers") != NULL) {
        compile_all_symbols(head);
        for(i = optind; i < argc; i++) 
            find_missing_headers(head, argv[i]);
    }
    else {
        if (use_config_files)
            read_config_files(head);
        compile_all_symbols(head);
        for(i = optind; i < argc; i++) 
            find_unused_headers(head, argv[i]);
        report_unused_headers(head);
    }

    if(0) dump_tree(head);
    file_free_list(head);
}

