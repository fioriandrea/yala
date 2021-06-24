#include <stdio.h>
#include "vm.h"

void
vm_init(struct vm *vm, struct bytecode *code)
{
        vm->code = code;
        vm->sp = vm->stack;
        vm->ip = 0;
}

static uint8_t
advance_ip(struct vm *vm)
{
        return bytecode_byte_at(vm->code, vm->ip++);
}

static void
push(struct vm *vm, struct value val)
{
        *vm->sp++ = val;
}

static struct value
pop(struct vm *vm)
{
        return *--vm->sp;
}

int
vm_run(struct vm *vm)
{
        struct value val0;
        struct value val1;
        uint8_t current;
        uint8_t constaddr;

        for (;;) {
        current = advance_ip(vm);
        switch (current) {
        case OP_LOCI:
                constaddr = advance_ip(vm);
                push(vm, bytecode_constant_at(vm->code, constaddr));
                break;
        case OP_ADDI:
                val1 = pop(vm);
                val0 = pop(vm);
                push(vm, value_from_c_int(val0.as.integer + val1.as.integer));
                break;
        case OP_SUBI:
                val1 = pop(vm);
                val0 = pop(vm);
                push(vm, value_from_c_int(val0.as.integer - val1.as.integer));
                break;
        case OP_MULI:
                val1 = pop(vm);
                val0 = pop(vm);
                push(vm, value_from_c_int(val0.as.integer * val1.as.integer));
                break;
        case OP_DIVI:
                val1 = pop(vm);
                val0 = pop(vm);
                push(vm, value_from_c_int(val0.as.integer / val1.as.integer));
                break;
        case OP_ZERO:
                push(vm, value_from_c_int(0));
                break;
        case OP_HALT:
                val0 = pop(vm);
                printf("HALT: ");
                print_value(val0);
                printf("\n");
                return 0;
                break;
        default:
                printf("NOT IMPLEMENTED: %s\n", opcodestring(current));
                return 1;
                break;
        }
        }
}

void
vm_free(struct vm *vm)
{
        bytecode_free(vm->code);
}