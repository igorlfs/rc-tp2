#include "common.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define ARG_IP 1
#define ARG_PORT 2

#define MAX_CMD_SIZE 2050

int client_id = 0;

void set_operation_type(char line[MAX_CMD_SIZE], BlogOperation *operation) {
  char *command = strtok(line, " ");

  if (strcmp(command, "list") == 0 && strcmp(strtok(NULL, " "), "topics") == 0) {

    operation->operation_type = LIST_TOPICS;

  } else if (strcmp(command, "subscribe") == 0) {
    char *topic = strtok(NULL, " ");

    strcpy(operation->topic, topic);

    operation->operation_type = SUBSCRIBE;

  } else if (strcmp(command, "unsubscribe") == 0) {
    char *topic = strtok(NULL, " ");

    strcpy(operation->topic, topic);

    operation->operation_type = UNSUBSCRIBE;

  } else if (strcmp(command, "publish") == 0 && strcmp(strtok(NULL, " "), "in") == 0) {

    fgets(line, MAX_CMD_SIZE, stdin);
    strcpy(operation->content, line);

    operation->operation_type = NEW_POST;

  } else if (strcmp(command, "exit") == 0) {

    operation->operation_type = EXIT;
  }
}

void loop(int client_socket) {
  while (true) {
    BlogOperation operation;
    char line[MAX_CMD_SIZE];

    operation.operation_type = UNKNOWN;

    if (client_id != 0) {
      fgets(line, MAX_CMD_SIZE, stdin);

      // Come a quebra de linha que vem com o fgets
      line[strcspn(line, "\n")] = 0;

      set_operation_type(line, &operation);
    } else {
      operation.client_id = client_id;
      operation.operation_type = NEW_CONNECTION;
    }

    if (send(client_socket, &operation, sizeof(BlogOperation), 0) == -1) {
      exit(EXIT_FAILURE);
    }
    ssize_t bytes_received;

    bytes_received = recv(client_socket, &operation, sizeof(BlogOperation), 0);
    if (bytes_received == -1) {
      exit(EXIT_FAILURE);
    }

    if (client_id == 0) {
      client_id = operation.client_id;
    }

    if (operation.operation_type == NEW_POST) {
      printf("new post added in %s by %.02d\n", operation.topic, operation.client_id);
    }
    if (operation.server_response == 1) {
      printf("%s", operation.content);
      operation.server_response = 0;
    }

    if (operation.operation_type == EXIT) {
      break;
    }
  }
}

int main(int argc, char *argv[]) {
  // Garante a quantidade de argumentos para o programa funcionar
  if (argc - 1 != ARG_PORT) {
    exit(EXIT_FAILURE);
  }

  int port = atoi(argv[ARG_PORT]);
  char *ip_address = argv[ARG_IP];

  // Define qual protocolo está sendo usado
  struct in_addr inaddr4;
  struct in6_addr inaddr6;
  int protocol = AF_INET;
  if (inet_pton(AF_INET, ip_address, &inaddr4) != 0) {
    protocol = AF_INET;
  } else if (inet_pton(AF_INET6, ip_address, &inaddr6) != 0) {
    protocol = AF_INET6;
  } else {
    exit(EXIT_FAILURE);
  }

  // Criando a socket
  int client_socket = socket(protocol, SOCK_STREAM, 0);
  if (client_socket == -1) {
    exit(EXIT_FAILURE);
  }

  // Definindo servidor e porta, com base no protocolo
  struct sockaddr_storage server_addr;
  if (protocol == AF_INET) {
    struct sockaddr_in *ipv4_addr = (struct sockaddr_in *)&server_addr;

    ipv4_addr->sin_family = AF_INET;
    ipv4_addr->sin_port = htons(port);
    ipv4_addr->sin_addr = inaddr4;
  } else if (protocol == AF_INET6) {
    struct sockaddr_in6 *ipv6_addr = (struct sockaddr_in6 *)&server_addr;

    ipv6_addr->sin6_family = AF_INET6;
    ipv6_addr->sin6_port = htons(port);
    ipv6_addr->sin6_addr = inaddr6;
  } else {
    exit(EXIT_FAILURE);
  }

  // Conectando ao servidor
  if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    exit(EXIT_FAILURE);
  }

  loop(client_socket);

  close(client_socket);
}
