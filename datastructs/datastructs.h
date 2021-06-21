#ifndef datastructs_h
#define datastructs_h

struct bytes;
void bytes_init(struct bytes *list);
int bytes_push(struct bytes *list, uint8_t data);
uint8_t bytes_pop(struct bytes *list);
void bytes_free(struct bytes *list);

#endif