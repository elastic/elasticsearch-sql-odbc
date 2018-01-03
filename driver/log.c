/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "log.h"

/*
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/strerror-s-strerror-s-wcserror-s-wcserror-s :
 * """
 * Your string message can be, at most, 94 characters long.
 * 
 * """
 */
#define LOG_EBUF_LEN	128
#define LOG_BUF_LEN		1024
/* TODO: make settable */
#define LOG_PATH		"C:\\Users\\bpi\\Temp\\mylog.txt"
int _esodbc_log_level = LOG_LEVEL_DBG;
static FILE *fp = NULL;

#if 0
static inline void log_file(int lvl, const char *fmt, va_list args)
{
	int ret, pos;
	char buff[LOG_BUF_LEN];
	time_t now = time(NULL);

	if (! fp) {
#ifdef _WIN32
		if (fopen_s(&fp, LOG_PATH, "a+"))
#else
		if (! (fp = fopen(LOG_PATH, "a+")))
#endif
			return;
	}

	/* FIXME: 4!WINx */
	if (ctime_s(buff, sizeof(buff), &now)) {
		/* writing failed */
		pos = 0;
	} else {
		/*
		 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/ctime-s-ctime32-s-ctime64-s-wctime-s-wctime32-s-wctime64-s : 
		 * """
		 *  The return value string contains exactly 26 characters and has the
		 *  form: Wed Jan 02 02:03:55 1980\n\0
		 * """
		 * Position on '\n'.
		 */
		pos = 24;
		if ((ret = snprintf(buff + pos, sizeof(buff) - pos, " - ")) < 0)
			/* 'should not happen' (R) */
			return;
		else
			pos += ret;
		if (sizeof(buff) <= pos)
			/* likely a BUG, too low LOG_BUF_LEN? */
			return;
	}

	ret = vsnprintf(buff + pos, sizeof(buff) - pos, fmt, args);
	if (0 <= ret) {
		pos += ret;
		/* if overrunning, correct the pos, to be able to add a \n\0 */
		if (sizeof(buff) < pos + /*\n\0*/2)
			pos = sizeof(buff) - 2;
		ret = snprintf(buff + pos, sizeof(buff) - pos, "\n");
		if (0 <= ret)
			pos += ret;
	}

	assert(pos <= sizeof(buff));
	/* TODO: thread-safety 4!WINx */
	if (fwrite(buff, sizeof(buff[0]), pos, fp) < 0) {
		fclose(fp);
		fp = NULL;
	}
}
#else


static inline void log_file(int level, int werrno, const char *func, 
		const char *srcfile, int lineno, const char *fmt, va_list args)
{
	time_t now = time(NULL);
	int ret;
	size_t pos;
	char buff[LOG_BUF_LEN];
	char ebuff[LOG_EBUF_LEN];
	/* keep in sync with esodbc_log_levels */
	static const char* level2str[] = { "ERROR", "WARN", "INFO", "DEBUG", };
	assert(level < sizeof(level2str)/sizeof(char *));

	if (! fp) {
#ifdef _WIN32
		if (fopen_s(&fp, LOG_PATH, "a+"))
#else
		if (! (fp = fopen(LOG_PATH, "a+")))
#endif
			return;
	}

	/* FIXME: 4!WINx */
	if (ctime_s(buff, sizeof(buff), &now)) {
		/* writing failed */
		pos = 0;
	} else {
		pos = strnlen(buff, sizeof(buff)) - /*\n*/1;
		/*
		 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/ctime-s-ctime32-s-ctime64-s-wctime-s-wctime32-s-wctime64-s : 
		 * """
		 *  The return value string contains exactly 26 characters and has the
		 *  form: Wed Jan 02 02:03:55 1980\n\0
		 * """
		 * Position on '\n'.
		 */
		assert(pos == 24);
	}

	/* write the debugging prefix */
	/* XXX: add release (file o.a.) printing? */
	if ((ret = snprintf(buff + pos, sizeof(buff) - pos, " - [%s] %s()@%s:%d ", 
					level2str[level], func, srcfile, lineno)) < 0)
		return;
	else
		pos += ret;

	/* write the error number details */
	if (werrno) {
		ret = snprintf(buff + pos, sizeof(buff) - pos, "(%d:", errno);
		if (ret < 0)
			return;
		else
			pos += ret;

		/* FIXME: 4!WINx */
		if (strerror_s(ebuff, sizeof(ebuff), errno))
			return;
		ret = snprintf(buff + pos, sizeof(buff) - pos, "%s) ", ebuff);
		if (ret < 0)
			return;
		else
			pos += ret;
	}

	/* write user's message */
	ret = vsnprintf(buff + pos, sizeof(buff) - pos, fmt, args);
	if (ret < 0)
		return;
	else
		pos += ret;

	/* if overrunning, correct the pos, to be able to add a \n\0 */
	if (sizeof(buff) < pos + /*\n\0*/2)
		pos = sizeof(buff) - 2;
	ret = snprintf(buff + pos, sizeof(buff) - pos, "\n");
	if (0 <= ret)
		pos += ret;
	assert(pos <= sizeof(buff));

	/* write the buffer to file */
	/* TODO: thread-safety 4!WINx */
	if (fwrite(buff, sizeof(buff[0]), pos, fp) < 0) {
		fclose(fp);
		fp = NULL;
	}
}
#endif

void _esodbc_log(int lvl, int werrno, const char *func, 
		const char *srcfile, int lineno, const char *fmt, ...)
{
	va_list args;
    va_start(args, fmt);
	log_file(lvl, werrno, func, srcfile, lineno, fmt, args);
	va_end(args);
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
