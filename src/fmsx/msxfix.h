#ifndef _MSXFIX_H_
#define _MSXFIX_H_

// Este header é inserido na 1ª linha de cada .c do núcleo (EMULib, fMSX, Z80).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Configuração do núcleo (antes eram flags -D do build antigo).
#ifndef BPS16
#define BPS16 1
#endif
#ifndef UNIX
#define UNIX 1
#endif
#ifndef LSB_FIRST
#define LSB_FIRST 1
#endif
#ifndef NARROW
#define NARROW 1
#endif

// Ativa as alterações do fork robgo dentro do núcleo (mixer com SCC+FM, Z80 em IRAM,
// menu simplificado, navegação de pastas). Estão marcadas com este nome no código.
#ifndef RG_TARGET_ROBGO_RG
#define RG_TARGET_ROBGO_RG 1
#endif

// Ajustes para compilar o fMSX fora do Unix.
#define main(x, y) fmsx_main(x, y)
#define abs(x) __abs(x)
#define GenericSetVideo SetVideo

int fmsx_main(int argc, char *argv[]);

#ifdef MSX_ESP32
/**
 * O ESP32 não tem diretório de trabalho (cwd), e o fMSX depende dele. Estas funções
 * emulam cwd, evitam stat() depois de readdir() (lento no SD) e resolvem "." e "..".
 */
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

DIR *msx_opendir(const char *);
struct dirent *msx_readdir(DIR *);
FILE *msx_fopen(const char *, const char *);
int msx_stat(const char *, struct stat *);
int msx_unlink(const char *);
int msx_chdir(const char *);
char *msx_getcwd(char *buf, size_t size);

#define chdir(x) msx_chdir(x)
#define getcwd(x, y) msx_getcwd(x, y)
#define opendir(x) msx_opendir(x)
#define readdir(x) msx_readdir(x)
#define fopen(x, y) msx_fopen(x, y)
#define stat(x, y) msx_stat(x, y)
#define unlink(x) msx_unlink(x)
#endif

// Todo malloc() do núcleo (RAM/VRAM do MSX, imagens de disco, estados salvos) passa por
// hal_alloc_big(): blocos grandes vão para a PSRAM, sem depender da configuração do malloc.
void *hal_alloc_big(size_t size);
#define malloc(n) hal_alloc_big(n)

#endif
