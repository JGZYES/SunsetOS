#include "libc.h"
#include "vga.h"

extern "C" {

constexpr size_t HEAP_SIZE = 4 * 1024 * 1024;
static uint8_t heap[HEAP_SIZE];
static size_t heap_ptr = 0;

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) *p++ = (unsigned char)value;
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (num--) *d++ = *s++;
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const unsigned char* p1 = (const unsigned char*)ptr1;
    const unsigned char* p2 = (const unsigned char*)ptr2;
    while (num--) if (*p1++ != *p2++) return p1[-1] - p2[-1];
    return 0;
}

void* memchr(const void* ptr, int value, size_t num) {
    const unsigned char* p = (const unsigned char*)ptr;
    unsigned char v = (unsigned char)value;
    while (num--) if (*p++ == v) return (void*)(p - 1);
    return nullptr;
}

void* memmove(void* dest, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) while (num--) *d++ = *s++;
    else { d += num; s += num; while (num--) *--d = *--s; }
    return dest;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

char* strncpy(char* dest, const char* src, size_t num) {
    char* d = dest;
    while (num-- && (*d++ = *src++) != '\0');
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++) != '\0');
    return dest;
}

char* strncat(char* dest, const char* src, size_t num) {
    char* d = dest;
    while (*d) d++;
    while (num-- && (*d = *src++)) d++;
    *d = '\0';
    return dest;
}

size_t strlen(const char* str) {
    const char* s = str;
    while (*s) s++;
    return s - str;
}

size_t strnlen(const char* str, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && str[len]) len++;
    return len;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) { str1++; str2++; }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

int strncmp(const char* str1, const char* str2, size_t num) {
    while (num-- && *str1 && (*str1 == *str2)) { str1++; str2++; }
    if (num == (size_t)-1) return 0;
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

int strcoll(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}

char* strchr(const char* str, int c) {
    while (*str) if (*str++ == c) return (char*)(str - 1);
    return nullptr;
}

char* strrchr(const char* str, int c) {
    const char* last = nullptr;
    while (*str) if (*str++ == c) last = str - 1;
    return (char*)last;
}

char* strpbrk(const char* str, const char* accept) {
    while (*str) {
        const char* a = accept;
        while (*a) if (*str == *a++) return (char*)str;
        str++;
    }
    return nullptr;
}

char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return nullptr;
    size_t needle_len = 0;
    while (needle[needle_len]) needle_len++;
    if (needle_len == 0) return (char*)haystack;
    while (*haystack) {
        size_t i = 0;
        while (i < needle_len && haystack[i] == needle[i]) i++;
        if (i == needle_len) return (char*)haystack;
        haystack++;
    }
    return nullptr;
}

static char* strtok_state = nullptr;

char* strtok(char* str, const char* delim) {
    if (str == nullptr) str = strtok_state;
    if (str == nullptr) return nullptr;
    while (*str) {
        const char* d = delim;
        while (*d) if (*str == *d++) { str++; break; }
        if (!*d) break;
    }
    if (*str == '\0') { strtok_state = nullptr; return nullptr; }
    char* start = str;
    while (*str) {
        const char* d = delim;
        while (*d) if (*str == *d++) { *str = '\0'; strtok_state = str + 1; return start; }
        str++;
    }
    strtok_state = nullptr;
    return start;
}

size_t strspn(const char* str, const char* accept) {
    size_t count = 0;
    while (*str && strchr(accept, *str)) { count++; str++; }
    return count;
}

size_t strcspn(const char* str, const char* reject) {
    size_t count = 0;
    while (*str && !strchr(reject, *str)) { count++; str++; }
    return count;
}

char* strerror(int errnum) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "Error %d", errnum);
    return buf;
}

int strerror_r(int errnum, char* buf, size_t buflen) {
    if (buflen == 0) return -1;
    snprintf(buf, buflen, "Error %d", errnum);
    return 0;
}

