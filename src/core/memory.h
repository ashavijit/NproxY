#ifndef NPROXY_MEMORY_H
#define NPROXY_MEMORY_H

#include "types.h"

typedef struct arena_block arena_block_t;

struct arena_block {
  u8 *start;
  usize cap;
  usize used;
  arena_block_t *next;
};

typedef struct {
  arena_block_t *head;
  usize block_size;
} arena_t;

arena_t *arena_create(usize block_size);
void *arena_alloc(arena_t *arena, usize size);
void *arena_alloc_aligned(arena_t *arena, usize size, usize align);
void arena_reset(arena_t *arena);
void arena_destroy(arena_t *arena);

#define arena_new(arena, T) ((T *)arena_alloc_aligned((arena), sizeof(T), _Alignof(T)))
#define arena_new_n(arena, T, n) ((T *)arena_alloc_aligned((arena), sizeof(T) * (n), _Alignof(T)))

#endif
