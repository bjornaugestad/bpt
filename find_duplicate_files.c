/* find_duplicate_files locates identical files, possibly
 * with a different name.
 */
#include <assert.h>
#include <stdbool.h>
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
int silent = 0; // print error messages? (Some will always be printed)

// We store the directories to traverse in an array.
const char *searchdirs[10240];
size_t nsearchdirs = 0;

// We store paths and hash values in structs like this:
struct entry {
    char *fpath;
    char *hash4k;
    char *fullhash;
};
struct entry *entries = NULL;
size_t nentries_max = 0; // How many have we allocated room for?
size_t nentries_used = 0; // How many has been used?

size_t initial_nentries = 300000; // We start off supporting that many entries

static void free_allocated_mem(void)
{
    size_t i;

    for (i = 0; i < nentries_used; i++) {
        if (entries[i].hash4k != entries[i].fullhash)
            free(entries[i].fullhash);

        free(entries[i].hash4k);
        free(entries[i].fpath);
    }

    free(entries);
}

static void add_entry(const char *fpath, char *hash4k)
{
    size_t cb;

    if (nentries_max == 0) {
        // first entry.
        cb = sizeof *entries * initial_nentries;
        if ((entries = malloc(cb)) == NULL) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }

        memset(entries, 0, cb);
        nentries_max = initial_nentries;
    }
    else if (nentries_used == nentries_max) {
        // reallocate
        struct entry *tmp;
        size_t old_size = nentries_used * sizeof *entries;
        size_t new_size = nentries_max * 2 * sizeof *entries;

        if ((tmp = realloc(entries, new_size)) == NULL) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        entries = tmp;
        nentries_max *= 2;
        memset(&entries[old_size], 0, new_size - old_size);
    }

    // Add element. Note that we create a copy of fpath, but *NOT* of
    // hash, since the callback function allocated room for hash.
    if ((entries[nentries_used].fpath = strdup(fpath)) == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    entries[nentries_used++].hash4k = hash4k;
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
        "-x Stay on file system. Dont' traverse into mounted filesystems.",
        "-v Verbose. Print info as we progress.",
        "-s silent. Don't print (most) error messages.",
        "-d debug. Print misc debugging info.",
        "",
    };

    size_t i, n = sizeof text / sizeof *text;
    for (i = 0; i < n; i++)
        puts(text[i]);
}

static int duplicate_name(const char *dirs[], size_t ndirs, const char *s)
{
    size_t i;

    for (i = 0; i < ndirs; i++)
        if (strcmp(dirs[i], s) == 0)
            return 1;

    return 0;
}

static int isdirectory(const char *s)
{
    struct stat st;

    assert(s != NULL);

    if (stat(s, &st) == -1) {
        perror(s);
        exit(EXIT_FAILURE);
    }

    return S_ISDIR(st.st_mode);
}


static void parse_command_line(int argc, char *argv[])
{
    int c;
    extern char *optarg;
    extern int optind;

    const char *options = "vhxds";

    if (argc == 1) {
        show_usage();
        exit(EXIT_FAILURE);
    }

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'h':
                show_usage();
                exit(EXIT_SUCCESS);

            case 's':
                silent = 1;
                break;

            case 'd':
                debug = 1;
                break;

            case 'v':
                verbose = 1;
                break;

            case 'x':
                ftw_flags |= FTW_MOUNT;
                break;

            case '?':
            default:
                show_usage();
                exit(EXIT_FAILURE);
        }
    }

    /* Now store the directories */
    while (argv[optind] != NULL) {
        if (duplicate_name(searchdirs, nsearchdirs, argv[optind])) {
            fprintf(stderr, "error: directory %s already specified\n", argv[optind]);
            exit(EXIT_FAILURE);
        }

        if (!isdirectory(argv[optind])) {
            fprintf(stderr, "%s is not a directory.\n", argv[optind]);
            exit(EXIT_FAILURE);
        }

        if (nsearchdirs == sizeof searchdirs / sizeof *searchdirs) {
            fprintf(stderr, "error: too many directories specified.\n");
            exit(EXIT_FAILURE);
        }

        searchdirs[nsearchdirs++] = argv[optind++];
    }

    if (nsearchdirs == 0) {
        fprintf(stderr, "Please specify one or more directories to search\n");
        exit(EXIT_FAILURE);
    }
}

