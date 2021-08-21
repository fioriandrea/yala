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
        while (vm->asp > vm->astack && vm->asp->type.id != VAL_VECTOR)
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

static void
runtime_error(struct vm *vm, int pos, char *fmt, ...)
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
        uint16_t arglong0;

        int indicesbuff[MAX_VECTOR_DIMENSIONS];
        int *indicesbuffp;

        for (;;) {
        current = advance_ip(vm);
        switch (current) {
        case OP_LOC_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                pushv(vm, bytecode_constant_at(vm->code, arglong0));
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
        case OP_FALSE:
                pushv(vm, value_from_c_bool(0));
                break;
        case OP_EMPTY_STRING:
                pushv(vm, value_from_c_string(""));
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
                if (val0.type.id == VAL_VECTOR) {
                        popa(vm);
                }
                break;
        case OP_POP_TO_ASTACK:
                val0 = popv(vm);
                pusha(vm, val0);
                break;
        case OP_LOAD_AND_LINK_VEC_TO_ASTACK_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                vm->code->constants.buffer[arglong0].as.vector.astackent = vm->asp;
                pusha(vm, bytecode_constant_at(vm->code, arg0));
                pushv(vm, bytecode_constant_at(vm->code, arg0));
                break;
        case OP_INIT_VEC_DIMS:
                arg0 = advance_ip(vm);
                for (int i = 0; i < arg0; i++) {
                        val0 = popv(vm);
                        pushd(vm, val0.as.integer);
                }
                val1 = popv(vm);
                val1.type.dimensions = vm->dsp - arg0;
                pushv(vm, val1);
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
        case OP_SET_INDEXED_LOCAL_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                arg0 = advance_ip(vm);
                val0 = vm->stack[arglong0];

                indicesbuffp = indicesbuff + arg0 - 1;
                for (int i = 0; i < arg0; i++) {
                        *indicesbuffp = popv(vm).as.integer;
                        indicesbuffp--;
                }
                for (int i = 0; i < arg0; i++) {
                        if (indicesbuff[i] >= val0.type.dimensions[i] || indicesbuff[i] < 0) {
                                runtime_error(vm, vm->ip - 1, "index out of bound (max index %d)", val0.type.dimensions[i] - 1);
                                return 1;
                        }
                }

                val1 = popv(vm);
                if (arg0 == val0.type.rank) {
                        vector_value_set_element_at(val0, index_flattened(val0.type.dimensions, indicesbuff, arg0), val1);
                } else {
                        for (int i = arg0; i < val0.type.rank; i++) {
                                indicesbuff[i] = 0;
                        }
                        int start = index_flattened(val0.type.dimensions, indicesbuff, val0.type.rank);
                        for (int i = 0; i < val1.type.size; i++) {
                                vector_value_set_element_at(val0, start + i, vector_value_get_element_at(val1, i));
                        }
                }
                break;
        case OP_GET_INDEX:
                arg0 = advance_ip(vm);
                indicesbuffp = indicesbuff + arg0 - 1;
                for (int i = 0; i < arg0; i++) {
                        *indicesbuffp = popv(vm).as.integer;
                        indicesbuffp--;
                }
                val0 = popv(vm);
                for (int i = 0; i < arg0; i++) {
                        if (indicesbuff[i] >= val0.type.dimensions[i] || indicesbuff[i] < 0) {
                                runtime_error(vm, vm->ip - 1, "index out of bound (max index %d, got %d)", val0.type.dimensions[i] - 1, indicesbuff[i]);
                                return 1;
                        }
                }
                if (arg0 == val0.type.rank) {
                        pushv(vm, vector_value_get_element_at(val0, index_flattened(val0.type.dimensions ,indicesbuff, arg0)));
                } else {
                        for (int i = arg0; i < val0.type.rank; i++) {
                                indicesbuff[i] = 0;
                        }
                        int start = index_flattened(val0.type.dimensions, indicesbuff, val0.type.rank);
                        int count = 1;
                        for (int i = arg0; i < val0.type.rank; i++) {
                                count *= val0.type.dimensions[i];
                        }
                        for (int i = start; i < start + count; i++) {
                                struct value from_main_vector = vector_value_get_element_at(val0, i);
                                pusha(vm, from_main_vector);
                        }
                        struct value result_value;
                        result_value.type.id = VAL_VECTOR;
                        result_value.type.rank = val0.type.rank - arg0;
                        result_value.type.size = count;
                        pusha(vm, result_value);
                        result_value.as.vector.astackent = vm->asp - 1;
                        result_value.type.dimensions = vm->dsp;
                        for (int i = 0; i < count; i++) {
                                pushd(vm, val0.type.dimensions[i + arg0]);
                        }
                        pushv(vm, result_value);
                }
                break;
        case OP_HALT:
                return 0;
        default:
                runtime_error(vm, vm->ip - 1, "NOT IMPLEMENTED: %s\n", opcodestring(current));
                return 0;
        }
        }
}