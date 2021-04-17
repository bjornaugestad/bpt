#include <assert.h>
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

static int print_verbose_stuff;
#if 0
__attribute__((format(printf,1,2)))
static void warning(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}


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
#endif

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

#define TYPE_UNKNOWN 0
#define TYPE_LIST 1
static const struct {
    int type;
    const char *name;
} typemap[] = {
    { TYPE_LIST, "list" },
};

static int g_type = TYPE_LIST;

static void assign_typename(const char *s)
{
    size_t i, n;

    assert(s != NULL);
    n = sizeof typemap / sizeof *typemap;
    for (i = 0; i < n; i++) {
        if (strcmp(typemap[i].name, s) == 0) {
            g_type = typemap[i].type;
            return;
        }
    }

    die("%s:unknown type name\n", s);
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
                assign_typename(optarg);
                break;

            case '?':
            default:
                exit(1);
        }
    }
}

static void printn(size_t nrefs, const char *template, const char *typename)
{
    assert(template != NULL);
    assert(typename != NULL);

    const char *s = typename; // Just shorten it for readability below

    switch (nrefs) {
        case 0: puts(template); break;
        case 1: printf(template, s); break;
        case 2: printf(template, s, s); break;
        case 3: printf(template, s, s, s); break;
        case 4: printf(template, s, s, s, s); break;
        case 5: printf(template, s, s, s, s, s); break;
        case 6: printf(template, s, s, s, s, s, s); break;

        default:
            die("Unhandled number of references");
    }
}

// Create a bunch of inline functions to wrap list access in typesafe wrappers
static void process_list(const char *typename)
{
    assert(typename != NULL);
    assert(strlen(typename) > 0);

    static const struct {
        size_t nrefs;           // How many times do we refer to typename?
        const char *template;
    } map [] = {
        { 2, "static inline %s* %s_new(void) { return list_new(); }" },
        { 2, "static inline void %s_free(%s *p) { list_free(p); }" },
        { 6, "static inline bool %s_dual_foreach(%s lst, %s *arg1, void *arg2, {}\n\n" },
        { 6, "static inline bool %s_foreach(%s lst, %s *args, listfunc f) {}\n\n" },
        { 6, "static inline bool %s_foreach_reversed(%s lst, %s *arg, listfunc f) {}\n\n" },
        { 6, "static inline bool %s_foreach_sep(%s lst, %s *arg, listfunc f, bool(*sep)(%s *arg)) {}\n\n" },
        { 6, "static inline bool %s_last(%s_iterator li) {}\n\n" },
        { 6, "static inline %s_iterator %s_delete(%s lst, %s_iterator i, dtor dtor_fn) {}\n\n" },
        { 6, "static inline %s_iterator %s_find(%s lst, const %s *data, {}\n\n" },
        { 6, "static inline %s_iterator %s_remove_node(%s lst, %s_iterator i) {}\n\n" },
        { 6, "static inline %s %s_add(%s lst, %s *data) {}\n\n" },
        { 6, "static inline %s %s_copy(%s lst, %s *(*copier)(const %s *), dtor dtor_fn) {}\n\n" },
        { 6, "static inline %s %s_insert(%s lst, %s *data) {}\n\n" },
        { 6, "static inline %s %s_merge(%s dest, %s src) {}\n\n" },
        { 6, "static inline %s %s_sublist_adaptor(%s lst, %s *(*adaptor)(%s *)) {}\n\n" },
        { 6, "static inline %s %s_sublist_copy(%s lst) {}\n\n" },
        { 6, "static inline %s %s_sublist_create(%s lst, int (*include_node)(%s *)) {}\n\n" },
        { 6, "static inline %s %s_sublist_create_neg(%s lst, int (*include_node)(%s *)) {}\n\n" },
        { 6, "static inline size_t %s_count(%s lst, int (*include_node)(%s *)) {}\n\n" },
        { 6, "static inline size_t %s_size(%s lst) {}\n\n" },
        { 6, "static inline int %s_end(%s_iterator li) {}\n\n" },
        { 6, "static inline %s_iterator %s_first(%s lst) {}\n\n" },
        { 6, "static inline %s_iterator %s_next(%s_iterator i) {}\n\n" },
        { 6, "static inline %s *%s_get(%s_iterator i) {}\n\n" },
        { 6, "static inline status_t %s_insert_after(%s_iterator li, %s *data) {}\n\n" },
        { 6, "static inline status_t %s_insert_before(%s_iterator li, %s *data) {}\n\n" },
        { 6, "static inline typedef bool (*listfunc)(%s *arg, %s *data); {}\n\n" },
        { 6, "static inline %s *%s_get_item(%s lst, size_t idx) {}\n\n" },
        { 6, "static inline void %s_sort(%s lst, int(*func)(const %s *p1, const void *p2)) {}\n\n" },
        { 6, "static inline void %s_sublist_free(%s lst); {}\n\n" },
    };

    size_t n = sizeof map / sizeof *map;
    for (size_t i = 0; i < n; i++) {
        printn(map[i].nrefs, map[i].template, typename);
    }
}


static void process(int type, const char *typename)
{
    switch (type) {
        case TYPE_LIST:
            process_list(typename);
            break;

        default:
            die("Internal error: unknown type %d\n", type);
    }
}

int main(int argc, char *argv[])
{

    parse_commandline(argc, argv);

    // TODO: Add functionality here.
    while (optind < argc) {
        process(g_type, argv[optind++]);
    }

    exit(0);
}