static char *hashbuf(const char *src, size_t srclen)
{
    int algo = GCRY_MD_SHA1;
    size_t digestsize = gcry_md_get_algo_dlen(algo);
    size_t i, stringsize = digestsize * 2 + 4;

    unsigned char *digest = malloc(digestsize);
    char *string = malloc(stringsize);

    if (digest == NULL || string == NULL)
        return NULL;

    gcry_md_hash_buffer(algo, digest, src, srclen);

    for (i = 0; i < digestsize; i++) {
        unsigned d = (unsigned)digest[i];
        sprintf(&string[i * 2], "%02x", d);
    }

    free(digest);
    return string;
}

static char *hashfile(const char *fpath, bool fullfile)
{
    int fd = -1;
    char *string = NULL;
    unsigned char *digest = NULL;
    void *contents = NULL;
    size_t mapsize = 0;

    // Memory map the file and hash it
    if ((fd = open(fpath, O_RDONLY)) == -1) {
        if (!silent)
            perror(fpath);
        goto err;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) 
        goto err;

    // Optimization: First pass hashes only 4K 
    mapsize = sb.st_size;
    if (!fullfile && mapsize > 4096)
        mapsize = 4096;

    contents = mmap(NULL, mapsize, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    fd = -1;
    if (contents == MAP_FAILED) {
        if (!silent)
            perror(fpath);
        return NULL;
    }

    string = hashbuf(contents, mapsize);

    munmap(contents, mapsize);
    return string;

err:
    if (contents != NULL)
        munmap(contents, mapsize);

    if (fd != -1)
        close(fd);

    free(digest);
    free(string);

    perror(fpath);
    return NULL;
}

int callback(const char *fpath, const struct stat *sb,
    int typeflag __attribute__((unused)),
    struct FTW *ftwbuf __attribute__((unused)))
{
    // Ignore anything but regular files with content
    if (!S_ISREG(sb->st_mode) || sb->st_size == 0)
        return 0;

    char *string = hashfile(fpath, false);
    if (string == NULL)
        goto err;

    // So far so good. Store path and hash somewhere suitable for qsort().
    add_entry(fpath, string);
    return 0;

err:
    perror(fpath);
    return -1;
}

static void traverse_directories(void)
{
    size_t i;
    int nopenfd = 12;

    assert(nsearchdirs > 0);

    for (i = 0; i < nsearchdirs; i++) {
        if (verbose)
            fprintf(stderr, "Checking directory:%s\n", searchdirs[i]);

        nftw(searchdirs[i], callback, nopenfd, ftw_flags);

        if (verbose)
            fprintf(stderr, "\n");
    }
}

static int cmp_4k(const void *v1, const void *v2)
{
    const struct entry *p1 = v1, *p2 = v2;
    return strcmp(p1->hash4k, p2->hash4k);
}

static int cmp_full(const void *v1, const void *v2)
{
    const struct entry *p1 = v1, *p2 = v2;
    return strcmp(p1->hash4k, p2->hash4k);
}

static void sort_results_4k(void)
{
    qsort(entries, nentries_used, sizeof *entries, cmp_4k);
}

static void sort_results_full(void)
{
    qsort(entries, nentries_used, sizeof *entries, cmp_full);
}

// If 4k hashes are equal, compute full hash.
// If hashes differ, move 4k hash to full hash for uniform sorting later on.
static void resolve_4k_dups(void)
{
    for (size_t i = 1; i < nentries_used; i++) {
        if (strcmp(entries[i - 1].hash4k, entries[i].hash4k) != 0) {
            entries[i - 1].fullhash = entries[i - 1].hash4k;
            entries[i].fullhash = entries[i].hash4k;
        }
    }

    // Now re-hash all files with a NULL fullhash.
    for (size_t i = 0; i < nentries_used; i++) {
        if (entries[i].fullhash == NULL) {
            entries[i].fullhash = hashfile(entries[i].fpath, true);
            if (entries[i].fullhash == NULL) {
                // File disappeared while this program ran?
            }
        }
    }
}

int main(int argc, char *argv[])
{
    size_t i;

    parse_command_line(argc, argv);
    traverse_directories();
    sort_results_4k();
    resolve_4k_dups();
    sort_results_full();

    // Assuming that we got this far, now what?
    // entries is now sorted by hash, so we can traverse it
    // to locate duplicates.
    for (i = 1; i < nentries_used; i++) {
        if (strcmp(entries[i - 1].fullhash, entries[i].fullhash) == 0)
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

    free_allocated_mem();

    return 0;
}
