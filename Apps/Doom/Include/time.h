#ifndef AV_TIME_H
#define AV_TIME_H
typedef long time_t;
struct tm {
	int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday,
		tm_isdst;
};
static inline time_t time(time_t *Out)
{
	if (Out)
		*Out = 0;
	return 0;
}
#endif
