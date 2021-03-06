#include <types.h>
#include <err.h>
#include <stdarg.h>
#include <string.h>

  bool
isspace(char c)
{
  switch (c) {
    case ' ':
    case '\n':
    case '\t':
      return true;
    default:
      return false;
  }
}

  bool
strncmp(const char *s1, const char *s2, size_t len)
{
  while (len-- > 0) {
    if (*s1 == 0 && *s2 == 0) {
      return true;
    } else if (*s1++ != *s2++) {
      return false;
    }
  }

  return true;
}

  bool
strcmp(const char *s1, const char *s2)
{
  size_t l1, l2;

  l1 = strlen(s1);
  l2 = strlen(s2);

  if (l1 == l2) {
    return strncmp(s1, s2, l1);
  } else {
    return false;
  }
}

  size_t
strlen(const char *s)
{
  size_t len = 0;

  while (*s++)
    len++;

  return len;
}

  size_t
strlcpy(char *dest, const char *src, size_t max)
{
  size_t i;

  for (i = 0; i < max - 1 && src[i] != 0; i++) {
    dest[i] = src[i];
  }

  dest[i] = 0;
  return i + 1;
}

  static size_t
printint(char *str, size_t max, uint64_t i, unsigned int base)
{
  unsigned char s[64];
  uint64_t d;
  int c = 0;

  do {
    d = i / base;
    i = i % base;
    if (i > 9) s[c++] = 'a' + (i-10);
    else s[c++] = '0' + i;
    i = d;
  } while (i > 0);

  d = 0;
  while (c > 0 && d < max) {
    str[d++] = s[--c];
  }

  return d;
}

  size_t
vsnprintf(char *str, size_t max, const char *fmt, va_list ap)
{
  uint64_t ind, u;
  char *s;
  int i;

  ind = 0;
  while (*fmt != 0 && ind < max) {
    if (*fmt != '%') {
      str[ind++] = *fmt++;
      continue;
    }

    fmt++;
    switch (*fmt) {
      case '%':
        str[ind++] = '%';
        break;
      case 'i':
        i = va_arg(ap, int32_t);
        if (i < 0) {
          str[ind++] = '-';
          i = -i;
        }

        if (ind >= max)
          break;

        ind += printint(str + ind, max - ind,
            i, 10);		
        break;
      case 'u':
        u = va_arg(ap, uint32_t);
        ind += printint(str + ind, max - ind,
            u, 10);
        break;
      case 'x':
        u = va_arg(ap, uint32_t);
        ind += printint(str + ind, max - ind, 
            u, 16);
        break;
      case 'b':
        u = va_arg(ap, uint32_t);
        ind += printint(str + ind, max - ind, 
            u, 2);
        break;
      case 'c':
        i = va_arg(ap, char);
        str[ind++] = i;
        break;
      case 's':
        s = va_arg(ap, char *);
        if (s == nil) {
          s = "(null)";
        }

        for (i = 0; ind < max && s[i]; i++)
          str[ind++] = s[i];
        break;
    }

    fmt++;
  }

  str[ind] = 0;
  return ind;
}

  size_t
snprintf(char *str, size_t max, const char *fmt, ...)
{
  int i;
  va_list ap;

  va_start(ap, fmt);
  i = vsnprintf(str, max, fmt, ap);
  va_end(ap);
  return i;
}

  char *
strtok(char *nstr, const char *sep)
{
  static char *str;
  int i, seplen;

  if (nstr != nil) {
    str = nstr;
  } else if (str == nil) {
    return nil;
  }

  nstr = str;
  seplen = strlen(sep);

  i = 0;
  while (str[i]) {
    if (strncmp(&str[i], sep, seplen)) {
      break;
    } else {
      i++;
    }
  }

  if (str[i]) {
    str[i++] = 0;
    while (str[i]) {
      if (!strncmp(&str[i], sep, seplen)) {
        break;
      } else {
        i += seplen;
      }
    }

    str = &str[i];
  } else {
    str = nil;
  }

  return nstr;
}
