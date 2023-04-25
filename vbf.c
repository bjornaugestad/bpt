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
    *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (*mem == MAP_FAILED)
        die_perror("%s", filename);

    close(fd);
}

int main(int argc, char *argv[])
{
    void *mem;
    size_t memsize;
    const char *filename = argv[1];
    int nrows, ncols;

    check_args(argc, argv);
    mmap_file(filename, &mem, &memsize);

    // Now ncurses initialization
    initscr();
    raw();
    noecho();
    getmaxyx(stdscr, nrows, ncols);

    // Put some text on the window 
    mvprintw(1, 10, "at y==1, x==10. Screen size is %d rows and %d cols", nrows, ncols);
    mvprintw(10, 1, "press any key to exit");
    refresh();
    getch();

    // Do we want to index the mmapped file? We do need to
    // know the start and end of each line, so we later can
    // display those lines. 


    // Teardown
    endwin(); 
    munmap(mem, memsize);
    return 0;
}

