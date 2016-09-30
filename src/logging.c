#include <stdio.h>
#include <stdarg.h>

#define LOGLEVEL 2
#define LVL_DEBUG 5
#define LVL_INFO 4
#define LVL_WARNING 3
#define LVL_ERROR 2
#define LVL_CRITICAL 1

void logdebug(const char *msg)
{
	if (LOGLEVEL >= LVL_DEBUG) {
		fprintf(stderr, "DEBUG:    %s\n", msg);
	}
}

void loginfo(const char *msg)
{
	if (LOGLEVEL >= LVL_INFO) {
		fprintf(stderr, "INFO:     %s\n", msg);
	}
}

void logwarning(const char *msg)
{
	if (LOGLEVEL >= LVL_WARNING) {
		fprintf(stderr, "WARNING:  %s\n", msg);
	}
}

void logerror(const char *msg)
{
	if (LOGLEVEL >= LVL_ERROR) {
		fprintf(stderr, "ERROR:    %s\n", msg);
	}
}

void logcritical(const char *msg)
{
	if (LOGLEVEL >= LVL_CRITICAL) {
		fprintf(stderr, "CRITICAL: %s\n", msg);
	}
}

void flogdebug(const char *fmt, ...)
{
	char buff[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buff, fmt, args);
	va_end(args);
	logdebug((const char *)buff);
}

void floginfo(const char *fmt, ...)
{
	char buff[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buff, fmt, args);
	va_end(args);
	loginfo((const char *)buff);
}

void flogwarning(const char *fmt, ...)
{
	char buff[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buff, fmt, args);
	va_end(args);
	logwarning((const char *)buff);
}

void flogerror(const char *fmt, ...)
{
	char buff[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buff, fmt, args);
	va_end(args);
	logerror((const char *)buff);
}

void flogcritical(const char *fmt, ...)
{
	char buff[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buff, fmt, args);
	va_end(args);
	logcritical((const char *)buff);
}
