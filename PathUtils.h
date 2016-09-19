#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include "CoronaLua.h"

struct PathData {
	int mDocumentsDir;	// Reference to documents directory
	int mPathForFile;	// Reference to pathForFile
	int mResourceDir;	// Reference to resource directory

	static PathData * Instantiate (lua_State * L);

	const char * Canonicalize (lua_State * L, bool bRead);
};

#endif