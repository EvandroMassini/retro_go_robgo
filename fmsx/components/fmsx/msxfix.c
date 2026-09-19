// Unused but needed because we define UNIX
int UseSound, UseZoom, SyncFreq, UseEffects;
int  ARGC;
char **ARGV;

#ifdef ESP_PLATFORM
/**
 * esp-idf has no concept of a current working directory, which fMSX relies heavily upon.
 * So we must emulate it. I've also added optimizations to bypass stat calls because
 * it makes using the built-in menu incredibly slow. It also returns NULL on certain filenames
 * to speed up boot significantly.
 *
 * To make it work, add `-include msxfix.h` to fMSX's cflags
*/
#include <rg_system.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include "carts_sha.h"

static char path_buffer[RG_PATH_MAX];
static char path_cwd[RG_PATH_MAX] = RG_STORAGE_ROOT;
static struct dirent dirent_cache;

const char *msx_ignore_files = ""; // "DEFAULT.FNT DEFAULT.CAS DRIVEA.DSK DRIVEB.DSK CARTA.ROM CARTB.ROM";

// Resolve . e .. antes de enviar o caminho ao VFS/FAT.
static const char *get_path(const char *path)
{
    char joined[RG_PATH_MAX];
    if (!path || !*path) { errno=ENOENT; return NULL; }
    int n = path[0]=='/' ? snprintf(joined,sizeof(joined),"%s",path)
                           : snprintf(joined,sizeof(joined),"%s/%s",path_cwd,path);
    if (n<0 || n>=sizeof(joined)) { errno=ENAMETOOLONG; return NULL; }
    size_t used=1;
    path_buffer[0]='/'; path_buffer[1]=0;
    const char *part=joined;
    while (*part) {
        while (*part=='/') ++part;
        const char *end=part;
        while (*end && *end!='/') ++end;
        size_t len=end-part;
        if (!len) break;
        if (len==1 && part[0]=='.') { part=end; continue; }
        if (len==2 && part[0]=='.' && part[1]=='.') {
            while (used>1 && path_buffer[used-1]!='/') --used;
            if (used>1) --used;
        } else {
            if (used>1) path_buffer[used++]='/';
            memcpy(path_buffer+used,part,len); used+=len;
        }
        path_buffer[used]=0;
        part=end;
    }
    return path_buffer;
}

int msx_chdir(const char *path)
{
    const char *resolved=get_path(path);
    if (!resolved) return -1;
    struct stat info;
    if (stat(resolved,&info)) return -1;
    if (!S_ISDIR(info.st_mode)) { errno=ENOTDIR; return -1; }
    strcpy(path_cwd,resolved);
    memset(&dirent_cache,0,sizeof(dirent_cache));
    return 0;
}

char *msx_getcwd(char *buf, size_t size)
{
    RG_LOGV("called");
    if (!buf)
        return strdup(path_cwd);
    if (strlen(path_cwd) < size)
        return strcpy(buf, path_cwd);
    return NULL;
}

DIR *msx_opendir(const char *path)
{
    RG_LOGV("called ('%s')", path);
    const char *resolved=get_path(path);
    return resolved ? opendir(resolved) : NULL;
}

struct dirent *msx_readdir(DIR *dir)
{
    struct dirent *ent = readdir(dir);
    if (ent == NULL)
        memset(&dirent_cache, 0, sizeof(dirent_cache));
    else
        dirent_cache = *ent;
    return ent;
}

FILE *msx_fopen(const char *path, const char *mode)
{
    RG_LOGV("called ('%s', '%s')", path, mode);
    if (strstr(msx_ignore_files, path))
        return NULL;
    if (strcmp(path, "CARTS.SHA") == 0)
    {
        path = RG_BASE_PATH_CACHE "/MSX_CARTS.SHA";
        if (!rg_storage_exists(path))
        {
            FILE *fp = fopen(path, "wb");
            fwrite(carts_sha, sizeof(carts_sha), 1, fp);
            fclose(fp);
        }
    }
    const char *resolved=get_path(path);
    return resolved ? fopen(resolved, mode) : NULL;
}

int msx_stat(const char *path, struct stat *sbuf)
{
    RG_LOGV("called ('%s')", path);
    if (strstr(msx_ignore_files, path))
        return -1;
#if defined(DT_REG) && defined(DT_DIR)
    // fMSX uses stat right after readdir just to check type
    // This is ultra slow on esp32, so we cache the last readdir
    // and if it has the same name, we fake sbuf
    if (sbuf && strcmp(path, dirent_cache.d_name) == 0)
    {
        sbuf->st_mode = 0;
        if (dirent_cache.d_type == DT_REG)
            sbuf->st_mode |= S_IFREG;
        if (dirent_cache.d_type == DT_DIR)
            sbuf->st_mode |= S_IFDIR;
        return 0;
    }
#endif
    const char *resolved=get_path(path);
    return resolved ? stat(resolved, sbuf) : -1;
}

int msx_unlink(const char *path)
{
    RG_LOGV("called ('%s')", path);
    const char *resolved=get_path(path);
    return resolved ? unlink(resolved) : -1;
}
#endif
