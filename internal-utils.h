
#pragma once

#include <variant>
#include <stdio.h>

namespace pathutils {

	// Return a FILE* when the given path is a stio/null path, otherwise return the path string as is.
	std::variant<FILE *, const char *> is_stdio_path(const char *path, bool dash_as_stdout = true, bool con_as_stderr = true);

}
