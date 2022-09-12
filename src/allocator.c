#include "proto.h"

void * attach_allocator_node (struct allocator_node ** list, struct allocator_node * node) {
  if (!node) return NULL;
  node -> previous = NULL;
  node -> next = *list;
  if (node -> next) node -> next -> previous = node;
  *list = node;
  return node -> data;
}

void * allocate (struct allocator_node ** list, size_t size) {
  if (size >= (size_t) -sizeof(struct allocator_node)) return NULL;
  return attach_allocator_node(list, malloc(sizeof(struct allocator_node) + size));
}

void * clear_allocate (struct allocator_node ** list, size_t size) {
  if (size >= (size_t) -sizeof(struct allocator_node)) return NULL;
  return attach_allocator_node(list, calloc(1, sizeof(struct allocator_node) + size));
}

void deallocate (struct allocator_node ** list, void * item) {
  if (!item) return;
  struct allocator_node * node = get_allocator_node(item);
  if (node -> previous)
    node -> previous -> next = node -> next;
  else
    *list = node -> next;
  if (node -> next) node -> next -> previous = node -> previous;
  free(node);
}

void * reallocate (struct allocator_node ** list, void * item, size_t size) {
  if (size >= (size_t) -sizeof(struct allocator_node)) return NULL;
  if (!item) return allocate(list, size);
  struct allocator_node * node = get_allocator_node(item);
  node = realloc(node, sizeof *node + size);
  if (!node) return NULL;
  if (node -> previous)
    node -> previous -> next = node;
  else
    *list = node;
  if (node -> next) node -> next -> previous = node;
  return node -> data;
}

void destroy_allocator_list (struct allocator_node * list) {
  while (list) {
    struct allocator_node * node = list;
    list = node -> next;
    free(node);
  }
}

void * plum_malloc (struct plum_image * image, size_t size) {
  if (!image) return NULL;
  struct allocator_node * list = image -> allocator;
  void * result = allocate(&list, size);
  image -> allocator = list;
  return result;
}

void * plum_calloc (struct plum_image * image, size_t size) {
  if (!image) return NULL;
  struct allocator_node * list = image -> allocator;
  void * result = clear_allocate(&list, size);
  image -> allocator = list;
  return result;
}

void * plum_realloc (struct plum_image * image, void * buffer, size_t size) {
  if (!image) return NULL;
  struct allocator_node * list = image -> allocator;
  void * result = reallocate(&list, buffer, size);
  if (result) image -> allocator = list;
  return result;
}

void plum_free (struct plum_image * image, void * buffer) {
  if (image) {
    struct allocator_node * list = image -> allocator;
    deallocate(&list, buffer);
    image -> allocator = list;
  } else
    free(buffer); // special compatibility mode for bad runtimes without access to C libraries
}
