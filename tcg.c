#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

/*
TODO:
- Add support for serializing functions to FILE*. OK.
- Add man page. OK
- Add GPL licence text to outfile
- Add long options
- Consider changing stdint.h to inttypes.h. OK
- Maybe add collection interfaces and types for storing objects in collections.
  We already have the stack interface, how about list(and maybe set and tree)?
  (Hmm, the meta_list ADT has so many member functions compared to the meta_stack
  ADT. Making the generated list interface type safe would be a real PITA.
- Maybe, just maybe, add support for postgres I/O
- TODO 20071209: Maybe add inplace constructors and init() functions? Nice on
  embedded targets with special mem handling.
*/

/* DONE
 * Added support for hybrid ADT's and static inline setters/getters. See -H.
 */

/* Pointers to the misc options */
const char* g_name = NULL;
const char* g_memberspec = NULL;
const char* g_prefix = NULL;
const char* g_basename = NULL;
const char* g_copyright = NULL;

int g_generate_comment_template = 0;
int g_generate_packfunctions = 0;
int g_generate_db_functions = 0;
int g_serialize = 0;
int g_write_xml = 0;
int g_verbose = 0;
int g_hybrid = 0;
int g_man_pages = 0;

/* We can add additional datastructures for the adt as well,
 * simple stuff like lists and stacks. This way we get typesafe
 * lists&stacks for the ADT.
 */
int g_add_stack_interface = 0;
int g_add_check_program = 0;

/*
 * Data structure we use to store the memberspec after parsing.
 * We can have n members, each member has a name and a datatype
 * and an optional sizespec. Let's make things easy and limit ourselves
 * to MEMBERS_MAX members, that way we can use an array to store
 * the spec. Let's also use a MEMBERNAME_MAX limitation to avoid mallocs.
 */
#define MEMBERS_MAX 100
#define MEMBERNAME_MAX 100
enum datatype {
    dtUnknown,
    dtInt,
    dtTime_t,
    dtSize_t,
    dtUint,
    dtd8,
    dtd16,
    dtd32,
    dtd64,
    dtu8,
    dtu16,
    dtu32,
    dtu64,
    dtFloat,
    dtDouble,
    dtChar,
    dtCharPointer,
    dtCharArray,
    dtUCharArray,
    dtVoidPointer,
    dtLongInt,
    dtLongLongInt,
    dtLongUint,
    dtLongLongUint
};

/*
 * Here we store the member specification.
 * Big note (20070114):
 * Support for BerkeleyDB is brand new and probably broken, hehe.
 * It's easy to generate the db code itself, the hard part is
 * to handle keys without resorting to a huge framework.
 */
size_t nmembers = 0;
struct memberspec {
    char name[MEMBERNAME_MAX + 1];
    enum datatype dt;
    int size_specified;
    size_t size;
} members[ MEMBERS_MAX];

/* DO NOT CHANGE THE VALUES OF THE LITERALS BELOW.
 * They are used as array index values and changing them will crash everything.
 * Note: A rewrite is probably in place...
 */
#define COPY_STRAIGHT   0
#define COPY_PRESIZED   1
#define COPY_PRESIZ_U   2
#define COPY_STRCPY     3
#define COPY_GRAB_PTR   4

/* Used when we assign new values in the set_ functions */
static const char* assignment_templates[] = {
    /* Straight assignment */
    "	assert(p != NULL);\n"
    "	p->%s = val;\n"
    ,

    /* Copy a string to a presized char array */
    "	size_t n;\n"
    "\n"
    "	assert(p != NULL);\n"
    "	assert(val != NULL);\n"
    "\n"
    "	n = strlen(val) + 1;\n"
    "	if (n > sizeof p->%s) {\n"
    "		errno = ENOSPC;\n"
    "		return 0;\n"
    "	}\n"
    "\n"
    "	memcpy(p->%s, val, n);\n"
    "	return 1;\n"
    ,

    /* Copy a string to a presized unsigned char array */
    "	assert(p != NULL);\n"
    "	assert(val != NULL);\n"
    "\n"
    "	if (strlen((const char*)val) >= sizeof p->%s) {\n"
    "		errno = ENOSPC;\n"
    "		return 0;\n"
    "	}\n"
    "\n"
    "	strcpy((char*)p->%s, (const char*)val);\n"
    "	return 1;\n"
    ,
    /* Copy a string to a char pointer */
    "	assert(p != NULL);\n"
    "	assert(val != NULL);\n"
    "\n"
    "	free(p->%s);\n"
    "	if ((p->%s = malloc(strlen(val) + 1)) == NULL)\n"
    "		return 0;\n"
    "\n"
    "	strcpy(p->%s, val);\n"
    "	return 1;\n"
    ,
    /* Copy a pointer by freeing the previous version
     * and then just assigning the new version */
    "	assert(p != NULL);\n"
    "	free(p->%s);\n"
    "	p->%s = val;\n"
};

/*
 * used when we copy values from one instance to another,
 * as in foo_copy(foo dest, const foo src)
 */
const struct tmpl {
    int namecount; /* How many times do we print name? */
    const char* template;
} copy_templates[] = {
    /* Straight assignment */
    {2,
        "	dest->%s = src->%s;\n"
        "\n"
    },

    /* Copy presized char array */
    {2,
        "	strcpy(dest->%s, src->%s);\n"
        "\n"
    },

    /* Copy presized unsigned char array */
    {3,
        "	memcpy(dest->%s, src->%s, sizeof dest->%s);\n"
        "\n"
    },

    /* Copy a char* to a char* */
    {8,
        "	free(dest->%s);\n"
        "	size_t n%s = strlen(src->%s) + 1;\n"
        "	if ((dest->%s = malloc(n%s)) == NULL)\n"
        "		return 0;\n"
        "\n"
        "	memcpy(dest->%s, src->%s, n%s);\n"
        "\n"
    },

    /* Copy a pointer by freeing the previous version
     * and then just assigning the new version */
    {3,
        "	free(dest->%s);\n"
        "	dest->%s = src->%s;\n"
        "\n"
    }
};

/* Write a member to the file. We use fwrite() in most cases except for strings. */
const struct tmpl write_templates[] = {
    /* COPY_STRAIGHT  */
    {2,
        "	if (fwrite(&p->%s, sizeof p->%s, 1, f) != 1)\n"
        "		return 0;\n"
        "\n"
    },

    /* COPY_PRESIZED */
    {2,
        "	if (fwrite(&p->%s, sizeof p->%s, 1, f) != 1)\n"
        "		return 0;\n"
        "\n"
    },

    /* COPY_PRESIZE_U */
    {2,
        "	if (fwrite(&p->%s, sizeof p->%s, 1, f) != 1)\n"
        "		return 0;\n"
        "\n"
    },

    /* COPY_STRCPY, write a char* with a size_t length specifier first. */
    {2,
        "	/* Write length of string, then string excl the \\0 */\n"
        "	if (p->%s == NULL)\n"
        "		cb = 0;\n"
        "	else\n"
        "		cb = strlen(p->%s);\n"
        "	if (fwrite(&cb, sizeof cb, 1, f) != 1)\n"
        "		return 0;\n"
        "	if (p->%s != NULL && fprintf(f, \"%%s\", p->%s) != (int)cb)\n"
        "		return 0;\n"
        "\n"
    },
    { 0, "WTF am I doing here?" }
};

