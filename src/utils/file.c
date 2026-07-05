#include "file.h"

#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include "path.h"

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

int file_remove_tree(const char* path) {
    uv_fs_t req;
    int result = uv_fs_scandir(NULL, &req, path, 0, NULL);
    if (result < 0) {
        uv_fs_req_cleanup(&req);
        return result == UV_ENOENT ? 0 : -1;
    }

    uv_dirent_t entry;
    while (uv_fs_scandir_next(&req, &entry) == 0) {
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }

        char child[1024];
        if (path_join(path, entry.name, child, sizeof(child)) != 0) {
            uv_fs_req_cleanup(&req);
            return -1;
        }

        if (entry.type == UV_DIRENT_DIR) {
            if (file_remove_tree(child) != 0) {
                uv_fs_req_cleanup(&req);
                return -1;
            }
        } else {
            uv_fs_t unlink_req;
            int unlink_result = uv_fs_unlink(NULL, &unlink_req, child, NULL);
            uv_fs_req_cleanup(&unlink_req);
            if (unlink_result < 0 && unlink_result != UV_ENOENT) {
                uv_fs_req_cleanup(&req);
                return -1;
            }
        }
    }
    uv_fs_req_cleanup(&req);

    result = uv_fs_rmdir(NULL, &req, path, NULL);
    uv_fs_req_cleanup(&req);
    return result == 0 || result == UV_ENOENT ? 0 : -1;
}
