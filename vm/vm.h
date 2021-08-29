#ifndef vm_h
#define vm_h

#include <stdint.h>

#include "../semantics/semantics.h"

#define STACK_MAX (1 << 10)
#define OP_READ_BUF_CAP (1 << 10)

struct stack_frame {
        union value *sp;
        union value *stackbase;
        union value *asp;
        int ip;
        struct bytecode *code;
};

struct vm {
        struct stack_frame *framese;
        union value stack[STACK_MAX];
        union value astack[STACK_MAX];
        struct stack_frame framestack[STACK_MAX];
};

void vm_init(struct vm *vm, struct bytecode *code);
void stack_frame_init(struct stack_frame *sf, union value *sp, union value *stackbase, union value *asp, struct bytecode *code);
int vm_run(struct vm *vm);

#endif