size_t strxfrm(char* dest, const char* src, size_t num) {
    size_t len = strlen(src);
    if (num > 0) {
        size_t copy_len = len < num - 1 ? len : num - 1;
        memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';
    }
    return len;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1 >= 'a' && *s1 <= 'z' ? *s1 - 32 : *s1;
        char c2 = *s2 >= 'a' && *s2 <= 'z' ? *s2 - 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return *s1 - *s2;
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    while (n-- && *s1 && *s2) {
        char c1 = *s1 >= 'a' && *s1 <= 'z' ? *s1 - 32 : *s1;
        char c2 = *s2 >= 'a' && *s2 <= 'z' ? *s2 - 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    if (n == (size_t)-1) return 0;
    return *s1 - *s2;
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

double strtod(const char* str, char** endptr) {
    if (endptr) *endptr = (char*)str;
    if (!str || !*str) return 0.0;
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    bool negative = false;
    if (*str == '-') { negative = true; str++; }
    else if (*str == '+') str++;
    double result = 0.0;
    while (is_digit(*str)) { result = result * 10 + (*str - '0'); str++; }
    if (*str == '.') {
        str++;
        double fraction = 1.0;
        while (is_digit(*str)) { fraction /= 10; result += (*str - '0') * fraction; str++; }
    }
    if (*str == 'e' || *str == 'E') {
        str++;
        int exp_sign = 1;
        if (*str == '-') { exp_sign = -1; str++; }
        else if (*str == '+') str++;
        int exp_val = 0;
        while (is_digit(*str)) { exp_val = exp_val * 10 + (*str - '0'); str++; }
        for (int i = 0; i < exp_val; i++) result *= (exp_sign > 0) ? 10 : 0.1;
    }
    if (endptr) *endptr = (char*)str;
    return negative ? -result : result;
}

float strtof(const char* str, char** endptr) {
    return (float)strtod(str, endptr);
}

long strtol(const char* str, char** endptr, int base) {
    if (endptr) *endptr = (char*)str;
    if (!str || !*str) return 0;
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    bool negative = false;
    if (*str == '-') { negative = true; str++; }
    long result = 0;
    while (*str) {
        char c = *str;
        int val = -1;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'a' && c <= 'z') val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') val = c - 'A' + 10;
        if (val < 0 || val >= base) break;
        result = result * base + val;
        str++;
    }
    if (endptr) *endptr = (char*)str;
    return negative ? -result : result;
}

unsigned long strtoul(const char* str, char** endptr, int base) {
    return (unsigned long)strtol(str, endptr, base);
}

long long strtoll(const char* str, char** endptr, int base) {
    return strtol(str, endptr, base);
}

unsigned long long strtoull(const char* str, char** endptr, int base) {
    return (unsigned long long)strtol(str, endptr, base);
}

int atoi(const char* str) {
    return (int)strtol(str, nullptr, 10);
}

long atol(const char* str) {
    return strtol(str, nullptr, 10);
}

long long atoll(const char* str) {
    return strtoll(str, nullptr, 10);
}

static const char* parse_format_int(const char* format, int* width) {
    *width = 0;
    while (*format >= '0' && *format <= '9') {
        *width = *width * 10 + (*format - '0');
        format++;
    }
    return format;
}

int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsprintf(str, format, args);
    va_end(args);
    return result;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

int vsprintf(char* str, const char* format, va_list ap) {
    return vsnprintf(str, 0x7FFFFFFF, format, ap);
}

static void put_char(char** dest, size_t* remaining, char c) {
    if (remaining && *remaining > 0) {
        **dest = c;
        (*dest)++;
        (*remaining)--;
    } else if (!remaining) {
        **dest = c;
        (*dest)++;
    }
}

static void put_string(char** dest, size_t* remaining, const char* s) {
    while (*s) {
        put_char(dest, remaining, *s);
        s++;
    }
}

static void put_int(char** dest, size_t* remaining, long value, int width, int base, bool negative) {
    char buf[32];
    int len = 0;
    if (value == 0) {
        buf[len++] = '0';
    } else {
        unsigned long v = negative ? -(unsigned long)value : (unsigned long)value;
        while (v > 0) {
            int d = v % base;
            buf[len++] = d < 10 ? '0' + d : 'a' + d - 10;
            v /= base;
        }
        if (negative) buf[len++] = '-';
    }
    for (int i = 0; i < width - len; i++) put_char(dest, remaining, ' ');
    for (int i = len - 1; i >= 0; i--) put_char(dest, remaining, buf[i]);
}

static void put_double(char** dest, size_t* remaining, double value, int prec) {
    if (prec < 0) prec = 6;
    if (value < 0) { put_char(dest, remaining, '-'); value = -value; }
    double intpart;
    double frac = modf(value, &intpart);
    put_int(dest, remaining, (long)intpart, 0, 10, false);
    if (prec > 0) {
        put_char(dest, remaining, '.');
        for (int i = 0; i < prec; i++) {
            frac *= 10;
            int d = (int)frac;
            put_char(dest, remaining, '0' + d);
            frac -= d;
        }
    }
}

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    char* dest = str;
    size_t remaining = size;
    while (*format) {
        if (*format != '%') {
            put_char(&dest, size ? &remaining : nullptr, *format++);
            continue;
        }
        format++;
        int width = 0;
        int prec = -1;
        format = parse_format_int(format, &width);
        if (*format == '.') {
            format++;
            format = parse_format_int(format, &prec);
        }
        switch (*format) {
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                put_string(&dest, size ? &remaining : nullptr, s);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                put_char(&dest, size ? &remaining : nullptr, c);
                break;
            }
            case 'd':
            case 'i': {
                int i = va_arg(ap, int);
                put_int(&dest, size ? &remaining : nullptr, i, width, 10, i < 0);
                break;
            }
            case 'u': {
                unsigned int u = va_arg(ap, unsigned int);
                put_int(&dest, size ? &remaining : nullptr, u, width, 10, false);
                break;
            }
            case 'x':
            case 'X': {
                unsigned int u = va_arg(ap, unsigned int);
                put_int(&dest, size ? &remaining : nullptr, u, width, 16, false);
                break;
            }
            case 'p': {
                void* p = va_arg(ap, void*);
                put_string(&dest, size ? &remaining : nullptr, "0x");
                put_int(&dest, size ? &remaining : nullptr, (unsigned long)p, 8, 16, false);
                break;
            }
            case 'f': {
                double d = va_arg(ap, double);
                put_double(&dest, size ? &remaining : nullptr, d, prec);
                break;
            }
            case '%':
                put_char(&dest, size ? &remaining : nullptr, '%');
                break;
            default:
                put_char(&dest, size ? &remaining : nullptr, *format);
                break;
        }
        if (*format) format++;
    }
    if (size && remaining > 0) *dest = '\0';
    return dest - str;
}

