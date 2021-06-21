#include <stdlib.h>
#include <stdint.h>

#include "datastructs.h"

void *
grow_array(void *buffer, int oldcap, int newcap, size_t size)
{
        if (newcap == 0 && buffer != NULL) {
                free(buffer);
                return NULL;
        } else {
                return realloc(buffer, size * newcap);
        }
}

LIST_FUNCTIONS(bytes, uint8_t)