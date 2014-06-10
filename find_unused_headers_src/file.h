#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include <sys/types.h>
#include <regex.h>

/* A file consists of a name and a list of symbols. */
struct file;
struct file *file_new(const char *name);
void file_free(struct file *p);
void file_add_symbol(struct file *p, const char *s);
size_t file_nsymbols(const struct file *p);
const char *file_symbol(const struct file *p, size_t idx);
const char *file_name(const struct file *p);

/* Add a new file at the tail of the list of files. */
struct file *file_new_tail(struct file *p, const char *name);
void file_free_list(struct file *p);
struct file *file_next(struct file *p);

/* Add file with unused header */
void file_add_unused(struct file *p, const char *s);
size_t file_nunused(const struct file *p);
const char *file_unused(const struct file *p, size_t idx);

regex_t* file_include_regex(struct file *p);
void file_compile_symbols(struct file *p);
regex_t* file_symbol_regex(struct file *p, size_t idx);

#endif

