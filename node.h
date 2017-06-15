#ifndef NODE
#define NODE
typedef struct node {
  void *item;
  struct node *next;
  struct node *prev;
} Node;
#endif
