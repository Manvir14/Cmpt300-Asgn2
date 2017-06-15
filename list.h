#include "node.h"

typedef struct list {
  Node *head;
  Node *tail;
  Node *current;
  struct list *next;
  int count;
  int beyond;
  int before;
} LIST;

Node *ItemCreate(void *item);

LIST *ListCreate();

int ListCount(LIST *list);

void *ListFirst(LIST *list);

void *ListLast(LIST *list);

void *ListNext(LIST *list);

void *ListPrev(LIST *list);

void *ListCurr(LIST *list);

int ListAdd(LIST *list, void *item);

int ListInsert(LIST *list, void *item);

int ListAppend(LIST *list,void *item);

int ListPrepend(LIST *list, void *item);

void *ListRemove(LIST *list);

void ListConcat(LIST *list1, LIST *list2);

void ListFree(LIST *list, void (*itemFree)(void *itemToBeFreed));

void *ListTrim(LIST *list);

void *ListSearch(LIST *list, int (*comparator)(void *item, void *comparisonArg), void *comparisonArg);
