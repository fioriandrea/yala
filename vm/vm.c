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
pushv(struct vm *vm, struct value val)
{
        *vm->sp++ = val;
}

static struct value
popv(struct vm *vm)
{
        return *--vm->sp;
}

static struct value
peekv(struct vm *vm, int offset)
{
        return *(vm->sp - offset);
}

int
vm_run(struct vm *vm)
{
        struct value val0;
        struct value val1;
        uint8_t current;
        uint8_t arg0;

        for (;;) {
        current = advance_ip(vm);
        switch (current) {
        case OP_LOCI:
                arg0 = advance_ip(vm);
                pushv(vm, bytecode_constant_at(vm->code, arg0));
                break;
        case OP_ADDI:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_int(val0.as.integer + val1.as.integer));
                break;
        case OP_SUBI:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_int(val0.as.integer - val1.as.integer));
                break;
        case OP_MULI:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_int(val0.as.integer * val1.as.integer));
                break;
        case OP_DIVI:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_int(val0.as.integer / val1.as.integer));
                break;
        case OP_IGRT:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(val0.as.integer > val1.as.integer));
                break;
        case OP_IGRTEQ:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(val0.as.integer >= val1.as.integer));
                break;
        case OP_ILT:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(val0.as.integer < val1.as.integer));
                break;
        case OP_ILEQ:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(val0.as.integer <= val1.as.integer));
                break;
        case OP_EQUA:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(values_equal(val0, val1)));
                break;
        case OP_NOT:
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(!val0.as.boolean));
                break;
        case OP_ZERO:
                pushv(vm, value_from_c_int(0));
                break;
        case OP_ONE:
                pushv(vm, value_from_c_int(1));
                break;
        case OP_SKIP:
                arg0 = advance_ip(vm);
                vm->ip += arg0;
                break;
        case OP_SKIPF:
                arg0 = advance_ip(vm);
                val0 = peekv(vm, 0);
                if (!val0.as.boolean) {
                        vm->ip += arg0;
                }
                break;
        case OP_POPV:
                popv(vm);
                break;
        case OP_HALT:
                val0 = popv(vm);
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