const struct tmpl read_templates[] = {
    /* COPY_STRAIGHT  */
    {2,
        "	if (fread(&p->%s, sizeof p->%s, 1, f) != 1)\n"
        "		return 0;\n"
        "\n"
    },

    /* COPY_PRESIZED */
    {2,
        "	if (fread(&p->%s, sizeof p->%s, 1, f) != 1)\n"
        "		return 0;\n"
        "\n"
    },

    /* COPY_PRESIZE_U */
    {2,
        "	if (fread(&p->%s, sizeof p->%s, 1, f) != 1)\n"
        "		return 0;\n"
        "\n"
    },

    /* COPY_STRCPY, read a char* with a size_t length specifier first. */
    {6,
        "	/* read length of string, then string excl the \\0 */\n"
        "	if (fread(&cb, sizeof cb, 1, f) != 1)\n"
        "		return 0;\n"
        "\n"
        "	free(p->%s); /* Free previous version. realloc() may work too. */\n"
        "	if (cb == 0)\n"
        "		p->%s = NULL;\n"
        "	else {\n"
        "		if ((p->%s = malloc(cb + 1)) == NULL)\n"
        "			return 0;\n"
        "\n"
        "		*p->%s = '\\0'; /* In case fread() fails */\n"
        "		if (fread(p->%s, cb, 1, f) != cb)\n"
        "			return 0;\n"
        "		p->%s[cb] = '\\0';\n"
        "	}\n"
        "\n"
    },
    { 0, "WTF am I doing here?" }
};

static struct map_type {
    const char* type;	/* s,i, lu, et al */
    const char* fmt;	/* printf() format string */
    int sizespec;		/* 1 if type+optional sizespec is a match. Used to differ between %s and %Ns */
    int points_to_mem;	/* 1 if the member points to memory that needs to be freed when we destroy the adt */
    int initializer;    /* Type of initializer, either 0 for straight foo=0, 1 for foo[0] = '\0', 2 for *foo = '\0' , 3 for foo = NULL; */

