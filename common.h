#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_TOPIC_SIZE 50
#define MAX_CONTENT_SIZE 2048

enum OperationTypes {
  NEW_CONNECTION = 1,
  NEW_POST,
  LIST_TOPICS,
  SUBSCRIBE,
  EXIT,
  UNSUBSCRIBE,
  UNKNOWN
};

typedef struct BlogOperation {
  int client_id;
  int operation_type;
  int server_response;
  char topic[MAX_TOPIC_SIZE];
  char content[MAX_CONTENT_SIZE];
} BlogOperation;

typedef struct NodeTopic {
  char topic[MAX_TOPIC_SIZE];
  struct NodeTopic *next;
} NodeTopic;

typedef struct LinkedList {
  NodeTopic head;
  int size;
} LinkedList;

typedef struct NodeSocket {
  struct NodeSocket *next;
  int *client_socket;
} NodeSocket;

void enqueue(int *client_socket);
int *dequeue(void);

LinkedList create_empty_list(void);
LinkedList create_list(char *topic);
bool insert_in_list(LinkedList *list, char *topic);
