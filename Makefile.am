AUTOMAKE_OPTIONS=subdir-objects

AM_CFLAGS=\
	-D_GNU_SOURCE \
	-Winline -Wshadow -W -Wformat -Wall -pedantic -std=gnu99 -O0 -ggdb

bin_PROGRAMS=extract bf tcg nitfind nitpick find_unused_headers\
	mpp spaces find_odd_indentation find_duplicate_files\
    find_long_lines\
	find_unique_files\
    append\
    vbf\
	typesafe\
	genman genutil reformat gint dupdate dupfind filesize\
	add

tcg_SOURCES=tcg.c
bf_SOURCES=bf.c
find_duplicate_files_LDADD=-lgcrypt
mpp_LDADD=-lmeta
extract_LDADD=-lmeta
EXTRA_DIST=$(man_MANS)
man_MANS=tcg.1 bf.1

find_unused_headers_SOURCES=\
	find_unused_headers_src/file.c\
	find_unused_headers_src/main.c\
	find_unused_headers_src/standard_headers.c\
	find_unused_headers_src/file.h\
	find_unused_headers_src/misc.h


dupfind_SOURCES=dup/dupfind.c
dupdate_SOURCES=dup/dupdate.c
dupfind_LDADD=-lgcrypt
dupdate_LDADD=-lgcrypt

vbf_LDADD=-lncurses

