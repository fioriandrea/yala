#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

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

void
vm_init(struct vm *vm, struct bytecode *code)
{
        vm->code = code;
        vm->sp = vm->stack;
        vm->asp = vm->astack;
        vm->ip = 0;
}

static uint8_t
advance_ip(struct vm *vm)
{
        return bytecode_byte_at(vm->code, vm->ip++);
}

static void
pushv(struct vm *vm, union value val)
{
        *vm->sp++ = val;
}

static union value
popv(struct vm *vm)
{
        return *--vm->sp;
}

static union value
peekv(struct vm *vm, int offset)
{
        return *(vm->sp - offset);
}

static void
pusha(struct vm *vm, union value val)
{
        *vm->asp++ = val;
}

static void
popa(struct vm *vm, int size) {
        for (int i = 0; i < size; i++) {
                vm->asp--;
        }
}

/* removes trailing new line */
static int
mgetline(char *buff, int cap)
{
        char c;
        char *p = buff;
        while (p - buff < cap - 1 && (c = getchar()) != EOF && c != '\n') {
                *p++ = c;
        }
        *p = '\0';
        return p - buff;
}

static int
atob(char *s)
{
#define STR_FALSE_LEN 6
        int i = 0;
        char buff[STR_FALSE_LEN + 1];

        while (*s && isspace(*s)) {
                s++;
        }
        while (*s && !isspace(*s) && i < STR_FALSE_LEN) {
                buff[i++] = *s++;
        }
        buff[i] = '\0';
        if (strcmp(buff, "true") == 0) {
                return 1;
        } else {
                return 0;
        }
#undef STR_FALSE_LEN
}

static void
dispatch_op_read(struct vm *vm, enum value_type vt, char *buffer, int cap)
{
        mgetline(buffer, cap);

        switch (vt) {
                case VAL_BOOLEAN:
                        pushv(vm, value_from_c_bool(atob(buffer)));
                        break;
                case VAL_INTEGER:
                        pushv(vm, value_from_c_int(atoi(buffer)));
                        break;
                case VAL_STRING:
                        pushv(vm, value_from_c_string(buffer));
                        break;
        }
}

static int *
read_from_stack_to_int_buffer(struct vm *vm, int *buffer, int len)
{
        int *bufferp = buffer + len - 1;
        for (int i = 0; i < len; i++) {
                *bufferp = popv(vm).integer;
                bufferp--;
        }
        return bufferp;
}

static int
is_out_of_bounds(struct vm *vm, int *indicesbuff, int *dimensionsbuff, int len)
{
        for (int i = 0; i < len; i++) {
                if (indicesbuff[i] >= dimensionsbuff[i] || indicesbuff[i] < 0) {
                        runtime_error(vm, vm->ip - 1, "index out of bound (max index %d)", dimensionsbuff[i] - 1);
                        return 1;
                }
        }
        return 0;
}

static void
load_indexing_prelude(struct vm *vm, int *indicesbuff, int nindices, int *dimensionsbuff, int rank)
{
                read_from_stack_to_int_buffer(vm, dimensionsbuff, rank);
                read_from_stack_to_int_buffer(vm, indicesbuff, nindices);
}

