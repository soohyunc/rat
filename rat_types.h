#ifndef _rat_types_h_
#define _rat_types_h_

typedef unsigned char	byte;

/*
 * the definitions below are valid for 32-bit architectures and will have to
 * be adjusted for 16- or 64-bit architectures
 */
typedef u_char  u_int8;
typedef u_short u_int16;
typedef u_long  u_int32;
typedef char	int8;
typedef short	int16;
typedef long	int32;

#ifdef WIN32
typedef __int64   int64;
#else
typedef long long int64;
#endif

typedef short  sample;

#endif /* _rat_types_h_ */
