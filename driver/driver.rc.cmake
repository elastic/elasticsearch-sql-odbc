//
// Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
// or more contributor license agreements. Licensed under the Elastic License;
// you may not use this file except in compliance with the Elastic License.
//

#include "windows.h"

/////////////////////////////////////////////////////////////////////////////
// Neutral (Default) resources

#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_ZZZ)
LANGUAGE LANG_NEUTRAL, SUBLANG_DEFAULT
#pragma code_page(1252)

/////////////////////////////////////////////////////////////////////////////
//
// Icon
//

ELASTICSEARCH_SQL_ICON	ICON	DISCARDABLE	"..\essql.ico"


/////////////////////////////////////////////////////////////////////////////
//
// Version
//

VS_VERSION_INFO VERSIONINFO
 FILEVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
 PRODUCTVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
 FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x40004L
 FILETYPE 0x2L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "000904b0"
        BEGIN
            VALUE "CompanyName", "Elasticsearch B.V."
            VALUE "FileDescription", "ODBC @ENCODING_VERBOSE@ driver for Elasticsearch"
            VALUE "FileVersion", "@DRV_VERSION@"
            VALUE "InternalName", "@DRV_NAME@@CMAKE_SHARED_LIBRARY_SUFFIX@"
            VALUE "LegalCopyright", "Copyright (C) perpetual, Elasticsearch B.V."
            VALUE "OriginalFilename", "@DRV_NAME@@CMAKE_SHARED_LIBRARY_SUFFIX@"
            VALUE "ProductName", "Elasticsearch"
            VALUE "ProductVersion", "@DRV_VERSION@"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x9, 1200
    END
END



#endif // Neutral (Default) resources
/////////////////////////////////////////////////////////////////////////////
