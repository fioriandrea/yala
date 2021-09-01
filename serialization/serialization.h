#ifndef serialization_h
#define serialization_h

#include <stdio.h>

#include "../semantics/semantics.h"

void
serialize_bytecode(struct bytecode *code, FILE *outfile);

#endif
