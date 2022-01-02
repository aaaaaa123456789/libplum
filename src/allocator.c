#include "proto.h"

void * attach_allocator_node (union allocator_node ** list, union allocator_node * node) {
  if (!node) return NULL;
  node -> previous = NULL;
  node -> next = *list;
  if (node -> next) node -> next -> previous = node;
  *list = node;
  return node + 1;
}

void * allocate (union allocator_node ** list, size_t size) {
  if (size >= (size_t) -sizeof(union allocator_node)) return NULL;
  return attach_allocator_node(list, malloc(sizeof(union allocator_node) + size));
}

void * clear_allocate (union allocator_node ** list, size_t size) {
  if (size >= (size_t) -sizeof(union allocator_node)) return NULL;
  return attach_allocator_node(list, calloc(1, sizeof(union allocator_node) + size));
}

void deallocate (union allocator_node ** list, void * item) {
  if (!item) return;
  union allocator_node * node = (union allocator_node *) item - 1;
  if (node -> previous)
    node -> previous -> next = node -> next;
  else
    *list = node -> next;
  if (node -> next) node -> next -> previous = node -> previous;
  free(node);
}

void * reallocate (union allocator_node ** list, void * item, size_t size) {
  if (size >= (size_t) -sizeof(union allocator_node)) return NULL;
  if (!item) return allocate(list, size);
  union allocator_node * node = (union allocator_node *) item - 1;
  node = realloc(node, sizeof *node + size);
  if (!node) return NULL;
  if (node -> previous)
    node -> previous -> next = node;
  else
    *list = node;
  if (node -> next) node -> next -> previous = node;
  return node + 1;
}

void destroy_allocator_list (union allocator_node * list) {
  while (list) {
    union allocator_node * node = list;
    list = node -> next;
    free(node);
  }
}

void * plum_malloc (struct plum_image * image, size_t size) {
  if (!image) return NULL;
  union allocator_node * list = image -> allocator;
  void * result = allocate(&list, size);
  image -> allocator = list;
  return result;
}

void * plum_calloc (struct plum_image * image, size_t size) {
  if (!image) return NULL;
  union allocator_node * list = image -> allocator;
  void * result = clear_allocate(&list, size);
  image -> allocator = list;
  return result;
}

void * plum_realloc (struct plum_image * image, void * buffer, size_t size) {
  if (!image) return NULL;
  union allocator_node * list = image -> allocator;
  void * result = reallocate(&list, buffer, size);
  image -> allocator = list;
  return result;
}

void plum_free (struct plum_image * image, void * buffer) {
  if (image) {
    union allocator_node * list = image -> allocator;
    deallocate(&list, buffer);
    image -> allocator = list;
  } else
    free(buffer); // special compatibility mode for bad runtimes without access to C libraries
}
