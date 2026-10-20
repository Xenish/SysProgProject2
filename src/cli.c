#include "cli.h"
#include <ctype.h>
#include <stdio.h>
int tokenize(char *line, char **argv) {
    char *readp = line, *writep = line;
    int argc = 0;
    while (*readp) {
        while (isspace((unsigned char)*readp)) ++readp;
        if (!*readp) break;
        if (argc == MAX_ARGS - 1) { fprintf(stderr, "Too many arguments\n"); return -1; }
        argv[argc++] = writep;
        char quote = 0;
        while (*readp && (quote || !isspace((unsigned char)*readp))) {
            char c = *readp++;
            if (c == '\\') {
                if (!*readp) { fprintf(stderr, "Trailing escape\n"); return -1; }
                *writep++ = *readp++;
            } else if (c == '\'' || c == '"') {
                if (!quote) quote = c;
                else if (quote == c) quote = 0;
                else *writep++ = c;
            } else *writep++ = c;
        }
        if (quote) { fprintf(stderr, "Unclosed quote\n"); return -1; }
        if (*readp) ++readp;
        *writep++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}
