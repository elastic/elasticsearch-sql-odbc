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
// Version
//

VS_VERSION_INFO VERSIONINFO
 FILEVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
 PRODUCTVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
 FILEFLAGSMASK VS_FFI_FILEFLAGSMASK
#ifdef _DEBUG
 FILEFLAGS VS_FF_DEBUG
#else
 FILEFLAGS 0x0L
#endif
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_DLL
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "000904b0"
        BEGIN
            VALUE "CompanyName", "Elasticsearch B.V."
            VALUE "FileDescription", "Elasticsearch DSN Editor Binding"
            VALUE "FileVersion", "@DRV_VERSION@"
            VALUE "InternalName", "@DRV_NAME@@CMAKE_SHARED_LIBRARY_SUFFIX@"
            VALUE "LegalCopyright", "Copyright (C) Elasticsearch B.V."
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
