#ifndef AV_STDINT_H
#define AV_STDINT_H

#include <System/Types.h>

typedef S8 int8_t;
typedef U8 uint8_t;
typedef S16 int16_t;
typedef U16 uint16_t;
typedef S32 int32_t;
typedef U32 uint32_t;
typedef S64 int64_t;
typedef U64 uint64_t;

typedef S8 int_least8_t;
typedef U8 uint_least8_t;
typedef S16 int_least16_t;
typedef U16 uint_least16_t;
typedef S32 int_least32_t;
typedef U32 uint_least32_t;
typedef S64 int_least64_t;
typedef U64 uint_least64_t;

typedef S64 int_fast8_t;
typedef U64 uint_fast8_t;
typedef S64 int_fast16_t;
typedef U64 uint_fast16_t;
typedef S64 int_fast32_t;
typedef U64 uint_fast32_t;
typedef S64 int_fast64_t;
typedef U64 uint_fast64_t;

typedef S64 intptr_t;
typedef U64 uintptr_t;
typedef S64 intmax_t;
typedef U64 uintmax_t;

#define INT8_MIN (-128)
#define INT8_MAX 127
#define UINT8_MAX 0xffu
#define INT16_MIN (-32767 - 1)
#define INT16_MAX 32767
#define UINT16_MAX 0xffffu
#define INT32_MIN (-2147483647 - 1)
#define INT32_MAX 2147483647
#define UINT32_MAX 0xffffffffu
#define INT64_MIN (-9223372036854775807ll - 1ll)
#define INT64_MAX 9223372036854775807ll
#define UINT64_MAX 0xffffffffffffffffull

#define INTPTR_MIN INT64_MIN
#define INTPTR_MAX INT64_MAX
#define UINTPTR_MAX UINT64_MAX
#define INTMAX_MIN INT64_MIN
#define INTMAX_MAX INT64_MAX
#define UINTMAX_MAX UINT64_MAX

#define SIZE_MAX UINT64_MAX
#define PTRDIFF_MIN INT64_MIN
#define PTRDIFF_MAX INT64_MAX

#define INT8_C(c) c
#define UINT8_C(c) c##u
#define INT16_C(c) c
#define UINT16_C(c) c##u
#define INT32_C(c) c
#define UINT32_C(c) c##u
#define INT64_C(c) c##ll
#define UINT64_C(c) c##ull
#define INTMAX_C(c) c##ll
#define UINTMAX_C(c) c##ull

#endif
