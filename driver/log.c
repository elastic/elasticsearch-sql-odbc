/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "defs.h"
#include "log.h"

#ifdef _WIN32
#define FILE_PATH_SEPARATOR		'\\'
#else /* _WIN32 */
#define FILE_PATH_SEPARATOR		'/'
#endif /* _WIN32 */

#define LOG_LEVEL_SEPARATOR		'?'

/*
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/strerror-s-strerror-s-wcserror-s-wcserror-s :
 * "Your string message can be, at most, 94 characters long."
 * Size of buffer to fetch errno-to-string message.
 */
#define LOG_ERRNO_BUF_SIZE	128

/* log level; disabled by default */
int _esodbc_log_level = LOG_LEVEL_DISABLED;
/* log file path -- process variable */
static TCHAR *log_path = NULL;
/* log file mutex -- process variable */
static SRWLOCK log_mux = SRWLOCK_INIT;

#define MUTEX_LOCK(_mux)	\
	(WaitForSingleObject(_mux, INFINITE) == WAIT_OBJECT_0)
#define MUTEX_UNLOCK(_mux)	ReleaseMutex(_mux)


static inline HANDLE log_file_handle(BOOL open)
{
	static HANDLE log_handle = INVALID_HANDLE_VALUE;
	if (open) {
		if (log_handle == INVALID_HANDLE_VALUE) {
			log_handle = CreateFile(
					log_path, /* file name ("path") */
					GENERIC_WRITE, /* desired access */
					FILE_SHARE_WRITE, /* share mode */
					NULL, /* security attributes */
					OPEN_ALWAYS, /* creation disposition */
					FILE_ATTRIBUTE_NORMAL, /* flags & attributes */
					NULL /* template */);
		}
	} else {
		if (log_handle != INVALID_HANDLE_VALUE) {
			CloseHandle(log_handle);
			log_handle = INVALID_HANDLE_VALUE;
		}
	}
	return log_handle;
}

BOOL log_init()
{
	int pos;
	/*
	 * Fully qualified path name of the log file:
	 * <path>\<E._LOG_FILE_PREFIX>_<datetime>_<pid><E._LOG_FILE_SUFFIX>
	 * Example:
	 * C:\Users\username\AppData\Local\Temp\esodbc_20181231235959_233.log
	 */
	static TCHAR path[MAX_PATH];
	TCHAR *qmark; /* question mark position */
	struct tm *then;
	time_t now = time(NULL);

	pos = GetEnvironmentVariable(_T(ESODBC_ENV_VAR_LOG_DIR), path,
			sizeof(path)/sizeof(path[0]));
	if (! pos) { /* 0 means error */
		/* env var wasn't defined OR error occured (which we can't log). */
		return GetLastError() == ERROR_ENVVAR_NOT_FOUND;
	}
	if (sizeof(path)/sizeof(path[0]) < pos) {
		/* path buffer too small */
		assert(0);
		return FALSE;
	}

	/* is there a log level specified? */
	if ((qmark = TSTRCHR(path, LOG_LEVEL_SEPARATOR))) {
		*qmark = 0; /* end the path here */
		pos = (int)(qmark - path); /* adjust the lenght of path */
		/* first letter will indicate the log level, with the default being
		 * debug, since this is mostly a tracing functionality */
		switch (qmark[1]) {
			case 'e':
			case 'E':
				_esodbc_log_level = LOG_LEVEL_ERR;
				break;
			case 'w':
			case 'W':
				_esodbc_log_level = LOG_LEVEL_WARN;
				break;
			case 'i':
			case 'I':
				_esodbc_log_level = LOG_LEVEL_INFO;
				break;
			default:
				_esodbc_log_level = LOG_LEVEL_DBG;
		}
	} else {
		/* default log level, if not specified, is debug */
		_esodbc_log_level = LOG_LEVEL_DBG;
	}

	/* break down time to date-time */
	if (! (then = localtime(&now))) {
		assert(0);
		return FALSE; /* should not happen */
	}

	/* build the log path name */
	path[pos ++] = FILE_PATH_SEPARATOR;
	pos = SNTPRINTF(path + pos, sizeof(path)/sizeof(path[0]),
			"%c" STPD "_%d%.2d%.2d%.2d%.2d%.2d_%u" STPD,
			FILE_PATH_SEPARATOR, _T(ESODBC_LOG_FILE_PREFIX),
			then->tm_year + 1900, then->tm_mon + 1, then->tm_mday,
			then->tm_hour, then->tm_min, then->tm_sec,
			GetCurrentProcessId(),
			_T(ESODBC_LOG_FILE_SUFFIX));
	if (sizeof(path)/sizeof(path[0]) < pos) {
		/* path buffer is too small */
		assert(0);
		return FALSE;
	}

	/* save the file path and open the file, to check path validity */
	log_path = path;
	return (log_file_handle(/* open*/TRUE) != INVALID_HANDLE_VALUE);
}

void log_cleanup()
{
	/* There is no need/function to destroy the SRWLOCK */
	log_file_handle(/* close */FALSE);

}

static void log_file_write(char *buff, size_t pos)
{
	HANDLE log_handle;
	DWORD written;

	log_handle = log_file_handle(/*open*/TRUE);
	if (log_handle == INVALID_HANDLE_VALUE) {
		return;
	}

	/* write the buffer to file */
	if (! WriteFile(
				log_handle, /*handle*/
				buff, /* buffer */
				(DWORD)(pos * sizeof(buff[0])), /*bytes to write */
				&written /* bytes written */,
				NULL /*overlap*/)) {
		log_file_handle(/* close */FALSE);
	} else {
#ifndef NDEBUG
#ifdef _WIN32
		FlushFileBuffers(log_handle);
#endif /* _WIN32 */
#endif /* NDEBUG */
	}
}

static inline void log_file(int level, int werrno, const char *func,
		const char *srcfile, int lineno, const char *fmt, va_list args)
{
	time_t now = time(NULL);
	int ret;
	size_t pos;
	char buff[ESODBC_LOG_BUF_SIZE];
	char ebuff[LOG_ERRNO_BUF_SIZE];
	const char *sfile, *next;
	/* keep in sync with esodbc_log_levels */
	static const char* level2str[] = { "ERROR", "WARN", "INFO", "DEBUG", };
	assert(level < sizeof(level2str)/sizeof(level2str[0]));

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

	/* drop path from file name */
	for (next = srcfile; next; next = strchr(sfile, FILE_PATH_SEPARATOR)) {
		sfile = next + 1;
	}

	/* write the debugging prefix */
	/* XXX: add release (file o.a.) printing? */
	if ((ret = snprintf(buff + pos, sizeof(buff) - pos, " - [%s] %s()@%s:%d ",
					level2str[level], func, sfile, lineno)) < 0)
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

	AcquireSRWLockExclusive(&log_mux);
	log_file_write(buff, pos);
	ReleaseSRWLockExclusive(&log_mux);
}

void _esodbc_log(int lvl, int werrno, const char *func,
		const char *srcfile, int lineno, const char *fmt, ...)
{
	va_list args;
    va_start(args, fmt);
	log_file(lvl, werrno, func, srcfile, lineno, fmt, args);
	va_end(args);
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
