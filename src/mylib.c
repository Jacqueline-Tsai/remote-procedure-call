/**
    * @file mylib.c
    * @brief This file contains the implementation of the functions that replace the libc functions.
    * @details 
    * The functions are used to replace standard libc functions.
    * The functions are used to send requests to the server and receive responses from the server.
    * This file contains the implementation of the following functions:
    * 1. open: The function is used to open a file. 
    * 2. read: The function is used to read from a file.
    * 3. write: The function is used to write to a file.
    * 4. close: The function is used to close a file.
    * 5. lseek: The function is used to change the file offset.
    * 6. stat: The function is used to get file status.
    * 7. unlink: The function is used to delete a name and possibly the file it refers to.
    * 8. getdirentries: The function is used to get directory entries.
    * 9. getdirtree: The function is used to get the directory tree.
    * 10. freedirtree: The function is used to free the directory tree.
    * The functions are implemented using the socket programming interface, TCP/IP protocol, and C programming language.
    * @author Jacqueline Tsai yunhsuat@andrew.cmu.edu
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <err.h>
#include "dirtree.h"

// Define the maximum message length
#define MAX_MSG_LEN 4096

// Define the offset to distinguish between the file descriptors returned by the server
#define FD_OFFSET 5000

// The following line declares a function pointer with the same prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT
int (*orig_close)(int fd);
int (*orig_read)(int fd, void *buf, size_t count);
int (*orig_write)(int fd, const void *buf, size_t count);
ssize_t (*orig_lseek)(int fildes, off_t offset, int whence);
int (*orig_stat)(const char *restrict pathname, struct stat *restrict statbuf);
int (*orig_unlink)(const char *pathname);
ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbyte, off_t *restrict basep);
struct dirtreenode *(*orig_getdirtree)(const char *path);
void (*orig_freedirtree)(struct dirtreenode *dt);

// socket file descriptor for the connection to the server
int sockfd;

/**
    * @brief Send a request to the server.
    * @param buf The buffer containing the request.
    * @param totalSize The size of the request.
    */
void sendRequest(char *buf, size_t totalSize) {
    // fprintf(stderr, "try sending %ld bytes...\n", totalSize);
    size_t sentSize = 0;
    while (sentSize < totalSize) {
        sentSize += send(sockfd, buf + sentSize, totalSize - sentSize, 0);
    }
    fprintf(stderr, "sent req | size: %ld | msg: %s\n", sentSize, buf);
}

/** 
    * @brief Receive a response from the server.
    * @param buf The buffer to store the response.
    * @param totalSize The size of the response.
    */
void receiveResponse(char *buf, size_t totalSize) {
    // fprintf(stderr, "try receving %ld bytes...\n", totalSize);
    size_t receivedSize = 0;
    while (receivedSize < totalSize) {
        int rv = recv(sockfd, buf + receivedSize, totalSize - receivedSize, 0);
        if (rv < 0) {
            err(1, 0);
            break;
        }  
        receivedSize += rv;
    }
    fprintf(stderr, "received res | size: %ld | msg: %s\n", receivedSize, buf);
}

/**
    * @brief Open a file.
    * @param pathname The path to the file.
    * @param flags The flags to open the file.
    * @param ... The mode to open the file.
    * @return The file descriptor.
    */
