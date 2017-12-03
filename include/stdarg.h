typedef char* va_list;

#define va_start(ap, last)      (ap = (va_list) &last + sizeof(last))
#define va_end(ap)              (ap = 0)
#define va_arg(ap, type)        *((type *) ap); ap += sizeof(type)

