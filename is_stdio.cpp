
#include "pathutils.hpp"
#include "pathutils.h"

#include <variant>
#include <stdio.h>

// static STRING_VAR(debug_file, "", "File to send the application diagnostic messages to. Accepts '-' or '1' for stdout, '+' or '2' for stderr, and also recognizes these on *all* platforms: NUL:, /dev/null, /dev/stdout, /dev/stderr, 1, 2");

// CON:, NUL:, AUX:, COM1:, COM2:, COM3:, COM4:, PRN:, LPT1:, LPT2:, LPT3:,
// 
// CON, PRN, AUX, NUL 
// COM1, COM2, COM3, COM4, COM5, COM6, COM7, COM8, COM9
// LPT1, LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, LPT9

namespace pathutils {

	static inline bool streq(const char *s1, const char *s2)
	{
		return strcmp(s1, s2) == 0;
	}

	static inline bool strieq(const char *s1, const char *s2)
	{
		return stricmp(s1, s2) == 0;
	}

	static inline bool strnieq(const char *s1, const char *s2, size_t len)
	{
		return strnicmp(s1, s2, len) == 0;
	}

	std::variant<FILE *, const char *> is_stdio_path(const char *path, bool dash_as_stdout, bool con_as_stderr)
	{
		if (strieq(path, "/dev/stdout") || strieq(path, "stdout") || strieq(path, "1")) {
			return stdout;
			//_canonical_filepath = "/dev/stdout";
		}
		if (strieq(path, "/dev/stderr") || strieq(path, "stderr") || strieq(path, "+") || strieq(path, "2")) {
			return stderr;
			//_canonical_filepath = "/dev/stderr";
		}
		if (strieq(path, "-")) {
			return (dash_as_stdout ? stdout : stderr);
			//_canonical_filepath = "/dev/stdout";
		}
		if (strlen(path) <= 5) {
			if (strieq(path, "con") || strieq(path, "con:")) {
				return (con_as_stderr ? stderr : stdout);
				//_canonical_filepath = "/dev/stderr";
			}
			if (strieq(path, "aux") || strieq(path, "aux:")) {
				return (con_as_stderr ? stderr : stdout);
				//_canonical_filepath = "/dev/stderr";
			}
			if (strieq(path, "prn") || strieq(path, "prn:")) {
				return (con_as_stderr ? stderr : stdout);
				//_canonical_filepath = "/dev/stderr";
			}
			if (strnieq(path, "com", 3) || strnieq(path, "lpt", 3)) {
				unsigned int offset = 3;
				if (strchr("123456789", path[3]))
					offset++;
				if (path[offset] == 0 || streq(path + offset, ":")) {
					return (con_as_stderr ? stderr : stdout);
					//_canonical_filepath = "/dev/stderr";
				}
			}
		}
		if (strieq(path, "/dev/null") || strieq(path, "nul") || strieq(path, "nul:")) {
			return (FILE *)nullptr;
			//_canonical_filepath = "/dev/null";
		}
		return path;
	}


extern "C"
int process_path_as_stdio(const char *path, FILE **stdio_handle)
{
	auto rv = is_stdio_path(path);
	if (std::holds_alternative<FILE *>(rv)) {
		*stdio_handle =  std::get<FILE *>(rv);
		return 1;
	}
	*stdio_handle = nullptr;
	return 0;
}

}

