#!/bin/bash
#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

# Reformats the driver code, using Artistic Style, to ensure consistency.


# By default, format the driver source code (no arguments are received);
# else append the arguments to the pre-set formating parameters 
if [ $# -lt 1 ] ; then
	ARGS=`git rev-parse --show-toplevel`/driver/*.[ch]
else
	ARGS=${@:1}
fi

#
# Ensure astyle is available and is of the right version to have all below
# parameters supported.
#
if ! which -s astyle ; then
    echo "ERROR: The astyle code formatter is not available. Exiting."
    exit 1
fi

REQUIRED_ASTYLE_VERSION=3.1
FOUND_ASTYLE_VERSION=$(expr "`astyle --version`" : ".* \([0-9].[0-9]\(.[0-9]\)\{0,1\}\)")

if [ -z "${FOUND_ASTYLE_VERSION}" ] ; then
    echo "ERROR: Required astyle version ${REQUIRED_ASTYLE_VERSION} not found."
    echo "       Could not determine astyle version."
    exit 2
fi

if [ "${REQUIRED_ASTYLE_VERSION}" != "${FOUND_ASTYLE_VERSION}" ] ; then
    echo "ERROR: Required astyle version ${REQUIRED_ASTYLE_VERSION} not found."
    echo "       Detected astyle version ${FOUND_ASTYLE_VERSION}"
    exit 3
fi

# A10: "One True Brace Style" uses linux braces and adds braces to unbraced
#      one line conditional statements. (and apparently loops too)
# k3 : align-pointer=name
# T  : Indent using all tab characters, if possible.
# xC : break a line if the code exceeds # characters.
# xU : Indent, instead of align, continuation lines following lines that
#      contain an opening paren '(' or an assignment '='.
# xt : Set the continuation indent for a line that ends with an opening paren
#      '(' or an assignment '='.
# w  : Indent multi-line preprocessor definitions ending with a backslash.
# S  : Indent 'switch' blocks so that the 'case X:' statements are indented in
#      the switch block
# xL : will cause the logical conditionals to be placed last on the previous
#      line.
# xW : Indent preprocessor blocks at brace level zero and immediately within a
#      namespace.
# xf :
# xh : Attach the return type to the function name (in deFinition and
#      declaration).
# n  : Do not retain a backup of the original file.
# --dry-run -Q : only output file that would be formatted
ASTYLE_PARAMS="-A10 -k3 -T -xC79 -xU -w -S -xt -xL -xf -xh -n"
astyle $ASTYLE_PARAMS $ARGS

