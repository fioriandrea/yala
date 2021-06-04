#include <stdio.h>
#include <stdlib.h>

#include "util.h"

int
main(int argc, char **argv)
{
        char *progname;
        char *fname;
        FILE *fp;
        size_t fsize;
        char *programtext;

        progname = *argv;
        if (argc == 1) {
                fprintf(stderr ,"usage: %s [OPTIONS] FILE\n", progname);
                exit(1);
        }

        fname = argv[1];
        fp = fopen(fname, "r");
        if (fp == NULL) {
                varperror("cannot open file '%s'", fname);
                exit(1);
        }
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        programtext = malloc(fsize + 1);
        if (programtext == NULL) {
                varperror("cannot allocate enough memory for file '%s'", fname);
                exit(1);
        }
        if (fread(programtext, 1, fsize, fp) != fsize) {
                varperror("cannot read whole file '%s'", fname);
                exit(1);
        }
        fclose(fp);
        programtext[fsize + 1] = '\0';
        printf("%s\n", programtext);
        free(programtext);
        return 0;
}