int sscanf(const char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsscanf(str, format, args);
    va_end(args);
    return result;
}

int vsscanf(const char* str, const char* format, va_list ap) {
    int count = 0;
    while (*format && *str) {
        if (*format == '%') {
            format++;
            if (*format == '%') { if (*str != '%') break; str++; format++; continue; }
            int width = 0;
            while (*format >= '0' && *format <= '9') { width = width * 10 + (*format - '0'); format++; }
            if (width == 0) width = 100;
            bool suppress = false;
            if (*format == '*') { suppress = true; format++; }
            char type = *format++;
            const char* start = str;
            while (*str == ' ' || *str == '\t' || *str == '\n') str++;
            if (type == 's') {
                char** dest = va_arg(ap, char**);
                char* p = (char*)str;
                while (*p && *p != ' ' && *p != '\t' && *p != '\n' && width--) p++;
                if (!suppress) {
                    size_t len = p - str;
                    *dest = (char*)malloc(len + 1);
                    for (size_t i = 0; i < len; i++) (*dest)[i] = str[i];
                    (*dest)[len] = '\0';
                    count++;
                }
                str = p;
            } else if (type == 'd') {
                int* dest = va_arg(ap, int*);
                int sign = 1;
                if (*str == '-') { sign = -1; str++; }
                int val = 0;
                while (*str >= '0' && *str <= '9' && width--) { val = val * 10 + (*str - '0'); str++; }
                if (!suppress && str > start) { *dest = val * sign; count++; }
            } else if (type == 'u') {
                unsigned int* dest = va_arg(ap, unsigned int*);
                unsigned int val = 0;
                while (*str >= '0' && *str <= '9' && width--) { val = val * 10 + (*str - '0'); str++; }
                if (!suppress && str > start) { *dest = val; count++; }
            } else if (type == 'c') {
                char** dest = va_arg(ap, char**);
                if (!suppress) { *dest = (char*)str; count++; }
                str += width > 0 ? width : 1;
            }
        } else if (*format == ' ' || *format == '\t' || *format == '\n') {
            while (*str == ' ' || *str == '\t' || *str == '\n') { str++; format++; }
        } else if (*format == *str) {
            str++; format++;
        } else {
            break;
        }
    }
    return count;
}

