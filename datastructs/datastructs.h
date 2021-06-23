#ifndef datastructs_h
#define datastructs_h

#define LIST_DECLARE(name, type) \
        struct name \
        { \
                int len; \
                int cap; \
                type *buffer; \
        }; \
        void name##_init(struct name *list); \
        int name##_push(struct name *list, type data); \
        type name##_pop(struct name *list); \
        type name##_at(struct name *list, int i); \
        int name##_len(struct name *list); \
        void name##_free(struct name *list);

#define TABLE_DECLARE(name, keytype, valtype) \
        struct name##_table_node { \
                keytype key; \
                valtype value; \
                struct name##_table_node *next; \
        }; \
        struct name { \
                int cap; \
                int nnodes; \
                struct name##_table_node **buckets; \
                int (*hash)(keytype); \
                int (*keyequal)(keytype, keytype); \
        }; \
        void name##_init(struct name *table, int (*hash)(keytype), int (*keyequal)(keytype, keytype)); \
        struct name##_table_node *name##_node_get(struct name *table, keytype key); \
        int name##_has(struct name *table, keytype key); \
        valtype name##_get(struct name *table, keytype key); \
        int name##_set(struct name *table, keytype key, valtype value); \
        void name##_free(struct name *list);

#endif