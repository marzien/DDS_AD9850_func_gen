/*
 * Minimal newlib system-call stubs for the GCC / STM32CubeIDE build
 * (no console I/O in this project).
 */
#include <errno.h>
#include <sys/stat.h>

int _write(int file, char *ptr, int len) { (void)file; (void)ptr; return len; }
int _read(int file, char *ptr, int len)  { (void)file; (void)ptr; (void)len; return 0; }
int _close(int file)                     { (void)file; return -1; }
int _lseek(int file, int ptr, int dir)   { (void)file; (void)ptr; (void)dir; return 0; }
int _isatty(int file)                    { (void)file; return 1; }
int _fstat(int file, struct stat *st)    { (void)file; st->st_mode = S_IFCHR; return 0; }
