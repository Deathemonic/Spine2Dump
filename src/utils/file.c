#include "file.h"

#include <stdlib.h>

#include <uv.h>

int file_read_all(const char* path, void** data, size_t* size) {
    uv_fs_t req;
    int fd = uv_fs_open(NULL, &req, path, UV_FS_O_RDONLY, 0, NULL);
    uv_fs_req_cleanup(&req);
    if (fd < 0) {
        return -1;
    }

    if (uv_fs_fstat(NULL, &req, fd, NULL) != 0) {
        uv_fs_req_cleanup(&req);
        uv_fs_close(NULL, &req, fd, NULL);
        uv_fs_req_cleanup(&req);
        return -1;
    }
    size_t length = (size_t)req.statbuf.st_size;
    uv_fs_req_cleanup(&req);

    char* buffer = malloc(length ? length : 1);
    if (buffer == NULL) {
        uv_fs_close(NULL, &req, fd, NULL);
        uv_fs_req_cleanup(&req);
        return -1;
    }

    size_t total = 0;
    while (total < length) {
        uv_buf_t iov = uv_buf_init(buffer + total, (unsigned)(length - total));
        ssize_t read = uv_fs_read(NULL, &req, fd, &iov, 1, (int64_t)total, NULL);
        uv_fs_req_cleanup(&req);
        if (read < 0) {
            free(buffer);
            uv_fs_close(NULL, &req, fd, NULL);
            uv_fs_req_cleanup(&req);
            return -1;
        }
        if (read == 0) {
            break;
        }
        total += (size_t)read;
    }

    uv_fs_close(NULL, &req, fd, NULL);
    uv_fs_req_cleanup(&req);

    *data = buffer;
    *size = total;
    return 0;
}

int file_write_all(const char* path, const void* data, size_t size) {
    uv_fs_t req;
    int fd = uv_fs_open(NULL, &req, path, UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_TRUNC, 0644,
                        NULL);
    uv_fs_req_cleanup(&req);
    if (fd < 0) {
        return -1;
    }

    size_t total = 0;
    while (total < size) {
        uv_buf_t iov = uv_buf_init((char*)data + total, (unsigned)(size - total));
        ssize_t written = uv_fs_write(NULL, &req, fd, &iov, 1, (int64_t)total, NULL);
        uv_fs_req_cleanup(&req);
        if (written < 0) {
            uv_fs_close(NULL, &req, fd, NULL);
            uv_fs_req_cleanup(&req);
            return -1;
        }
        total += (size_t)written;
    }

    uv_fs_close(NULL, &req, fd, NULL);
    uv_fs_req_cleanup(&req);
    return 0;
}