void* malloc(size_t size) {
    if (size == 0) return nullptr;
    size = (size + 7) & ~7;
    if (heap_ptr + size > HEAP_SIZE) return nullptr;
    void* ptr = &heap[heap_ptr];
    heap_ptr += size;
    return ptr;
}

void* calloc(size_t nmemb, size_t size) {
    void* ptr = malloc(nmemb * size);
    if (ptr) memset(ptr, 0, nmemb * size);
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    (void)ptr;
    return malloc(size);
}

void free(void* ptr) {
    (void)ptr;
}

void reset_heap(void) {
    heap_ptr = 0;
}

char* strdup(const char* str) {
    size_t len = strlen(str) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, str, len);
    return copy;
}

char* strndup(const char* str, size_t n) {
    size_t len = strnlen(str, n);
    char* copy = (char*)malloc(len + 1);
    if (copy) { memcpy(copy, str, len); copy[len] = '\0'; }
    return copy;
}

int abs(int n) { return n < 0 ? -n : n; }
long labs(long n) { return n < 0 ? -n : n; }
long long llabs(long long n) { return n < 0 ? -n : n; }
div_t div(int numer, int denom) { div_t d; d.quot = numer / denom; d.rem = numer % denom; return d; }
ldiv_t ldiv(long numer, long denom) { ldiv_t d; d.quot = numer / denom; d.rem = numer % denom; return d; }
lldiv_t lldiv(long long numer, long long denom) { lldiv_t d; d.quot = numer / denom; d.rem = numer % denom; return d; }

void abort(void) { while (1); }
int atexit(void (*function)(void)) { (void)function; return 0; }
char* getenv(const char* name) { (void)name; return nullptr; }
int system(const char* command) { (void)command; return 0; }
int putenv(char* string) { (void)string; return -1; }
int setenv(const char* name, const char* value, int overwrite) { (void)name; (void)value; (void)overwrite; return -1; }
int unsetenv(const char* name) { (void)name; return -1; }

time_t time(time_t* t) { if (t) *t = 0; return 0; }
clock_t clock(void) { return 0; }
double difftime(time_t time1, time_t time2) { return (double)(time1 - time2); }
time_t mktime(struct tm* tm) { (void)tm; return 0; }
time_t mkutime(struct tm* tm) { (void)tm; return 0; }

