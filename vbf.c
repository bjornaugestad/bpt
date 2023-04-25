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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    struct stat st;
    int fd;
    void *mem;
    const char *filename = argv[1];

    if (argc != 2) {
        fprintf(stderr, "USAGE: vbf filename\n");
        exit(EXIT_FAILURE);
    }

    if ((fd = open(filename, O_RDONLY)) == -1) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    if (fstat(fd, &st)) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    if (st.st_size == 0) {
        fprintf(stderr, "vbf: file %s is empty\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        perror(filename);
        exit(EXIT_FAILURE);
    }
    close(fd);


    // Now we're ready to start processing
    // Just for proof: print one line
    {
        char line[4096], *p;

        p = strchr(mem, '\n');
        if (p == NULL)
            fprintf(stderr, "Could not find a line in the mapped file named %s\n", filename);
        else {
            ptrdiff_t delta = p - (char *)mem;
            if ((size_t)delta >= sizeof line) 
                fprintf(stderr, "%s: Found a line, but it was too long for our buffer\n", filename);
            else {
                memcpy(line, mem, delta);
                line[delta] = '\0';
                printf("%s\n", line);
            }
        }
    }



    // Teardown
    munmap(mem, st.st_size);
    return 0;
}

