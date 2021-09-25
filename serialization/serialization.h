/*
 * Copyright (C) 2021 Andrea Fiori <andrea.fiori.1998@gmail.com>
 *
 * Licensed under GPLv3, see file LICENSE in this source tree.
 */

#ifndef serialization_h
#define serialization_h

#include <stdio.h>

#include "../semantics/semantics.h"

void serialize_bytecode(struct bytecode *code, FILE *outfile);
char *deserialize_bytecode(struct bytecode *code, char *p);

#endif
