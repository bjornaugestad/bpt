#include "file.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


/* We allocate room for symbols in chunks. */
#define CHUNKSIZE 10

/* We keep symbols in this struct to be able to
 * precompile the regular expression used to match
 * symbol with text. */
struct symbol {
    char *name;
    regex_t reg;
};

struct file {
    char *name;
    regex_t include; 
    struct symbol **symbols;
    size_t symbols_used;
    size_t symbols_size;

    /* We track includes of this file by storing 
     * the names of files which include us but don't
     * have to. */
    char **files;
    size_t files_used;
    size_t files_size;

    struct file *next;
};

struct file *file_new(const char *name)
{
    struct file *p;
    char buf[1024];
    int rc;

    assert(name != NULL);

    if( (p = calloc(1, sizeof *p)) == NULL
    || (p->name = malloc(strlen(name) + 1)) == NULL) {
        free(p);
        return NULL;
    }

    strcpy(p->name, name);

    /* Prepare the regexp used to search for include-statements */
    snprintf(buf, sizeof buf, "#include[ ]*[\"<]%s[\">]", name);
    if( (rc = regcomp(&p->include, buf, 0)) != 0) {
        fprintf(stderr, "Error compiling %s\n", buf);
        abort();
    }

    return p;
}

struct file *file_new_tail(struct file *p, const char *name)
{
    struct file *new;

    assert(p != NULL);
    assert(name != NULL);

    if( (new = file_new(name)) == NULL)
        return NULL;

    /* Locate end of current list, and append new file object */
    while(p->next != NULL)
        p = p->next;
    p->next = new;
    return new;
}

void file_free(struct file *p)
{
    if(p) {
        size_t i;
        for(i = 0; i < p->symbols_used; i++) {
            free(p->symbols[i]->name);
            regfree(&p->symbols[i]->reg);
            free(p->symbols[i]);
        }

        for(i = 0; i < p->files_used; i++) 
            free(p->files[i]);

        free(p->symbols);
        free(p->files);
        free(p->name);
        regfree(&p->include);
        free(p);
    }
}

void file_free_list(struct file *p)
{
    if(p) {
        file_free_list(p->next);
        file_free(p);
    }
}

void file_add_symbol(struct file *p, const char *s)
{
    assert(p != NULL);
    assert(s != NULL);

    /* Make room for char* if necessary */
    if(p->symbols_size == 0) {
        if( (p->symbols = calloc(CHUNKSIZE, sizeof *p->symbols)) == NULL) 
            abort();
        p->symbols_size = CHUNKSIZE;
        p->symbols_used = 0;
    }
    else if(p->symbols_used == p->symbols_size) {
        size_t cb = sizeof *p->symbols * p->symbols_size * 2; 
        if( (p->symbols = realloc(p->symbols, cb)) == NULL)
            abort();

        p->symbols_size *= 2;
    }

    assert(p->symbols_used < p->symbols_size);

    /* Now allocate room for the symbol struct. */
    if( (p->symbols[p->symbols_used] = malloc(sizeof **p->symbols)) == NULL)
        abort();

    /* Duplicate and copy the incoming symbol */
    if( (p->symbols[p->symbols_used]->name = malloc(strlen(s) + 1)) == NULL)
        abort();
    strcpy(p->symbols[p->symbols_used++]->name, s);
}

void file_add_unused(struct file *p, const char *s)
{
    assert(p != NULL);
    assert(s != NULL);

    /* Make room for char* if necessary */
    if(p->files_size == 0) {
        if( (p->files = calloc(CHUNKSIZE, sizeof *p->files)) == NULL) 
            abort();
        p->files_size = CHUNKSIZE;
        p->files_used = 0;
    }
    else if(p->files_used == p->files_size) {
        size_t cb = sizeof *p->files * p->files_size * 2; 
        if( (p->files = realloc(p->files, cb)) == NULL)
            abort();

        p->files_size *= 2;
    }

    assert(p->files_used < p->files_size);

    /* Duplicate and copy the incoming file */
    if( (p->files[p->files_used] = malloc(strlen(s) + 1)) == NULL)
        abort();
    strcpy(p->files[p->files_used++], s);
}

const char *file_name(const struct file *p)
{
    assert(p != NULL);
    return p->name;
}

size_t file_nsymbols(const struct file *p)
{
    assert(p != NULL);
    return p->symbols_used;
}

const char *file_symbol(const struct file *p, size_t idx)
{
    assert(p != NULL);
    assert(idx < p->symbols_used);
    return p->symbols[idx]->name;
}

size_t file_nunused(const struct file *p)
{
    assert(p != NULL);
    return p->files_used;
}

const char *file_unused(const struct file *p, size_t idx)
{
    assert(p != NULL);
    assert(idx < p->files_used);
    return p->files[idx];
}

struct file *file_next(struct file *p)
{
    assert(p != NULL);
    return p->next;
}

regex_t* file_include_regex(struct file *p)
{
    assert(p != NULL);
    return &p->include;
}

void file_compile_symbols(struct file *p)
{
    size_t i;
    int rc;
    char buf[2048];

    assert(p != NULL);

    for(i = 0; i < p->symbols_used; i++) {
        snprintf(buf, sizeof buf, "\\<%s\\>", p->symbols[i]->name);
        rc = regcomp(&p->symbols[i]->reg, buf, 0);
        if(rc != 0) {
            fprintf(stderr, "Could not regcomp %s\n", buf);
            abort();
        }

    }
}

regex_t* file_symbol_regex(struct file *p, size_t idx)
{
    assert(p != NULL);
    assert(idx < p->symbols_used);
    return &p->symbols[idx]->reg;
}

