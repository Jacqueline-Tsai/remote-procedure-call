/** 
    * @file server.c
    * @brief Server side implementation of the file system server.
    * @details The server listens for incoming connections from clients and handles requests from clients.
    * The server supports the following system calls:
    * 1. open
    * 2. read
    * 3. write
    * 4. close
    * 5. lseek
    * 6. stat
    * 7. unlink
    * 8. getdirentries
    * 9. getdirtree
    * 10. freedirtree
    * The server sends the response back to the client after processing the request.
    * The server is multi-threaded and can handle multiple clients concurrently.
    * The server is implemented using the socket programming interface, TCP/IP protocol, andC programming language.
    * @author Jacqueline Tsai yunhsuat@andrew.cmu.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sys/dir.h>
#include "dirtree.h"

// Define the maximum message length
#define MAX_MSG_LEN 4096

// socket file descriptor for the connection to the server
int sockfd, sessfd;

/**
    * @brief Handle the open system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_open(const char *buf, char* retBuf) {
    fprintf(stderr, "enter func: handle_open\n");
    // Request Format:
    // | pathname length | pathname    | flags  | mode      |
    // | int(4)          | c_string(n) | int(4) | mode_t(4) |
    size_t req_length[4] = {sizeof(uint32_t), 0, sizeof(uint32_t), sizeof(uint32_t)};
    int req_offsets[5] = {0};
    memcpy(&req_length[1], buf + req_offsets[0], req_length[0]);
    for (int i = 0; i < 4; i++) { 
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }

    char *pathname = malloc(req_length[1] + 1);
    memcpy(pathname, buf + req_offsets[1], req_length[1]);
    int flags;
    memcpy(&flags, buf + req_offsets[2], req_length[2]);
    mode_t mode;
    memcpy(&mode, buf + req_offsets[3], req_length[3]);

    int fd = open(pathname, flags, mode);

    // Response Format:
    // | fd     | errno  |
    // | int(4) | int(4) |
    memcpy(retBuf, &fd, sizeof(int));
    memcpy(retBuf + sizeof(int), &errno, sizeof(int));
    if (fd == -1) {
        perror("open error");
    }
    fprintf(stderr, "handle_open | req | pathname %s | flag %d | mode %d\n", pathname, flags, mode);
    fprintf(stderr, "handle_open | ret | fd %d | errno %d | retBuf %s |\n", fd, errno, retBuf);
    return 2 * sizeof(int);
}

/**
    * @brief Handle the read system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_read(const char *buf, char* retBuf) {
    fprintf(stderr, "enter func: handle_read\n");
    // Define the format of the message.
    // Extendability: We can add more fields to the message by adding more offsets and updating totalSize.
    // Request Format:
    // | fd     | count  |
    // | int(4) | int(4) |
    size_t req_length[2] = {4, 4};
    int req_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }

    int fd, count;
    memcpy(&fd, buf + req_offsets[0], req_length[0]);
    memcpy(&count, buf + req_offsets[1], req_length[1]);

    // Response Format:
    // | bytes read | errno  | data          |
    // | int(4)     | int(4) | string(count) |
    size_t res_length[3] = {sizeof(uint32_t), sizeof(uint32_t), count};
    int res_offsets[4] = {0};
    for (int i = 0; i < 3; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }

    char *data = (char*)malloc(count);
    int bytes_read = read(fd, data, count);

    memcpy(retBuf + res_offsets[0], &bytes_read, res_length[0]);
    memcpy(retBuf + res_offsets[1], &errno, res_length[1]);
    memcpy(retBuf + res_offsets[2], data, res_length[2]);

    if (bytes_read == -1) {
        perror("read error");
    }
    fprintf(stderr, "handle_read | req | fd: %d | count: %d\n", fd, count);
    fprintf(stderr, "handle_read | res | bytes_read: %d | errno: %d | data: %s\n", bytes_read, errno, data);
    free(data);
    return res_offsets[3];
}

/** 
    * @brief Handle the write system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_write(const char *buf, char* retBuf) {
    fprintf(stderr, "enter func: handle_write\n");
    // Request Format:
    // | fd     | count  | data  |
    // | int(4) | int(4) | count |
    size_t req_length[3] = {sizeof(uint32_t), sizeof(uint32_t), 0};
    int req_offsets[4] = {0};
    for (int i = 0; i < 3; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }

    int fd, count;
    memcpy(&fd, buf + req_offsets[0], req_length[0]);
    memcpy(&count, buf + req_offsets[1], req_length[1]);
    char *data = (char*)malloc(count);
    memcpy(data, buf + req_offsets[2], count);


    // Response Format:
    // | bytes written | errno  |
    // | int(4)        | int(4) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }

    ssize_t bytes_written = write(fd, data, count);
    
    memcpy(retBuf + res_offsets[0], &bytes_written, res_length[0]);
    memcpy(retBuf + res_offsets[1], &errno, res_length[1]);
    if (bytes_written == -1) {
        perror("write error");
    }

    fprintf(stderr, "handle_write | req | fd %d | count %d | data %s\n", fd, count, data);
    fprintf(stderr, "handle_write | res | bytes_written %ld | errno %d\n", bytes_written, errno);
    return res_offsets[2];
}

/**
    * @brief Handle the close system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_close(const char *buf, char* retBuf) {
    fprintf(stderr, "enter func: handle_close\n");
    // Request Format:
    // | fd     |
    // | int(4) |
    size_t req_length[1] = {sizeof(uint32_t)};
    int req_offsets[2] = {0};
    for (int i = 0; i < 1; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }

    int fd;
    memcpy(&fd, buf + req_offsets[0], req_length[0]);

    // | success | errno  |
    // | int(4)  | int(4) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }
    int success = close(fd);
    memcpy(retBuf + res_offsets[0], &success, res_length[0]);
    memcpy(retBuf + res_offsets[1], &errno, res_length[1]);
    if (success != 0) {
        perror("close error");
    }
    fprintf(stderr, "handle_close | req | fd %d\n", fd);
    fprintf(stderr, "handle_close | res | success %d | errno %d\n", success, errno);
    return res_offsets[2];
}

/**
    * @brief Handle the lseek system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_lseek(const char *buf, char* retBuf) {
    fprintf(stderr, "enter func: handle_lseek\n");
    // Request Format:
    // | fd     | offset | whence |
    // | int(4) | int(8) | int(4) |
    size_t req_length[3] = {sizeof(uint32_t), sizeof(uint64_t), sizeof(uint32_t)};
    int req_offsets[4] = {0};
    for (int i = 0; i < 3; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }

    int fd;
    off_t offset;
    int whence;
    memcpy(&fd, buf + req_offsets[0], req_length[0]);
    memcpy(&offset, buf + req_offsets[1], req_length[1]);
    memcpy(&whence, buf + req_offsets[2], req_length[2]);

    // Response Format:
    // | new offset | errno  |
    // | int(8)     | int(4) |
    size_t res_length[2] = {sizeof(uint64_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }

    off_t new_offset = lseek(fd, offset, whence);
    memcpy(retBuf + res_offsets[0], &new_offset, res_length[0]);
    memcpy(retBuf + res_offsets[1], &errno, res_length[1]);
    fprintf(stderr, "handle_lseek | req | fd %d | offset %ld | whence %d\n", fd, offset, whence);
    fprintf(stderr, "handle_lseek | res | new_offset %ld | errno %d\n", new_offset, errno);
    if (errno != 0) {
        perror("lseek error");
    }
    return res_offsets[2];
}

/**
    * @brief Handle the stat system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_stat(const char *buf, char* retBuf) {
    fprintf(stderr, "enter func: handle_stat\n");
    // Request Format:
    // | pathname length | pathname    | statbuf
    // | int(4)          | c_string(n) | stat_size
    size_t req_length[3] = {sizeof(uint32_t), 0, sizeof(struct stat)};
    int req_offsets[4] = {0};
    memcpy(&req_length[1], buf + req_offsets[0], req_length[0]);
    for (int i = 0; i < 3; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }

    char *pathname = malloc(req_length[1] + 1);
    struct stat* statbuf = malloc(req_length[2]);
    memcpy(pathname, buf + req_offsets[1], req_length[1]);
    memcpy(statbuf, buf + req_offsets[2], req_length[2]);

    int success = stat(pathname, statbuf);

    // Response Format:
    // | res | errno  |
    // | int | int(4) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }

    memcpy(retBuf + res_offsets[0], &success, res_length[0]);
    memcpy(retBuf + res_offsets[1], &errno, res_length[1]);
    fprintf(stderr, "handle_stat | req | pathname %s\n", pathname);
    fprintf(stderr, "handle_stat | res | success %d | errno %d\n", success, errno);
    if (errno != 0) {
        perror("stat error");
    }
    return res_offsets[2];
}

/**
    * @brief Handle the unlink system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_unlink(const char *buf, char* retBuf) {
    fprintf(stderr, "enter func: handle_unlink\n");
    // Request Format:
    // | pathname length | pathname    |
    // | int(4)          | c_string(n) |
    size_t req_length[2] = {sizeof(uint32_t), 0};
    int req_offsets[3] = {0};
    memcpy(&req_length[1], buf + req_offsets[0], req_length[0]);
    for (int i = 0; i < 2; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char *pathname = malloc(req_length[1] + 1);
    memcpy(pathname, buf + req_offsets[1], req_length[1]);
    
    int success = unlink(pathname);

    // Response Format:
    // | res | errno  |
    // | int | int(4) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }
    memcpy(retBuf + res_offsets[0], &success, res_length[0]);
    memcpy(retBuf + res_offsets[1], &errno, res_length[1]);
    if (errno != 0) {
        perror("unlink error");
    }
    fprintf(stderr, "handle_unlink | req | pathname %s\n", pathname);
    fprintf(stderr, "handle_unlink | res | success %d | errno %d\n", success, errno);
    return res_offsets[2];
}

/**
    * @brief Handle the getdirentries system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_getdirentries(const char *buf, char* retBuf) {
    fprintf(stderr, "enter func: handle_getdirentries\n");
    // Request Format:
    // | fd     | nbyte  | basep  |
    // | int(4) | int(4) | int(8) |
    size_t req_length[3] = {sizeof(uint32_t), sizeof(uint32_t), sizeof(uint64_t)};
    int req_offsets[4] = {0};
    for (int i = 0; i < 3; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    int fd, nbyte;
    off_t basep;
    memcpy(&fd, buf + req_offsets[0], req_length[0]);
    memcpy(&nbyte, buf + req_offsets[1], req_length[1]);
    memcpy(&basep, buf + req_offsets[2], req_length[2]);

    size_t bytes_read = getdirentries(fd, retBuf, nbyte, &basep);

    // Response Format:
    // | bytes read | errno  |
    // | int(4)     | int(4) |
    // | data               |
    // | string(bytes read) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }

    char retBuf1[res_offsets[2]];
    memcpy(retBuf1 + res_offsets[0], &bytes_read, res_length[0]);
    memcpy(retBuf1 + res_offsets[1], &errno, res_length[1]);
    if (errno != 0) {
        perror("getdirentries error");
    }

    if (send(sessfd, retBuf1, res_offsets[2], 0) == -1) {
        fprintf(stderr, "server send failed\n");
        return 0;
    }

    fprintf(stderr, "handle_getdirentries | req | fd %d | nbyte %d | basep %ld\n", fd, nbyte, basep);
    fprintf(stderr, "handle_getdirentries | res | bytes_read %ld | errno %d\n", bytes_read, errno);
    return bytes_read;
}

/**
    * @brief Handle the getdirtree system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
void serialize_dirtree(struct dirtreenode *node, char *buf, int* offset) {
    if (node == NULL) {
        return;
    }
    int name_len = strlen(node->name);
    memcpy(buf + *offset, node->name, name_len);
    buf[*offset + name_len] = '\0';
    *offset += name_len + 1;
    memcpy(buf + *offset, &node->num_subdirs, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    for (int i = 0; i < node->num_subdirs; i++) {
        serialize_dirtree(node->subdirs[i], buf, offset);
    }
}

/** 
    * @brief Handle the getdirtree system call.
    * @param buf The buffer containing the request.
    * @param retBuf The buffer to store the response.
    * @return The size of the response.
    */