    int returns_status; /* Does the set function return a status code or not */
    int can_pack;		/* Can we pack this data type into an unsigned char buffer? */
    int pack_size;		/* How long is the data type when we pack it? 0 means char arr and fixed len */
    int can_serialize;	/* Boolean, can we write it to file or not? */
    int assign_template;/* Index to array of assignment and copy template snippets */
    enum datatype dt;   /* Internal datatype */
    const char* cdecl;	/* Declaring vars */
    const char* cparam; /* Param type for access functions(set) */
    const char* cret;	/* Return type for access functions (get) */
} map[] = {
    { "s",	"\"s\"",	1, 0, 1, 1, 1, 0, 1, COPY_PRESIZED,	dtCharArray,	"char %s[%lu]",				"const char*",			"const char*" },
    { "S",	"\"s\"",	1, 0, 1, 1, 1, 0, 1, COPY_PRESIZ_U,	dtUCharArray,	"unsigned char %s[%lu]",	"const unsigned char*",	"const unsigned char*" },
    { "s",	"\"s\"",	0, 1, 3, 1, 0, 0, 1, COPY_STRCPY,	dtCharPointer,	"char* %s",					"const char*",			"const char*" },

    { "d",	"\"d\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtInt,			"int %s",					"int",				"int" },
    { "t",	"\"t\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtTime_t,		"time_t %s",				"time_t",			"time_t" },
    { "u",	"\"u\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtUint,			"unsigned int %s",			"unsigned int",		"unsigned int" },
    { "z",	"\"zu\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtSize_t,		"size_t %s",				"size_t",			"size_t" },

    { "d8",	"PRId8",	0, 0, 0, 0, 1, 1, 1, COPY_STRAIGHT,	dtd8,			"int8_t %s",            "int8_t",   "int8_t" },
    { "d16","PRId16",	0, 0, 0, 0, 1, 2, 1, COPY_STRAIGHT,	dtd16,  		"int16_t %s",           "int16_t",  "int16_t" },
    { "d32","PRId32",	0, 0, 0, 0, 1, 4, 1, COPY_STRAIGHT,	dtd32,  		"int32_t %s",           "int32_t",  "int32_t" },
    { "d64","PRId64",	0, 0, 0, 0, 1, 8, 1, COPY_STRAIGHT,	dtd64,  		"int64_t %s",           "int64_t",  "int64_t" },
    { "u8",	"PRIu8",	0, 0, 0, 0, 1, 1, 1, COPY_STRAIGHT,	dtu8,   		"uint8_t %s",           "uint8_t",  "uint8_t" },
    { "u16","PRIu16",	0, 0, 0, 0, 1, 2, 1, COPY_STRAIGHT,	dtu16,  		"uint16_t %s",          "uint16_t", "uint16_t" },
    { "u32","PRIu32",	0, 0, 0, 0, 1, 4, 1, COPY_STRAIGHT,	dtu32,  		"uint32_t %s",          "uint32_t", "uint32_t" },
    { "u64","PRIu64",	0, 0, 0, 0, 1, 8, 1, COPY_STRAIGHT,	dtu64,  		"uint64_t %s",          "uint64_t", "uint64_t" },

    { "f",	"\"f\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtFloat,		"float %s",				"float",				"float" },
    { "g",	"\"g\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtDouble,   	"double %s",			"double",				"double" },
    { "p",	"NONE",		0, 1, 3, 0, 0, 0, 0, COPY_GRAB_PTR,	dtVoidPointer,	"void* %s", 			"void*",				"void*" },
    { "ld",	"\"ld\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtLongInt,  	"long %s",  			"long",					"long" },
    { "lu",	"\"lu\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtLongUint, 	"unsigned long %s", 	"unsigned long",		"unsigned long" },
    { "lld","\"lld\"",	0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtLongLongInt,	"long %s",  			"long long",			"long long" },
    { "llu","\"llu\"",  0, 0, 0, 0, 0, 0, 1, COPY_STRAIGHT,	dtLongLongUint,	"unsigned long long %s","unsigned long long",	"unsigned long long" },
};

static void p(FILE* f, const char* fmt, ...)
    __attribute__ ((format(printf, 2, 3)));

static void p(FILE* f, const char* fmt, ...)
{
    int rc;
    va_list ap;

    va_start(ap, fmt);
    rc = vfprintf(f, fmt, ap);
    va_end(ap);

    if (rc <= 0) {
        perror("vfprintf");
        exit(EXIT_FAILURE);
    }
}


static const char* ltrim(const char* s)
{
    while (isspace((unsigned char)*s))
        s++;

    return s;
}

/* Remove trailing spaces */
static void rtrim(char *s)
{
    size_t i = strlen(s);
    if (i > 0) {
        i--;
        while (i > 0 && isspace((unsigned char)s[i]))
            s[i--] = '\0';
    }
}

static const char* filename(const char* suffix)
{
    const char *s;
    static char sz[10000];
    if (g_basename != NULL)
        s = g_basename;
    else
        s = g_name;

    sprintf(sz, "%s.%s", s, suffix);
    return sz;
}

static void upper(char *s)
{
    while (*s != '\0') {
        if (islower((unsigned char)*s))
            *s = toupper((unsigned char)*s);
        s++;
    }
}

struct map_type* lookup_map(struct memberspec* pm)
{
    size_t i, nelem = sizeof map / sizeof *map;
    for (i = 0; i < nelem; i++) {
        if (map[i].dt == pm->dt)
            return &map[i];
    }

    fprintf(stderr, "Syntax error: type not found.\n");
    exit(EXIT_FAILURE);
}

int can_pack(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->can_pack;
}

static int can_serialize(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->can_serialize;
}

size_t pack_size(struct memberspec *pm)
{
    if (pm->dt == dtCharArray || pm->dt == dtUCharArray) {
        assert(pm->size_specified);
        return pm->size;
    }
    else {
        struct map_type* m = lookup_map(pm);
        return m->pack_size;
    }
}

int returns_status(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->returns_status;
}

int points_to_mem(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->points_to_mem;
}

int get_initializer_type(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->initializer;
}

static const char* get_assignment_code(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return assignment_templates[m->assign_template];
}

static int get_copy_count(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return copy_templates[m->assign_template].namecount;
}

static const char* get_copy_code(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return copy_templates[m->assign_template].template;
}

static int get_write_count(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return write_templates[m->assign_template].namecount;
}

static const char* get_write_code(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return write_templates[m->assign_template].template;
}

static int get_read_count(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return read_templates[m->assign_template].namecount;
}

static const char* get_read_code(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return read_templates[m->assign_template].template;
}

const char* get_cdecl(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->cdecl;
}

const char* c_return_value(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->cret;
}

const char* get_cparam(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->cparam;
}

const char* get_fmt(struct memberspec *pm)
{
    struct map_type* m = lookup_map(pm);
    return m->fmt;
}

static void map_type(const char* type, struct memberspec* pm)
{

    size_t i, nelem = sizeof map / sizeof *map;
    for (i = 0; i < nelem; i++) {
        if (strcmp(map[i].type, type) == 0) {
            if ((map[i].sizespec && pm->size_specified)
            || (!map[i].sizespec && !pm->size_specified)) {
                pm->dt = map[i].dt;
                return;
            }
        }
    }

    fprintf(stderr, "Syntax error: Unknown type(%s) found.\n", type);
    exit(EXIT_FAILURE);
}

/* Return a pointer to the start of the n'th memberspec in the string s
 * The first character of the name == start of spec.
 */
static const char* get_spec_start(const char* s, size_t n)
{
    /* The first spec starts here */
    if (n == 0)
        return s;

    for (;;) {
        if (*s == ';')
            n--;
        else if (*s == '\0') {
            /* Premature end of input */
            fprintf(stderr, "Syntax error in memberspec, premature end of input\n");
            exit(EXIT_FAILURE);
        }

        s++;
        if (n == 0)
            return s;
    }
}

static void get_memberspec(const char* s, size_t i, struct memberspec* pm)
{
    char type[200] = "";

    pm->name[0] = '\0';
    pm->size_specified = 0;
    pm->size = 0;

    s = get_spec_start(s, i);
    i = 0;
    while (*s != '\0' && *s != '=' && i < sizeof pm->name)
        pm->name[i++] = *s++;
    pm->name[i] = '\0';

    if (*s != '=') {
        fprintf(stderr, "%s:Syntax error in memberspec, missing =\n", g_name);
        exit(EXIT_FAILURE);
    }

    s++; /* Skip = */
    if (*s != '%') {
        fprintf(stderr, "%s:Syntax error in memberspec, missing %%\n", g_name);
        exit(EXIT_FAILURE);
    }
    s++; /* Skip % */

    /* Now parse the typespec, we may have an optional length specifier */
    while (isdigit((unsigned char)*s)) {
        pm->size = pm->size * 10 + (*s - '0');
        pm->size_specified = 1;
        s++;
    }

    /* Copy the typespec to a separate string and then map it */
    i = 0;
    while (*s != '\0' && *s != ';')
        type[i++] = *s++;

    if (*s != ';') {
        fprintf(stderr, "%s: Syntax error in memberspec, missing ;\n", g_name);
        exit(EXIT_FAILURE);
    }

    map_type(type, pm);
}

static int use_c99_types(void)
{
    size_t i;

    for (i = 0; i < nmembers; i++) {
        if (members[i].dt == dtd8
        || members[i].dt == dtd16
        || members[i].dt == dtd32
        || members[i].dt == dtd64
        || members[i].dt == dtu8
        || members[i].dt == dtu16
        || members[i].dt == dtu32
        || members[i].dt == dtu64)
            return 1;
    }

    return 0;
}

static void show_usage(void)
{
    FILE *f = stdout;
/* Odd indentation to make the actual text fit in a 80 col window */
p(f, "tcg is short for Tiny Code Generator and is a program which creates\n");
p(f, "an implementation of an abstract data type (ADT) in C.\n");
p(f, "\n");
p(f, "USAGE:\n");
p(f, "   tcg options\n");
p(f, "OPTIONS\n");
p(f, "   -n name\n");
p(f, "      Name of the ADT. \n");
p(f, "\n");
p(f, "   -m memberspec\n");
p(f, "      Specifies the members in the ADT. We use (a slightly modified)\n");
p(f, "      printf() syntax to specify the type of the members.\n");
p(f, "      Each memberspec is *terminated* with a semicolon(;).\n");
p(f, "         %%d   int\n");
p(f, "         %%t   time_t\n");
p(f, "         %%z   size_t\n");
p(f, "         %%d8  C99 int8_t\n");
p(f, "         %%d16 C99 int16_t\n");
p(f, "         %%d32 C99 int32_t\n");
p(f, "         %%d64 C99 int64_t\n");
p(f, "         %%u   unsigned int\n");
p(f, "         %%u8  C99 uint8_t\n");
p(f, "         %%u16 C99 uint16_t\n");
p(f, "         %%u32 C99 uint32_t\n");
p(f, "         %%u64 C99 uint64_t\n");
p(f, "         %%f   float\n");
p(f, "         %%g   double\n");
p(f, "         %%s   char*\n");
p(f, "         %%Ns  char array, where N is number of characters including\n");
p(f, "               the '\\0' string terminator.\n");
p(f, "         %%p   pointer to void.\n");
p(f, "      Example:\n");
p(f, "         -m \"name=%%20s;age=%%d;average_salary=%%g;\"\n");
p(f, "\n");
p(f, "   -p prefix\n");
p(f, "      A prefix to be used in the guard macro name. If the ADT is\n");
p(f, "      named foo, a guard macro will be named FOO_H unless a prefix\n");
p(f, "      is used as well. If the prefix is MYLIB, the guard will be \n");
p(f, "      named MYLIB_FOO_H\n");
p(f, "\n");
p(f, "   -b basename\n");
p(f, "      tcg will create two files, a header file and a c file. The\n");
p(f, "      basename for these files will be the same as the name of\n");
p(f, "      the adt unless -b basename is used.\n");
p(f, "\n");
p(f, "   -C\n");
p(f, "      Generate a check program, which will test the ADT.\n");
p(f, "\n");
p(f, "   -H\n");
p(f, "     Create a hybrid ADT, where the struct definition is in the header file,\n");
p(f, "     along with the normal typedef. A hybrid ADT will also implement all\n");
p(f, "     access functions as inline functions in the header file.\n");
p(f, "\n");
p(f, "   -P\n");
p(f, "     Generate pack functions. When this option is used, three functions\n");
p(f, "     are added to the ADT. These three functions will be named adtname_pack(),\n");
p(f, "     adtname_unpack() and adtname_packsize(). Not that not all data types\n");
p(f, "     can be packed into a char buffer, only char arrays and the C99 fixed\n");
p(f, "     size integers(intX_t and uintX_t) will be packed/unpacked and included\n");
p(f, "     in the size computation. The reason is that we only pack data types\n");
p(f, "     we can represent on all platforms.\n");
p(f, "     char[] members will be packed with exact max length, including\n");
p(f, "     the trailing zero.\n");
p(f, "     We do not use the ntohl() function(and friends) since we want\n");
p(f, "     the code to work on non-POSIX platforms. For now we always do\n");
p(f, "     the conversion to/from NBO.\n");
p(f, "\n");
p(f, "   -v Set verbose flag\n");
p(f, "\n");
p(f, "   -h Print this text\n");
p(f, "\n");
p(f, "   -c copyright. Add copyright notice.\n");
p(f, "\n");
p(f, "   -S\n");
p(f, "     Add a type safe stack interface\n");
p(f, "   -M\n");
p(f, "     Add man page templates for the member functions\n");
p(f, "   -F\n");
p(f, "     Add functions to read and write the ADT from/to a FILE*\n");
p(f, "   -D\n");
p(f, "     Add functions to read and write the ADT from/to a Berkeley DB file\n");
p(f, "   -X\n");
p(f, "     Add functions to write the ADT to a FILE*, formatted as XML\n");
p(f, "\n");
}

static void parse_options(int argc, char *argv[])
{
    int c;
    extern char* optarg;

    while ((c = getopt(argc, argv, "vhn:m:p:b:c:d:SCDPMHFX")) != -1) {
        switch (c) {
            case 'H':
                g_hybrid = 1;
                break;

            case 'v':
                g_verbose = 1;
                break;

            case 'M':
                g_man_pages = 1;
                break;

            case 'n':
                g_name = optarg;
                break;

            case 'm':
                g_memberspec = optarg;
                break;

            case 'p':
                g_prefix = optarg;
                break;

            case 'b':
                g_basename = optarg;
                break;

            case 'c':
                g_copyright = optarg;
                break;

            case 'S':
                g_add_stack_interface = 1;
                break;

            case 'C':
                g_add_check_program = 1;
                break;

            case 'P':
                g_generate_packfunctions = 1;
                break;

            case 'D':
                g_generate_db_functions = 1;
                break;

            case 'F':
                g_serialize = 1;
                break;

            case 'X':
                g_write_xml = 1;
                break;

            case 'h':
                show_usage();
                exit(EXIT_SUCCESS);

            default:
                show_usage();
                exit(EXIT_FAILURE);
        }
    }
}

/* check that all the params we need was provided */
static void check_options(void)
{
    int ok = 1;
    if (g_name == NULL || strlen(g_name) == 0) {
        fprintf(stderr, "-n name is required\n");
        ok = 0;
    }

    /* We now allow empty structs */
    if (g_memberspec == NULL || strlen(g_memberspec) == 0) {
        fprintf(stderr, "-m memberspec is required\n");
        ok = 0;
    }

    if (!ok)
        exit(EXIT_FAILURE);
}

/* Check stuff and warn the user if we find inconsistencies or errors */
static void check_semantics(void)
{
    /* Warn if we want to pack structs with unpackable members */
    size_t i;

    if (g_generate_packfunctions) {
        for (i = 0; i < nmembers; i++) {
            if (!can_pack(&members[i])) {
                fprintf(stderr, "warning: The member %s cannot be packed\n",
                    members[i].name);
            }
        }
    }

    /* We cannot serialize void* members as we do not know their sizes */
    if (g_serialize || g_write_xml) {
        for (i = 0; i < nmembers; i++) {
            if (!can_serialize(&members[i]))
                fprintf(stderr, "warning: The member %s cannot be serialized\n",
                    members[i].name);
        }
    }
}

/* Count the number of memberspecs in s */
static size_t count_memberspecs(const char* s)
{
    size_t n = 0;

    while (*s != '\0') {
        if (*s == ';')
            n++;
        s++;
    }

    return n;
}

/* Parse the memberspec string and add the members to the members array.
 * Die if an error occurs or the input is invalid. The parsing itself is
 * straight forward. Each member is specified like this: name=typespec
 * and members are separated by space. We use a couple of utility functions
 * which count the number of specs and retrieves one spec at a time.
 */
static void parse_memberspec(void)
{
    size_t i, nspecs;
    char* s;
    struct memberspec ms;

    /* Empty structure? */
    if (g_memberspec == NULL)
        return;

    /* We need a copy of the memberspec to be able to write to it.
     * (I know that we in theory could write to g_memberspec, but
     * that just isn't nice...)
     */
    if ((s = malloc(strlen(g_memberspec) + 1)) == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    strcpy(s, ltrim(g_memberspec));
    rtrim(s);

    nspecs = count_memberspecs(s);
    if (nspecs == 0) {
        fprintf(stderr, "member count is zero. ADT has no members\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < nspecs; i++) {
        get_memberspec(s, i, &ms);

        /* Now add the newly found memberspec to the array */
        members[nmembers++] = ms;
    }
}

static FILE* open_file(const char* suffix)
{
    FILE* f;

    if ((f = fopen(filename(suffix), "w")) == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    return f;
}

static void add_guard_start(FILE *f)
{
    char guard[10000];

    /* Create the proper guard */
    if (g_prefix != NULL)
        sprintf(guard, "%s_%s_H", g_prefix, g_name);
    else
        sprintf(guard, "%s_H", g_name);

    upper(guard);

    /* guard start */
    p(f, "#ifndef %s\n#define %s\n\n", guard, guard);
}

static void add_guard_end(FILE *f)
{
    /* guard end */
    p(f, "#endif\n");
}

static void add_cplusplus_wrapper_start(FILE *f)
{
    /* C++ wrapper start */
    p(f, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
}

static void add_cplusplus_wrapper_end(FILE *f)
{
    p(f, "#ifdef __cplusplus\n}\n#endif\n\n");
}


static void add_adt_declarations(FILE *f)
{
    p(f, "/* The ADT(s) we declare */\n");
    p(f, "typedef struct %s_tag* %s;\n", g_name, g_name);
    if (g_add_stack_interface)
        p(f, "typedef struct %s_stack_tag* %s_stack;\n", g_name, g_name);
    p(f, "\n");
}

static const char* getter_name(const char* adt, const char* member)
{
    static char buf[1024];

    strcpy(buf, adt);
    strcat(buf, "_get_");
    strcat(buf, member);
    return buf;
}

static const char* setter_name(const char* adt, const char* member)
{
    static char buf[1024];
    strcpy(buf, adt);
    strcat(buf, "_set_");
    strcat(buf, member);
    return buf;
}

static void define_setters_and_getters(FILE *f)
{
    size_t i;
    const char* scope = "";

    if (g_hybrid)
        scope = "static inline ";

    p(f, "/* Get functions for the members of the %s ADT */\n", g_name);
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        const char* fname = getter_name(g_name, pm->name);
        p(f, "%s%s %s(%s p)\n",
            scope,
            c_return_value(pm),
            fname,
            g_name);
        p(f, "{\n");
        p(f, "\tassert(p != NULL);\n");
        p(f, "\treturn p->%s;\n", pm->name);
        p(f, "}\n");
        p(f, "\n");
    }

    p(f, "/* Assignment functions for the members of the %s ADT */\n", g_name);
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        const char* cc = get_assignment_code(pm);
        const char* fname = setter_name(g_name, pm->name);
        const char* return_type = "void";
        if (returns_status(pm))
            return_type = "int";

        p(f, "%s%s %s(%s p, %s val)\n",
            scope,
            return_type,
            fname,
            g_name,
            get_cparam(pm));

        p(f, "{\n");
        p(f, cc, pm->name, pm->name, pm->name, pm->name, pm->name, pm->name);

        p(f, "}\n");
        p(f, "\n");
    }
}

static void declare_setters_and_getters(FILE *f)
{
    size_t i;

    p(f, "/* Get functions for the members of the %s ADT */\n", g_name);
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        p(f, "%s %s_get_%s(%s p);\n",
            c_return_value(pm),
            g_name,
            members[i].name,
            g_name);
    }
    p(f, "\n");

    p(f, "/* Assignment(set) functions for the %s ADT */\n", g_name);
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        const char* return_type = "void";
        if (returns_status(pm))
            return_type = "int";

        p(f, "%s %s_set_%s(%s p, %s val);\n",
            return_type, g_name, pm->name, g_name, get_cparam(pm));
    }
    p(f, "\n");
}

static void add_setters_and_getters(FILE *f)
{
    if (g_hybrid)
        define_setters_and_getters(f);
    else
        declare_setters_and_getters(f);
}

static void define_struct(FILE* f)
{
    size_t i;

    p(f, "/* Definition of the %s ADT */\n", g_name);
    p(f, "struct %s_tag {\n", g_name);
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        p(f, "\t");
        if (pm->size_specified)
            p(f, get_cdecl(pm), pm->name, (unsigned long)pm->size);
        else
            p(f, get_cdecl(pm), pm->name);

        p(f, ";\n");
    }
    p(f, "};\n\n");
}

static const char* ctor_name(const char* adt)
{
    static char buf[1024];

    strcpy(buf, adt);
    strcat(buf, "_new");
    return buf;
}

static const char* dtor_name(const char* adt)
{
    static char buf[1024];

    strcpy(buf, adt);
    strcat(buf, "_free");
    return buf;
}

static const char* copy_name(const char* adt)
{
    static char buf[1024];

    strcpy(buf, adt);
    strcat(buf, "_copy");
    return buf;
}


static void add_standard_function_declarations(FILE *f)
{
    const char* ctor = ctor_name(g_name);
    const char* dtor = dtor_name(g_name);
    const char* copy = copy_name(g_name);

    p(f, "/* constructor and destructor for the %s ADT */\n", g_name);
    p(f, "%s %s(void);\n", g_name, ctor);
    p(f, "void %s(%s p);\n", dtor, g_name);
    p(f, "void %s_init(%s p);\n", g_name, g_name);
    p(f, "%s %s_inplace_new(void *mem);\n", g_name, g_name);

    p(f, "int %s(%s dest, const %s src);\n", copy, g_name, g_name);
    p(f, "\n");
}

static void add_pack_declarations(FILE *f)
{
    if (g_generate_packfunctions) {
        p(f, "/* Functions that pack the struct from/to a unsigned char buffer */\n");
        p(f, "size_t %s_pack (%s p, unsigned char* buf, size_t cb);\n", g_name, g_name);
        p(f, "size_t %s_unpack(%s p, const unsigned char* buf, size_t cb);\n", g_name, g_name);
        p(f, "size_t %s_packsize(void);\n", g_name);
        p(f, "\n");
    }
}

static void add_serialize_declarations(FILE *f)
{
    if (g_serialize) {
        p(f, "/* Functions that read/write the struct from/to a FILE* */\n");
        p(f, "int %s_read (%s p, FILE* f);\n", g_name, g_name);
        p(f, "int %s_write(%s p, FILE* f);\n", g_name, g_name);
        p(f, "\n");
    }

    if (g_write_xml) {
        p(f, "int %s_write_xml(%s p, FILE* f);\n", g_name, g_name);
    }
}

static void add_stack_declaration(FILE *f)
{
    if (g_add_stack_interface) {
        char stackname[10000];
        sprintf(stackname, "%s_stack", g_name);
        const char* ctor = ctor_name(stackname);
        const char* dtor = dtor_name(stackname);

        p(f, "/* stack interface for the %s ADT */\n", g_name);
        p(f, "%s %s(void);\n", stackname, ctor);
        p(f, "void %s(%s p);\n", dtor, stackname);
        p(f, "int %s_push(%s p, %s val);\n", stackname, stackname, g_name);
        p(f, "void %s_pop(%s p);\n", stackname, stackname);
        p(f, "%s %s_top(%s p);\n", g_name, stackname, stackname);
        p(f, "\n");
    }
}

static void add_copyright(FILE *f)
{
    if (g_copyright != NULL)
        p(f, "/* %s */\n\n", g_copyright);
}


static void generate_header(void)
{
    FILE *f;

    f = open_file("h");
    add_copyright(f);
    add_guard_start(f);

    /* Include stdint.h in the header if we use C99 datatypes */
    if (use_c99_types())
        p(f, "#include <stdint.h>\n");

    if (g_serialize || g_write_xml)
        p(f, "#include <stdio.h> /* for FILE* */\n");

    if (g_hybrid) {
        p(f, "#include <assert.h>\n");
        p(f, "#include <stdlib.h>\n");
        p(f, "#include <string.h>\n");
        p(f, "#include <errno.h>\n");
    }

    if (g_generate_packfunctions)
        p(f, "#include <stddef.h> /* for size_t */\n");

    if (use_c99_types()
    || g_generate_packfunctions
    || g_hybrid
    || g_serialize
    || g_write_xml)
        p(f, "\n");

    add_cplusplus_wrapper_start(f);
    add_adt_declarations(f);

    if (g_hybrid)
        define_struct(f);

    add_standard_function_declarations(f);
    add_setters_and_getters(f);
    add_pack_declarations(f);
    add_serialize_declarations(f);
    add_stack_declaration(f);
    add_cplusplus_wrapper_end(f);
    add_guard_end(f);
    fclose(f);
}

static void add_standard_headers(FILE* f)
{
    /* Add the standard headers we always include */
    p(f, "#include <stdlib.h>\n");
    p(f, "#include <stdio.h>\n");
    p(f, "#include <assert.h>\n");
    p(f, "#include <string.h>\n");
    p(f, "#include <errno.h>\n");
    p(f, "#include <inttypes.h>\n");

    if (g_generate_db_functions)
        p(f, "#include <db.h>\n");
    p(f, "\n");

    p(f, "#include <%s>\n", filename("h"));
    p(f, "\n");
}

static void define_ctor(FILE* f)
{
    p(f, "%s %s_new(void)\n", g_name, g_name);
    p(f, "{\n");
    p(f, "\t%s p;\n", g_name);
    p(f, "\n");

    /* Allocate memory for the object */
    p(f, "\tif ((p = malloc(sizeof *p)) != NULL)\n");
    p(f, "\t\t%s_init(p);\n", g_name);
    p(f, "\n");
    p(f, "\treturn p;\n");
    p(f, "}\n");
    p(f, "\n");
}

static void define_inplace_ctor(FILE* f)
{
    p(f, "%s %s_inplace_new(void *mem)\n", g_name, g_name);
    p(f, "{\n");
    p(f, "\t%s p;\n", g_name);
    p(f, "\n");
    p(f, "\tassert(mem != NULL);\n");
    p(f, "\n");

    p(f, "\tp = mem;\n");
    p(f, "\t%s_init(p);\n", g_name);
    p(f, "\n");
    p(f, "\treturn p;\n");
    p(f, "}\n");
    p(f, "\n");
}

static void define_init(FILE* f)
{
    size_t i;

    p(f, "void %s_init(%s p)\n", g_name, g_name);
    p(f, "{\n");
    p(f, "\tassert(p != NULL);\n\n");

    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        int it = get_initializer_type(pm);
        switch (it) {
            case 0:
                p(f, "\tp->%s = 0;\n", pm->name);
                break;

            case 1:
                p(f, "\tp->%s[0] = '\\0';\n", pm->name);
                break;

            case 2:
                p(f, "\t*p->%s = '\\0';\n", pm->name);
                break;

            case 3:
                p(f, "\tp->%s = NULL;\n", pm->name);
                break;
            default:
                fprintf(stderr, "Unhandled initializer type(%d)\n", it);
                break;
        }
    }
    p(f, "}\n");
    p(f, "\n");
}

static void define_dtor(FILE* f)
{
    size_t i;

    p(f, "void %s_free(%s p)\n", g_name, g_name);
    p(f, "{\n");
    p(f, "\tif (p != NULL) {\n");
    for (i = 0; i < nmembers; i++) {
        if (points_to_mem(&members[i]))
            p(f, "\t\tfree(p->%s);\n", members[i].name);
    }
    p(f, "\t\tfree(p);\n");
    p(f, "\t}\n");
    p(f, "}\n");
    p(f, "\n");
}

static void define_copy(FILE* f)
{
    size_t i;

    p(f, "int %s_copy(%s dest, const %s src)\n", g_name, g_name, g_name);
    p(f, "{\n");
    p(f, "	assert(src != NULL);\n");
    p(f, "	assert(dest != NULL);\n");
    p(f, "\n");

    /* Copy each member */
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        const char* cc = get_copy_code(pm);
        int n = get_copy_count(pm);
        (void)n;

        /* Ugly, but works. We just add more names than any format string uses. */
        p(f, cc, pm->name, pm->name, pm->name, pm->name, pm->name, pm->name, pm->name, pm->name);
    }
    p(f, "\treturn 1;\n");
    p(f, "}\n");
    p(f, "\n");
}

static void define_write(FILE *f)
{
    size_t i;

    p(f, "int %s_write(%s p, FILE* f)\n", g_name, g_name);
    p(f, "{\n");

    /* Do we need to declare cb? Only if one of the members
     * need it. */
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];

        if (can_serialize(pm) && members[i].dt == dtCharPointer) {
            p(f, "	size_t cb;\n");
            break;
        }
    }

    p(f, "	assert(p != NULL);\n");
    p(f, "	assert(f != NULL);\n");
    p(f, "\n");

    /* Copy each member */
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        if (!can_serialize(pm))
            continue;
        const char* cc = get_write_code(pm);
        int n = get_write_count(pm);
        (void)n;

        /* Ugly, but works. We just add more names than any format string uses. */
        p(f, cc, pm->name, pm->name, pm->name, pm->name, pm->name, pm->name);
    }
    p(f, "\treturn 1;\n");
    p(f, "}\n");
    p(f, "\n");
}

static void define_read(FILE *f)
{
    size_t i;

    p(f, "int %s_read(%s p, FILE* f)\n", g_name, g_name);
    p(f, "{\n");

    /* Do we need to declare cb? Only if one of the members
     * need it. */
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];

        if (can_serialize(pm) && members[i].dt == dtCharPointer) {
            p(f, "	size_t cb;\n");
            break;
        }
    }

    p(f, "	assert(p != NULL);\n");
    p(f, "	assert(f != NULL);\n");
    p(f, "\n");

    /* Copy each member */
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        if (!can_serialize(pm))
            continue;
        const char* cc = get_read_code(pm);
        int n = get_read_count(pm);
        (void)n;

        /* Ugly, but works. We just add more names than any format string uses. */
        p(f, cc, pm->name, pm->name, pm->name, pm->name, pm->name, pm->name, pm->name, pm->name);
    }
    p(f, "\treturn 1;\n");
    p(f, "}\n");
    p(f, "\n");
}

