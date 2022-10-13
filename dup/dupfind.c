/* find_duplicate_files locates identical files, possibly
 * with a different name.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <gcrypt.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

int ftw_flags = FTW_ACTIONRETVAL | FTW_STOP | FTW_PHYS;
int verbose = 0;
int debug = 0;

// We store paths and hash values in structs like this:
struct entry {
    char *fpath;
    char *hash;
};
struct entry *entries = NULL;
size_t nentries_max = 0; // How many have we allocated room for?
size_t nentries_used = 0; // How many has been used?

size_t initial_nentries = 300000; // We start off supporting that many entries

static void add_entry(const char *fpath, const char *hash)
{
    if (nentries_max == 0) {
        // first entry.
        if ((entries = malloc(sizeof *entries * initial_nentries)) == NULL) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }

        nentries_max = initial_nentries;
    }
    else if (nentries_used == nentries_max) {
        // reallocate
        struct entry *tmp;
        size_t new_size = nentries_max * 2 * sizeof *entries;

        if ((tmp = realloc(entries, new_size)) == NULL) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        entries = tmp;
        nentries_max *= 2;
    }

    // Add element. 
    if ((entries[nentries_used].fpath = strdup(fpath)) == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    if ((entries[nentries_used++].hash = strdup(hash)) == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
}

static void show_usage(void)
{
    static const char *text[] = {
        "USAGE: find_duplicate_files [options] directory...",
        "",
        "The program will locate ALL files in the directories specified.",
        "Then it will compare the contents and report paths to duplicates.",
        "",
        "options",
        "-v Verbose. Print info as we progress.",
        "-d debug. Print misc debugging info.",
        "",
    };

    size_t i, n = sizeof text / sizeof *text;
    for (i = 0; i < n; i++)
        puts(text[i]);
}

static void parse_command_line(int argc, char *argv[])
{
    int c;
    extern char *optarg;
    extern int optind;

    const char *options = "vhd";

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'h':
                show_usage();
                exit(EXIT_SUCCESS);

            case 'd':
                debug = 1;
                break;

            case 'v':
                verbose = 1;
                break;

            case '?':
            default:
                show_usage();
                exit(EXIT_FAILURE);
        }
    }
}

static void read_results(void)
{
    const char *filename =".dupdb";
    FILE *f;
    char fpath[20000], hash[1000];

    if ((f = fopen(filename, "r")) == NULL) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    while (fscanf(f, "%[^\t]\t%[^\n]\n", fpath, hash) == 2) {
        add_entry(fpath, hash);
    }

    fclose(f);
}

int main(int argc, char *argv[])
{
    size_t i;

    parse_command_line(argc, argv);

    read_results();

    // Assuming that we got this far, now what?
    // entries is now sorted by hash, so we can traverse it
    // to locate duplicates.
    for (i = 1; i < nentries_used; i++) {
        if (strcmp(entries[i - 1].hash, entries[i].hash) == 0)
            printf("'%s'\t'%s'\n", entries[i - 1].fpath, entries[i].fpath);
    }

    // TODO: We probably want to be smarter about our output. 
    // Today's version is a PITA to read since a hash is printed at least twice.
    // It's just confusing. Some kind of UI would be nice too,
    // if it helps us with deleting duplicates. GUI or TUI? 
    // curses or Qt/glade/glib?
    //
    // It'd also be nice if we could specify a master tree and a tree with 
    // possible duplicates. Since we in most cases do have e.g. $HOME/Pictures
    // and want to check if we somehow miss stuff from e.g. mybackupfile, 
    // we know which duplicate to keep, and which to delete too.
    // The ui may become tricky though. We need two directories and we
    // need to know which one's the master directory. It's not hard, but
    // we want to be sure that we get things in the right order and not
    // mix masterdir with copydir.

    return 0;
}
