#ifndef DOOM_AV_COMPAT_MATH_H
#define DOOM_AV_COMPAT_MATH_H

#define M_E 2.71828182845904523536
#define M_LOG2E 1.44269504088896340736
#define M_LOG10E 0.43429448190325182765
#define M_LN2 0.69314718055994530942
#define M_LN10 2.30258509299404568402
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#define M_1_PI 0.31830988618379067154
#define M_2_PI 0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2 1.41421356237309504880
#define M_SQRT1_2 0.70710678118654752440

#define HUGE_VAL (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define HUGE_VALL (__builtin_huge_vall())
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))

static inline double fabs(double X)
{
	return X < 0.0 ? -X : X;
}

static inline float fabsf(float X)
{
	return X < 0.0f ? -X : X;
}

static inline long double fabsl(long double X)
{
	return X < 0.0L ? -X : X;
}

static inline double floor(double X)
{
	long long I = (long long)X;
	if ((double)I > X)
		I--;
	return (double)I;
}

static inline float floorf(float X)
{
	int I = (int)X;
	if ((float)I > X)
		I--;
	return (float)I;
}

static inline double ceil(double X)
{
	long long I = (long long)X;
	if ((double)I < X)
		I++;
	return (double)I;
}

static inline float ceilf(float X)
{
	int I = (int)X;
	if ((float)I < X)
		I++;
	return (float)I;
}

static inline double trunc(double X)
{
	return (double)((long long)X);
}

static inline float truncf(float X)
{
	return (float)((int)X);
}

static inline double round(double X)
{
	return X >= 0.0 ? floor(X + 0.5) : ceil(X - 0.5);
}

static inline float roundf(float X)
{
	return X >= 0.0f ? floorf(X + 0.5f) : ceilf(X - 0.5f);
}

static inline double sqrt(double X)
{
	if (X <= 0.0)
		return 0.0;

	double Guess = X;
	for (int I = 0; I < 16; I++)
		Guess = 0.5 * (Guess + X / Guess);
	return Guess;
}

static inline float sqrtf(float X)
{
	if (X <= 0.0f)
		return 0.0f;

	float Guess = X;
	for (int I = 0; I < 12; I++)
		Guess = 0.5f * (Guess + X / Guess);
	return Guess;
}

static inline double pow(double X, double Y)
{
	int E = (int)Y;
	if ((double)E != Y)
		return 1.0;

	double Result = 1.0;
	int Negative = 0;

	if (E < 0) {
		Negative = 1;
		E = -E;
	}

	while (E) {
		if (E & 1)
			Result *= X;
		X *= X;
		E >>= 1;
	}

	return Negative ? 1.0 / Result : Result;
}

static inline float powf(float X, float Y)
{
	return (float)pow((double)X, (double)Y);
}

static inline int isnan(double X)
{
	return X != X;
}

static inline int isinf(double X)
{
	return X == INFINITY || X == -INFINITY;
}

static inline int isfinite(double X)
{
	return !isnan(X) && !isinf(X);
}

#endif