static const char* wday_name[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* mon_name[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char* asctime(const struct tm* tm) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%s %s %d %02d:%02d:%02d 1970\n",
             wday_name[tm->tm_wday], mon_name[tm->tm_mon], tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    return buf;
}

char* ctime(const time_t* timep) { return asctime(localtime(timep)); }

struct tm* gmtime(const time_t* timep) {
    static struct tm tm;
    (void)timep;
    tm.tm_sec = 0; tm.tm_min = 0; tm.tm_hour = 0;
    tm.tm_mday = 1; tm.tm_mon = 0; tm.tm_year = 70;
    tm.tm_wday = 4; tm.tm_yday = 0; tm.tm_isdst = 0;
    return &tm;
}

struct tm* localtime(const time_t* timep) { return gmtime(timep); }

size_t strftime(char* s, size_t max, const char* format, const struct tm* tm) {
    (void)tm;
    return snprintf(s, max, "%04d-%02d-%02d %02d:%02d:%02d",
                    1970, 1, 1, 0, 0, 0);
}

int rename(const char* oldpath, const char* newpath) {
    (void)oldpath; (void)newpath;
    return -1;
}

int remove(const char* filename) {
    (void)filename;
    return -1;
}

int renameat(int oldfd, const char* oldpath, int newfd, const char* newpath) {
    (void)oldfd; (void)oldpath; (void)newfd; (void)newpath;
    return -1;
}

FILE* tmpfile(void) { return nullptr; }
char* tmpnam(char* s) { if (s) strcpy(s, "/tmp/XXXXXX"); return s; }
char* tmpnam_r(char* s) { return tmpnam(s); }
char* tempnam(const char* dir, const char* pfx) { (void)dir; (void)pfx; return nullptr; }

FILE* fopen(const char* filename, const char* mode) {
    (void)filename; (void)mode;
    FILE* f = (FILE*)malloc(sizeof(FILE));
    if (!f) return nullptr;
    memset(f, 0, sizeof(FILE));
    return f;
}

FILE* freopen(const char* filename, const char* mode, FILE* stream) {
    (void)filename; (void)mode; (void)stream;
    return nullptr;
}

FILE* fopencookie(void* cookie, const char* mode, cookie_io_functions_t io_funcs) {
    (void)cookie; (void)mode; (void)io_funcs;
    return nullptr;
}

int fclose(FILE* stream) {
    if (stream) free(stream);
    return 0;
}

int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

int feof(FILE* stream) { return stream ? stream->eof : 0; }
int ferror(FILE* stream) { return stream ? stream->error : 0; }
void clearerr(FILE* stream) { if (stream) { stream->eof = 0; stream->error = 0; } }

int fseek(FILE* stream, long offset, int whence) {
    (void)stream; (void)offset; (void)whence;
    return 0;
}

long ftell(FILE* stream) {
    (void)stream;
    return 0;
}

off_t ftello(FILE* stream) {
    (void)stream;
    return 0;
}

void rewind(FILE* stream) {
    (void)stream;
}

int fgetpos(FILE* stream, fpos_t* pos) {
    (void)stream; (void)pos;
    return 0;
}

int fsetpos(FILE* stream, const fpos_t* pos) {
    (void)stream; (void)pos;
    return 0;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    (void)ptr; (void)size; (void)nmemb; (void)stream;
    return 0;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    (void)ptr; (void)size; (void)nmemb; (void)stream;
    return 0;
}

int fgetc(FILE* stream) { return EOF; }
char* fgets(char* s, int size, FILE* stream) { (void)s; (void)size; (void)stream; return nullptr; }
int fputc(int c, FILE* stream) { (void)c; (void)stream; return c; }
int fputs(const char* s, FILE* stream) { (void)s; (void)stream; return 0; }
int getc(FILE* stream) { return fgetc(stream); }
int getchar(void) { return EOF; }
char* gets(char* s) { (void)s; return nullptr; }
int putc(int c, FILE* stream) { return fputc(c, stream); }
int putchar(int c) { return fputc(c, nullptr); }
int puts(const char* s) {
    while (*s) fputc(*s++, nullptr);
    fputc('\n', nullptr);
    return 0;
}
int ungetc(int c, FILE* stream) { (void)c; (void)stream; return EOF; }
void setbuf(FILE* stream, char* buf) { (void)stream; (void)buf; }
int setvbuf(FILE* stream, char* buf, int mode, size_t size) {
    (void)stream; (void)buf; (void)mode; (void)size;
    return 0;
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    for (int i = 0; i < len; i++) fputc(buf[i], stream);
    return len;
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    for (int i = 0; i < len; i++) fputc(buf[i], stream);
    return len;
}

int fscanf(FILE* stream, const char* format, ...) {
    (void)stream; (void)format;
    return 0;
}

int vfscanf(FILE* stream, const char* format, va_list ap) {
    (void)stream; (void)format; (void)ap;
    return 0;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    for (int i = 0; i < len; i++) fputc(buf[i], nullptr);
    return len;
}

int vprintf(const char* format, va_list ap) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    for (int i = 0; i < len; i++) fputc(buf[i], nullptr);
    return len;
}