static void define_write_xml(FILE *f)
{
    size_t i;

    p(f, "int %s_write_xml(%s p, FILE* f)\n", g_name, g_name);
    p(f, "{\n");
    p(f, "	assert(p != NULL);\n");
    p(f, "	assert(f != NULL);\n");
    p(f, "\n");

    p(f, "	fprintf(f, \"<%s>\\n\");\n", g_name);

    /* Copy each member */
    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        if (!can_serialize(pm))
            continue;

        /* NOTE: Remember to test for null values */
        if (members[i].dt == dtCharPointer) {
            p(f, "	if (p->%s == NULL)\n", pm->name);
            p(f,"		fprintf(f, \"\\t<%s></%s>\\n\");\n", pm->name, pm->name);
            p(f,"	else\n");
            p(f,"		fprintf(f, \"\\t<%s>%%\" %s \"</%s>\\n\", p->%s);\n", pm->name, get_fmt(pm), pm->name, pm->name);
        }
        else
            p(f,"	fprintf(f, \"\\t<%s>%%\" %s \"</%s>\\n\", p->%s);\n", pm->name, get_fmt(pm), pm->name, pm->name);
    }
    p(f, "	fprintf(f, \"</%s>\\n\");\n", g_name);
    p(f, "\treturn 1;\n");
    p(f, "}\n");
    p(f, "\n");
}

