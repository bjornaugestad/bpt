/* Program to add spaces where appropriate in C source code.
 * The main issue is to keep state and not mess up when in comments or literals.
 * - Comments we support are the normal slash* and *slash  and //
 * - Literals are ".." and '..' 
 *
 * Create a state machine handling the states we care * about:
 *  - C comments, C++ comments, char literals and '' literals, as well as normal.
 *
 * We can have comment-tokens within literals, but not the other way around.
 * AFAICT we need to support just two characters, current and previous char.
 * If curr==" and prev != backslash, we have a ", which means that we either
 *    enter or leave literal state. Same goes for '
 * If curr==/ and prev == /, we enter C++ comment state. It's ended with \n
 * If curr==* and prev ==/, we enter C comment state. 
 * If curr==/ and prev ==*, we leave C comment state.
 *
 * USAGE: spaces filename... 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>


/* State handler functions */
static void normal(FILE *out, int last, int c, int next);
static void c_comment(FILE *out, int last, int c, int next);
static void cxx_comment(FILE *out, int last, int c, int next);
static void string_literal(FILE *out, int last, int c, int next);
static void literal(FILE *out, int last, int c, int next);
void (*handler)(FILE *out, int last, int c, int next) = NULL;

/* Keep current line in a buffer so we can inspect previous 'tokens' */
char linebuf[20*102400] = { '\0' };
size_t buflen = 0;

static void empty(void)
{
    buflen = 0;
}
    
static void add(int c)
{
    if (buflen + 2 == sizeof linebuf) {
        fprintf(stderr, "Buffer overflow\n");
        fprintf(stderr, "Current Buffer value:%s\n", linebuf);
        fprintf(stderr, "Current Buffer length:%zu\n", buflen);
        exit(EXIT_FAILURE);
    }
    linebuf[buflen++] = c;
    linebuf[buflen] = '\0'; /* Make it a valid string */
}

static int buf_ends_with(const char *s)
{
    size_t i, slen = strlen(s);

    /* Is buffer shorter than string? If so, we 
     * cannot have a match. */
    if(buflen < slen)
        return 0;

    /* Compare strings in reverse */
    for(i = 0; i < slen; i++) {
        if(linebuf[buflen - i - 1] != s[slen - i - 1])
            return 0;
    }

    return 1;
}

/* Is the current line an include statement? */
static int include_statement(void)
{
    if(buflen == 0)
        return 0;
    if(strstr(linebuf, "#include"))
        return 1;

    if(strstr(linebuf, "include") && linebuf[0] == '#')
        return 1;

    /* We can still have #[ws]include lines, but assume that we don't */
    return 0;
}

/* Are we de-referencing a pointer, as in foo->a, where curr == a? */
static int deref_pointer(void)
{
    if (buflen < 3)
        return 0;

    if(isalpha(linebuf[buflen - 1])
    && linebuf[buflen - 2] == '>'
    && linebuf[buflen - 3] == '-')
        return 1;
    return 0;
}

/* --a or ++a?  */
static int operator(void)
{
    if (buflen < 3)
        return 0;

    if(isalpha(linebuf[buflen - 1])
    && ((linebuf[buflen - 2] == '-' && linebuf[buflen - 3] == '-')
        || (linebuf[buflen - 2] == '+' && linebuf[buflen - 3] == '+')))
        return 1;
    return 0;
}


static int getnext(FILE *f)
{
    int c;

    c = fgetc(f);
    ungetc(c, f);
    return c;
}


/* Return true if linebuf ends with if( for( or while( */
static int paren_preceeded_by_keyword(void)
{
    const char *expr[] = {
        "if(", "for(", "while(", "switch("
    };

    size_t i, n = sizeof expr / sizeof *expr;

    for(i = 0; i < n; i++) {
        if (buf_ends_with(expr[i]))
            return 1;
    }

    return 0;
}

