#include <stdio.h>

#include "semantics.h"

void
value_print(struct value v)
{
        switch (v.type) {
        case VAL_INTEGER:
                printf("%d", v.as.integer);
                return;
        case VAL_BOOLEAN:
                printf("%s", v.as.boolean ? "true" : "false");
                return;
        }
        printf("unreachable value type %d", v.type);
}

struct value
value_from_c_int(int i)
{
        struct value v;
        v.type = VAL_INTEGER;
        v.as.integer = i;
        return v;
}

struct value
value_from_c_bool(int b)
{
        struct value v;
        v.type = VAL_BOOLEAN;
        v.as.boolean = b;
        return v;
}

int
values_equal(struct value val0, struct value val1)
{
        switch (val0.type) {
        case VAL_INTEGER:
                return val0.as.integer == val1.as.integer;
        case VAL_BOOLEAN:
                return val0.as.boolean == val1.as.boolean;
        }
        return -1;
}