static void add_serialize_definitions(FILE *f)
{
    if (g_serialize) {
        define_write(f);
        define_read(f);
    }

    if (g_write_xml) {
        define_write_xml(f);
    }
}

static void generate_stack_interface(FILE *f)
{
    char stackname[10000];
    char sz[10000];

    if (!g_add_stack_interface)
        return;

    sprintf(stackname, "%s_stack", g_name);
    strcpy(sz, stackname);
    upper(sz);

    p(f, "/* stack interface for the %s ADT */\n", g_name);
    p(f, "#define %s_SIZE 10\n", sz);
    p(f, "\n");
    p(f, "struct %s_tag {\n", stackname);
    p(f, "\tsize_t nelem;\n");
    p(f, "\tsize_t used;\n");
    p(f, "\t%s* data;\n", g_name);
    p(f, "};\n");
    p(f, "\n");

    p(f, "%s %s_new(void)\n", stackname, stackname);
    p(f, "{\n");
    p(f, "	%s p;\n", stackname);
    p(f, "	if ((p = malloc(sizeof *p)) == NULL\n");
    p(f, "	||  (p->data = calloc(%s_SIZE, sizeof *p->data)) == NULL) {\n", sz);
    p(f, "		free(p);\n");
    p(f, "		p = NULL;\n");
    p(f, "	}\n");
    p(f, "	else {\n");
    p(f, "		p->nelem = %s_SIZE;\n", sz);
    p(f, "		p->used = 0;\n");
    p(f, "	}\n");
    p(f, "\n");
    p(f, "	return p;\n");
    p(f, "}\n");
    p(f, "\n");

    p(f, "void %s_free(%s p)\n", stackname, stackname);
    p(f, "{\n");
    p(f, "	if (p != NULL) {\n");
    p(f, "		free(p->data);\n");
    p(f, "		free(p);\n");
    p(f, "	}\n");
    p(f, "}\n");
    p(f, "\n");

    p(f, "int %s_push(%s p, %s val)\n", stackname, stackname, g_name);
    p(f, "{\n");
    p(f, "	assert(p != NULL);\n");
    p(f, "	assert(val != NULL);\n");
    p(f, "\n");
    p(f, "	if (p->used == p->nelem) {\n");
    p(f, "		size_t cb = p->nelem * sizeof *p->data * 2;\n");
    p(f, "		%s* tmp = realloc(p->data, cb);\n", g_name);
    p(f, "		if (tmp == NULL)\n");
    p(f, "			return 0;\n");
    p(f, "		p->data = tmp;\n");
    p(f, "		p->nelem *= 2;\n");
    p(f, "	}\n");
    p(f, "	p->data[p->used++] = val;\n");
    p(f, "	return 1;\n");
    p(f, "}\n");
    p(f, "\n");

    p(f, "void %s_pop(%s p)\n", stackname, stackname);
    p(f, "{\n");
    p(f, "	assert(p != NULL);\n");
    p(f, "	assert(p->used > 0);\n");
    p(f, "	p->used--;\n");
    p(f, "}\n");
    p(f, "\n");

    p(f, "%s %s_top(%s p)\n", g_name, stackname, stackname);
    p(f, "{\n");
    p(f, "	assert(p != NULL);\n");
    p(f, "	assert(p->used > 0);\n");
    p(f, "	return p->data[p->used - 1];\n");
    p(f, "}\n");
    p(f, "\n");

}

