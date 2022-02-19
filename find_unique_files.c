// find unique files. 
// Find files in one directory tree, but not in another. I need this to manage
// rsynced directory trees from backups and recovery images. The problem
// is often the sheer volume of files, 100K+. It's a PITA to use 
// find_duplicate_files when we're actually looking for files in one tree, 
// which aren't present in another tree. 
//
// Usage: find_unique_files master-dir eval-dir
// We look for files in eval-dir. If they aren't in master-dir, then
// they're considered to be unique. 
//
// Eval-dir can be a subdirectory of master-dir. If so, duplicates will
// be handled.
//
// This program doesn't care about differences in file contents. It only
// looks for files in one tree which aren't in another tree. It could
// probably be written as a very short script, but wth, we like coding :)
// BTW, what would a script look like?
// find master-dir -type f > somefile
// find eval-dir -type f | while read f; do
//     fn = $(basename $f); 
//     grep -qwF "$fn" somefile
//     if test $? -ne 0; then
//      echo $f
//  done
//   Thats pretty much it. Will be very slow though, due to grepping.
//   Or will it? Let's test
//
// Implementation details:
//
// * Traverse master-dir to find all files
// * Traverse eval-dir to find each file
// * if a file from eval-dir isn't in master-dir's list of files, then 
//   it is considered a unique file.
//
// Q: Do we care about directories? Not really. Empty ones can be found
// using find -type d -empty, and non-empty will be found since we find
// the files in it.
//
// Q: Do we care about symlinks? Not really. We care about not losing
// or deleting files when merging trees. 
//
// We start off by copying find_duplicate_files.c and modify it.
//
// boa@20210316
//


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>

#include <unistd.h>

int ftw_flags = FTW_ACTIONRETVAL | FTW_STOP | FTW_PHYS;
int verbose = 0;
int debug = 0;
int silent = 0; // print error messages? (Some will always be printed)

// We store the directories to traverse in an array.
static char masterdir[10240];
static char evaldir[10240];

// We store paths values in structs like this:
struct entry {
    char *fpath; // path to file in master-dir, incl name
    char *fname; // basename of file, for faster lookup.
};

struct entry *entries = NULL;
size_t nentries_max = 0; // How many have we allocated room for?
size_t nentries_used = 0; // How many has been used?

size_t initial_nentries = 300000; // We start off supporting that many entries

static void add_entry(const char *fpath)
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

    if ((entries[nentries_used].fpath = strdup(fpath)) == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    // Now get the basename of the file, including extension of course.
    // I.e., anything to the right of the rightmost slash, if any.
    entries[nentries_used].fname = strrchr(fpath, '/');
    if (entries[nentries_used].fname == NULL) {
        fprintf(stderr, "error: Expected a / somewhere in path: %s\n", fpath);
        exit(EXIT_FAILURE);
    }

    // Skip slash
    entries[nentries_used].fname++;

    nentries_used++;
}