size_t handle_getdirtree(const char *buf, char* retBuf) {
    // fprintf(stderr, "enter func: handle_getdirtree\n");
    // Request Format:
    // | pathname length | pathname    |
    // | int(4)          | c_string(n) |
    size_t req_length[2] = {sizeof(uint32_t), 0};
    int req_offsets[3] = {0};
    memcpy(&req_length[1], buf + req_offsets[0], req_length[0]);
    for (int i = 0; i < 2; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    
    char *folder_path = malloc(req_length[1] + 1);
    memcpy(folder_path, buf + req_offsets[1], req_length[1]);
    folder_path[req_length[1]] = '\0';

    struct dirtreenode* root = getdirtree(folder_path);

    int ret_data_length = 0;
    serialize_dirtree(root, retBuf, &ret_data_length);

    // Response Format:
    // | data_length |
    // | int(4)      |
    // | node_name_len | node_name | node_num_subdirs | ...
    // | int(4)        | string(n) | int(4)              | ...    
    char retBuf1[sizeof(uint32_t)];
    memcpy(retBuf1, &ret_data_length, sizeof(uint32_t));
    if (send(sessfd, retBuf1, sizeof(uint32_t), 0) == -1) {
        fprintf(stderr, "server send failed\n");
        return 0;
    }

    // fprintf(stderr, "handle_getdirtree | req | folder_path %s\n", folder_path);
    // fprintf(stderr, "handle_getdirtree | res | ret_data_length %d\n", ret_data_length);
    freedirtree(root);
    return ret_data_length;
}

/**
    * @brief Main function to set up the server and handle client requests.
    * @param argc The number of arguments.
    * @param argv The arguments.
    * @return 0 if successful, 1 if error.
    */
int main(int argc, char**argv) {
    char buf[MAX_MSG_LEN+1];
    char *serverport;
    unsigned short port;
    int rv;
    struct sockaddr_in srv, cli;
    socklen_t sa_size;
    
    // Get environment variable indicating the port of the server
    serverport = getenv("serverport15440");
    if (serverport) port = (unsigned short)atoi(serverport);
    else port=15440;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);    // TCP/IP socket
    if (sockfd<0) err(1, 0);            // in case of error
    
    // setup address structure to indicate server port
    memset(&srv, 0, sizeof(srv));            // clear it first
    srv.sin_family = AF_INET;            // IP family
    srv.sin_addr.s_addr = htonl(INADDR_ANY);    // don't care IP address
    srv.sin_port = htons(port);            // server port

    // bind to our port
    rv = bind(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
    if (rv<0) err(1,0);
    
    // start listening for connections
    rv = listen(sockfd, 5);
    if (rv<0) err(1,0);
    
    // main server loop, handle clients one at a time
    while (1) {
        // wait for next client, get session socket
        sa_size = sizeof(struct sockaddr_in);
        sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);

        if (sessfd<0) {
            err(1,0);
            continue;
        }
        pid_t pid = fork();
        if (pid != 0) {
            close(sessfd);
            continue;
        }
        close(sockfd);
        
        // get messages and send replies to this client, until it goes away
        while ( (rv=recv(sessfd, buf, MAX_MSG_LEN, 0)) > 0) {
            buf[rv]=0;        // null terminate string to print
            int op;
            memcpy(&op, buf, sizeof(uint32_t));
            char *p = buf;
            p += sizeof(uint32_t);
            size_t retLen;
            char retBuf[MAX_MSG_LEN+1];
            switch (op) {
                case 0:
                    retLen = handle_open(p, retBuf);
                    break;
                case 1:
                    retLen = handle_read(p, retBuf);
                    break;
                case 2:
                    retLen = handle_write(p, retBuf);
                    break;
                case 3:
                    retLen = handle_close(p, retBuf);
                    break;
                case 4:
                    retLen = handle_lseek(p, retBuf);
                    break;
                case 5:
                    retLen = handle_stat(p, retBuf);
                    break;
                case 6:
                    retLen = handle_unlink(p, retBuf);
                    break;
                case 7:
                    retLen = handle_getdirentries(p, retBuf);
                    break;
                case 8:
                    retLen = handle_getdirtree(p, retBuf);
                    break;
                default:
                    retLen = 0;
            }
            // fprintf(stderr, "retLen %ld\n", retLen);
            retBuf[retLen] = '\0';
            if (send(sessfd, retBuf, retLen, 0) == -1) {
                fprintf(stderr, "server send failed\n");
            }
        }
        // either client closed connection, or error
        if (rv<0) err(1,0);
        close(sessfd);
        break;
    }
    
    fprintf(stderr, "server shutting down cleanly\n");
    // close socket
    close(sockfd);

    return 0;
}