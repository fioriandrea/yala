#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "frontend/frontend.h"

char *
load_program(char *progname, char *fname, int *proglen)
{
        FILE *fp;
        size_t fsize;
        char *programtext;

        fp = fopen(fname, "r");
        if (fp == NULL) {
                varperror("%s: cannot open file '%s'", progname, fname);
                exit(1);
        }
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        programtext = malloc(fsize + 1);
        if (programtext == NULL) {
                varperror("%s: cannot allocate enough memory for file '%s'", progname, fname);
                exit(1);
        }
        if (fread(programtext, 1, fsize, fp) != fsize) {
                varperror("%s: cannot read whole file '%s'", progname, fname);
                exit(1);
        }
        fclose(fp);
        programtext[fsize + 1] = '\0';
        *proglen = fsize;
        return programtext;
}

int
main(int argc, char **argv)
{
        char *progname;
        char *programtext;
        int proglen;

        progname = *argv;
        if (argc < 2) {
                fprintf(stderr ,"usage: %s [OPTIONS] FILE\n", progname);
                exit(1);
        }
        programtext = load_program(progname, argv[1], &proglen);

        parse(programtext, proglen);
        free(programtext);
        return 0;
}