static void show_usage(void)
{
    static const char *text[] = {
        "USAGE: find_unique_files [options] master-dir eval-dir",
        "",
        "The program will look for files from eval-dir which aren't present in master-dir",
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

    /* Now store the directories. We need two, master and eval. */
    if (argc - optind != 2) {
        show_usage();
        exit(EXIT_FAILURE);
    }

    /* Path too long? */
    size_t n = strlen(argv[optind]);
    if (n >= sizeof masterdir) {
        fprintf(stderr, "error: path too long\n");
        exit(EXIT_FAILURE);
    }

    memcpy(masterdir, argv[optind], n);

    /* Now the same for evaldir */
    optind++;
    n = strlen(argv[optind]);
    if (n >= sizeof evaldir) {
        fprintf(stderr, "error: path too long\n");
        exit(EXIT_FAILURE);
    }

    memcpy(evaldir, argv[optind], n);

    /* Are they directories though? Check */
    if (!isdirectory(masterdir) || !isdirectory(evaldir)) {
        show_usage();
        exit(EXIT_FAILURE);
    }
}

// is current object, file or otherwise, part of eval-dir? Hmm, hard to tell,
// given the risk of duplicate names. foo/bar/baz.c vs /usr/local/src/foo/bar/baz.c
// is troublesome if eval-dir is just bar and master-dir is /. What do? We have to
// manage this in case eval-dir is a sub-dir of master-dir.
static int is_part_of_eval(const char *s)
{
    static char *evaldup;
    static size_t evallen, dupsize;

    // Get a copy of eval-dir and add slashes so strstr() won't be confused.
    if (evaldup == NULL) {
        evallen = strlen(evaldir);
        dupsize = evallen + 3;
        if ((evaldup = malloc(dupsize)) == NULL) {
            fprintf(stderr, "error: out of memory\n");
            exit(EXIT_FAILURE);
        }

        // What about leading slashes? bar/ != foobar/, right? I guess we 
        // need one. May create problems for corner cases. Let's try.
        if (evaldir[0] == '/')
            evaldup[0] = '\0'; // No need for slash, we'll get it from evaldir
        else
            strcpy(evaldup, "/"); // leading slash

        strcat(evaldup, evaldir);

        // Append slash if needed
        if (evaldir[evallen - 1] != '/')
            strcat(evaldup, "/");
    }

    // Right, so now we have something to search for. 
    return strstr(s, evaldup) != NULL;
}

int master_callback(const char *fpath, const struct stat *sb, int typeflag __attribute__((unused)), struct FTW *ftwbuf __attribute__((unused)))
{
    static unsigned long nfiles;

    // Ignore paths which include eval-dir's path. 
    if (is_part_of_eval(fpath))
        return 0;

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

    // So far so good. Store path somewhere suitable for qsort().
    add_entry(fpath);
    if (verbose)
        fprintf(stderr, "\r%lu", ++nfiles);

    return 0;
}

static int eval_cmp(const void *v1, const void *v2)
{
    const char *key = v1;
    const struct entry *p = v2;

    return strcmp(p->fname, key) == 0;
}

static int in_master_dir(const char *fpath)
{
    assert(fpath != NULL);
    assert(strlen(fpath) > 0);

    return bsearch(fpath, entries, sizeof *entries, nentries_used, eval_cmp) != NULL;
}

// Check if file is in master, or if 
int eval_callback(const char *fpath, const struct stat *sb, int typeflag __attribute__((unused)), struct FTW *ftwbuf __attribute__((unused)))
{
    // Ignore anything but regular files
    if (!S_ISREG(sb->st_mode)) {
        if (debug)
            fprintf(stderr, "Skipping %s. Not a regular file.\n", fpath);
        return 0;
    }

    // Is file found in master-dir? If not, we have a unique file
    if (in_master_dir(fpath))
        puts(fpath);

    return 0;
}

static void traverse_master(void)
{
    int nopenfd = 12;

    assert(strlen(masterdir) > 0);

    if (verbose)
        fprintf(stderr, "Checking directory:%s\n", masterdir);

    nftw(masterdir, master_callback, nopenfd, ftw_flags);
    if (verbose)
        fprintf(stderr, "\n");
}

static void traverse_eval(void)
{
    int nopenfd = 12;

    assert(strlen(evaldir) > 0);

    if (verbose)
        fprintf(stderr, "Checking directory:%s\n", evaldir);

    nftw(evaldir, eval_callback, nopenfd, ftw_flags);
    if (verbose)
        fprintf(stderr, "\n");
}

static int cmp(const void *v1, const void *v2)
{
    const struct entry *p1 = v1, *p2 = v2;
    return strcmp(p1->fname, p2->fname);
}

static void sort_master_files(void)
{
    if (verbose)
        fprintf(stderr, "Sorting %zu found files\n", nentries_used);

    qsort(entries, nentries_used, sizeof *entries, cmp);
}

int main(int argc, char *argv[])
{
    // TODO: Expand paths so that . and ~ won't create issues
    // We can't do full expansion, can we? Hmm...

    parse_command_line(argc, argv);
    traverse_master();
    sort_master_files();
    traverse_eval();

    return 0;
}

