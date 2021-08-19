#include <stdarg.h>
#include <stdio.h>

#include "vm.h"

void
vm_init(struct vm *vm, struct bytecode *code)
{
        vm->code = code;
        vm->sp = vm->stack;
        vm->asp = vm->astack;
        vm->dsp = vm->dstack;
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

static void
pusha(struct vm *vm, struct value val)
{
        *vm->asp++ = val;
}

static void
popa(struct vm *vm) {
        vm->asp--;
        while (vm->asp > vm->astack && vm->asp->type.type != VAL_VECTOR)
                vm->asp--;
}

static void
pushd(struct vm *vm, int i)
{
        *vm->dsp++ = i;
}

static struct value
peekv(struct vm *vm, int offset)
{
        return *(vm->sp - offset);
}

static uint16_t
join_bytes(uint8_t left, uint8_t right)
{
        uint16_t res = left;
        res = ((uint16_t) res) << 8;
        res = res | right;
        return res;
}

static void
runtime_error(struct vm *vm, uint8_t pos, char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        struct lineinfo linfo = linelist_at(&vm->code->lines, pos);
        fprintf(stderr, "runtime error ");
        fprintf(stderr, "[at %d:%d]: ", linfo.line, linfo.linepos);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}

int
vm_run(struct vm *vm)
{
        struct value val0;
        struct value val1;
        uint8_t current;
        uint8_t arg0, arg1;
        uint8_t arglong0;

        for (;;) {
        current = advance_ip(vm);
        switch (current) {
        case OP_LOCI:
        case OP_LOCS:
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
                if (val1.as.integer == 0) {
                        runtime_error(vm, vm->ip - 1, "division by 0");
                        return 0;
                }
                pushv(vm, value_from_c_int(val0.as.integer / val1.as.integer));
                break;
        case OP_IGRT:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(compare_values(val0, val1) > 0));
                break;
        case OP_IGRTEQ:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(compare_values(val0, val1) >= 0));
                break;
        case OP_ILT:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(compare_values(val0, val1) < 0));
                break;
        case OP_ILEQ:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(compare_values(val0, val1) <= 0));
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
        case OP_SKIP_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                vm->ip += arglong0;
                break;
        case OP_SKIP_BACK_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                vm->ip -= arglong0;
                break;
        case OP_SKIPF_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                val0 = peekv(vm, 1);
                if (!val0.as.boolean) {
                        vm->ip += arglong0;
                }
                break;
        case OP_POPV:
                val0 = popv(vm);
                if (val0.type.type == VAL_VECTOR) {
                        popa(vm);
                }
                break;
        case OP_POP_TO_ASTACK:
                val0 = popv(vm);
                pusha(vm, val0);
                break;
        case OP_PATCH_VEC:
                arg0 = advance_ip(vm);
                vm->code->constants.buffer[arg0].as.vector.astackent = vm->asp;
                pusha(vm, bytecode_constant_at(vm->code, arg0));
                pushv(vm, bytecode_constant_at(vm->code, arg0));
                break;
        case OP_VEC_TYPE:
                val0 = popv(vm);
                for (int i = 0; i < val0.as.integer; i++) {
                        val1 = popv(vm);
                        pushd(vm, val1.as.integer);
                }
                {
                        struct value val2;
                        val2 = popv(vm);
                        val2.type.meta.vector.dimensions = vm->dsp - val0.as.integer;
                        pushv(vm, val2);
                }
                break;
        case OP_NEWLINE:
                printf("\n");
                break;
        case OP_WRITE:
                arg0 = advance_ip(vm);
                for (struct value *p = vm->sp - arg0; p < vm->sp; p++) {
                        value_print(*p);
                }
                vm->sp = vm->sp - arg0;
                break;
        case OP_GET_LOCAL_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                pushv(vm, vm->stack[arglong0]);
                break;
        case OP_SET_LOCAL_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                vm->stack[arglong0] = popv(vm);
                break;
        case OP_HALT:
                return 0;
        default:
                runtime_error(vm, vm->ip - 1, "NOT IMPLEMENTED: %s\n", opcodestring(current));
                return 0;
        }
        }
}