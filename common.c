#include "common.h"
#include <stdlib.h>
#include <string.h>

LinkedList create_empty_list(void) {
  LinkedList list;
  list.size = 0;
  return list;
}

LinkedList create_list(char *topic) {
  NodeTopic *new_node_copy = (NodeTopic *)malloc(sizeof(NodeTopic));
  new_node_copy->next = NULL;
  strcpy(new_node_copy->topic, topic);

  LinkedList list;
  list.head = *new_node_copy;
  list.size = 1;
  return list;
}

bool insert_in_list(LinkedList *list, char *topic) {
  NodeTopic *new_node_copy = (NodeTopic *)malloc(sizeof(NodeTopic));
  new_node_copy->next = NULL;
  strcpy(new_node_copy->topic, topic);

  for (NodeTopic *node = &list->head; node != NULL; node = node->next) {
    if (strcmp(node->topic, new_node_copy->topic) == 0) {
      return false;
    }
    if (node->next == NULL) {
      list->size++;
      node->next = new_node_copy;
      break;
    }
  }
  return true;
}

NodeSocket *head = NULL;
NodeSocket *tail = NULL;

void enqueue(int *client_socket) {
  NodeSocket *new_node = malloc(sizeof(NodeSocket));

  new_node->client_socket = client_socket;
  new_node->next = NULL;

  if (tail == NULL) {
    head = new_node;
  } else {
    tail->next = new_node;
  }

  tail = new_node;
}

int *dequeue(void) {
  if (head == NULL) {
    return NULL;
  }

  int *result = head->client_socket;
  NodeSocket *auxiliary = head;

  head = head->next;
  if (head == NULL) {
    tail = NULL;
  }

  free(auxiliary);
  return result;
}