int
vm_run(struct vm *vm)
{
        char readbuff[OP_READ_BUF_CAP];
        union value val0;
        union value val1;
        enum opcode current;
        uint8_t arg0, arg1;
        uint16_t arglong0;

        int indicesbuff[MAX_VECTOR_DIMENSIONS];
        int dimensionsbuff[MAX_VECTOR_DIMENSIONS];

        for (;;) {
        current = advance_ip(vm);
        switch (current) {
        case OP_LOC_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                pushv(vm, bytecode_constant_at(vm->code, arglong0));
                break;
        case OP_PUSH_BYTE:
                val0 = value_from_c_int(advance_ip(vm));
                pushv(vm, val0);
                break;
        case OP_ADDI:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_int(val0.integer + val1.integer));
                break;
        case OP_SUBI:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_int(val0.integer - val1.integer));
                break;
        case OP_MULI:
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_int(val0.integer * val1.integer));
                break;
        case OP_DIVI:
                val1 = popv(vm);
                val0 = popv(vm);
                if (val1.integer == 0) {
                        runtime_error(vm, vm->ip - 1, "division by 0");
                        return 0;
                }
                pushv(vm, value_from_c_int(val0.integer / val1.integer));
                break;
        case OP_GRT:
                arg0 = advance_ip(vm);
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(compare_values(val0, val1, arg0) > 0));
                break;
        case OP_GRTEQ:
                arg0 = advance_ip(vm);
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(compare_values(val0, val1, arg0) >= 0));
                break;
        case OP_LT:
                arg0 = advance_ip(vm);
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(compare_values(val0, val1, arg0) < 0));
                break;
        case OP_LEQ:
                arg0 = advance_ip(vm);
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(compare_values(val0, val1, arg0) <= 0));
                break;
        case OP_EQUA:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                val1 = popv(vm);
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(values_equal(val0, val1, arg0, arg1)));
                break;
        case OP_NOT:
                val0 = popv(vm);
                pushv(vm, value_from_c_bool(!val0.boolean));
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
                if (!val0.boolean) {
                        vm->ip += arglong0;
                }
                break;
        case OP_POPV:
                popv(vm);
                break;
        case OP_POPA:
                val0 = popv(vm);
                popa(vm, val0.vector.size);
                break;
        case OP_POP_TO_ASTACK:
                val0 = popv(vm);
                pusha(vm, val0);
                break;
        case OP_LOAD_AND_LINK_VEC_TO_ASTACK_LONG:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                arglong0 = join_bytes(arg0, arg1);
                vm->code->constants.buffer[arglong0].vector.astackent = vm->asp - vm->code->constants.buffer[arglong0].vector.size;
                pushv(vm, bytecode_constant_at(vm->code, arglong0));
                break;
        case OP_NEWLINE:
                printf("\n");
                break;
        case OP_WRITE: {
                arg0 = advance_ip(vm);
                for (union value *p = vm->sp - arg0 * 3; p < vm->sp;) {
                        union value val = *p++;
                        enum value_type type = (p++)->integer;
                        enum value_type base = (p++)->integer;
                        value_print(val, type, base);
                }
                for (int i = 0; i < arg0; i++) {
                        popv(vm);
                        enum value_type type = popv(vm).integer;
                        union value val = popv(vm);
                        if (type == VAL_VECTOR)
                                popa(vm, val.vector.size);
                }
                break;
        }
        case OP_READ:
                arg0 = advance_ip(vm);
                dispatch_op_read(vm, arg0, readbuff, OP_READ_BUF_CAP);
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
                arg0 = advance_ip(vm); /* how many indices */
                arg1 = advance_ip(vm); /* rank */
                val0 = vm->stack[arglong0];

                load_indexing_prelude(vm, indicesbuff, arg0, dimensionsbuff, arg1);


                if (is_out_of_bounds(vm, indicesbuff, dimensionsbuff, arg0))
                        return 1;

                val1 = popv(vm);
                if (arg0 == arg1) {
                        val0.vector.astackent[index_flattened(dimensionsbuff, indicesbuff, arg0)] = val1;
                } else {
                        for (int i = arg0; i < arg1; i++) {
                                indicesbuff[i] = 0;
                        }
                        int start = index_flattened(dimensionsbuff, indicesbuff, arg1);
                        for (int i = 0; i < val1.vector.size; i++) {
                                val0.vector.astackent[start + i] = val1.vector.astackent[i];
                        }
                }
                break;
        case OP_GET_INDEX:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);

                load_indexing_prelude(vm, indicesbuff, arg0, dimensionsbuff, arg1);

                val0 = popv(vm);

                if (is_out_of_bounds(vm, indicesbuff, dimensionsbuff, arg0))
                        return 1;

                if (arg0 == arg1) {
                        union value from_main_vector = val0.vector.astackent[index_flattened(dimensionsbuff, indicesbuff, arg0)];
                        pushv(vm, from_main_vector);
                } else {
                        for (int i = arg0; i < arg1; i++) {
                                indicesbuff[i] = 0;
                        }
                        int start = index_flattened(dimensionsbuff, indicesbuff, arg1);
                        int count = 1;
                        for (int i = arg0; i < arg1; i++) {
                                count *= dimensionsbuff[i];
                        }
                        for (int i = 0; i < count; i++) {
                                union value from_main_vector = val0.vector.astackent[start + i];
                                pusha(vm, from_main_vector);
                        }
                        union value result_value;
                        result_value.vector.size = count;
                        result_value.vector.astackent = vm->asp - count;
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