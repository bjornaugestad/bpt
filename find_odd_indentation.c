#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>


static int count(const char *s)
{
    int n = 0;

    while(isspace(*s++))
        n++;

    return n;
}

int main(int argc, char *argv[])
{
    FILE *f;
    unsigned lineno;
    int indent;
    char line[10240];

    if (argc == 1) {
        fprintf(stderr, "USAGE: %s file...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    while (*++argv != NULL) {
        if ((f = fopen(*argv, "r")) == NULL) {
            perror(*argv);
            continue;
        }

        lineno = 0;
        indent = 0;
        while (fgets(line, sizeof line, f) != NULL) {
            int d, curr_indent = count(line);
            d = curr_indent - indent;
            lineno++;

            if (indent > 3 && d > 4) 
                printf("%s(%d): warning: Odd indentation 1\n", *argv, lineno);
            else if (curr_indent > 3 && curr_indent % 4 != 0)
                printf("%s(%d): warning: Odd indentation 2\n", *argv, lineno);

            indent = curr_indent;
        }

        fclose(f);
    }

    exit(EXIT_SUCCESS);
}
