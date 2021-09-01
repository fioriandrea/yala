#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frontend/frontend.h"

#include "semantics/semantics.h"
#include "vm/vm.h"

static void varperror(char *fmt, ...);
static void progvarperror(char *fmt, ...);
static void progerror(char *fmt, ...);
static void run_run(char *programtext, int proglen);

enum run_mode {
        RUN_RUN,
        RUN_COMPILE,
        RUN_EXECUTE,
};

struct run_mode_string {
        char *str;
        enum run_mode rm;
};

struct run_mode_string run_mode_strings[] = {
        {"run", RUN_RUN},
        {"compile", RUN_COMPILE},
        {"execute", RUN_EXECUTE},
        {NULL, 0},
};

static char *progname = "";
static int display_tree;
static int display_bytecode;
static int run_mode;
static char *input_path;

static void
parse_cli_arguments(int argc, char **argv)
{
        progname = *argv++;
        argc--;
        if (argc == 0) {
                progerror("must supply mode\n");
                exit(1);
        }
        char *run_mode_str = *argv++;
        argc--;
        for (struct run_mode_string *rms = run_mode_strings; ; rms++) {
                if (rms->str == NULL) {
                        progerror("unrecognized mode %s\n", run_mode_str);
                        exit(1);
                }
                if (strcmp(rms->str, run_mode_str) == 0) {
                        run_mode = rms->rm;
                        break;
                }
        }
        while (argc > 0 && (*argv)[0] == '-' && (*argv)[1] == '-') {
                char *option = *argv++;
                argc--;
                if (strcmp(option, "--display-tree") == 0) {
                        display_tree = 1;
                } else if (strcmp(option, "--display-bytecode") == 0) {
                        display_bytecode = 1;
                }
        }
        if (argc == 0) {
                progerror("must supply a file\n");
                exit(1);
        }
        input_path = *argv++;
        argc--;
}

static void
varperror(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, ": %s\n", strerror(errno));
        va_end(args);
}

static void
progerror(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "%s: ", progname);
        vfprintf(stderr, fmt, args);
        va_end(args);
}

static void
progvarperror(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "%s: ", progname);
        varperror("");
        va_end(args);
}

static char *
load_program(char *fname, int *proglen)
{
        FILE *fp;
        size_t fsize;
        char *programtext;

        fp = fopen(fname, "r");
        if (fp == NULL) {
                progvarperror("cannot open file '%s'", fname);
                exit(1);
        }
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        programtext = malloc(fsize + 1);
        if (programtext == NULL) {
                progvarperror("cannot allocate enough memory for file '%s'", fname);
                exit(1);
        }
        if (fread(programtext, 1, fsize, fp) != fsize) {
                progvarperror("cannot read whole file '%s'", fname);
                exit(1);
        }
        fclose(fp);
        programtext[fsize] = '\0';
        *proglen = fsize;
        return programtext;
}

static void
run_run(char *programtext, int proglen)
{
        struct tree_node *root;
        struct bytecode *code;
        struct vm vm;

        root = parse(programtext, proglen);
        if (!root)
                exit(1);

        if (display_tree)
                tree_node_print(root);

        code = generate_bytecode(root);
        if (code == NULL)
                exit(1);

        tree_node_free(root);
        free(programtext);

        if (display_bytecode)
                disassemble(code);

        vm_init(&vm, code);
        vm_run(&vm);
}

int
main(int argc, char **argv)
{
        char *programtext;
        int proglen;

        parse_cli_arguments(argc, argv);

        programtext = load_program(input_path, &proglen);

        switch (run_mode) {
                case RUN_RUN:
                        run_run(programtext, proglen);
                        break;
                case RUN_COMPILE:

                break;
                case RUN_EXECUTE:
                
                break;
        }

        return 0;
}