int scanf(const char* format, ...) {
    (void)format;
    return 0;
}

int vscanf(const char* format, va_list ap) {
    (void)format; (void)ap;
    return 0;
}

int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isalpha(int c) { return isupper(c) || islower(c); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int isgraph(int c) { return c >= 33 && c <= 126; }
int isprint(int c) { return c >= 32 && c <= 126; }
int toupper(int c) { if (c >= 'a' && c <= 'z') return c - 32; return c; }
int tolower(int c) { if (c >= 'A' && c <= 'Z') return c + 32; return c; }
int _toupper(int c) { if (c >= 'a' && c <= 'z') return c - 32; return c; }
int _tolower(int c) { if (c >= 'A' && c <= 'Z') return c + 32; return c; }

static double pi = 3.14159265358979323846;

double sin(double x) {
    x = fmod(x, 2 * pi);
    double result = 0, term = x;
    for (int i = 1; i <= 15; i++) { result += term; term *= -x * x / ((2 * i) * (2 * i + 1)); }
    return result;
}

double cos(double x) {
    x = fmod(x, 2 * pi);
    double result = 1, term = 1;
    for (int i = 1; i <= 15; i++) { term *= -x * x / ((2 * i - 1) * (2 * i)); result += term; }
    return result;
}

double tan(double x) { double c = cos(x); if (fabs(c) < 1e-10) return 0; return sin(x) / c; }
double asin(double x) { if (x < -1) x = -1; if (x > 1) x = 1; return atan2(x, sqrt(1 - x * x)); }
double acos(double x) { return pi / 2 - asin(x); }
double atan(double x) { return atan2(x, 1.0); }

double atan2(double y, double x) {
    if (x > 0) return atan(y / x);
    if (x < 0 && y >= 0) return atan(y / x) + pi;
    if (x < 0 && y < 0) return atan(y / x) - pi;
    if (x == 0 && y > 0) return pi / 2;
    if (x == 0 && y < 0) return -pi / 2;
    return 0;
}

double sinh(double x) { return (exp(x) - exp(-x)) / 2; }
double cosh(double x) { return (exp(x) + exp(-x)) / 2; }
double tanh(double x) { double ex = exp(x), emx = exp(-x); return (ex - emx) / (ex + emx); }

double exp(double x) {
    double result = 1, term = 1;
    for (int i = 1; i <= 20; i++) { term *= x / i; result += term; }
    return result;
}

double log(double x) {
    if (x <= 0) return 0;
    double result = 0;
    while (x >= 2) { x /= 2.718281828; result += 1; }
    x -= 1;
    double term = x;
    for (int i = 2; i <= 30; i++) {
        if (i % 2 == 0) result -= term / i;
        else result += term / i;
        term *= x;
    }
    return result;
}

double log10(double x) { return log(x) / log(10); }
double log2(double x) { return log(x) / log(2); }

double sqrt(double x) {
    if (x <= 0) return 0;
    double guess = x / 2.0;
    for (int i = 0; i < 20; i++) guess = (guess + x / guess) / 2.0;
    return guess;
}

double pow(double x, double y) {
    if (y == 0) return 1;
    if (y == 1) return x;
    if (y == (int)y && y > 0 && y < 100) {
        double result = 1;
        for (int i = 0; i < (int)y; i++) result *= x;
        return result;
    }
    return exp(y * log(x));
}

double frexp(double x, int* exp) {
    if (x == 0) { *exp = 0; return 0; }
    *exp = 0;
    while (x >= 1) { x /= 2; (*exp)++; }
    while (x < 0.5) { x *= 2; (*exp)--; }
    return x;
}

double ldexp(double x, int exp) {
    while (exp > 0) { x *= 2; exp--; }
    while (exp < 0) { x /= 2; exp++; }
    return x;
}

double modf(double x, double* intpart) {
    *intpart = x >= 0 ? floor(x) : ceil(x);
    return x - *intpart;
}

double fmod(double x, double y) { return x - floor(x / y) * y; }
double ceil(double x) { return x >= 0 ? (x == (int)x ? x : (int)x + 1) : (int)x; }
double floor(double x) { return x >= 0 ? (int)x : (x == (int)x ? x : (int)x - 1); }
double fabs(double x) { return x < 0 ? -x : x; }
double hypot(double x, double y) { return sqrt(x * x + y * y); }
double expm1(double x) { return exp(x) - 1; }
double log1p(double x) { return log(1 + x); }

int __errno_location(void) { return 0; }
void* __memcpy_chk(void* dest, const void* src, size_t len, size_t slen) { (void)slen; return memcpy(dest, src, len); }
void* __memmove_chk(void* dest, const void* src, size_t len, size_t slen) { (void)slen; return memmove(dest, src, len); }
void* __mempcpy(void* dest, const void* src, size_t len) { return (char*)memcpy(dest, src, len) + len; }
int __snprintf_chk(char* str, size_t size, int flag, size_t slen, const char* format, ...) {
    (void)flag; (void)slen; va_list args; va_start(args, format); int result = vsnprintf(str, size, format, args); va_end(args); return result;
}
int __vsnprintf_chk(char* str, size_t size, int flag, size_t slen, const char* format, va_list ap) {
    (void)flag; (void)slen; return vsnprintf(str, size, format, ap);
}
size_t __strlcpy(char* dest, const char* src, size_t size) {
    size_t len = strlen(src);
    if (size > 0) { size_t copy_len = len < size - 1 ? len : size - 1; memcpy(dest, src, copy_len); dest[copy_len] = '\0'; }
    return len;
}
size_t __strlcat(char* dest, const char* src, size_t size) {
    size_t dlen = strnlen(dest, size);
    size_t slen = strlen(src);
    if (dlen < size) { size_t copy_len = slen < size - dlen - 1 ? slen : size - dlen - 1; memcpy(dest + dlen, src, copy_len); dest[dlen + copy_len] = '\0'; }
    return dlen + slen;
}
int __sprintf_chk(char* str, size_t size, int flag, size_t slen, const char* format, ...) {
    (void)flag; (void)slen; va_list args; va_start(args, format); int result = vsnprintf(str, size, format, args); va_end(args); return result;
}
int __vsprintf_chk(char* str, size_t size, int flag, size_t slen, const char* format, va_list ap) {
    (void)flag; (void)slen; return vsnprintf(str, size, format, ap);
}

void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function) {
    (void)assertion; (void)file; (void)line; (void)function;
    printf("Assertion failed!\n");
    while (1);
}

