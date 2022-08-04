#ifndef LIBSECP256K1_CONFIG_H
#define LIBSECP256K1_CONFIG_H

/* Define this symbol to use the native field inverse implementation */
#define USE_FIELD_INV_BUILTIN 1

/* Define this symbol to use no num implementation */
#define USE_NUM_NONE 1

/* Define this symbol to use the native scalar inverse implementation */
#define USE_SCALAR_INV_BUILTIN 1

/* Set ecmult gen precision bits */
#define ECMULT_GEN_PREC_BITS 4

/* Set window size for ecmult precomputation */
#define ECMULT_WINDOW_SIZE 15

#define	UINT32_MAX		0xFFFFFFFF

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#endif /*LIBSECP256K1_CONFIG_H*/