int open(const char *pathname, int flags, ...) {
    fprintf(stderr, "mylib: open called | path %s\n", pathname);

    mode_t mode=0;
    if (flags & O_CREAT) {
        va_list a;
        va_start(a, flags);
        mode = va_arg(a, mode_t);
        va_end(a);
    }
    // Define the format of the message.
    // Extendability: We can add more fields to the message by adding more offsets and updating totalSize.
    // Request Format:
    // | op     | pathname length | pathname    | flags  | mode      |
    // | int(4) | int(4)          | c_string(n) | int(4) | mode_t(4) |
    int num_fields = 5;
    size_t req_length[5] = {4, 4, strlen(pathname), 4, 4};
    int req_offsets[6] = {0};
    for (int i = 0; i < 5; i++) { 
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }

    char reqBuf[req_offsets[num_fields]];
    int op = 0, path_len = strlen(pathname);
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &path_len, req_length[1]);
    memcpy(reqBuf + req_offsets[2], pathname, req_length[2]);
    memcpy(reqBuf + req_offsets[3], &flags, req_length[3]);
    memcpy(reqBuf + req_offsets[4], &mode, req_length[4]);
    sendRequest(reqBuf, req_offsets[5]);

    // Response Format:
    // | fd     | errno  |
    // | int(4) | int(4) |
    char resBuf[2 * sizeof(int)];
    receiveResponse(resBuf, 2 * sizeof(int));
    int fd;
    memcpy(&fd, resBuf, sizeof(int));
    memcpy(&errno, resBuf + sizeof(int), sizeof(int));
    if (fd != -1) fd += FD_OFFSET;

    fprintf(stderr, "mylib: open returned | fd %d | errno %d\n\n", fd, errno);
    return fd;
}

/** 
    * @brief Read smaller than MAX_MSG_LEN bytes from a file.
    * @param fd The file descriptor.
    * @param buf The buffer to store the data.
    * @param count The number of bytes to read.
    * @return The number of bytes read.
    */
ssize_t readHelper(int fd, void *buf, size_t count) {
    fprintf(stderr, "mylib: readHelper called | fd %d | count %zu\n", fd, count);
    // Define the format of the message.
    // Extendability: We can add more fields to the message by adding more offsets and updating totalSize.
    // Request Format:
    // | op     | fd     | count  |
    // | int(4) | int(4) | int(4) |

    int op = 1;
    size_t req_length[3] = {sizeof(uint32_t), sizeof(uint32_t), sizeof(uint32_t)};
    int req_offsets[4] = {0};
    for (int i = 0; i < 3; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char reqBuf[req_offsets[3]];
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &fd, req_length[1]);
    memcpy(reqBuf + req_offsets[2], &count, req_length[2]);
    sendRequest(reqBuf, req_offsets[3]);


    // Response Format:
    // | bytes read | errno  | data               |
    // | int(4)     | int(4) | string(bytes read) |
    size_t res_length[3] = {sizeof(uint32_t), sizeof(uint32_t), count};
    int res_offsets[4] = {0};
    for (int i = 0; i < 3; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }

    char resBuf[res_offsets[3]];
    receiveResponse(resBuf, res_offsets[3]);
    int bytes_read;
    memcpy(&bytes_read, resBuf + res_offsets[0], res_length[0]);
    memcpy(&errno, resBuf + res_offsets[1], res_length[1]);
    memcpy(buf, resBuf + res_offsets[2], res_length[2]);

    fprintf(stderr, "mylib: readHelper returned | bytes_read: %d | err %d | data %s\n\n", bytes_read, errno, (char *)buf);
    return errno == 0? bytes_read: -1;
}

/** 
    * @brief Read from a file.
    * @param fd The file descriptor.
    * @param buf The buffer to store the data.
    * @param count The number of bytes to read.
    * @return The number of bytes read.
    */
ssize_t read(int fd, void *buf, size_t count) {
    fprintf(stderr, "mylib: read called | fd %d | count %zu\n", fd, count);
    if (fd < FD_OFFSET) {
        return orig_read(fd, buf, count);
    }
    fd -= FD_OFFSET;

    size_t maxLen = MAX_MSG_LEN - 8;
    int total_bytes_read = 0;
    while (count != 0) {
        size_t bytes_read = count > maxLen? maxLen: count;
        bytes_read = readHelper(fd, buf + total_bytes_read, bytes_read);
        if (bytes_read == -1) {
            fprintf(stderr, "mylib: read failed | errno %d\n\n", errno);
            return -1;
        }
        if (bytes_read == 0) {
            break;
        }
        total_bytes_read += bytes_read;
        count -= bytes_read;
        // fprintf(stderr, "| total_bytes_read: %d | count: %zu\n", total_bytes_read, count); 
    }

    fprintf(stderr, "mylib: read returned | bytes_read %d\n\n", total_bytes_read);
    return total_bytes_read;
}

