#ifndef vm_h
#define vm_h

#include <stdint.h>

#include "../semantics/semantics.h"

#define STACK_MAX (1 << 10)

struct vm {
        struct bytecode *code;
        int ip;
        struct value *sp;
        struct value stack[STACK_MAX];
};

void vm_init(struct vm *vm, struct bytecode *code);
int vm_run(struct vm *vm);
void vm_free(struct vm *vm);

#endif