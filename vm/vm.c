#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

static void
runtime_error(struct vm *vm, char *fmt, ...)
{
        vm->error = 1;
        va_list args;
        va_start(args, fmt);
        struct lineinfo linfo = LIST_AT(&vm->framese->fn.code->lines, vm->framese->ip);
        fprintf(stderr, "runtime error ");
        fprintf(stderr, "[at %d:%d]: ", linfo.line, linfo.linepos);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}

void
vm_init(struct vm *vm, struct bytecode *code)
{
        vm->framese = vm->framestack;
        vm->argsp = vm->argstack;
        vm->argasp = vm->astack + STACK_MAX;
        struct value_function fn;
        fn.code = code;
        fn.envindex = 0;
        stack_frame_init(vm->framese, vm->stack, vm->stack, vm->astack, fn);
        vm->error = 0;
}

void
stack_frame_init(struct stack_frame *sf, union value *sp, union value *stackbase, union value *asp, struct value_function fn)
{
        sf->ip = 0;
        sf->sp = sp;
        sf->stackbase = stackbase;
        sf->asp = asp;
        sf->fn = fn;
}

#define VM_SP(vm) (vm->framese->sp)
#define VM_STACKBASE(vm) (vm->framese->stackbase)
#define VM_ASP(vm) (vm->framese->asp)
#define VM_CODE(vm) (vm->framese->fn.code)
#define VM_ENVINDEX(vm) (vm->framese->fn.envindex)
#define VM_IP(vm) (vm->framese->ip)

static uint8_t
advance_ip(struct vm *vm)
{
        return bytecode_byte_at(VM_CODE(vm), VM_IP(vm)++);
}

static uint16_t
advance_long_ip(struct vm *vm)
{
        uint8_t left = advance_ip(vm);
        uint8_t right = advance_ip(vm);
        return join_bytes(left, right);
}

static void
pushv(struct vm *vm, union value val)
{
        if (VM_SP(vm) - (vm->stack + STACK_MAX) >= 0) {
                runtime_error(vm, "stack overflow");
                return;
        }
        *(VM_SP(vm)++) = val;
}

static union value
popv(struct vm *vm)
{
        return *(--VM_SP(vm));
}

static union value
peekv(struct vm *vm, int offset)
{
        return *(VM_SP(vm) - offset);
}

static void
pusha(struct vm *vm, union value val)
{
        if (VM_ASP(vm) - (vm->astack + STACK_MAX) >= 0) {
                runtime_error(vm, "stack overflow");
                return;
        }
        *(VM_ASP(vm)++) = val;
}

static void
asp_move_up(struct vm *vm, int offset)
{
        if ((VM_ASP(vm) + offset) - (vm->astack + STACK_MAX) > 0) {
                runtime_error(vm, "stack overflow");
                return;
        }
        VM_ASP(vm) += offset;
}