/** 
    * @brief Write smaller than MAX_MSG_LEN bytes to a file.
    * @param fd The file descriptor.
    * @param buf The buffer to store the data.
    * @param count The number of bytes to write.
    * @return The number of bytes written.
*/
ssize_t writeHelper(int fd, const void *buf, size_t count){
    fprintf(stderr, "mylib: writeHelper called | fd %d | count %ld\n", fd, count);
    // Define the format of the message.
    // Extendability: We can add more fields to the message by adding more offsets and updating totalSize.
    // Request Format:
    // | op     | fd     | count  | data  |
    // | int(4) | int(4) | int(4) | count |
    int op = 2;
    size_t req_length[4] = {sizeof(uint32_t), sizeof(uint32_t), sizeof(uint32_t), count};
    int req_offsets[5] = {0};
    for (int i = 0; i < 4; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char reqBuf[req_offsets[4]];
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &fd, req_length[1]);
    memcpy(reqBuf + req_offsets[2], &count, req_length[2]);
    memcpy(reqBuf + req_offsets[3], buf, req_length[3]);
    sendRequest(reqBuf, req_offsets[4]);

    // Response Format:
    // | bytes written | errno  |
    // | int(4)        | int(4) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }
    char resBuf[res_offsets[2]];
    receiveResponse(resBuf, res_offsets[2]);
    int bytes_written;
    memcpy(&bytes_written, resBuf + res_offsets[0], res_length[0]);
    memcpy(&errno, resBuf + res_offsets[1], res_length[1]);

    fprintf(stderr, "mylib: writeHelper returned | bytes_written %d | errno %d\n", bytes_written, errno);
    return errno == 0? bytes_written: -1;
}

/** 
    * @brief Write to a file.
    * @param fd The file descriptor.
    * @param buf The buffer to store the data.
    * @param count The number of bytes to write.
    * @return The number of bytes written.
*/
ssize_t write(int fd, const void *buf, size_t count){
    fprintf(stderr, "mylib: write called | fd %d | count %ld | buf %s | buf_len %ld\n", fd, count, (char *)buf, strlen((char *)buf));
    if (fd < FD_OFFSET) {
        return orig_write(fd, buf, count);
    }
    fd -= FD_OFFSET;
    // const void * test_buf = "12abcde\0 4385eorud,sir";
    // fd = orig_open("foo", O_RDWR);
    // return orig_write(fd, test_buf, count);

    // call helper function from 0-maxLen, maxLen-2*maxLen, 2*maxLen-3*maxLen, ...
    size_t maxLen = MAX_MSG_LEN - 12;
    int total_bytes_written = 0;
    while (count != 0) {
        size_t bytes_written = count > maxLen? maxLen: count;
        bytes_written = writeHelper(fd, buf + total_bytes_written, bytes_written);
        if (bytes_written == -1) {
            fprintf(stderr, "mylib: write failed | errno %d\n\n", errno);
            return -1;
        }
        total_bytes_written += bytes_written;
        count -= bytes_written;
    }
    fprintf(stderr, "mylib: write returned | bytes_written %d\n\n", total_bytes_written);
    return total_bytes_written == 0? -1: total_bytes_written;
}

/** 
    * @brief Close a file.
    * @param fd The file descriptor.
    * @return 0 if successful, -1 if error.
    */
int close(int fd) {
    fprintf(stderr, "mylib: close called | fd %d\n", fd);
    if (fd < FD_OFFSET) {
        return orig_close(fd);
    }
    fd -= FD_OFFSET;
    // Define the format of the message.
    // Extendability: We can add more fields to the message by adding more offsets and updating totalSize.
    // Request Format:
    // | op     | fd     |
    // | int(4) | int(4) |
    int op = 3;
    size_t req_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int req_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char reqBuf[req_offsets[2]];
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &fd, req_length[1]);
    sendRequest(reqBuf, req_offsets[2]);

    // Response Format:
    // | success | errno  |
    // | int(4)  | int(4) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }
    char resBuf[res_offsets[2]];
    receiveResponse(resBuf, res_offsets[2]);
    int success;
    memcpy(&success, resBuf + res_offsets[0], res_length[0]);
    memcpy(&errno, resBuf + res_offsets[1], res_length[1]);

    fprintf(stderr, "mylib: close returned | success: %d | errno: %d\n\n", success, errno);
    return success;
}

