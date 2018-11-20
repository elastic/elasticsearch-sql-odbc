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

/* global file log */
esodbc_filelog_st *_gf_log = NULL;

BOOL log_init()
{
	int cnt;
	wchar_t *qmark; /* question mark position */
	wstr_st str_level;
	int log_level;
	/* PID buffer */
	wchar_t pid_buff[sizeof("4294967295")];
	wstr_st pid = (wstr_st) {
		pid_buff, sizeof(pid_buff)/sizeof(*pid_buff)
	};
	/* directory path */
	wchar_t dpath_buff[MAX_PATH + 1];
	wstr_st dpath = (wstr_st) {
		dpath_buff, sizeof(dpath_buff)/sizeof(*dpath_buff)
	};
	/* full file path */
	wchar_t fpath_buff[MAX_PATH + 1];
	wstr_st fpath = (wstr_st) {
		fpath_buff, sizeof(fpath_buff)/sizeof(*fpath_buff)
	};

	cnt = GetEnvironmentVariable(MK_WPTR(ESODBC_LOG_DIR_ENV_VAR),
			dpath.str, (DWORD)dpath.cnt);
	assert(0 <= cnt);
	if (! cnt) { /* 0 means error */
		/* env var wasn't defined OR error occured (which we can't log). */
		return GetLastError() == ERROR_ENVVAR_NOT_FOUND;
	} else if (dpath.cnt <= (size_t)cnt) {
		/* path buffer too small */
		assert(0);
		return FALSE;
	}

	/* is there a log level specified? */
	if ((qmark = wcschr(dpath.str, LOG_LEVEL_SEPARATOR))) {
		*qmark = 0; /* end the path here */
		dpath.cnt = qmark - dpath.str; /* adjust the length of path */
		str_level.str = qmark + 1;
		str_level.cnt = cnt - dpath.cnt - /* separator */1;
		log_level = parse_log_level(&str_level);
	} else {
		dpath.cnt = cnt;
		/* default log level, if not specified, is debug */
		log_level = LOG_LEVEL_DBG;
	}

	pid.cnt = i64tot((int64_t)GetCurrentProcessId(), pid.str, /*w?*/TRUE);
	if (pid.cnt <= 0) {
		assert(0);
		return FALSE;
	}

	if (! filelog_print_path(&fpath, &dpath, &pid)) {
		return FALSE;
	}

	_gf_log = filelog_new(&fpath, log_level);
	return _gf_log != NULL;
}

void log_cleanup()
{
	if (_gf_log) {
		filelog_del(_gf_log);
		_gf_log = NULL;
	}
}

int parse_log_level(wstr_st *level)
{
	if (level->cnt < 1) {
		return LOG_LEVEL_DISABLED;
	}
	/* first letter will indicate the log level */
	switch ((unsigned)level->str[0] | 0x20) {
		case 'e':
			return LOG_LEVEL_ERR;
		case 'w':
			return LOG_LEVEL_WARN;
		case 'i':
			return LOG_LEVEL_INFO;
		case 'd':
			return LOG_LEVEL_DBG;
	}
	return LOG_LEVEL_DISABLED;
}

/*
 * Fully qualified path name of the log file:
 * <dir_path>\<E._LOG_FILE_PREFIX>_<datetime>_<ident><E._LOG_FILE_SUFFIX>
 * Example:
 * C:\Users\username\AppData\Local\Temp\esodbc_20181231235959_233.log
 */
BOOL filelog_print_path(wstr_st *dest, wstr_st *dir_path, wstr_st *ident)
{
	wstr_st dir = *dir_path;
	int cnt;
	time_t now = time(NULL);
	struct tm *then = localtime(&now); /* "single tm structure per thread" */

	if (! then) {
		assert(0); /* should not happen */
		return FALSE;
	}

	/* strip trailing path separator */
	for (; 0 < dir.cnt; dir.cnt --) {
		if (dir.str[dir.cnt - 1] != MK_WPTR(FILE_PATH_SEPARATOR)) {
			break;
		}
	}
	if (dir.cnt <= 0) {
		/* input was just '\' (or empty) */
		return FALSE;
	}

	/* build the log full path name */
	cnt = _snwprintf(dest->str, dest->cnt,
			L"%.*s" "%c" "%s" "_%d%.2d%.2d%.2d%.2d%.2d_" "%.*s" "%s",
			(int)dir.cnt, dir.str,
			FILE_PATH_SEPARATOR,
			MK_WPTR(ESODBC_LOG_FILE_PREFIX),
			then->tm_year + 1900, then->tm_mon + 1, then->tm_mday,
			then->tm_hour, then->tm_min, then->tm_sec,
			(int)ident->cnt, ident->str,
			MK_WPTR(ESODBC_LOG_FILE_SUFFIX));

	if (cnt <= 0 || dest->cnt <= (size_t)cnt) {
		/* fpath buffer is too small */
		return FALSE;
	} else {
		dest->cnt = cnt;
	}

	return TRUE;
}