static void
popa(struct vm *vm, int size) {
        for (int i = 0; i < size; i++) {
                VM_ASP(vm)--;
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
                default:
                        exit(100);
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
                        runtime_error(vm, "index out of bound (max index %d)", dimensionsbuff[i] - 1);
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

static void get_local_long(struct vm *vm);
static void set_local_long(struct vm *vm);
static void set_index_local_long(struct vm *vm, int *indicesbuff, int *dimensionsbuff);
static void get_index(struct vm *vm, int *indicesbuff, int *dimensionsbuff);

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
        if (vm->error)
                return vm->error;
        current = advance_ip(vm);
        switch (current) {
        case OP_LOCI_LONG:
        case OP_LOCS_LONG:
        case OP_LOCF_LONG:
                arglong0 = advance_long_ip(vm);
                pushv(vm, bytecode_constant_at(VM_CODE(vm), arglong0));
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
                        runtime_error(vm, "division by 0");
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
        case OP_TRUE:
                pushv(vm, value_from_c_bool(1));
                break;
        case OP_EMPTY_STRING:
                pushv(vm, value_from_c_string(""));
                break;
        case OP_SKIP_LONG:
                arglong0 = advance_long_ip(vm);
                VM_IP(vm) += arglong0;
                break;
        case OP_SKIP_BACK_LONG:
                arglong0 = advance_long_ip(vm);
                VM_IP(vm) -= arglong0;
                break;
        case OP_SKIPF_LONG:
                arglong0 = advance_long_ip(vm);
                val0 = peekv(vm, 1);
                if (!val0.boolean) {
                        VM_IP(vm) += arglong0;
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
        case OP_ASTACK_SHIFT_UP:
                asp_move_up(vm, popv(vm).integer);
                break;
        case OP_LOC_ALINK_LONG:
                arglong0 = advance_long_ip(vm);
                LIST_AT(&VM_CODE(vm)->constants, arglong0).vector.astackent = VM_ASP(vm) - bytecode_constant_at(VM_CODE(vm), arglong0).vector.size;
                pushv(vm, bytecode_constant_at(VM_CODE(vm), arglong0));
                break;
        case OP_NEWLINE:
                printf("\n");
                break;
        case OP_WRITE: {
                arg0 = advance_ip(vm);
                for (union value *p = VM_SP(vm) - arg0 * 3; p < VM_SP(vm);) {
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
        case OP_CALL:
                arg0 = advance_ip(vm); /* function arity */
                val0 = peekv(vm, arg0 + 1);
                stack_frame_init(vm->framese + 1, VM_SP(vm), VM_SP(vm) - arg0, VM_ASP(vm), val0.function);
                vm->framese++;
                break;
        case OP_SHIFT_ASTACKENT_TO_BASE:
                val0 = peekv(vm, 1);
                for (int i = 0; i < val0.vector.size; i++) {
                       *(vm->framese[-1].asp + i) = *(val0.vector.astackent + i);
                }
                vm->framese->sp[-1].vector.astackent = vm->framese[-1].asp;
                vm->framese[-1].asp += val0.vector.size;
                break;
        case OP_RETURN:
                arg0 = advance_ip(vm); /* function arity */
                val0 = popv(vm);
                vm->framese--;
                vm->framese->sp -= arg0 + 1;
                pushv(vm, val0);
                break;
        case OP_ARGSTACK_LOAD:
                arg0 = advance_ip(vm);
                arg1 = advance_ip(vm);
                val0 = VM_STACKBASE(vm)[arg0];
                if (arg1) {
                        vm->argasp -= val0.vector.size;
                        memcpy(vm->argasp, val0.vector.astackent, val0.vector.size * sizeof(union value));
                        val0.vector.astackent = vm->argasp;
                }
                *vm->argsp++ = val0;
                break;
        case OP_ARGSTACK_PEEK:
                pushv(vm, *(vm->argsp - 1));
                break;
        case OP_ARGSTACK_UNLOAD:
                arg0 = advance_ip(vm);
                val0 = *--vm->argsp;
                if (arg0) {
                        vm->argasp += val0.vector.size;
                }
                break;
        case OP_GET_LOCAL_LONG:
                get_local_long(vm);
                break;
        case OP_SET_LOCAL_LONG:
                set_local_long(vm);
                break;
        case OP_SET_INDEX_LOCAL_LONG:
                set_index_local_long(vm, indicesbuff, dimensionsbuff);
                break;
        case OP_GET_INDEX:
                get_index(vm, indicesbuff, dimensionsbuff);
                break;
        case OP_HALT:
                return 0;
        default:
                runtime_error(vm, "NOT IMPLEMENTED: %s\n", opcodestring(current));
                return 1;
        }
        }
}

static void
get_local_long(struct vm *vm)
{
        uint16_t offset = advance_long_ip(vm);
        uint16_t index = advance_long_ip(vm);
        struct stack_frame *fp = offset == 0 ? vm->framese : vm->framestack + VM_ENVINDEX(vm);
        pushv(vm, fp[-offset].stackbase[index]);
}

static void
set_local_long(struct vm *vm)
{
        uint16_t offset = advance_long_ip(vm);
        uint16_t index = advance_long_ip(vm);
        struct stack_frame *fp = offset == 0 ? vm->framese : vm->framestack + VM_ENVINDEX(vm);
        fp[-offset].stackbase[index] = popv(vm);
}

static void
set_index_local_long(struct vm *vm, int *indicesbuff, int *dimensionsbuff)
{
        uint16_t offset = advance_long_ip(vm);
        uint16_t index = advance_long_ip(vm);
        uint8_t nindices = advance_ip(vm);
        uint8_t rank = advance_ip(vm);
        struct stack_frame *fp = offset == 0 ? vm->framese : vm->framestack + VM_ENVINDEX(vm);
        union value val0 = fp[-offset].stackbase[index];

        load_indexing_prelude(vm, indicesbuff, nindices, dimensionsbuff, rank);

        if (is_out_of_bounds(vm, indicesbuff, dimensionsbuff, nindices)) {
                runtime_error(vm, "index out of bounds");
                return;
        }

        union value val1 = popv(vm);
        if (nindices == rank) {
                val0.vector.astackent[index_flattened(dimensionsbuff, indicesbuff, nindices)] = val1;
        }
        else {
                for (int i = nindices; i < rank; i++) {
                        indicesbuff[i] = 0;
                }
                int start = index_flattened(dimensionsbuff, indicesbuff, rank);
                for (int i = 0; i < val1.vector.size; i++) {
                        val0.vector.astackent[start + i] = val1.vector.astackent[i];
                }
        }
}

static void
get_index(struct vm *vm, int *indicesbuff, int *dimensionsbuff)
{
        uint8_t nindices = advance_ip(vm);
        uint8_t rank = advance_ip(vm);

        load_indexing_prelude(vm, indicesbuff, nindices, dimensionsbuff, rank);

        union value val0 = popv(vm);

        if (is_out_of_bounds(vm, indicesbuff, dimensionsbuff, nindices)) {
                runtime_error(vm, "index out of bounds");
                return;
        }

        if (nindices == rank) {
                union value from_main_vector = val0.vector.astackent[index_flattened(dimensionsbuff, indicesbuff, nindices)];
                pushv(vm, from_main_vector);
        } else {
                for (int i = nindices; i < rank; i++) {
                        indicesbuff[i] = 0;
                }
                int start = index_flattened(dimensionsbuff, indicesbuff, rank);
                int count = 1;
                for (int i = nindices; i < rank; i++) {
                        count *= dimensionsbuff[i];
                }
                for (int i = 0; i < count; i++) {
                        union value from_main_vector = val0.vector.astackent[start + i];
                        pusha(vm, from_main_vector);
                }
                union value result_value;
                result_value.vector.size = count;
                result_value.vector.astackent = VM_ASP(vm) - count;
                pushv(vm, result_value);
        }
}
