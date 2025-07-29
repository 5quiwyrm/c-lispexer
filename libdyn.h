#ifndef __LIBDYN
#define __LIBDYN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This is a naive implementation of vec_append.
// This relies on your vec structure having a capacity, item and count field.
// `item` has to have the same type as `*vec->items`.
#define vec_append(vec, item) do { \
    vec_reserve((vec), (vec)->count + 1); \
    (vec)->items[(vec)->count++] = (item); \
} while (0) \

// If at all possible prioritise using vec_append_many over vec_append.
#define vec_append_many(vec, append_items, n) do { \
    vec_reserve((vec), (vec)->count + (n)); \
    memcpy((vec)->items + (vec)->count, append_items, n); \
    (vec)->count += n; \
} while (0) \

#define VEC_INIT_CAPACITY 256

// Zeroes vec ptr.
#define vec_ptr_init(vecptr) do { \
    (vecptr)->count = 0; \
    (vecptr)->capacity = VEC_INIT_CAPACITY; \
    (vecptr)->items = NULL; \
    vec_reserve((vecptr), 0); \
} while (0) \

// This relies on vec having a capacity, item and count field.
// `new_size` is an int.
#define vec_reserve(vec, new_size) do { \
    if ((vec)->capacity == 0) { \
        (vec)->capacity = VEC_INIT_CAPACITY; \
    } \
    while ((vec)->capacity < new_size) { \
        (vec)->capacity *= 2; \
    } \
    (vec)->items = realloc((vec)->items, (vec)->capacity * sizeof(*(vec)->items)); \
} while (0) \

// This again relies on vec having a capacity, item and count field.
// `iter_ident` is the identifier the for loop will be using. Note that
// `iter_ident` is a pointer to the loop's items, not the item itself.
// `type` is the type of the vec's items.
#define vec_foreach(type, iter_ident, vec) \
    for (type *iter_ident = (vec)->items; iter_ident < (vec)->items + (vec)->count; iter_ident++)

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} DString;

#define dstring_append(dstr, ch) do { \
    vec_append((dstr), ch); \
    vec_reserve((dstr), (dstr)->count + 1); \
    (dstr)->items[(dstr)->count] = 0; \
} while (0) \

#define dstring_append_many(dstr, append_str, n) do { \
    vec_reserve((dstr), (dstr)->count + n); \
    vec_append_many((dstr), append_str, n); \
    (dstr)->items[(dstr)->count] = 0; \
} while (0) \

#define dstring_clear(dstr) do { \
    (dstr)->count = 0; \
    if ((dstr)->items != NULL) (dstr)->items[0] = '\0'; \
} while (0) \

#endif
