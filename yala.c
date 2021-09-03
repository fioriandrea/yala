#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frontend/frontend.h"
#include "semantics/semantics.h"
#include "serialization/serialization.h"
#include "vm/vm.h"

static void varperror(char *fmt, ...);
static void progvarperror(char *fmt, ...);
static void progerror(char *fmt, ...);
static void run_run(char *programtext, int proglen);
static void run_compile(char *programtext, int proglen);

enum run_mode {
        RUN_RUN,
        RUN_COMPILE,
        RUN_EXECUTE,
        RUN_HELP,
};

struct run_mode_string {
        char *str;
        enum run_mode rm;
};

struct run_mode_string run_mode_strings[] = {
        {"run", RUN_RUN},
        {"compile", RUN_COMPILE},
        {"execute", RUN_EXECUTE},
        {"help", RUN_HELP},
        {NULL, 0},
};

static char *progname = "";
static int display_tree;
static int display_bytecode;
static int no_execute = 0;
static int run_mode;
static char *run_mode_str;
static char *input_path = NULL;
static char *output_path = NULL;

static void
print_help()
{
        printf("usage: yala <mode> [options] input_file\n\n"
                "The modes are:\n\n"
                "run                     compile and run a Yala program\n"
                "compile                 compile a Yala program\n"
                "execute                 execute a compiled Yala program\n"
                "help                    prints this help\n\n"
                "The options are:\n\n"
                "--display-tree          show the syntax tree. Applicable in run and compile mode.\n"
                "--display-bytecode      show the bytecode. Applicable in all modes.\n"
                "--no-execute            do not execute the program. Applicable in run and compile mode.\n"
                "--output out_file       outputs compiled code to out_file. Applicable in compile mode.\n"
              );
}

static void
parse_option(char *option, int *argcp, char ***argvp)
{
        if (strcmp(option, "--display-tree") == 0 && (run_mode == RUN_RUN || run_mode == RUN_COMPILE)) {
                display_tree = 1;
        } else if (strcmp(option, "--display-bytecode") == 0 && (run_mode != RUN_HELP)) {
                display_bytecode = 1;
        } else if (strcmp(option, "--no-execute") == 0 && (run_mode == RUN_RUN || run_mode == RUN_EXECUTE)) {
                no_execute = 1;
        } else if (strcmp(option, "--output") == 0 && (run_mode == RUN_COMPILE)) {
                (*argcp)--;
                output_path = *((*argvp)++);
        } else {
                progerror("unrecognized option %s in mode %s\n", option, run_mode_str);
                exit(1);
        }
}

static void
parse_cli_arguments(int argc, char **argv)
{
        progname = *argv++;
        argc--;
        if (argc == 0) {
                print_help();
                exit(1);
        }
        run_mode_str = *argv++;
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
                parse_option(option, &argc, &argv);
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

static struct tree_node *
parse_file(char *programtext, int proglen)
{
        struct tree_node *root;

        root = parse(programtext, proglen);
        if (!root)
                exit(1);

        if (display_tree)
                tree_node_print(root);

        return root;
}

static struct bytecode
compile_tree(char *programtext, struct tree_node *root)
{
        struct bytecode *code;

        code = generate_bytecode(root);
        if (code == NULL)
                exit(1);

        tree_node_free(root);
        free(programtext);

        if (display_bytecode)
                disassemble(code);

        return *code;
}

static void
execute_code(struct bytecode *code)
{

        struct vm vm;

        if (no_execute)
                return;

        vm_init(&vm, code);
        vm_run(&vm);
}

static void
run_run(char *programtext, int proglen)
{
        struct tree_node *root = parse_file(programtext, proglen);
        struct bytecode code = compile_tree(programtext, root);
        execute_code(&code);
}

static void
run_compile(char *programtext, int proglen)
{
        struct tree_node *root = parse_file(programtext, proglen);
        struct bytecode code = compile_tree(programtext, root);
        if (output_path == NULL) {
                progerror("must supply output file\n");
                exit(1);
        }
        FILE *outfile = fopen(output_path, "w");
        if (outfile == NULL) {
                progvarperror("cannot open file %s", output_path);
                exit(1);
        }
        serialize_bytecode(&code, outfile);
        fclose(outfile);
}

static void
run_execute(char *programtext, int proglen)
{
        struct bytecode code;
        deserialize_bytecode(&code, programtext);
        if (display_bytecode)
                disassemble(&code);
        execute_code(&code);
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
                        run_compile(programtext, proglen);
                        break;
                case RUN_EXECUTE:
                        run_execute(programtext, proglen);
                        break;
                case RUN_HELP:
                        print_help();
                        break;
        }

        return 0;
}