typedef unsigned long long u64;
typedef unsigned int u32;

static u64 __divmoddi4_internal(u64 numerator, u64 denominator, u64* remainder) {
    if (denominator == 0) {
        if (remainder) *remainder = 0;
        return 0;
    }
    if (numerator < denominator) {
        if (remainder) *remainder = numerator;
        return 0;
    }
    u64 quotient = 0;
    u64 d = denominator;
    int shift = 0;
    while ((d & ((u64)1 << 63)) == 0 && d < numerator) {
        d <<= 1;
        shift++;
    }
    for (int i = shift; i >= 0; i--) {
        quotient <<= 1;
        if (d <= numerator) {
            numerator -= d;
            quotient |= 1;
        }
        d >>= 1;
    }
    if (remainder) *remainder = numerator;
    return quotient;
}

extern "C" {
long long __divmoddi4(long long numerator, long long denominator) {
    int neg = 0;
    if (numerator < 0) { numerator = -numerator; neg ^= 1; }
    if (denominator < 0) { denominator = -denominator; neg ^= 1; }
    u64 rem;
    __divmoddi4_internal(numerator, denominator, &rem);
    if (neg) rem = -rem;
    return rem;
}
}

void* operator new(size_t size) {
    void* ptr = malloc(size);
    return ptr;
}

void* operator new[](size_t size) {
    void* ptr = malloc(size);
    return ptr;
}

void operator delete(void* ptr) {
    free(ptr);
}

void operator delete[](void* ptr) {
    free(ptr);
}

void operator delete(void* ptr, size_t) {
    free(ptr);
}

void operator delete[](void* ptr, size_t) {
    free(ptr);
}

}
