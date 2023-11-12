#include "common.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Espera-se que os argumentos sempre venham nessa ordem :)
#define ARG_PROTOCOL_VERSION 1
#define ARG_PORT 2

#define MAX_PENDING 20

#define MAX_CLIENTS 10
#define THREAD_POOL_SIZE 10

pthread_t thread_pool[THREAD_POOL_SIZE];

int used_clients[MAX_CLIENTS];
LinkedList all_topics;
LinkedList topics_per_client[MAX_CLIENTS];

pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_threads = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_clients = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_all_topics = PTHREAD_MUTEX_INITIALIZER;

/// Retorna o protocolo com base em `string` ou interrompe o programa
/// caso a string não seja reconhecida.
int get_protocol(char *string) {
  if (strcmp(string, "v4") == 0) {
    return AF_INET;
  }
  if (strcmp(string, "v6") == 0) {
    return AF_INET6;
  }
  exit(EXIT_FAILURE);
}

void create_topic(BlogOperation *operation) {
  if (all_topics.size == 0) {
    all_topics = create_list(operation->topic);
  } else {
    insert_in_list(&all_topics, operation->topic);
  }
}

void new_connection(BlogOperation *operation, const int *socket) {
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (used_clients[i] == 0) {
      used_clients[i] = *socket;
      printf("client %.02d connected\n", i + 1);
      operation->client_id = i + 1;
      return;
    }
  }
  exit(EXIT_FAILURE);
}

void subscribe(BlogOperation *operation) {
  LinkedList *topics = &topics_per_client[operation->client_id - 1];

  if (topics->size == 0) {
    *topics = create_list(operation->topic);
    printf("client %.02d subscribed to %s\n", operation->client_id, operation->topic);
  } else if (insert_in_list(topics, operation->topic)) {
    printf("client %.02d subscribed to %s\n", operation->client_id, operation->topic);
  } else {
    operation->server_response = 1;
    strcpy(operation->content, "error: already subscribed\n");
  }
}

void unsubscribe(BlogOperation *operation) {
  LinkedList *topics = &topics_per_client[operation->client_id - 1];
  NodeTopic *previous_node = NULL;

  for (NodeTopic *node = &topics->head; node != NULL; node = node->next) {
    if (strcmp(operation->topic, node->topic) == 0) {
      topics->size--;

      if (previous_node != NULL) {
        previous_node->next = node->next;
        free(node);
      } else if (topics->size != 0) {
        free(previous_node);
        topics->head = *topics->head.next;
      }

      printf("client %.02d unsubscribe to %s\n", operation->client_id, operation->topic);

      break;
    }
    previous_node = node;
  }
}

void list_topics(BlogOperation *operation) {
  if (all_topics.size == 0) {
    operation->server_response = 1;
    strcpy(operation->content, "no topics available\n");
  } else {
    char messages_string[MAX_CONTENT_SIZE] = "";
    NodeTopic *node = &all_topics.head;
    while (true) {
      if (node == NULL) {
        break;
      }
      strcat(messages_string, node->topic);
      if (node->next != NULL) {
        strcat(messages_string, ";");
      }
      node = node->next;
    }
    operation->server_response = 1;
    strcat(messages_string, "\n");
    stpcpy(operation->content, messages_string);
  }
}

void disconnect_client(BlogOperation *operation) {
  used_clients[operation->client_id - 1] = 0;
  topics_per_client[operation->client_id - 1] = create_empty_list();
  printf("client %.02d disconnected\n", operation->client_id);
}

void publish(BlogOperation *operation) {
  printf("new post added in %s by %.02d\n", operation->topic, operation->client_id);

  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (used_clients[i] > 0) {
      LinkedList *topics = &topics_per_client[i];

      for (NodeTopic *node = &topics->head; node != NULL; node = node->next) {
        if (strcmp(node->topic, operation->topic) == 0) {

          operation->server_response = 1;

          if (send(used_clients[i], operation, sizeof(BlogOperation), 0) == -1) {
            exit(EXIT_FAILURE);
          }

          break;
        }
      }
    }
  }
}

