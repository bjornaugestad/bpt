/*
 * gint - generate integer types.
 * I've been exposed to Ada and need ranged types in C too. This is a
 * fresh implementation of an old trick : use a struct with one member
 * to get type safety.
 * struct { int value; } age_t; 
 * struct { int value; } salary_t; 
 *
 * this way we cannot mix age_t variables with salary_t variables.
 *
 * We generate access functions too, like set(), get(), equals(), which
 * all follow the same pattern: {stem}_(set|get|equals)
 * value's type can vary, and can be specified by our user.
 *
 * Input format: We just need a long list of type name stems, like age, salary,
 * foo, bar, whatever. Along with an optional type. Syntax? How about stem[:type]?
 * Works for me.
 *
 * We use a list approach so all types can end up in the same file. 
 * This way it's easy to include one single file.
 *
 * So far, so good, but what about strings? We want name_t, address_t and so forth, right?
 * So support strings as well. Syntax? If we already have stem:type, we could
 * extend that pattern to stem:type:length. Does this mean that we easily can
 * do foo:uint64_t:1000 to get an array of uint64_t? I guess it does, lol :)
 * We just need to be smart about strings, so we terminate them properly.
 * so if type == "char", then add and test for null characters.
 *
 * So far, so good, but what about arrays? Do we even want them? What we do want,
 * is ranged integers with max values. Do we want min values too? IOW, do we want from..to?
 * I guess we do. Do we want sets as well? Are we re-implementing enums here?
 *
 * Do we want a preprocessor and additional syntax? That'd be even nicer, but 
 * smells of overkill...
 *
 * Do we want a preprocessor and additional syntax? That'd be even nicer, but 
 * smells of overkill. Why not just use Ada in that case? Using C++ will funnily enough
 * not solve the problem since we still need to support fixed length strings.
 * Maybe we can use tgc instead?
 *
 * Meh, tcg is great but not suited for this task. 
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void die(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

static const char* get_stem(const char *src)
{
    static char buf[1024];
    const char *s;
    size_t n;

    assert(src != NULL);

    s = strchr(src, ':');
    if (s == NULL) {
        if (sizeof buf <= strlen(src))
            die("Destination buffer too small");

        strcpy(buf, src);
        return buf;
    }

    // We have a type, which we don't want as part of stem
    n = (size_t)(s - src);
    if (n == 0)
        die("missing stem part in stem:type");

    if (n >= sizeof buf)
        die("stem part too long");

    strncpy(buf, src, n);
    buf[n] = 0;
    return buf;
}

static const char* get_type(const char *src)
{
    static char buf[1024];
    size_t i;

    const char *type = strchr(src, ':');
    if (type == NULL) {
        strcpy(buf, "int");
        return buf;
    }
    else
        type++; // skip the : 

    // Copy type to buf
    for (i = 0; i < sizeof buf - 1 && *type != '\0' && *type != ':';)
        buf[i++] = *type++;

    buf[i] = '\0';
    return buf;
}

static size_t get_count(const char *src)
{
    int result = 0;
    const char *s;

    s = strchr(src, ':');
    if (s == NULL)
        return 0; // no type specified, so hence no count

    s++; // skip first colon and then look for next(count)
    s = strchr(s, ':');
    if (s == NULL)
        return 0; // no count specified

    // OK, we have a count specified. Convert it to size_t
    s++;
    result = atoi(s);
    if (result < 0)
        die("Count must be positive");

    return result;
}

// What's our from..to syntax? 
// 1. from and to are signed integers so we can specify -10..-2, -100..100
//    and so forth.
// 2. If only one value present, then the other value is 0.
// 3. If single value < 0, then to = 0. if single value > 0, then from = 0.
// 4. Use .. to separate the two values
static void get_from_to(const char *src, int *from, int *to)
{
    int n;
    const char *s;

    assert(src != NULL);
    assert(from != NULL);
    assert(to != NULL);

    *from = *to = 0;

    // Skip to length/range parf, if any exist. Remember syntax: stem[:type[:range]]
    s = strchr(src, ':');
    if (s == NULL)
        return; // all good, no type and hence no range

    s++;  // skip type's :
    s = strchr(s, ':');
    if (s == NULL)
        return; // all good, no range.

    n = sscanf(s, ":%d..%d", from, to);
    if (n == 2)
        return; // all good, from..to range specified
    else if (n == 0) {
        return; // all good, no range specified
    }
    else if (n == 1) {
        if (*from > 0) {
            *to = *from;
            *from = 0;
        }
        else
            *to = 0;

        return; // all good, single value specified.
    }
    else
        die("Unknown value in get_from_to");
}


static void process_string(FILE *f, const char *stem, size_t count)
{
    assert(f != NULL);
    assert(stem != NULL);
    assert(strlen(stem) > 0);
    assert(count > 1); // must have room for more than just the NUL

    fprintf(f, "typedef struct { char value[%zu]; } %s_t;\n", count + 1, stem);
    fprintf(f, "static inline void %s_set(%s_t *p, const char *src)\n", stem, stem);
    fprintf(f, "{\n");
    fprintf(f, "    size_t slen;\n");
    fprintf(f, "\n");
    fprintf(f, "    assert(p != NULL);\n");
    fprintf(f, "    assert(src != NULL);\n");
    fprintf(f, "\n");
    fprintf(f, "    slen = strlen(src);\n");
    fprintf(f, "    assert(strlen(src) < sizeof p->value);\n");
    fprintf(f, "    memcpy(p->value, src, slen);\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");

	fprintf(f, "static inline const char *%s_get(const %s_t *p)\n", stem, stem);
    fprintf(f, "{\n");
    fprintf(f, "    assert(p != NULL);\n");
    fprintf(f, "    return p->value;\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");

	fprintf(f, "static inline size_t %s_len(const %s_t *p)\n", stem, stem);
    fprintf(f, "{\n");
    fprintf(f, "    assert(p != NULL);\n");
    fprintf(f, "    return strlen(p->value);\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");
}

static void process_ranged_integer(FILE *f, const char *stem, const char *type, int from, int to)
{
    assert(f != NULL);
    assert(stem != NULL);
    assert(type != NULL);
    assert(strlen(stem) > 0);
    assert(strlen(type) > 0);

    fprintf(f, "typedef struct { %s value; } %s_t;\n", type, stem);
    fprintf(f, "static inline void %s_set(%s_t *p, %s value)\n", stem, stem, type);
    fprintf(f, "{\n");
    fprintf(f, "    assert(p != NULL);\n");
    fprintf(f, "    assert(value >= %d && value <= %d);\n", from, to);
    fprintf(f, "    p->value = value;\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");

    fprintf(f, "static inline %s %s_get(const %s_t *p)\n", type, stem, stem);
    fprintf(f, "{\n");
    fprintf(f, "    assert(p != NULL);\n");
    fprintf(f, "    return p->value;\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");

    fprintf(f, "static inline int %s_equal(const %s_t *p1, const %s_t *p2)\n", stem, stem, stem);
    fprintf(f, "{\n");
    fprintf(f, "    assert(p1 != NULL);\n");
    fprintf(f, "    assert(p2 != NULL);\n");
    fprintf(f, "    return p1->value == p2->value;\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");
}

static void process(FILE *f, const char *src)
{
    const char *stem, *type;

    stem = get_stem(src);
    type = get_type(src);

    if (strcmp(type, "string") == 0) {
        size_t count = get_count(src);
        process_string(f, stem, count);
        return;
    }

    // Right, so no strings at this point.
    // We can therefore use the length for something saner, like ranges.
    int from, to;
    get_from_to(src, &from, &to);

    if (from != 0 || to != 0) {
        process_ranged_integer(f, stem, type, from, to);
        return;
    }

    fprintf(f, "typedef struct { %s value; } %s_t;\n", type, stem);
    fprintf(f, "static inline void %s_set(%s_t *p, %s value)\n", stem, stem, type);
    fprintf(f, "{\n");
    fprintf(f, "    assert(p != NULL);\n");
    fprintf(f, "    p->value = value;\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");

    fprintf(f, "static inline %s %s_get(const %s_t *p)\n", type, stem, stem);
    fprintf(f, "{\n");
    fprintf(f, "    assert(p != NULL)\n");
    fprintf(f, "    return p->value;\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");

    fprintf(f, "static inline int %s_equal(const %s_t *p1, const %s_t *p2)\n", stem, stem, stem);
    fprintf(f, "{\n");
    fprintf(f, "    assert(p != NULL)\n");
    fprintf(f, "    return p1->value == p2->value;\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");
}

static void add_initial_stuff(FILE *f)
{
    fprintf(f, "#ifndef REPLACEME_H\n");
    fprintf(f, "#define REPLACEME_H\n");
    fprintf(f, "\n");
    fprintf(f, "#include <assert.h>\n");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "#include <string.h>\n");
    fprintf(f, "\n");
}

static void add_final_stuff(FILE *f)
{
    fprintf(f, "#endif\n");
}

int main(int argc, char *argv[])
{
    int i;
    FILE *f = stdout;

    add_initial_stuff(f);

    for (i = 1; i < argc; i++)
        process(f, argv[i]);

    add_final_stuff(f);
}


