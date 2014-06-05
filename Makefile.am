AM_CFLAGS=\
	-D_GNU_SOURCE \
	-Winline -Wshadow -W -Wformat -Wall -pedantic -std=gnu99 -Werror -O -DNDEBUG

bin_PROGRAMS=bf tcg 
tcg_SOURCES=tcg.c
bf_SOURCES=bf.c
EXTRA_DIST=$(man_MANS)
man_MANS=tcg.1 bf.1


