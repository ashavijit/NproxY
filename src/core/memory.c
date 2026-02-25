#include "memory.h"

#include <stdlib.h>
#include <string.h>

static arena_block_t *block_create(usize size) {
  arena_block_t *block = malloc(sizeof(arena_block_t) + size);
  if (!block)
    return NULL;
  block->start = (u8 *)(block + 1);
  block->cap = size;
  block->used = 0;
  block->next = NULL;
  return block;
}

arena_t *arena_create(usize block_size) {
  arena_t *arena = malloc(sizeof(arena_t));
  if (!arena)
    return NULL;
  arena->block_size = block_size;
  arena->head = block_create(block_size);
  if (!arena->head) {
    free(arena);
    return NULL;
  }
  return arena;
}

void *arena_alloc_aligned(arena_t *arena, usize size, usize align) {
  arena_block_t *block = arena->head;
  usize pad = (align - ((usize)(block->start + block->used) % align)) % align;
  usize need = size + pad;

  if (block->used + need > block->cap) {
    usize bsize = need > arena->block_size ? need : arena->block_size;
    arena_block_t *nb = block_create(bsize);
    if (!nb)
      return NULL;
    nb->next = arena->head;
    arena->head = nb;
    block = nb;
    pad = 0;
  }

  void *ptr = block->start + block->used + pad;
  block->used += need;
  return ptr;
}

void *arena_alloc(arena_t *arena, usize size) {
  return arena_alloc_aligned(arena, size, sizeof(void *));
}

void arena_reset(arena_t *arena) {
  arena_block_t *b = arena->head;
  while (b->next) {
    arena_block_t *next = b->next;
    free(b);
    b = next;
  }
  arena->head = b;
  arena->head->used = 0;
}

void arena_destroy(arena_t *arena) {
  arena_block_t *b = arena->head;
  while (b) {
    arena_block_t *next = b->next;
    free(b);
    b = next;
  }
  free(arena);
}
