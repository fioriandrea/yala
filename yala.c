#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frontend/frontend.h"

#include "semantics/semantics.h"
#include "vm/vm.h"

void
varperror(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, ": %s\n", strerror(errno));
        va_end(args);
}

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
        struct tree_node *root;
        int semanticsres;
        struct bytecode *code;
        struct vm vm;

        progname = *argv;
        if (argc < 2) {
                fprintf(stderr ,"usage: %s [OPTIONS] FILE\n", progname);
                exit(1);
        }
        programtext = load_program(progname, argv[1], &proglen);

        root = parse(programtext, proglen);
        if (!root)
                exit(1);
        treeprint(root);

        analyze_semantics(&semanticsres, root);
        if (!semanticsres)
                exit(1);

        code = generate_bytecode(root);
        if (!code)
                exit(1);
        disassemble(code);

        vm_init(&vm, code);
        vm_run(&vm);

        tree_node_free(root);
        bytecode_free(code);
        free(programtext);
        return 0;
}