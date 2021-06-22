#ifndef datastructs_h
#define datastructs_h

#define LIST_DECLARE(name, type) \
        struct name; \
        void name##_init(struct name *list); \
        int name##_push(struct name *list, type data); \
        type name##_pop(struct name *list); \
        void name##_free(struct name *list);

LIST_DECLARE(bytes, uint8_t)

#endif