static void generate_check_program(FILE* f)
{
    char sz[10000];
    char stk[10000];

    if (!g_add_check_program)
        return;

    sprintf(sz, "%s_CHECK", g_name);
    upper(sz);

    sprintf(stk, "%s_stack", g_name);

    p(f, "#ifdef %s\n", sz);
    p(f, "int main(void)\n");
    p(f, "{\n");

    if (g_serialize || g_write_xml)
        p(f, "	FILE *f;\n");

    if (g_serialize) {
        p(f, "	%s rwobj;\n", g_name);
        p(f, "\n");
    }

    if (g_add_stack_interface) {
        p(f, "	size_t i, nelem = 10;\n");
        p(f, "	%s stk;\n", stk);
    }

    p(f, "	%s obj;\n", g_name);
    p(f, "	obj = %s_new();\n", g_name);

    if (g_serialize) {
        p(f, "	if ((rwobj = %s()) == NULL)\n", ctor_name(g_name));
        p(f, "		return 77;\n");
        p(f, "\n");
        p(f, "	printf(\"Writing object to file\\n\");\n");
        p(f, "	if ((f = fopen(\"%s.testing\", \"w\")) == NULL)\n", g_name);
        p(f, "		return 77;\n");
        p(f, "\n");
        p(f, "	if (!%s_write(obj, f))\n", g_name);
        p(f, "		return 77;\n");
        p(f, "\n");
        p(f, "	fclose(f);\n");
        p(f, "\n");
        p(f, "	if ((f = fopen(\"%s.testing\", \"r\")) == NULL)\n", g_name);
        p(f, "		return 77;\n");
        p(f, "\n");
        p(f, "	printf(\"Reading object from file\\n\");\n");
        p(f, "	if (!%s_read(rwobj, f))\n", g_name);
        p(f, "		return 77;\n");
        p(f, "\n");
        p(f, "	fclose(f);\n");
        p(f, "\n");
    }

    if (g_write_xml) {
        p(f, "	/* Write the object as XML */\n");
        p(f, "	if ((f = fopen(\"%s.testing\",\"w\")) == NULL)\n", g_name);
        p(f, "		return 77;\n");
        p(f, "\n");
        p(f, "	if (!%s_write_xml(obj, f))\n", g_name);
        p(f, "		return 77;\n");
        p(f, "\n");
        p(f, "	fclose(f);\n");
        p(f, "\n");
    }

    /* Free the read/write test object */
    if (g_serialize)
        p(f, "	%s(rwobj);\n", dtor_name(g_name));

    p(f, "	%s_free(obj);\n", g_name);
    p(f, "\n");

    if (g_add_stack_interface) {
        p(f, "	/* Testing stack interface */\n");
        p(f, "	stk = %s_new();\n", stk);
        p(f, "	assert(stk != NULL);\n");
        p(f, "\n");

        p(f, "	for(i = 0; i < nelem; i++) {\n");
        p(f, "		int rc;\n");
        p(f, "		obj = %s_new();\n", g_name);
        p(f, "		assert(obj != NULL);\n");
        p(f, "		rc = %s_push(stk, obj);\n", stk);
        p(f, "		assert(rc != 0);\n");
        p(f, "		printf(\"Item %%lu added\\n\", (unsigned long)i);\n");
        p(f, "	}\n");
        p(f, "\n");
        p(f, "	for(i = nelem; i-- > 0; ) {\n");
        p(f, "		obj = %s_top(stk);\n", stk);
        p(f, "		assert(obj != NULL);\n");
        p(f, "		%s_free(obj);\n", g_name);
        p(f, "		%s_pop(stk);\n", stk);
        p(f, "		printf(\"Item %%lu popped\\n\", (unsigned long)i);\n");
        p(f, "	}\n");

        p(f, "	%s_free(stk);\n", stk);
        p(f, "	\n");
    }

    if (g_generate_packfunctions && g_memberspec != NULL) {
        p(f, "	/* Testing pack() and unpack() functions */\n");
        p(f, "	{\n");
        p(f, "		%s m1 = %s_new(), m2 = %s_new();\n", g_name, g_name, g_name);
        p(f, "		size_t cb;\n");
        p(f, "		unsigned char buf[1024];\n");
        p(f, "\n");
        p(f, "		if ((cb = %s_pack(m1, buf, sizeof buf)) == 0) {\n", g_name);
        p(f, "			fprintf(stderr, \"%s_pack() failed\\n\");\n", g_name);
        p(f, "			return 77;\n");
        p(f, "		}\n");
        p(f, "\n");
        p(f, "		if (!%s_unpack(m2, buf, cb)) {\n", g_name);
        p(f, "			fprintf(stderr, \"%s_unpack() failed\\n\");\n", g_name);
        p(f, "			return 77;\n");
        p(f, "		}\n");
        p(f, "\n");
        p(f, "		if (memcmp(m1, m2, sizeof *m1)) {\n");
        p(f, "			fprintf(stderr, \"The two messages differ.\\n\");\n");
        p(f, "			return 77;\n");
        p(f, "		}\n");
        p(f, "\n");
        p(f, "		%s_free(m1);\n", g_name);
        p(f, "		%s_free(m2);\n", g_name);
        p(f, "	}\n");
        p(f, "\n");
    }

    p(f, "\treturn EXIT_SUCCESS;\n");
    p(f, "}\n");
    p(f, "#endif\n");
}

