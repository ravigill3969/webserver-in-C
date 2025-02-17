#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "checksum.h"
#include "queue.h"

#define MAX_THREADS 5

void parseJSON(const char *json, char *username, char *password) {
    sscanf(json, "{\"username\":\"%[^\"]\",\"password\":\"%[^\"]\"}", username,
           password);
}

void extractBodyAndParse(const char *request) {
    char *body = strstr(request, "\r\n\r\n");
    if (body) {
        body += 4;  // Skip "\r\n\r\n"
        char username[50], password[50];
        parseJSON(body, username, password);
        printf("Username: %s\n", username);
        printf("Password: %s\n", password);
    } else {
        printf("No JSON body found!\n");
    }
}

void *threadsHandler() {
    int client_fd = deqeueClient();
    printf("%d\n", client_fd);

    char recv_buffer[1000];
    int bytes_received = recv(client_fd, recv_buffer, sizeof(recv_buffer), 0);

    if (bytes_received < 0) {
        printf("something is wrong%s \n", strerror(errno));
    } else {
        recv_buffer[bytes_received] = '\0';

        printf("received %s\n", recv_buffer);

        extractBodyAndParse(recv_buffer);

        char method[10], path[1024], version[10];
        sscanf(recv_buffer, "%s %s %s", method, path, version);

        FILE *fptr = fopen("index.html", "r");

        if (fptr == NULL) {
            printf("Not able to open the file.\n");
            const char *error_response =
                "HTTP/1.1 404 Not Found\r\n\r\nPage not found.";
            send(client_fd, error_response, strlen(error_response), 0);
            return NULL;
        }

        fseek(fptr, 0, SEEK_END);
        long file_size = ftell(fptr);
        fseek(fptr, 0, SEEK_SET);

        char *myString = (char *)malloc(file_size + 1);
        if (!myString) {
            printf("Memory allocation failed.\n");
            fclose(fptr);
            return NULL;
        }

        fread(myString, 1, file_size, fptr);
        myString[file_size] = '\0';
        fclose(fptr);

        uint32_t checksum = crc32(myString, file_size);

        char response[file_size + sizeof(checksum) + 10000];

        if (strcmp(path, "/login") == 0) {
            char responseLogin[20000];
            myString = "{\"message\":\"success\"}";
            snprintf(responseLogin, sizeof(responseLogin),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/json\r\n"
                     "Content-Length: %ld\r\n"
                     "\r\n"
                     "%s",
                     strlen(myString), myString);
            send(client_fd, responseLogin, strlen(responseLogin), 0);
        } else if (strcmp(path, "/") == 0) {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html\r\n"
                     "Content-Length: %ld\r\n"
                     "Check-Sum: %08X\r\n"
                     "\r\n"
                     "%s",
                     file_size, checksum, myString);
            free(myString);
            send(client_fd, response, strlen(response), 0);
        } else {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 404 Not Found\r\n\r\nPage not found.");
        }
    }

    close(client_fd);

    return NULL;
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("Logs from your program will appear here!\n");

    generate_crc32_table();

    int server_fd, client_addr_len, client_fd;
    struct sockaddr_in client_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
        0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(4221),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) !=
        0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    int connection_backlog = 10;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    pthread_t thread[MAX_THREADS];

    int thread_index = 0;

    while (1) {
        printf("server is listening\n");

        client_addr_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                           &client_addr_len);

        if (client_fd == -1) {
            printf("Accept failed: %s \n", strerror(errno));
            continue;
        }

        enqeueClient(client_fd);

        pthread_create(&thread[thread_index], NULL, threadsHandler, NULL);

        thread_index = (thread_index + 1) % MAX_THREADS;

        pthread_detach(thread[thread_index]);
    }

    close(server_fd);

    return 0;
}