void *handle_connection(void *socket) {
  int *client_socket = (int *)socket;

  while (true) {
    BlogOperation operation;
    ssize_t bytes_received = recv(*client_socket, &operation, sizeof(operation), 0);

    if (bytes_received == -1) {
      free(socket);
      exit(EXIT_FAILURE);
    }

    switch (operation.operation_type) {
    case NEW_POST: {
      pthread_mutex_lock(&mutex_all_topics);
      create_topic(&operation);
      pthread_mutex_unlock(&mutex_all_topics);

      pthread_mutex_lock(&mutex_clients);
      publish(&operation);
      pthread_mutex_unlock(&mutex_clients);
      break;
    }
    case NEW_CONNECTION: {
      pthread_mutex_lock(&mutex_clients);
      new_connection(&operation, client_socket);
      pthread_mutex_unlock(&mutex_clients);
      break;
    }
    case SUBSCRIBE: {
      pthread_mutex_lock(&mutex_all_topics);
      create_topic(&operation);
      pthread_mutex_unlock(&mutex_all_topics);

      subscribe(&operation);
      break;
    }
    case UNSUBSCRIBE:
      unsubscribe(&operation);
      break;
    case LIST_TOPICS: {
      pthread_mutex_lock(&mutex_all_topics);
      list_topics(&operation);
      pthread_mutex_unlock(&mutex_all_topics);
      break;
    }
    case EXIT: {
      pthread_mutex_lock(&mutex_clients);
      disconnect_client(&operation);
      pthread_mutex_unlock(&mutex_clients);
      break;
    }
    }

    if (operation.operation_type != UNSUBSCRIBE && operation.operation_type != NEW_POST) {
      if (send(*client_socket, &operation, sizeof(BlogOperation), 0) == -1) {
        free(socket);
        exit(EXIT_FAILURE);
      }
    }

    if (operation.operation_type == EXIT) {
      close(*client_socket);
      free(socket);
      return NULL;
    }
  }
}

void *thread_function(void * /*unused*/) {
  pthread_detach(pthread_self());

  while (true) {
    pthread_mutex_lock(&mutex_threads);
    int *p_client;

    if ((p_client = dequeue()) == NULL) {
      pthread_cond_wait(&condition_var, &mutex_threads);
      p_client = dequeue();
    };

    pthread_mutex_unlock(&mutex_threads);
    if (p_client != NULL) {
      handle_connection(p_client);
    }
  }
}

int main(int argc, char *argv[]) {
  all_topics = create_empty_list();
  // Garante a quantidade de argumentos para o programa funcionar
  if (argc < ARG_PORT) {
    exit(EXIT_FAILURE);
  }

  memset(used_clients, 0, sizeof(int) * MAX_CLIENTS);

  for (int i = 0; i < THREAD_POOL_SIZE - 1; ++i) {
    pthread_create(&thread_pool[i], NULL, thread_function, NULL);
  }

  int port = atoi(argv[ARG_PORT]);
  int protocol = get_protocol(argv[ARG_PROTOCOL_VERSION]);

  // Criando a socket
  int server_socket = socket(protocol, SOCK_STREAM, 0);
  if (server_socket == -1) {
    exit(EXIT_FAILURE);
  }

  // Definindo servidor e porta, com base no protocolo
  struct sockaddr_storage server_addr;
  if (protocol == AF_INET) {
    struct sockaddr_in *ipv4_addr = (struct sockaddr_in *)&server_addr;

    ipv4_addr->sin_family = AF_INET;
    ipv4_addr->sin_port = htons(port);
    ipv4_addr->sin_addr.s_addr = INADDR_ANY;
  } else if (protocol == AF_INET6) {
    struct sockaddr_in6 *ipv6_addr = (struct sockaddr_in6 *)&server_addr;

    ipv6_addr->sin6_family = AF_INET6;
    ipv6_addr->sin6_port = htons(port);
    ipv6_addr->sin6_addr = in6addr_any;
  } else {
    exit(EXIT_FAILURE);
  }

  // Associando socket com o servidor e a porta definidos
  if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    exit(EXIT_FAILURE);
  }

  struct sockaddr_storage client_address_storage;
  void *client_addr;
  if (protocol == AF_INET) {
    client_addr = (struct sockaddr_in *)&client_address_storage;
  } else {
    client_addr = (struct sockaddr_in6 *)&client_address_storage;
  }
  socklen_t client_addr_len = sizeof(client_addr);

  // Esperando conexão do cliente
  if (listen(server_socket, MAX_PENDING) == -1) {
    exit(EXIT_FAILURE);
  }

  while (true) {
    int client_socket;

    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == -1) {
      exit(EXIT_FAILURE);
    }

    int *p_client = malloc(sizeof(int));
    *p_client = client_socket;
    pthread_mutex_lock(&mutex_threads);
    enqueue(p_client);
    pthread_cond_signal(&condition_var);
    pthread_mutex_unlock(&mutex_threads);
  }
}
