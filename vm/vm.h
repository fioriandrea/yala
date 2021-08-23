#ifndef vm_h
#define vm_h

#include <stdint.h>

#include "../semantics/semantics.h"

#define STACK_MAX (1 << 10)
#define OP_READ_BUF_CAP (1 << 10)

struct vm {
        struct bytecode *code;
        int ip;
        union value *sp;
        union value stack[STACK_MAX];
        union value *asp;
        union value astack[STACK_MAX];
};

void vm_init(struct vm *vm, struct bytecode *code);
int vm_run(struct vm *vm);

#endif