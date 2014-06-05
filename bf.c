/**
 * bf.c Buffers stdin and writes it to argv[1] when no more
 * data is available. The idea is to be able to use command lines like
 * sed 's/foo/bar/' < foo | bf foo. Not the most useful command, but
 * then we don't have to do: sed 's/foo/bar' < foo > x; mv x foo
 * which really sucks if sed (or any other program) fails.
 *
 * We do not use temp. files. We should, but then we'd have to read the
 * data twice in case the target file resides on another file system than
 * the temp file. Instead we create a file name based on target file name
 * plus pid on the same file system. Later we unlink the target file and
 * rename the temp file.
 *
 * Copyright(c) 1989-2006, B. Augestad, bjorn.augestad@gmail.com 
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	char filename[10240]; /* os/fs independent size... */
	char line[10240];
	FILE *f;

	/* Some tools, like indent, reads and writes to the same file,
	 * We do not want to overwrite the original with an empty file
	 * if we didn't receive any input.
	 */
	int written_at_least_one_line = 0;

	if(argc != 2) {
		fprintf(stderr, "USAGE: bf filename\n");
		exit(EXIT_FAILURE);
	}

	if(strlen(argv[1]) + 100 > sizeof(filename)) {
		fprintf(stderr, "Filename too long: %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	sprintf(filename, "%s.%lu", argv[1], (unsigned long)getpid());
	if( (f = fopen(filename, "w")) == NULL) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	while(fgets(line, sizeof(line), stdin)) {
		if(fputs(line, f) == EOF) {
			perror(filename);
			unlink(filename);
			exit(EXIT_FAILURE);
		}

		written_at_least_one_line = 1;
	}

	if(fclose(f)) {
		perror(filename);
		unlink(filename);
		exit(EXIT_FAILURE);
	}

	if(!written_at_least_one_line) {
		unlink(filename);
		exit(EXIT_SUCCESS);
	}

	/* Unlink the original file and rename the temp file .
	 * The original file doesn't have to exist, so ignore ENOENT */
	/* NOTE: Why unlink() when ANSI C remove() exists? 20061226 boa */
	if(unlink(argv[1]) && errno != ENOENT) {
		perror(argv[1]);
		unlink(filename);
		exit(EXIT_FAILURE);
	}

	if(rename(filename, argv[1])) {
		perror(argv[1]);
		fprintf(stderr, "Your data may be found in the file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