BOOL filelog_reset(esodbc_filelog_st *log)
{
	if (ESODBC_LOG_MAX_RETRY < log->fails) {
		/* disable logging alltogether on this logger */
		log->level = LOG_LEVEL_DISABLED;
		log->handle = INVALID_HANDLE_VALUE;
		return FALSE;
	}
	if (log->handle != INVALID_HANDLE_VALUE) {
		CloseHandle(log->handle);
	}
	log->handle = CreateFile(
			log->path, /* file name ("path") */
			GENERIC_WRITE, /* desired access */
			FILE_SHARE_WRITE, /* share mode */
			NULL, /* security attributes */
			OPEN_ALWAYS, /* creation disposition */
			FILE_ATTRIBUTE_NORMAL, /* flags & attributes */
			NULL /* template */);
	if (log->handle == INVALID_HANDLE_VALUE) {
		log->fails ++;
		return FALSE;
	}
	return TRUE;
}

esodbc_filelog_st *filelog_new(wstr_st *path, int level)
{
	esodbc_filelog_st *log;

	if (! (log = malloc(sizeof(*log) +
					(path->cnt + /*\0*/1) * sizeof(*log->path)))) {
		return NULL;
	}
	memset(log, 0, sizeof(*log));

	log->path = (wchar_t *)((char *)log + sizeof(*log));
	wcsncpy(log->path, path->str, path->cnt);
	log->path[path->cnt] = L'\0';

	log->level = level;
	log->handle = INVALID_HANDLE_VALUE;
	ESODBC_MUX_INIT(&log->mux);

	if (LOG_LEVEL_INFO <= level) {
#ifndef NDEBUG
		_LOG(log, LOG_LEVEL_INFO, /*werr*/0, "level: %d, file: " LWPDL ".",
			level, LWSTR(path));
#endif /* NDEBUG */
		_LOG(log, LOG_LEVEL_INFO, /*werr*/0, "driver version: %s.",
			ESODBC_DRIVER_VER);
	}

	return log;
}

void filelog_del(esodbc_filelog_st *log)
{
	if (! log) {
		return;
	}
	if (log->handle != INVALID_HANDLE_VALUE) {
		CloseHandle(log->handle);
	}
	ESODBC_MUX_DEL(&log->mux);
	free(log);
}

static BOOL filelog_write(esodbc_filelog_st *log, char *buff, size_t cnt)
{
	DWORD written;

	/* write the buffer to file */
	if (! WriteFile(
			log->handle, /*handle*/
			buff, /* buffer */
			(DWORD)(cnt * sizeof(buff[0])), /*bytes to write */
			&written /* bytes written */,
			NULL /*overlap*/)) {
		log->fails ++;
		if (filelog_reset(log)) {
			/* reattempt the write, if reset is successfull */
			if (filelog_write(log, buff, cnt)) {
				log->fails = 0;
				return TRUE;
			}
		}
		return FALSE;
	} else {
#ifndef NDEBUG
#ifdef _WIN32
		//FlushFileBuffers(log->handle);
#endif /* _WIN32 */
#endif /* NDEBUG */
	}
	return TRUE;
}

static inline void filelog_log(esodbc_filelog_st *log,
	int level, int werrno, const char *func, const char *srcfile, int lineno,
	const char *fmt, va_list args)
{
	time_t now = time(NULL);
	int ret;
	size_t pos;
	char buff[ESODBC_LOG_BUF_SIZE];
	char ebuff[LOG_ERRNO_BUF_SIZE];
	const char *sfile, *next;
	/* keep in sync with esodbc_log_levels */
	static const char *level2str[] = { "ERROR", "WARN", "INFO", "DEBUG", };
	assert(level < sizeof(level2str)/sizeof(level2str[0]));

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

	/* drop path from source file name */
	for (next = srcfile; next; next = strchr(sfile, FILE_PATH_SEPARATOR)) {
		sfile = next + 1;
	}

	/* write the debugging prefix */
	if ((ret = snprintf(buff + pos, sizeof(buff) - pos, " - [%s] %s()@%s:%d ",
					level2str[level], func, sfile, lineno)) < 0) {
		return;
	} else {
		pos += ret;
	}

	/* write the error number details */
	if (werrno) {
		ret = snprintf(buff + pos, sizeof(buff) - pos, "(%d:", errno);
		if (ret < 0) {
			return;
		} else {
			pos += ret;
		}

		if (strerror_s(ebuff, sizeof(ebuff), errno)) {
			return;
		}
		ret = snprintf(buff + pos, sizeof(buff) - pos, "%s) ", ebuff);
		if (ret < 0) {
			return;
		} else {
			pos += ret;
		}
	}

	/* write user's message */
	ret = vsnprintf(buff + pos, sizeof(buff) - pos, fmt, args);
	if (ret < 0) {
		return;
	} else {
		pos += ret;
	}

	/* if overrunning, correct the pos, to be able to add a \n\0 */
	if (sizeof(buff) < pos + /*\n\0*/2) {
		pos = sizeof(buff) - 2;
	}
	ret = snprintf(buff + pos, sizeof(buff) - pos, "\n");
	if (0 <= ret) {
		pos += ret;
	}
	assert(pos <= sizeof(buff));

	ESODBC_MUX_LOCK(&log->mux);
	filelog_write(log, buff, pos);
	ESODBC_MUX_UNLOCK(&log->mux);
}

void _esodbc_log(esodbc_filelog_st *log, int lvl, int werrno,
	const char *func, const char *srcfile, int lineno, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	filelog_log(log, lvl, werrno, func, srcfile, lineno, fmt, args);
	va_end(args);
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
