typedef	signed char		      int8_t;
typedef	unsigned char		    uint8_t;
typedef	short			          int16_t;
typedef	unsigned short		  uint16_t;
typedef	int			            int32_t;
typedef	unsigned int		    uint32_t;
typedef	long long		        int64_t;
typedef	unsigned long long	uint64_t;

typedef double			        double_t;
typedef float			          float_t;
typedef long			          ptrdiff_t;
typedef	unsigned long		    size_t;
typedef	long			          ssize_t;

typedef uint64_t            u64;
typedef uint32_t            u32;
typedef uint16_t            u16;
typedef uint8_t             u8;

typedef int64_t            i64;
typedef int32_t            i32;
typedef int16_t            i16;
typedef int8_t             i8;

typedef unsigned int       uint;
typedef unsigned short     ushort;

#define nil 0

typedef uint8_t	            bool;

#define false 0
#define true 1

#define LEN(X)  (sizeof(X)/sizeof(X[0]))

#define beto16(X) (\
	((uint16_t) (((uint8_t *) X)[0]) << 8) |\
	((uint16_t) (((uint8_t *) X)[1]) << 0) )\

#define beto32(X) (\
	((uint32_t) (((uint8_t *) X)[0]) << 24) |\
	((uint32_t) (((uint8_t *) X)[1]) << 16) |\
	((uint32_t) (((uint8_t *) X)[2]) << 8) |\
	((uint32_t) (((uint8_t *) X)[3]) << 0) )

#define beto64(X) (\
	((uint64_t) (((uint8_t *) X)[0]) << 56) |\
	((uint64_t) (((uint8_t *) X)[1]) << 48) |\
	((uint64_t) (((uint8_t *) X)[2]) << 40) |\
	((uint64_t) (((uint8_t *) X)[3]) << 32) |\
	((uint64_t) (((uint8_t *) X)[4]) << 24) |\
	((uint64_t) (((uint8_t *) X)[5]) << 16) |\
	((uint64_t) (((uint8_t *) X)[6]) << 8) |\
	((uint64_t) (((uint8_t *) X)[7]) << 0) )

