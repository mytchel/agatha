bool
isspace(char c);

bool
strncmp(const char *s1, const char *s2, size_t len);

bool
strcmp(const char *s1, const char *s2);

size_t
strlen(const char *s);

char *
strtok(char *str, const char *sep);

size_t
strlcpy(char *dst, const char *src, size_t max);

size_t
snprintf(char *str, size_t max, const char *fmt, ...);

int
sscanf(const char *str, const char *fmt, ...);

long
strtol(const char *nptr, char **endptr, int base);

size_t
vsnprintf(char *str, size_t max, const char *fmt, va_list ap);

int
vfscanf(int fd, const char *fmt, va_list ap);

int
vsscanf(const char *str, const char *fmt, va_list ap);

