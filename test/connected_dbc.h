#ifndef __CONNECTED_DBC_H__
#define __CONNECTED_DBC_H__

extern "C" {
#if defined(_WIN32) || defined (WIN32)
#include <windows.h>
#include <tchar.h>
#endif /* _WIN32/WIN32 */

#include <sql.h>
#include <sqlext.h>

#include "queries.h"
} // extern C

#if defined(_WIN32) || defined (WIN32)
#define STRDUP	_strdup
#else /* _WIN32/WIN32 */
#define STRDUP	strdup
#endif /* _WIN32/WIN32 */

/* convenience casting macros */
#define ATTACH_ANSWER(_h, _s, _l)   attach_answer((esodbc_stmt_st *)_h, _s, _l)
#define ATTACH_SQL(_h, _s, _l)      attach_sql((esodbc_stmt_st *)_h, _s, _l)

class ConnectedDBC {
  protected:
    SQLHANDLE env, dbc, stmt;

  ConnectedDBC();
  virtual ~ConnectedDBC();
};

#endif /* __CONNECTED_DBC_H__ */
