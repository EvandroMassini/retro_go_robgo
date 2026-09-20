// Símbolos exigidos pelo fMSX quando compilado com UNIX, mais a emulação de cwd no ESP32.
int UseSound, UseZoom, SyncFreq, UseEffects;
int ARGC;
char **ARGV;

#ifdef MSX_ESP32
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include "carts_sha.h"

#define MSX_PATH_MAX 256
#define MSX_ROOT "/sd"

static char path_buffer[MSX_PATH_MAX];
static char path_cwd[MSX_PATH_MAX] = MSX_ROOT;
static struct dirent dirent_cache;

// Resolve caminhos relativos ao cwd emulado, tratando "." e ".." antes de ir ao VFS/FAT.
static const char *get_path(const char *path)
{
    char joined[MSX_PATH_MAX];
    if (!path || !*path) { errno = ENOENT; return NULL; }
    int n = path[0] == '/' ? snprintf(joined, sizeof(joined), "%s", path)
                           : snprintf(joined, sizeof(joined), "%s/%s", path_cwd, path);
    if (n < 0 || n >= (int)sizeof(joined)) { errno = ENAMETOOLONG; return NULL; }
    size_t used = 1;
    path_buffer[0] = '/'; path_buffer[1] = 0;
    const char *part = joined;
    while (*part) {
        while (*part == '/') ++part;
        const char *end = part;
        while (*end && *end != '/') ++end;
        size_t len = end - part;
        if (!len) break;
        if (len == 1 && part[0] == '.') { part = end; continue; }
        if (len == 2 && part[0] == '.' && part[1] == '.') {
            while (used > 1 && path_buffer[used - 1] != '/') --used;
            if (used > 1) --used;
        } else {
            if (used + len + 2 >= MSX_PATH_MAX) { errno = ENAMETOOLONG; return NULL; }
            if (used > 1) path_buffer[used++] = '/';
            memcpy(path_buffer + used, part, len); used += len;
        }
        path_buffer[used] = 0;
        part = end;
    }
    return path_buffer;
}

int msx_chdir(const char *path)
{
    const char *resolved = get_path(path);
    if (!resolved) return -1;
    struct stat info;
    if (stat(resolved, &info)) return -1;
    if (!S_ISDIR(info.st_mode)) { errno = ENOTDIR; return -1; }
    strcpy(path_cwd, resolved);
    memset(&dirent_cache, 0, sizeof(dirent_cache));
    return 0;
}

char *msx_getcwd(char *buf, size_t size)
{
    if (!buf) return strdup(path_cwd);
    if (strlen(path_cwd) < size) return strcpy(buf, path_cwd);
    return NULL;
}

DIR *msx_opendir(const char *path)
{
    const char *resolved = get_path(path);
    return resolved ? opendir(resolved) : NULL;
}

struct dirent *msx_readdir(DIR *dir)
{
    struct dirent *ent = readdir(dir);
    if (ent == NULL) memset(&dirent_cache, 0, sizeof(dirent_cache));
    else dirent_cache = *ent;
    return ent;
}

FILE *msx_fopen(const char *path, const char *mode)
{
    const char *resolved = get_path(path);
    if (!resolved) return NULL;
    // CARTS.SHA (tabela de mapeadores de cartucho) vem embutida no firmware:
    // se não existir no cartão, grava uma cópia ao lado da BIOS.
    if (strcmp(path, "MSX2/CARTS.SHA") == 0 && mode[0] == 'r') {
        struct stat sb;
        if (stat(resolved, &sb) != 0) {
            FILE *fp = fopen(resolved, "wb");
            if (fp) {
                fwrite(carts_sha, sizeof(carts_sha) - 1, 1, fp);   // sem o '\0' final
                fclose(fp);
            }
        }
    }
    return fopen(resolved, mode);
}

int msx_stat(const char *path, struct stat *sbuf)
{
    // O fMSX chama stat() logo depois de readdir() só para saber o tipo.
    // No ESP32 isso é lentíssimo; se for o mesmo nome, fingimos com o d_type já lido.
    if (sbuf && dirent_cache.d_name[0] && strcmp(path, dirent_cache.d_name) == 0) {
        sbuf->st_mode = 0;
        if (dirent_cache.d_type == DT_REG) sbuf->st_mode |= S_IFREG;
        if (dirent_cache.d_type == DT_DIR) sbuf->st_mode |= S_IFDIR;
        return 0;
    }
    const char *resolved = get_path(path);
    return resolved ? stat(resolved, sbuf) : -1;
}

int msx_unlink(const char *path)
{
    const char *resolved = get_path(path);
    return resolved ? unlink(resolved) : -1;
}
#endif
