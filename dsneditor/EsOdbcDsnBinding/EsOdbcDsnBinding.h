#pragma once

/* needed for HWND definition (just a void*) */
#include <Windows.h>

/* flag passed to callback to indicate that an overwrite of existing DSN
 * is desired by user. */
#define ESODBC_DSN_OVERWRITE_FLAG	(1<<0)

#define ESODBC_DSN_NO_ERROR							0
/* Code returned by callback to signal that given DSN name already exists. */
#define ESODBC_DSN_EXISTS_ERROR					-1
/* The receivd DSN string (connection/00-list) is NULL. */
#define ESODBC_DSN_ISNULL_ERROR					-2
/* The receivd DSN string couldn't be parsed. */
#define ESODBC_DSN_INVALID_ERROR				-3
/* The receivd DSN name is invalid. */
#define ESODBC_DSN_NAME_INVALID_ERROR		-4
/* Non charachteristic (system?) error. */
#define ESODBC_DSN_GENERIC_ERROR				-127

/* 
 * Callback into the driver definition.
 *
 * Arguments:
 * - arg: opaque value provided back to the function (from and back into the
 *   driver);
 * - connectionString: serialized form of the DSN set.
 * - connStrLength: the lenght of the connectionString (excluding null
 *   terminator);
 * - errorMessage: out parameter, conveying the failure message (on failure);
 * - messageMaxLength: size of the errorMessage buffer (i.e. how much can the
 *   driver write into 'errorMessage');
 */
typedef int(*driver_callback_ft)(void *arg,	const wchar_t *connectionString,
	wchar_t *errorMessage, size_t messageMaxLength, unsigned flags);

/* 
 * Main entry point into the GUI.
 *
 * Arguments:
 * - hwnd: window handler passed through the driver; currently only used when
 *   there's an exception loading or in the assembley;
 * - onConnect: if true, the GUI must disable all controls
 *   non-connection-related (like DSN name, details, log file etc);
 * - dsnInW: connection string input parameter; could be empty/NULL.
 * - cbConnectionTest: callback from the GUI into the driver when the user
 *   wants to test the input parameters.
 * - argConnectionTest: argument to be passed back to cbConnectionTest;
 * - cbSaveDsn: callback from the GUI itno the driver when the user wants to
 *   save a connection string (here called also 'DSN');
 * - argSaveDsn: argument to be passed back to cbSaveDsn.
 *
 * Return:
 * - success: lenght of output connection string;
 * - non-failure: 0 (user canceled);
 * - failure: negative.
 */

#ifdef __cplusplus
extern "C"
#endif /* __cpluplus */
#ifdef _WINDLL
__declspec(dllexport)
#else /* _WINDLL */
__declspec(dllimport)
#endif /* _WINDLL */
int EsOdbcDsnEdit(HWND hwnd, BOOL onConnect, wchar_t *dsnInW,
	driver_callback_ft cbConnectionTest, void *argConnectionTest,
	driver_callback_ft cbSaveDsn, void *argSaveDsn);