static void normal(FILE *out, int last, int c, int next)
{
    const char * tokens = ";,>=/%<-+";
    const char * digitokens = ";,>=/%<";

    if(c == '"' && last != '\\')
        handler = string_literal;
    else if (c == '\'' && last != '\\')
        handler = literal;
    else if (c == '*' && last == '/') 
        handler = c_comment;
    else if (c == '/' && last == '/')
        handler = cxx_comment;

    /* Do not touch include-statements as filenames tend to
     * contain stuff like <foo-bar.h>. Maybe add a new state? */
    if(include_statement())
        goto skip;

    /* So far so good. Now to the actual space management stuff.  */
    if (c == '(') {
        if (paren_preceeded_by_keyword() || last == '=')
            fputc(' ', out);
    }
    else if (c == '>') {
        if(isalnum(last) && !include_statement())
            fputc(' ', out);
    }
    else if (c == '{') {
        if(last == ')' || last == '=')
            fputc(' ', out);
    }
    else if (c == '<') {
        if(isalnum(last))
            fputc(' ', out);
    }
    else if (isalpha(c)) {
        /* We have a character. If prev was non-char token, add space */
        if(strchr(tokens, last) 
        && !include_statement()
        && !deref_pointer()
        && !operator())
            fputc(' ', out);
    }
    else if (isdigit(c)) {
        if(strchr(digitokens, last))
            fputc(' ', out);
    }
    else if(c == '+') {
        if(isalnum(last)) {
            if(last == 'e' && isdigit(next)) {
                /* Do nothing */
            }
            else if (next != '+')
                fputc(' ', out);
        }
    }
    else if (c == '-') {
        if (last == ']')
            fputc(' ', out);
        else if(isalnum(last)) {
            // Keep floats with scientific format (1e-03)
            if(last == 'e' && isdigit(next)) {
                /* Do nothing */
            }
            else if (next != '>' && next != '-') // p->foo a++
                fputc(' ', out);
        }
    }
    else if (c == '=') {
        if(isalnum(last) || last == ']')
            fputc(' ', out);
    }
    else if (c == '/') {
        if(isalnum(last) && !include_statement())
            fputc(' ', out);
        else if (last == ']')
            fputc(' ', out);
    }
    else if (c == '&') {
        if(last == '=' || last == ',')
            fputc(' ', out);
    }

skip:

    /* Always print c */
    fputc(c, out);
}

static void c_comment(FILE *out, int last, int c, int next)
{
    (void)next;

    fputc(c, out);
    if (c == '/' && last == '*')
        handler = normal;
}

static void cxx_comment(FILE *out, int last, int c, int next)
{
    (void)next;

    fputc(c, out);
    if (c == '\n' && last != '\\')
        handler = normal;
}

static void string_literal(FILE *out, int last, int c, int next)
{
    (void)next;

    fputc(c, out);
    if (c == '"' && last != '\\')
        handler = normal;
}

static void literal(FILE *out, int last, int c, int next)
{
    (void)next;

    fputc(c, out);
    if (c == '\'' && last != '\\')
        handler = normal;
}


static void process(FILE *in, FILE *out)
{
    int c, last = 0, next;

    while ( (c = fgetc(in)) != EOF) {
        if(c == '\n')
            empty();
        else
            add(c);

        next = getnext(in);
        handler(out, last, c, next);
        last = c;
    }
}

int main(int argc, char *argv[])
{
    char *newname;
    FILE *in, *out;

    if (argc == 1) {
        fprintf(stderr, "USAGE: spaces filename...\n");
        exit(EXIT_FAILURE);
    }

    while(*++argv) {
        handler = normal;
        if ( (newname = malloc(strlen(*argv) + 2 + 1)) == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        sprintf(newname, "%s.s", *argv);
        if (rename(*argv, newname) != 0) {
            perror(*argv);
            exit(EXIT_FAILURE);
        }

        if ( (in = fopen(newname, "r")) == NULL) {
            perror(newname);
            exit(EXIT_FAILURE);
        }

        if ( (out = fopen(*argv, "w")) == NULL) {
            perror(*argv);
            exit(EXIT_FAILURE);
        }

        process(in, out);
        fclose(in);
        fclose(out);
        unlink(newname);
        free(newname);
    }

    exit(EXIT_SUCCESS);
}