/** 
    * @brief Change the file offset.
    * @param fd The file descriptor.
    * @param offset The offset to change.
    * @param whence The position to change.
    * @return The new offset.
    */
ssize_t lseek(int fd, off_t offset, int whence)
{
    fprintf(stderr, "mylib: called | fd %d | offset %ld | whence %d\n", fd, offset, whence);
    if (fd < FD_OFFSET) {
        return orig_lseek(fd, offset, whence);
    }
    fd -= FD_OFFSET;
    // Request Format:
    // | op     | fd     | offset | whence |
    // | int(4) | int(4) | int(8) | int(4) |
    int op = 4;
    size_t req_length[4] = {sizeof(uint32_t), sizeof(uint32_t), sizeof(uint64_t), sizeof(uint32_t)};
    int req_offsets[5] = {0};
    for (int i = 0; i < 4; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char reqBuf[req_offsets[4]];
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &fd, req_length[1]);
    memcpy(reqBuf + req_offsets[2], &offset, req_length[2]);
    memcpy(reqBuf + req_offsets[3], &whence, req_length[3]);
    sendRequest(reqBuf, req_offsets[4]);

    // Response Format:
    // | new offset | errno  |
    // | int(8)     | int(4) |
    size_t res_length[2] = {sizeof(uint64_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }
    char resBuf[res_offsets[2]];
    receiveResponse(resBuf, res_offsets[2]);
    off_t new_offset;
    memcpy(&new_offset, resBuf + res_offsets[0], res_length[0]);
    memcpy(&errno, resBuf + res_offsets[1], res_length[1]);
    fprintf(stderr, "mylib: lseek returned | new_offset: %ld | errno: %d\n\n", new_offset, errno);
    return new_offset;
}

/** 
    * @brief Get file status.
    * @param pathname The path to the file.
    * @param statbuf The buffer to store the status.
    * @return 0 if successful, -1 if error.
    */
int stat(const char *restrict pathname, struct stat *restrict statbuf) {
    fprintf(stderr, "mylib: stat called | path %s | %ld\n", pathname, sizeof(struct stat));
    // Request Format:
    // | op     | pathname length | pathname    | statbuf
    // | int(4) | int(4)          | c_string(n) | stat_size
    int op = 5;
    size_t req_length[4] = {sizeof(uint32_t), sizeof(uint32_t), strlen(pathname), sizeof(struct stat)};
    int req_offsets[5] = {0};
    for (int i = 0; i < 4; i++) { 
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char reqBuf[req_offsets[4]];
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &req_length[2], req_length[1]);
    memcpy(reqBuf + req_offsets[2], pathname, req_length[2]);
    memcpy(reqBuf + req_offsets[3], statbuf, req_length[3]);
    sendRequest(reqBuf, req_offsets[4]);

    // Response Format:
    // | res    | errno  |
    // | int(4) | int(4) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }
    char resBuf[res_offsets[2]];
    receiveResponse(resBuf, res_offsets[2]);
    int success;
    memcpy(&success, resBuf + res_offsets[0], res_length[0]);
    memcpy(&errno, resBuf + res_offsets[1], res_length[1]);

    fprintf(stderr, "mylib: stat returned | success: %d | errno: %d\n\n", success, errno);
    return success;
}

/** 
    * @brief Delete a name and possibly the file it refers to.
    * @param pathname The path to the file.
    * @return 0 if successful, -1 if error.
    */
int unlink(const char *pathname){
    fprintf(stderr, "mylib: unlink called | path %s\n", pathname);
    // Request Format:
    // | op     | pathname length | pathname    |
    // | int(4) | int(4)          | c_string(n) |
    int op = 6;
    size_t req_length[3] = {sizeof(uint32_t), sizeof(uint32_t), strlen(pathname)};
    int req_offsets[4] = {0};
    for (int i = 0; i < 3; i++) { 
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char reqBuf[req_offsets[3]];
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &req_length[2], req_length[1]);
    memcpy(reqBuf + req_offsets[2], pathname, req_length[2]);
    sendRequest(reqBuf, req_offsets[3]);

    // Response Format:
    // | res    | errno  |
    // | int(4) | int(4) |
    size_t res_length[2] = {sizeof(uint32_t), sizeof(uint32_t)};
    int res_offsets[3] = {0};
    for (int i = 0; i < 2; i++) {
        res_offsets[i + 1] = res_offsets[i] + res_length[i];
    }
    char resBuf[res_offsets[2]];
    receiveResponse(resBuf, res_offsets[2]);
    int success;
    memcpy(&success, resBuf + res_offsets[0], res_length[0]);
    memcpy(&errno, resBuf + res_offsets[1], res_length[1]);
    
    fprintf(stderr, "mylib: unlink returned | success %d | errno %d\n\n", success, errno);
    return success;
}

/** 
    * @brief Get directory entries.
    * @param fd The file descriptor.
    * @param buf The buffer to store the data.
    * @param nbyte The number of bytes to read.
    * @param basep The base pointer.
    * @return The number of bytes read.
    */
ssize_t getdirentries(int fd, char *buf, size_t nbyte, off_t *restrict basep) {
    fprintf(stderr, "mylib: getdirentries called | fd %d | nbyte %zu\n", fd, nbyte);
    if (fd < FD_OFFSET) {
        return orig_getdirentries(fd, buf, nbyte, basep);
    }
    fd -= FD_OFFSET;
    // Define the format of the message.
    // Extendability: We can add more fields to the message by adding more offsets and updating totalSize.
    // Request Format:
    // | op     | fd     | nbyte  | basep  |
    // | int(4) | int(4) | int(4) | int(8) |
    int op = 7;
    size_t req_length[4] = {sizeof(uint32_t), sizeof(uint32_t), sizeof(uint32_t), sizeof(uint64_t)};
    int req_offsets[5] = {0};
    for (int i = 0; i < 4; i++) {
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char reqBuf[req_offsets[4]];
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &fd, req_length[1]);
    memcpy(reqBuf + req_offsets[2], &nbyte, req_length[2]);
    memcpy(reqBuf + req_offsets[3], basep, req_length[3]);
    sendRequest(reqBuf, req_offsets[4]);

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
    char resBuf[res_offsets[2]];
    receiveResponse(resBuf, res_offsets[2]);
    int bytes_read;
    memcpy(&bytes_read, resBuf + res_offsets[0], res_length[0]);
    memcpy(&errno, resBuf + res_offsets[1], res_length[1]);
    if (errno != 0) {
        fprintf(stderr, "mylib: getdirentries failed | errno %d\n\n", errno);
        return bytes_read;
    }
    receiveResponse(buf, bytes_read);
    buf[bytes_read] = '\0';

    fprintf(stderr, "mylib: getdirentries returned | bytes_read %d | errno %d | data %s\n\n", bytes_read, errno, buf);
    return bytes_read;
}

/** 
    * @brief Get the directory tree.
    * @param path The path to the directory.
    * @return The directory tree.
    */
void deserialize_dirtree(struct dirtreenode *node, char *buf, int* offset) {
    if (node == NULL) {
        return;
    }
    int name_len = strlen(buf + *offset);
    node->name = (char *)malloc(name_len + 1);
    memcpy(node->name, buf + *offset, name_len);
    node->name[name_len] = '\0';
    *offset += name_len + 1;
    memcpy(&node->num_subdirs, buf + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    node->subdirs = (struct dirtreenode **)malloc(node->num_subdirs * sizeof(struct dirtreenode *));
    for (int i = 0; i < node->num_subdirs; i++) {
        node->subdirs[i] = (struct dirtreenode *)malloc(sizeof(struct dirtreenode));
        deserialize_dirtree(node->subdirs[i], buf, offset);
    }
}

/** 
    * @brief Get the directory tree.
    * @param path The path to the directory.
    * @return The directory tree.
    */
struct dirtreenode *getdirtree(const char *path){
    // fprintf(stderr, "mylib: getdirtree called | path %s\n", path);

    // Request Format:
    // | op     | pathname length | pathname    |
    // | int(4) | int(4)          | c_string(n) |
    int op = 8;
    size_t req_length[3] = {sizeof(uint32_t), sizeof(uint32_t), strlen(path)};
    int req_offsets[4] = {0};
    for (int i = 0; i < 3; i++) { 
        req_offsets[i + 1] = req_offsets[i] + req_length[i];
    }
    char reqBuf[req_offsets[3]];
    memcpy(reqBuf + req_offsets[0], &op, req_length[0]);
    memcpy(reqBuf + req_offsets[1], &req_length[2], req_length[1]);
    memcpy(reqBuf + req_offsets[2], path, req_length[2]);
    sendRequest(reqBuf, req_offsets[3]);

    // Response Format:
    // | data_length |
    // | int(4)      |
    // | node_name_len | node_name | node_num_subdirs | ...
    // | int(4)        | n           | int(4)              | ...    
    char resBuf1[sizeof(uint32_t)];
    receiveResponse(resBuf1, sizeof(uint32_t));
    int ret_data_length;
    memcpy(&ret_data_length, resBuf1, sizeof(uint32_t));
    // fprintf(stderr, "mylib: getdirtree returned | ret_data_length: %d\n", ret_data_length);

    char resBuf[ret_data_length + 1];
    receiveResponse(resBuf, ret_data_length);
    resBuf[ret_data_length] = '\0';

    struct dirtreenode *root = (struct dirtreenode *)malloc(sizeof(struct dirtreenode));
    int offset = 0;
    deserialize_dirtree(root, resBuf, &offset);

    // fprintf(stderr, "mylib: getdirtree returned | root: %s | num_subdirs: %d\n\n", root->name, root->num_subdirs);
    return root;
}

/** 
    * @brief Free the directory tree locally.
    * @param dt The directory tree.
    */
void freedirtree(struct dirtreenode *dt){
    // fprintf(stderr, "mylib: freedirtree called\n");
    return orig_freedirtree(dt);
}

/** 
    * @brief Connect to the server.
    * @return 0 if successful, -1 if error.
    */
int connectServer() {
    char *serverip;
    char *serverport;
    unsigned short port;
    int rv;
    struct sockaddr_in srv;
    
    // Get environment variable indicating the ip address of the server
    serverip = getenv("server15440");
    if (serverip) fprintf(stderr, "Got environment variable server15440: %s\n", serverip);
    else {
        fprintf(stderr, "Environment variable server15440 not found.  Using 127.0.0.1\n");
        serverip = "127.0.0.1";
    }
    
    // Get environment variable indicating the port of the server
    serverport = getenv("serverport15440");
    if (serverport) fprintf(stderr, "Got environment variable serverport15440: %s\n", serverport);
    else {
        fprintf(stderr, "Environment variable serverport15440 not found.  Using 15440\n");
        serverport = "15440";
    }
    port = (unsigned short)atoi(serverport);
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);    // TCP/IP socket
    if (sockfd<0) err(1, 0);            // in case of error
    
    // setup address structure to point to server
    memset(&srv, 0, sizeof(srv));            // clear it first
    srv.sin_family = AF_INET;            // IP family
    srv.sin_addr.s_addr = inet_addr(serverip);    // IP address of server
    srv.sin_port = htons(port);            // server port

    // actually connect to the server
    rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
    if (rv<0) err(1,0);

    return 0;
}

/**
    * @brief Init function to set the function pointers to the original functions.
    * Automatically called when program is started.
    */
void _init(void) {
    // set function pointer orig_open to point to the original open function
    orig_open = dlsym(RTLD_NEXT, "open");
    orig_close = dlsym(RTLD_NEXT, "close");
    orig_read = dlsym(RTLD_NEXT, "read");
    orig_write = dlsym(RTLD_NEXT, "write");
    orig_lseek = dlsym(RTLD_NEXT, "lseek");
    orig_stat = dlsym(RTLD_NEXT, "stat");
    orig_unlink = dlsym(RTLD_NEXT, "unlink");
    orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
    orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
    orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
    connectServer();
}