static void generate_sizefunc(FILE *f)
{
    size_t i, cb, cbFixed;

    p(f, "size_t %s_packsize(void)\n", g_name);
    p(f, "{\n");

    /* Compute size of fixed size members */
    cbFixed = 0;
    for (i = 0; i < nmembers; i++) {
        if (can_pack(&members[i])) {
            if ((cb = pack_size(&members[i])) > 0)
                cbFixed += cb;
        }
    }

    p(f, "\t/* Size of fixed size members */\n");
    p(f, "\tsize_t cb = %zu;\n", cbFixed);
    p(f, "\treturn cb;\n");
    p(f, "}\n\n");
}

/* Here we create functions that either pack or unpack the data members into a buffer.  */
static void generate_packfunc(FILE *f)
{
    size_t i;

    p(f, "size_t %s_pack(%s p, unsigned char* buf, size_t size)\n", g_name, g_name);
    p(f, "{\n");
    p(f, "	size_t i = 0, cb = %s_packsize();\n", g_name);
    p(f, "	assert(p != NULL);\n");
    p(f, "	assert(buf != NULL);\n");
    p(f, "	assert(size >= cb);\n");
    p(f, "\n");

    for (i = 0; i < nmembers; i++) {
        struct memberspec *pm = &members[i];
        if (!can_pack(pm))
            continue;

        switch (pm->dt) {
            case dtUCharArray:
            case dtCharArray:
                p(f, "\tmemcpy(&buf[i], p->%s, sizeof p->%s);\n", pm->name, pm->name);
                p(f, "\ti += sizeof p->%s;\n", pm->name);
                p(f, "\n");
                break;

            case dtd8:
            case dtu8:
                p(f, "\tbuf[i++] = p->%s;\n", pm->name);
                p(f, "\n");
                break;

            case dtd16:
            case dtu16:
                p(f, "\tbuf[i++] = (p->%s & 0xff00) >> 8;\n", pm->name);
                p(f, "\tbuf[i++] = (p->%s & 0x00ff);\n", pm->name);
                p(f, "\n");
                break;

            case dtd32:
            case dtu32:
                p(f, "\tbuf[i++] = (p->%s & 0xff000000) >> 24;\n", pm->name);
                p(f, "\tbuf[i++] = (p->%s & 0x00ff0000) >> 16;\n", pm->name);
                p(f, "\tbuf[i++] = (p->%s & 0x0000ff00) >>  8;\n", pm->name);
                p(f, "\tbuf[i++] = (p->%s & 0x000000ff);\n", pm->name);
                p(f, "\n");
                break;

            case dtd64:
            case dtu64:
                p(f, "\tbuf[i++] = (unsigned char)((p->%s & 0xff00000000000000) >> 56);\n", pm->name);
                p(f, "\tbuf[i++] = (unsigned char)((p->%s & 0x00ff000000000000) >> 48);\n", pm->name);
                p(f, "\tbuf[i++] = (unsigned char)((p->%s & 0x0000ff0000000000) >> 40);\n", pm->name);
                p(f, "\tbuf[i++] = (unsigned char)((p->%s & 0x000000ff00000000) >> 32);\n", pm->name);
                p(f, "\tbuf[i++] = (unsigned char)((p->%s & 0x00000000ff000000) >> 24);\n", pm->name);
                p(f, "\tbuf[i++] = (unsigned char)((p->%s & 0x0000000000ff0000) >> 16);\n", pm->name);
                p(f, "\tbuf[i++] = (unsigned char)((p->%s & 0x000000000000ff00) >>  8);\n", pm->name);
                p(f, "\tbuf[i++] = (unsigned char)((p->%s & 0x00000000000000ff));\n", pm->name);
                p(f, "\n");
                break;

            default:
                fprintf(stderr, "Internal error\n");
                abort();
        }
    }

    p(f, "\tassert(i <= size);\n");
    p(f, "\tassert(i == cb);\n");
    p(f, "\treturn i;\n");
    p(f, "}\n");
    p(f, "\n");
}

