// vbf -- view big file
// We have some massive files which eat up all the RAM. Even Vim uses
// the full file size of RAM, so 20GB file will use 20+GB RAM. 
// That's meh, so we need a "view" command with simple paging functionality
// which uses mmap() instead of reading the whole file.
//
// Simple algo:
// -----------
// 1. mmap() file
// 2. Figure out screen size (ncurses)
// 3. Show n lines, where n == number of lines on display (-1 for status?)
// 4. Accept some simple vi-ish commands for navigating
//    ctrl-d/u (half a page down/up) , ctrl-f/b(full page forward/backward), j/k (one line down/up)
//    q(for quit) 
//    the z commands (z ENTER for current line at top,
//    z. for current line at middle, z- for current line at bottom.
//    / for search, n(next), ?(prev)

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ncurses.h>

void *g_mem;
size_t g_memsize;
const char *g_cmem;

__attribute__((format(printf,1,2)))
static void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

__attribute__((format(printf,1,2)))
static void die_perror(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s", strerror(errno));
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}

static void check_args(int argc, char *argv[])
{
    struct stat st;

    if (argc != 2)
        die("USAGE: vbf filename\n");

    if (stat(argv[1], &st))
        die_perror("%s\n", argv[1]);
}

static void mmap_file(const char *filename, void **mem, size_t *memsize)
{
    int fd;
    struct stat st;

    if ((fd = open(filename, O_RDONLY)) == -1)
        die_perror("%s", filename);

    if (fstat(fd, &st))
        die_perror("%s", filename);

    if (st.st_size == 0)
        die("vbf: file %s is empty\n", filename);

    *memsize = st.st_size;
    *mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (*mem == MAP_FAILED)
        die_perror("%s", filename);

    close(fd);
}

// Do we want to index the mmapped file? We do need to
// know the start and end of each line, so we later can
// display those lines. Since each line ends at \n and
// next line always starts at \n + 1, we don't need to
// index both the start and end of a line.
//
// What if the file is binary without any \n? I guess
// that makes it a one-liner?
//
// What if the last line is missing a newline? Then the
// end is at memsize. Same goes for single-line files.
//
// How do we store the indices? In a long array where
// the array index == line number? Makes sense, but
// we will need lots and lots of those indices. 
// The production logs are big, 33,526,328 lines for
// today's first 12 hours of production. IOW, we must
// scale for hundreds of millions of lines. One size_t
// is 8 bytes. 100 million line indices will therefore
// use 800MB of RAM. That's a lot, and kinda the opposite 
// of what we're trying to achieve here.
//
// Can we do partial indexing? I guess we can, but how?
// Can we use datatypes smaller than size_t? Not really.
// What if we prealloc and later have to realloc()? Can't see a problem RN :)
// Can we assume that the data is static? Yes, if we MAP_PRIVATE.
//
// Let's give indexing a go and time it.
// And here are the results: It takes 1:24m to index 17GB (33" lines)
// when caches are dropped prior to indexing. That's of course way
// too long. Vim (view -n) uses 1:50m, for comparison. 
//
// Anyway, we still need to know the start and end of the
// lines we want to display. We can't wait for minutes
// so we need to index on demand. How?
// 1. We always start at the top of the file (do we really?),
//    so for starters we can index nrows lines.

// 2. When the user scrolls down, we index the lines about to be displayed.
//    We may want to avoid reindexing if the user scrolls down, then up, then down.
//    Probably not a biggie, if indexing 200 lines is quick. Let's measure :)
//    a) Indexing 1000 lines takes 0.8 seconds
//    b) Indexing 200 lines takes 0.8 seconds

//    Not sure if these numbers are correct as e.g. 1" takes 1.4 seconds.
//    Why the non-linear performance? Probably due to IO.

//    Anyway, we still need a sliding window and dynamic indexing, so let's
//    just do that. How? It gets very tricky if we implement the 
//    commands g and G for top and bottom. Going to the bottom will
//    either require a lot of indexing, or we have to be clever and index
//    from the end and backwards. We can do that.

// 3. If we implement "goto line" then the user may choose to go to
//    the middle of the file. That requires a lot of indexing too.
//    
// Should we use an indexing thread? Not sure. Let's start without one.
// Let's see how far we can go using IOD (indexing on demand).

#if 0
#define NROWS_INITIAL (1024*1024*100) // 100 MB rows
#else
#define NROWS_INITIAL 1000000
#endif

static size_t *prows;    // The pointer to all our rows
static size_t nrowsused; // Total number of used elements in the prows array
static size_t rowsize;   // Total number of allocate elements

static void index_init(void)
{
    size_t cb = sizeof *prows * NROWS_INITIAL;

    if (prows == NULL && (prows = malloc(cb)) == NULL)
        die("Out of memory");

    memset(prows, 0, cb);
    nrowsused = 0;
    rowsize = NROWS_INITIAL;
}

static inline int index_full(void)
{
    return nrowsused == rowsize;
}


// Double the size of the index, but not exponentially.
// We add NROWS_INITIAL each time the index goes full.
// If that's too slow, go exponential.
static void index_expand(void)
{
    size_t newsize = rowsize + NROWS_INITIAL;
    void *tmp;

    tmp = realloc(prows, newsize);
    if (tmp == NULL)
        die("Out of memory");
    
}

static void index_add(size_t val)
{
    if (index_full())
        index_expand();

    prows[nrowsused++] = val;
}

static void index_file(const void *mem, size_t memsize, size_t nlines)
{
    const char *s, *cmem = mem;

    (void)memsize;
    index_init();

    // First index is always at 0, i.e. start of memory
    index_add(0);

    while (nlines-- && (s = strchr(cmem, '\n')) != NULL) {
        assert(*s == '\n');
        ptrdiff_t delta = s - (const char*)mem + 1; // Offset from start of file. Skip newline
        index_add((size_t)delta);
        cmem = s + 1;
    }

    // TODO: Last line issues? What if last char ain't newline?
}

static size_t get_row(size_t index, int *rowlen)
{
    assert(index < nrowsused);

    *rowlen = prows[index + 1] - prows[index]; // -1 for newline, which we skip
    return prows[index];
}

static void dump(const char *s, size_t n)
{
    FILE *fp = fopen("dump.txt", "a");
    while (n--)
        fputc(*s++, fp);

    fclose(fp);
}

int main(int argc, char *argv[])
{
    const char *filename = argv[1];
    int nrows, ncols;

    check_args(argc, argv);
    mmap_file(filename, &g_mem, &g_memsize);
    g_cmem = g_mem;


    // Now ncurses initialization
    initscr();
    raw();
    noecho();
    getmaxyx(stdscr, nrows, ncols);

    if (1) index_file(g_mem, g_memsize, nrows * 2);

    // Now show the first nrows of the file, just to see that we're not fscked
    if (1) {
        int i;

        for (i = 0; i < nrows; i++) {
            int len;
            size_t offset = get_row(i, &len);
            if (len == 0)
                continue; // blank line

            mvaddnstr(i, 0, &g_cmem[offset], len);
            dump(&g_cmem[offset], len);
        }

        refresh();

        getch();
        goto teardown; // Early exit for profiling reasons
    }


    // Put some text on the window 
    mvprintw(1, 10, "at y==1, x==10. Screen size is %d rows and %d cols", nrows, ncols);
    mvprintw(10, 1, "press any key to exit");
    mvaddnstr(0, 0, "hello, world", 10);
    refresh();
    getch();


 teardown:
    endwin(); 
    munmap(g_mem, g_memsize);
    return 0;
}

