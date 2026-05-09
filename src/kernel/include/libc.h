#pragma once

#include "stdint.h"
#include "stddef.h"
#include "stdarg.h"

#ifdef __cplusplus
extern "C" {
#endif

void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
void* memchr(const void* ptr, int value, size_t num);
void* memmove(void* dest, const void* src, size_t num);

char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t num);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t num);
size_t strlen(const char* str);
size_t strnlen(const char* str, size_t maxlen);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t num);
int strcoll(const char* s1, const char* s2);
char* strchr(const char* str, int c);
char* strrchr(const char* str, int c);
char* strpbrk(const char* str, const char* accept);
char* strstr(const char* haystack, const char* needle);
char* strtok(char* str, const char* delim);
size_t strspn(const char* str, const char* accept);
size_t strcspn(const char* str, const char* reject);
char* strerror(int errnum);
int strerror_r(int errnum, char* buf, size_t buflen);
size_t strxfrm(char* dest, const char* src, size_t num);
int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, size_t n);

double strtod(const char* str, char** endptr);
float strtof(const char* str, char** endptr);
long strtol(const char* str, char** endptr, int base);
unsigned long strtoul(const char* str, char** endptr, int base);
long long strtoll(const char* str, char** endptr, int base);
unsigned long long strtoull(const char* str, char** endptr, int base);
int atoi(const char* str);
long atol(const char* str);
long long atoll(const char* str);

int sscanf(const char* str, const char* format, ...);
int vsscanf(const char* str, const char* format, va_list ap);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vsprintf(char* str, const char* format, va_list ap);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
void reset_heap(void);
char* strdup(const char* str);
char* strndup(const char* str, size_t n);

int abs(int n);
long labs(long n);
long long llabs(long long n);
typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

void abort(void);
void exit(int status);
void _exit(int status);
int atexit(void (*function)(void));
int on_exit(void (*function)(int, void*), void* arg);
char* getenv(const char* name);
int system(const char* command);
int putenv(char* string);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);

typedef long time_t;
typedef long clock_t;

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

time_t time(time_t* t);
clock_t clock(void);
double difftime(time_t time1, time_t time2);
time_t mktime(struct tm* tm);
time_t mkutime(struct tm* tm);
char* asctime(const struct tm* tm);
char* ctime(const time_t* timep);

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct tm* gmtime(const time_t* timep);
struct tm* localtime(const time_t* timep);
size_t strftime(char* s, size_t max, const char* format, const struct tm* tm);

int rename(const char* oldpath, const char* newpath);
int remove(const char* filename);
int renameat(int oldfd, const char* oldpath, int newfd, const char* newpath);
char* tmpnam(char* s);
char* tmpnam_r(char* s);

#define BUFSIZ 512
#define EOF (-1)
#define FILENAME_MAX 256
#define FOPEN_MAX 16
#define _IONBF 0
#define _IOFBF 1
#define _IOLBF 2
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define TMP_MAX 1000

typedef long off_t;
typedef long fpos_t;

typedef struct _FILE {
    unsigned char* buffer;
    size_t buffer_size;
    size_t buffer_pos;
    size_t buffer_len;
    int eof;
    int error;
    int mode;
    unsigned int cookie;
} FILE;

typedef struct {
    void* read_fn;
    void* write_fn;
    void* seek_fn;
    void* close_fn;
} cookie_io_functions_t;

FILE* fopen(const char* filename, const char* mode);
FILE* freopen(const char* filename, const char* mode, FILE* stream);
FILE* fopencookie(void* cookie, const char* mode, cookie_io_functions_t io_funcs);
int fclose(FILE* stream);
int fflush(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
off_t ftello(FILE* stream);
void rewind(FILE* stream);
int fgetpos(FILE* stream, fpos_t* pos);
int fsetpos(FILE* stream, const fpos_t* pos);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fgetc(FILE* stream);
char* fgets(char* s, int size, FILE* stream);
int fputc(int c, FILE* stream);
int fputs(const char* s, FILE* stream);
int getc(FILE* stream);
int getchar(void);
char* gets(char* s);
int putc(int c, FILE* stream);
int putchar(int c);
int puts(const char* s);
int ungetc(int c, FILE* stream);
void clearerr(FILE* stream);
void setbuf(FILE* stream, char* buf);
int setvbuf(FILE* stream, char* buf, int mode, size_t size);
int fprintf(FILE* stream, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list ap);
int fscanf(FILE* stream, const char* format, ...);
int vfscanf(FILE* stream, const char* format, va_list ap);
int printf(const char* format, ...);
int vprintf(const char* format, va_list ap);
int scanf(const char* format, ...);
int vscanf(const char* format, va_list ap);

int iscntrl(int c);
int isspace(int c);
int isupper(int c);
int islower(int c);
int isalpha(int c);
int isdigit(int c);
int isxdigit(int c);
int isalnum(int c);
int ispunct(int c);
int isgraph(int c);
int isprint(int c);
int toupper(int c);
int tolower(int c);
int _toupper(int c);
int _tolower(int c);

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double exp(double x);
double log(double x);
double log10(double x);
double log2(double x);
double sqrt(double x);
double pow(double x, double y);
double frexp(double x, int* exp);
double ldexp(double x, int exp);
double modf(double x, double* intpart);
double fmod(double x, double y);
double ceil(double x);
double floor(double x);
double fabs(double x);
double hypot(double x, double y);
double expm1(double x);
double log1p(double x);

int __errno_location(void);
void* __memcpy_chk(void* dest, const void* src, size_t len, size_t slen);
void* __memmove_chk(void* dest, const void* src, size_t len, size_t slen);
void* __mempcpy(void* dest, const void* src, size_t len);
int __snprintf_chk(char* str, size_t size, int flag, size_t slen, const char* format, ...);
int __vsnprintf_chk(char* str, size_t size, int flag, size_t slen, const char* format, va_list ap);
size_t __strlcpy(char* dest, const char* src, size_t size);
size_t __strlcat(char* dest, const char* src, size_t size);
int __sprintf_chk(char* str, size_t size, int flag, size_t slen, const char* format, ...);
int __vsprintf_chk(char* str, size_t size, int flag, size_t slen, const char* format, va_list ap);

void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function);

#ifdef __cplusplus
}
#endif
