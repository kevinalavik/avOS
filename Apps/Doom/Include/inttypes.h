#ifndef AV_INTTYPES_H
#define AV_INTTYPES_H

#include <stdint.h>

typedef struct {
	intmax_t quot;
	intmax_t rem;
} imaxdiv_t;

#define PRId8 "d"
#define PRIi8 "i"
#define PRIo8 "o"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"

#define PRId16 "d"
#define PRIi16 "i"
#define PRIo16 "o"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"

#define PRId32 "d"
#define PRIi32 "i"
#define PRIo32 "o"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"

#define PRId64 "lld"
#define PRIi64 "lli"
#define PRIo64 "llo"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"

#define PRIdLEAST8 PRId8
#define PRIiLEAST8 PRIi8
#define PRIoLEAST8 PRIo8
#define PRIuLEAST8 PRIu8
#define PRIxLEAST8 PRIx8
#define PRIXLEAST8 PRIX8
#define PRIdLEAST16 PRId16
#define PRIiLEAST16 PRIi16
#define PRIoLEAST16 PRIo16
#define PRIuLEAST16 PRIu16
#define PRIxLEAST16 PRIx16
#define PRIXLEAST16 PRIX16
#define PRIdLEAST32 PRId32
#define PRIiLEAST32 PRIi32
#define PRIoLEAST32 PRIo32
#define PRIuLEAST32 PRIu32
#define PRIxLEAST32 PRIx32
#define PRIXLEAST32 PRIX32
#define PRIdLEAST64 PRId64
#define PRIiLEAST64 PRIi64
#define PRIoLEAST64 PRIo64
#define PRIuLEAST64 PRIu64
#define PRIxLEAST64 PRIx64
#define PRIXLEAST64 PRIX64

#define PRIdFAST8 PRId64
#define PRIiFAST8 PRIi64
#define PRIoFAST8 PRIo64
#define PRIuFAST8 PRIu64
#define PRIxFAST8 PRIx64
#define PRIXFAST8 PRIX64
#define PRIdFAST16 PRId64
#define PRIiFAST16 PRIi64
#define PRIoFAST16 PRIo64
#define PRIuFAST16 PRIu64
#define PRIxFAST16 PRIx64
#define PRIXFAST16 PRIX64
#define PRIdFAST32 PRId64
#define PRIiFAST32 PRIi64
#define PRIoFAST32 PRIo64
#define PRIuFAST32 PRIu64
#define PRIxFAST32 PRIx64
#define PRIXFAST32 PRIX64
#define PRIdFAST64 PRId64
#define PRIiFAST64 PRIi64
#define PRIoFAST64 PRIo64
#define PRIuFAST64 PRIu64
#define PRIxFAST64 PRIx64
#define PRIXFAST64 PRIX64

#define PRIdMAX "lld"
#define PRIiMAX "lli"
#define PRIoMAX "llo"
#define PRIuMAX "llu"
#define PRIxMAX "llx"
#define PRIXMAX "llX"

#define PRIdPTR "lld"
#define PRIiPTR "lli"
#define PRIoPTR "llo"
#define PRIuPTR "llu"
#define PRIxPTR "llx"
#define PRIXPTR "llX"

#define SCNd8 "hhd"
#define SCNi8 "hhi"
#define SCNo8 "hho"
#define SCNu8 "hhu"
#define SCNx8 "hhx"
#define SCNd16 "hd"
#define SCNi16 "hi"
#define SCNo16 "ho"
#define SCNu16 "hu"
#define SCNx16 "hx"
#define SCNd32 "d"
#define SCNi32 "i"
#define SCNo32 "o"
#define SCNu32 "u"
#define SCNx32 "x"
#define SCNd64 "lld"
#define SCNi64 "lli"
#define SCNo64 "llo"
#define SCNu64 "llu"
#define SCNx64 "llx"
#define SCNdMAX "lld"
#define SCNiMAX "lli"
#define SCNoMAX "llo"
#define SCNuMAX "llu"
#define SCNxMAX "llx"
#define SCNdPTR "lld"
#define SCNiPTR "lli"
#define SCNoPTR "llo"
#define SCNuPTR "llu"
#define SCNxPTR "llx"

intmax_t imaxabs(intmax_t N);
imaxdiv_t imaxdiv(intmax_t Numer, intmax_t Denom);
intmax_t strtoimax(const char *Nptr, char **Endptr, int Base);
uintmax_t strtoumax(const char *Nptr, char **Endptr, int Base);

#endif
