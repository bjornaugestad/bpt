/**
 * NOTE: This is a copy of mpp.c. We change it a bit to support 
 * preprocessing of C code instead of C embedded in HTML.
 * The purpose is to better be able to embed C/C++ template code
 * within the meta code generator. 
 * The rules are these:
 * - Anything between <% and %> will be converted to a const char* array
 * - The string will be named whatever you name it.
 * - The proper syntax is therefore <% stringname ... %>
 * Since this is an internal tool, no <[ ]> is needed.
 * No prototyping is needed and will be removed.
 *
 * Note that we don't create arrays of strings, just strings. This is
 * done to make it easier to embed printf directives within the string.
 *
 * Syntax example:
 
<% mystring 
int somefunc(int i, int bar)
{
	...
}
%>
	fprintf(f, mystring, optional_args);

 * Limits:
 * Line length: 2048
 *
 * Version: 0.0.1
 * Author : bjorn.augestad@gmail.com
 */

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring.h>
#include <assert.h>



#include <highlander.h>
#include <meta_misc.h>

/* Line number in current file */
char* g_current_file = NULL;
char* g_outputfile = NULL;

static int m_skip_line_numbers = 0;
static int lineno = 0;

/* local helper functions */
static void print_line_directive(FILE *f, const char* file, int line);
static void show_help(void);
static void process_file(char* name);
static void write_buffer(FILE* fout, const char* html, const char* name);

static void get_stringname(FILE* f, char* name);

int main(int argc, char *argv[])
{
	int c; 
	extern int optind;
	extern char* optarg;

	while( (c = getopt(argc, argv, "Eh")) != EOF) {
		switch(c) {
			case 'h':
				show_help();
				exit(EXIT_SUCCESS);
				break;

			case 'E':
				m_skip_line_numbers = 1;
				break;

			default:
				fprintf(stderr, "mpp: Unknown parameter\n");
				exit(EXIT_FAILURE);
		}
	}

	if(optind == argc) {
		fprintf(stderr, "mpp: No input files\n");
		exit(EXIT_FAILURE);
	}
	else if(g_outputfile != NULL && optind + 1 < argc) {
		fprintf(stderr, "-o option is only valid if input is one file\n");
		exit(EXIT_FAILURE);
	}

	{
		int print = 0;
		if(argc - optind > 1)
			print = 1;


		while(optind < argc) {
			if(print)
				printf("%s...\n", argv[optind]);

			process_file(argv[optind++]);
		}
	}


	exit(EXIT_SUCCESS);
}


static void show_help(void)
{
	printf("No help yet\n");
}

#if 0
static const char* legal_name(const char* base)
{
	char *s;
	static char name[1024];

	/* Create legal name */
	strcpy(name, base);
	s = name;
	while(*s != '\0') {
		if(!isalnum(*s) && *s != '_')
			*s = '_';

		s++;
	}

	return name;
}

/* call this function with full path.
 * It creates *the* function name we want to use. 
 */
static const char* function_name(char* file)
{
	static char name[1024];
	char base[1024];

	assert(file != NULL);

	get_basename(file, base, NULL, sizeof base);
	
	strcpy(name, "hipp_");
	strcat(name, legal_name(base));
	return name;
}
#endif


static void write_buffer(FILE* f, const char* str, const char* name)
{
	const char *s;
	char last = '\0';

	s = str;
	fprintf(f, "static const char* %s = \n", name);
	fprintf(f, "\"");
	while(*s) {
		if(*s == '\\') {
			fprintf(f, "\\\\");
		}
		else if(*s == '"' && last != '\\')
			fprintf(f, "\\%c", *s);
		else if(*s == '\n')
			fprintf(f, "\\n\"\n\"");
		else
			fputc(*s, f);


		last = *s;
		s++;
	}

	fprintf(f, "\";\n");
}

static const char* remove_ext(char* name)
{
	static char buf[10000];
	char *s;

	strcpy(buf, name);
	s = buf + strlen(buf) - 1;
	while(s != buf) {
		if(*s == '.') {
			*s = '\0';
			break;
		}
		s--;
	}

	if(s == buf) {
		fprintf(stderr, "mpp: internal error\n");
		exit(EXIT_FAILURE);
	}
	
	return buf;
}

int peek(FILE* f)
{
	int c = fgetc(f);
	ungetc(c, f);
	return c;
}

static void process_file(char* name)
{
	FILE *fin, *fout;
	char fname[10240];
	char base[10240];
	char stringname[1000] = { '\0' };
	int in_tag = 0;
	int last = '\0', c;

	/* buffers html */
	cstring buf = cstring_new();

	get_basename(name, NULL, base, sizeof base);

	if(g_outputfile != NULL)
		strcpy(fname, g_outputfile);
	else {
		strcpy(fname, remove_ext(base));
		strcat(fname, ".c");
	}

	lineno = 1;
	g_current_file = name;
	if( (fin = fopen(name, "r")) == NULL) {
		perror(name);
		exit(EXIT_FAILURE);
	}
	else if( (fout = fopen(fname, "w")) == NULL) {
		perror(fname);
		exit(EXIT_FAILURE);
	}

	print_line_directive(fout, g_current_file, lineno);

	while( (c = fgetc(fin)) != EOF) {
		if(c == '\n')
			lineno++;

		if(!in_tag) {
			if(last == '\n' && c == '<' && peek(fin) == '%') {
				/* We're starting tag mode */
				in_tag = 1;
				print_line_directive(fout, g_current_file, lineno);
				fgetc(fin); /* Skip % */
				get_stringname(fin, stringname);
			}
			else 
				fputc(c, fout);
		}
		else if(c == '%' && peek(fin) == '>') {
			/* We're leaving tag mode */
			in_tag = 0;
			write_buffer(fout, c_str(buf), stringname);
			cstring_recycle(buf);
			fgetc(fin); /* Skip > */
		}
		else if (!cstring_charcat(buf, c)) {
            /* We're in tag mode and did not find %> */
            die("Out of memory");
        }
			
		last = c;
	}

	/* Buffers at end of file are meaningless as they cannot be accessed */
	if(cstring_length(buf) > 0) {
		fprintf(stderr, "Warning: Buffer lost due to EOF\n");
		fprintf(fout, "%s", c_str(buf));
	}

	fclose(fin);
	fclose(fout);
}

static void print_line_directive(FILE *f, const char* file, int line)
{
	if(!m_skip_line_numbers) 
		fprintf(f, "#line %d \"%s\"\n", line, file);
}


/*
 * The syntax goes like this: <% stringname ... %>
 * and the file is now positioned at the first space after the first %.
 * The space has not been read. We skip ws and read name. 
 * Errors are fatal...
 */
static void get_stringname(FILE* f, char* name)
{
	int c;

	/* Skip ws */
	while( (c = peek(f)) != EOF) {
		if(!isspace(c))
			break;
		fgetc(f);
	}

	/* Now read the name */
	while( (c = fgetc(f)) != EOF && !isspace(c)) 
		*name++ = c;
	
	if(c == '\n')
		lineno++;

	*name = '\0';
}

