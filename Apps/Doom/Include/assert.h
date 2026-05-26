#ifndef AV_ASSERT_H
#define AV_ASSERT_H
#include <stdlib.h>
#define assert(expr) \
	do {             \
		if (!(expr)) \
			abort(); \
	} while (0)
#endif