static void generate_unpackfunc(FILE *f)
{
    size_t i;
    const char *cdecl;

    p(f, "size_t %s_unpack(%s p, const unsigned char* buf, size_t size)\n", g_name, g_name);
    p(f, "{\n");
    p(f, "	size_t i = 0, cb = %s_packsize();\n", g_name);
    p(f, "\n");
    p(f, "	assert(p != NULL);\n");
    p(f, "	assert(buf != NULL);\n");
    p(f, "	assert(cb <= size);\n");

    for (i = 0; i < nmembers; i++) {
        struct memberspec *pm = &members[i];
        if (!can_pack(pm))
            continue;

        switch (pm->dt) {
            case dtUCharArray:
            case dtCharArray:
                p(f, "\tmemcpy(p->%s, &buf[i], sizeof p->%s);\n", pm->name, pm->name);
                p(f, "\ti += sizeof(p->%s);\n", pm->name);
                p(f, "\n");
                break;

            case dtd8:
            case dtu8:
                p(f, "\tp->%s = buf[i++];\n", pm->name);
                p(f, "\n");
                break;

            case dtd16:
            case dtu16:
                p(f, "\tp->%s  = buf[i++] << 8;\n", pm->name);
                p(f, "\tp->%s |= buf[i++];\n", pm->name);
                p(f, "\n");
                break;

            case dtd32:
            case dtu32:
                p(f, "\tp->%s  = buf[i++] << 24;\n", pm->name);
                p(f, "\tp->%s |= buf[i++] << 16;\n", pm->name);
                p(f, "\tp->%s |= buf[i++] <<  8;\n", pm->name);
                p(f, "\tp->%s |= buf[i++];\n", pm->name);
                p(f, "\n");
                break;

            case dtd64:
            case dtu64:
                cdecl = get_cparam(pm);
                p(f, "\tp->%s  = (%s)buf[i++] << 56;\n", pm->name, cdecl);
                p(f, "\tp->%s |= (%s)buf[i++] << 48;\n", pm->name, cdecl);
                p(f, "\tp->%s |= (%s)buf[i++] << 40;\n", pm->name, cdecl);
                p(f, "\tp->%s |= (%s)buf[i++] << 32;\n", pm->name, cdecl);
                p(f, "\tp->%s |= (%s)buf[i++] << 24;\n", pm->name, cdecl);
                p(f, "\tp->%s |= (%s)buf[i++] << 16;\n", pm->name, cdecl);
                p(f, "\tp->%s |= (%s)buf[i++] <<  8;\n", pm->name, cdecl);
                p(f, "\tp->%s |= buf[i++];\n", pm->name);
                p(f, "\n");
                break;

            default:
                fprintf(stderr, "Internal error\n");
                abort();
        }
    }
    p(f, "\tassert(i <= size);\n");
    p(f, "\tassert(i == cb);\n");
    p(f, "\treturn i;\n");
    p(f, "}\n");
    p(f, "\n");
}

static void generate_db_functions(FILE *f)
{
    (void)f;
}


static void generate_packfunctions(FILE *f)
{
    if (g_generate_packfunctions) {
        generate_sizefunc(f);
        generate_packfunc(f);
        generate_unpackfunc(f);
    }
}

static void generate_implementation(void)
{
    FILE *f;

    f = open_file("c");
    add_copyright(f);
    add_standard_headers(f);

    if (!g_hybrid)
        define_struct(f);

    define_ctor(f);
    define_inplace_ctor(f);
    define_init(f);
    define_dtor(f);
    define_copy(f);

    /* Now for the access(get) functions */
    if (!g_hybrid)
        define_setters_and_getters(f);

    add_serialize_definitions(f);
    generate_stack_interface(f);
    generate_packfunctions(f);
    generate_db_functions(f);
    generate_check_program(f);
    fclose(f);
}

static void create_one_manpage(
    const char* adt,
    const char* retval,
    const char* func,
    const char* arg,
    const char* desc,
    const char* see_also)
{
    char buf[1024];
    char date[1024];
    FILE* f;

    (void)desc;
    time_t now = time(NULL);
    struct tm* ptm = localtime(&now);

    strftime(date, sizeof date, "%b %d, %Y", ptm);

    sprintf(buf, "%s.3", func);
    if ((f = fopen(buf, "w")) == NULL) {
        perror(buf);
        exit(EXIT_FAILURE);
    }

    p(f, ".Dd %s\n", date);
    p(f, ".Os %s\n", adt);
    p(f, ".Dt %s\n", adt);
    p(f, ".Sh NAME\n");
    p(f, ".Nm %s()\n", func);
    p(f, ".Nd %s\n", func);
    p(f, ".Sh SYNOPSIS\n");
    p(f, "#include <%s.h>\n", adt);
    p(f, ".Fo \"%s %s\"\n", retval, func);
    p(f, "%s", arg);
    p(f, ".Fc\n");

    if (desc != NULL) {
        p(f, ".Sh DESCRIPTION\n");
        p(f, "%s\n", desc);
    }

    if (see_also != NULL) {
        p(f, ".Sh SEE ALSO\n");
        p(f, "%s", see_also);
    }
    p(f, ".Sh AUTHOR\n");
    p(f, ".An Your name goes here\n");
    fclose(f);
}

static void create_ctor_manpage(void)
{
    char desc_buf[1024];
    char sa_buf[1024];

    const char* desc = "The \n.Nm\nfunction creates a new instance of the\n.Nm %s\nADT.";
    const char* args = ".Fa \"void\"\n";
    const char* ctor = ctor_name(g_name);

    sprintf(desc_buf, desc, g_name);
    sprintf(sa_buf, ".Xr %s 3\n", dtor_name(g_name));
    create_one_manpage(g_name, g_name, ctor, args,  desc_buf, sa_buf);
}

static void create_dtor_manpage(void)
{
    char desc_buf[1024];
    char sa_buf[1024];
    char parambuf[1024];

    const char* dtor = dtor_name(g_name);

    const char* desc = "The \n.Nm\nfunction frees all memory allocated by %s and its members.";
    sprintf(desc_buf, desc, g_name);

    sprintf(parambuf, ".Fa \"%s p\"\n", g_name);

    sprintf(sa_buf, ".Xr %s 3\n", ctor_name(g_name));
    create_one_manpage(g_name, "void", dtor, parambuf, desc_buf, sa_buf);
}

static void create_copy_manpage(void)
{
    const char* copy = copy_name(g_name);
    const char* desc =
        "The\n.Nm\nfunction copies the contents of src to dest."
        "The copy will be a so called deep copy.";
    char parambuf[1024];

    sprintf(parambuf,
        ".Fa \"%s dest\"\n"
        ".Fa \"const %s src\"\n",
        g_name, g_name);

    create_one_manpage(g_name, "int", copy, parambuf,  desc, "\n");
}

static void generate_man_pages(void)
{
    if (!g_man_pages)
        return;

    char parambuf[1024];

    size_t i;


    create_ctor_manpage();
    create_dtor_manpage();
    create_copy_manpage();

    for (i = 0; i < nmembers; i++) {
        struct memberspec* pm = &members[i];
        const char* setter = setter_name(g_name, members[i].name);
        const char* getter = getter_name(g_name, members[i].name);

        const char* return_type = "void";
        if (returns_status(pm))
            return_type = "int";

        /* Param string for the setter function */
        sprintf(parambuf,
            ".Fa \"%s p\"\n"
            ".Fa \"%s val\"\n",
            g_name,
            get_cparam(pm));

        create_one_manpage(g_name, return_type, setter, parambuf, "Hello", "\n");

        /* Create param string for the getter function */
        sprintf(parambuf, ".Fa \"%s p\"\n", g_name);
        create_one_manpage(g_name, c_return_value(pm), getter, parambuf, "World", "\n");
    }
}

static void generate_code(void)
{
    generate_header();
    generate_implementation();
    generate_man_pages();
}

int main(int argc, char* argv[])
{

    parse_options(argc, argv);
    check_options();

    parse_memberspec();
    check_semantics();
    generate_code();

    exit(EXIT_SUCCESS);
}
