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
int silent = 0; // print error messages? (Some will always be printed)

// We store the directories to traverse in an array.
const char *searchdirs[10240];
size_t nsearchdirs = 0;

// We store paths and hash values in structs like this:
struct entry {
    char *fpath;
    char *hash;
};
struct entry *entries = NULL;
size_t nentries_max = 0; // How many have we allocated room for?
size_t nentries_used = 0; // How many has been used?

size_t initial_nentries = 300000; // We start off supporting that many entries

static void add_entry(const char *fpath, char *hash)
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

    // Add element. Note that we create a copy of fpath, but *NOT* of
    // hash, since the callback function allocated room for hash.
    if ((entries[nentries_used].fpath = strdup(fpath)) == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    entries[nentries_used++].hash = hash;
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

int callback(const char *fpath, const struct stat *sb, int typeflag __attribute__((unused)), struct FTW *ftwbuf __attribute__((unused)))
{
    int fd = -1, algo = GCRY_MD_SHA1;
    size_t digestsize = gcry_md_get_algo_dlen(algo);
    size_t i, stringsize = digestsize * 2 + 1;
    char *string = NULL, *s;
    unsigned char *digest = NULL;
    void *contents = NULL;
    static unsigned long nfiles;

    // Ignore anything but regular files
    if (!S_ISREG(sb->st_mode)) {
        if (debug)
            fprintf(stderr, "Skipping %s. Not a regular file.\n", fpath);
        return 0;
    }

    if (sb->st_size == 0) {
        if (debug)
            fprintf(stderr, "Skipping empty file %s\n", fpath);
        return 0;
    }

    // Memory map the file and hash it
    if ((fd = open(fpath, O_RDONLY)) == -1) {
        if (!silent)
            perror(fpath);
        return 0;
    }

    if ((contents = mmap(NULL, sb->st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        close(fd);
        if (!silent)
            perror(fpath);
        return 0;
    }

    if ((digest = malloc(digestsize)) == NULL)
        goto err;

    gcry_md_hash_buffer(algo, digest, contents, sb->st_size);

    s = string = malloc(stringsize);
    if (s == NULL)
        goto err;

    for (i = 0; i < digestsize; i++, s += 2)
        sprintf(s, "%02x", digest[i]);

    // So far so good. Store path and hash somewhere suitable
    // for qsort().
    add_entry(fpath, string);
    if (verbose)
        fprintf(stderr, "\r%lu", ++nfiles);

    munmap(contents, sb->st_size);
    close(fd);
    free(digest);
    return 0;

err:
    if (contents != NULL)
        munmap(contents, sb->st_size);

    if (fd != -1)
        close(fd);

    free(digest);
    free(string);

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

static int cmp(const void *v1, const void *v2)
{
    const struct entry *p1 = v1, *p2 = v2;
    return strcmp(p1->hash, p2->hash);
}

static void sort_results(void)
{
    if (verbose)
        fprintf(stderr, "Sorting %zu found files\n", nentries_used);

    qsort(entries, nentries_used, sizeof *entries, cmp);
}

int main(int argc, char *argv[])
{
    size_t i;

    parse_command_line(argc, argv);
    traverse_directories();
    sort_results();

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
