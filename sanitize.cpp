
#include "pathutils.hpp"
#include "pathutils.h"

#include <variant>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace pathutils {

	// CON, PRN, AUX, NUL 
	// COM1, COM2, COM3, COM4, COM5, COM6, COM7, COM8, COM9
	// LPT1, LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, LPT9

	static inline bool streq(const char *s1, const char *s2)
	{
		return strcmp(s1, s2) == 0;
	}

	static inline bool strneq(const char *s1, const char *s2, size_t len)
	{
		return strncmp(s1, s2, len) == 0;
	}

	static inline bool strieq(const char *s1, const char *s2)
	{
		return stricmp(s1, s2) == 0;
	}

	static inline bool strnieq(const char *s1, const char *s2, size_t len)
	{
		return strnicmp(s1, s2, len) == 0;
	}

	bool name_is_antiquated_dos_device(const char *path, size_t len)
	{
		if (len <= 5 && len >= 3) {
			if (strnieq(path, "nul", 3) || strnieq(path, "con", 3) || strnieq(path, "aux", 3) || strnieq(path, "prn", 3)) {
				if (len == 3 || strneq(path + 3, ":", len - 3)) {
					return true;
				}
				return true;
			}
			if (strnieq(path, "com", 3) || strnieq(path, "lpt", 3)) {
				unsigned int offset = 3;
				if (len > 3 && ('1' <= path[3] && path[3] <= '9'))
					offset++;
				if (len == offset || strneq(path + offset, ":", len - offset)) {
					return true;
				}
			}
		}
		return false;
	}

	bool name_is_antiquated_dos_device(const char *path)
	{
		auto len = strlen(path);
		if (len <= 5) {
			return name_is_antiquated_dos_device(path, len);
		}
		return false;
	}



}


	{
		char fbuf[PATH_MAX];
		fz_format_output_path(ctx, fbuf, sizeof fbuf, output, 0);
		fz_normalize_path(ctx, fbuf, sizeof fbuf, fbuf);
		fz_sanitize_path(ctx, fbuf, sizeof fbuf, fbuf);
		out = fz_new_output_with_path(ctx, fbuf, 0);
	}

	{
		fz_close_device(ctx, dev);
		wri->count += 1;
		fz_format_output_path(ctx, path, sizeof path, wri->path, wri->count);
		fz_normalize_path(ctx, path, sizeof path, path);
		fz_sanitize_path(ctx, path, sizeof path, path);
		wri->save(ctx, wri->pixmap, path);
	}

  case CURLOPT_SANITIZE_WITH_EXTREME_PREJUDICE:
		data->set.sanitize_with_extreme_prejudice = (0 != va_arg(param, long)) ? 1L : 0L;
		break;

	// Sanitize the filename for *all* OSes: do not accept *? wildcards and other hacky characters *anywhere*: 
	{
		char *sanitized;
		CurlSanitizeCode sc = curl_sanitize_file_name(&sanitized, copy, 0);
		Curl_safefree(copy);
		if (sc)
			return NULL;
		copy = sanitized;
	}

	{
		char *sanitized;
		CurlSanitizeCode sc = curl_sanitize_file_name(&sanitized, *filename, 0);
		Curl_safefree(*filename);
		if (sc) {
			if (sc == CURL_SANITIZE_ERR_OUT_OF_MEMORY)
				return CURLE_OUT_OF_MEMORY;
			return CURLE_URL_MALFORMAT;
		}
		*filename = sanitized;
	}

	{
		char *sanitized;
		CurlSanitizeCode sc = curl_sanitize_file_name(&sanitized, Curl_dyn_ptr(&dyn),
																					 (CURL_SANITIZE_ALLOW_PATH |
																						 CURL_SANITIZE_ALLOW_RESERVED));
		Curl_dyn_free(&dyn);
		if (sc)
			return CURLE_URL_MALFORMAT;
		*result = sanitized;
		return CURLE_OK;
	}




	{ % macro sanitize_path_segment(segment) -% }
	{
		{
			segment.replace("[", "__lb_")
				.replace("]", "_rb_")
				.replace("(", "_lp_")
				.replace(")", "_rp_")
				.replace("<=>", "_spshp_")
				.replace("operator>", "operator__gt_")
				.replace("operator~", "operator_bnot_")
				.replace("->", "__arrow_")
				.replace("=", "_eq_")
				.replace("!", "__not_")
				.replace("+", "_plus_")
				.replace("-", "_minus_")
				.replace("&", "_and_")
				.replace("|", "_or_")
				.replace("^", "_xor_")
				.replace("*", "__star_")
				.replace("/", "_slash_")
				.replace("%", "_mod_")
				.replace("<", "_lt_")
				.replace(">", "_gt_")
				.replace("~", "_dtor_")
				.replace(",", "_comma_")
				.replace(":", "_")
				.replace(" ", "_")
		}
	}
	{ %- endmacro % }




	{
		utility::sanitize_filename(file_path_partial);
		res.set_static_file_info_unsafe(static_dir_ + file_path_partial);
		res.end();
	});

	TEST_CASE("normalize_path") {
		CHECK(crow::utility::normalize_path("/abc/def") == "/abc/def/");
		CHECK(crow::utility::normalize_path("path\\to\\directory") == "path/to/directory/");
	} // normalize_path

	TEST_CASE("sanitize_filename")
	{
		auto sanitize_filename = [](string s) {
			crow::utility::sanitize_filename(s);
			return s;
			};
		CHECK(sanitize_filename("abc/def") == "abc/def");
		CHECK(sanitize_filename("abc/../def") == "abc/_/def");
		CHECK(sanitize_filename("abc/..\\..\\..//.../def") == "abc/_\\_\\_//_./def");
		CHECK(sanitize_filename("abc/..../def") == "abc/_../def");
		CHECK(sanitize_filename("abc/x../def") == "abc/x../def");
		CHECK(sanitize_filename("../etc/passwd") == "_/etc/passwd");
		CHECK(sanitize_filename("abc/AUX") == "abc/_");
		CHECK(sanitize_filename("abc/AUX/foo") == "abc/_/foo");
		CHECK(sanitize_filename("abc/AUX:") == "abc/__");
		CHECK(sanitize_filename("abc/AUXxy") == "abc/AUXxy");
		CHECK(sanitize_filename("abc/AUX.xy") == "abc/_.xy");
		CHECK(sanitize_filename("abc/NUL") == "abc/_");
		CHECK(sanitize_filename("abc/NU") == "abc/NU");
		CHECK(sanitize_filename("abc/NuL") == "abc/_");
		CHECK(sanitize_filename("abc/LPT1\\") == "abc/_\\");
		CHECK(sanitize_filename("abc/COM1") == "abc/_");
		CHECK(sanitize_filename("ab?<>:*|\"cd") == "ab_______cd");
		CHECK(sanitize_filename("abc/COM9") == "abc/_");
		CHECK(sanitize_filename("abc/COM") == "abc/COM");
		CHECK(sanitize_filename("abc/CON") == "abc/_");
		CHECK(sanitize_filename("/abc/") == "_abc/");
	}




	static void map_path_to_dest(char* dst, size_t dstsiz, const char* inpath)
	{
		char srcpath[PATH_MAX];

#if 0
		// deal with 'specials' too:
		if (!strcmp(inpath, "/dev/null") || !fz_strcasecmp(inpath, "nul:") || !strcmp(inpath, "/dev/stdout"))
		{
			fz_strncpy_s(dst, inpath, dstsiz);
			return;
		}
#endif

		if (!fz_realpath(inpath, srcpath))
		{
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot process file path to a sane absolute path: %s", inpath);
		}

		// did the user request output file path mapping?
		if (!output_path_mapping_spec[0].abs_target_path[0])
		{
			ASSERT(dstsiz >= PATH_MAX);
			strncpy(dst, srcpath, dstsiz);
		} else
		{
			ASSERT(dstsiz >= PATH_MAX);
			ASSERT(dst != NULL);
			*dst = 0;
			size_t dst_len = 0;

			// find the best = shortest mapping:
			for (int idx = 0; idx < countof(output_path_mapping_spec) && output_path_mapping_spec[idx].abs_target_path[0]; idx++)
			{
				// TODO: cope with UNC paths mixed & mashed with classic paths.

				// First we find the common prefix.
				// Then we check how many path parts are left over from the CWD.
				// each left-over part is represented by a single _
				// concat those into the first part to be appended.
				// append the remainder of the source path then, cleaning it up
				// to get rid of drive colons and other 'illegal' chars, replacing them with _.
				// This is your mapped destination path, guaranteed to be positioned
				// WITHIN the given target path (which is prefixed to the generated
				// RELATIVE path!)
				//
				// Example:
				// given
				//   CWD = 	  C:/a/b/c
				//   TARGET = T:/t
				// we then get for these inputs:
				//   C:/a/b/c/d1  -> d1        (leftover: <nil>)     -> d1             -> T:/t/d1
				//   C:/a/b/d     -> d         (leftover: c)         -> _/c            -> T:/t/_/c
				//   C:/a/e/f     -> e/f       (leftover: b/c)       -> __/e/f         -> T:/t/__/e/f
				//   C:/x/y/z     -> x/y/z     (leftover: a/b/c)     -> ___/x/y/z      -> T:/t/___/x/y/z
				//   D:/a/b/c     -> D:/a/b/c  (leftover: C:/a/b/c)  -> ____/D:/a/b/c  -> T:/t/____/D_/a/b/c
				//   C:/a         ->           (leftover: b/c)       -> __             -> T:/t/__
				//   C:/a/b       ->           (leftover: c)         -> _              -> T:/t/_
				//   C:/x         -> x         (leftover: a/b/c)     -> ___/x          -> T:/t/___/x
				//   D:/a         -> D:/a      (leftover: C:/a/b/c)  -> ____/D:/a      -> T:/t/____/D_/a
				// thus every position in the directory tree *anywhere in the system* gets encoded to its own
				// unique subdirectory path within the "target path" directory tree -- of course, ASSUMING
				// you don't have any underscore-only leading directories in any of (relative) paths you feed
				// this mapper... ;-)
				const char* common_prefix = output_path_mapping_spec[idx].abs_cwd_as_mapping_source;
				const char* sep = common_prefix;
				const char* prefix_end = common_prefix;

				do
				{
					sep = find_next_dirsep(sep);
					// match common prefix:
					size_t cpfxlen = sep - common_prefix;
					if (strncmp(common_prefix, srcpath, cpfxlen) != 0)
					{
						// failed to match; the common prefix is our previous match length.
						break;
					}
					// check edge case: srcpath has a longer part name at the end of the match,
					// e.g. `b` vs. `bb`: `/a/b[/...]` vs. `/a/b[b/...]`
					if (*sep == 0 && srcpath[cpfxlen] != '/')
					{
						// failed to match; the common prefix is our previous match length.
						break;
					}
					prefix_end = sep;
				} while (*sep);

				// count the path parts left over from the CWD:
				char dotdot[PATH_MAX] = "";
				int ddpos = 0;

				// each left-over part is represented by a single _
				while (*sep)
				{
					dotdot[ddpos++] = '_';
					sep = find_next_dirsep(sep);
				}

				if (ddpos > 0)
					dotdot[ddpos++] = '/';
				dotdot[ddpos] = 0;

				// append the remainder of the source path then, cleaning it up
				// to get rid of drive colons and other 'illegal' chars, replacing them with _.
				size_t common_prefix_length = prefix_end - common_prefix;
				const char* remaining_inpath_part = srcpath + common_prefix_length;
				// skip leading '/' separators in remaining_inpath_part as they will only clutter the output
				while (*remaining_inpath_part == '/')
					remaining_inpath_part++;

				char appendedpath[PATH_MAX];
				int rv = fz_snprintf(appendedpath, sizeof(appendedpath), "%s/%s%s", output_path_mapping_spec[idx].abs_target_path, dotdot, remaining_inpath_part);
				if (rv <= 0 || rv >= dstsiz)
				{
					appendedpath[sizeof(appendedpath) - 1] = 0;
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot map file path to a sane sized absolute path: dstsize: %zu, srcpath: %s, dstpath: %s", dstsiz, srcpath, appendedpath);
				}

				// sanitize the appended part: lingering drive colons, wildcards, etc. will be replaced by _:
				fz_sanitize_path_ex(appendedpath, "^$!", "_", 0, strlen(output_path_mapping_spec[idx].abs_target_path));

				if (dst_len == 0 || dst_len > strlen(appendedpath))
				{
					ASSERT(dstsiz >= PATH_MAX);
					strncpy(dst, appendedpath, dstsiz);
					dst[dstsiz - 1] = 0;
					dst_len = strlen(appendedpath);
				}
			}
		}
	}




	// find the best = shortest mapping:
	for (int idx = 0; idx < countof(output_path_mapping_spec) && output_path_mapping_spec[idx].abs_target_path[0]; idx++)
	{
		// TODO: cope with UNC paths mixed & mashed with classic paths.

		// First we find the common prefix.
		// Then we check how many path parts are left over from the CWD.
		// each left-over part is represented by a single _
		// concat those into the first part to be appended.
		// append the remainder of the source path then, cleaning it up
		// to get rid of drive colons and other 'illegal' chars, replacing them with _.
		// This is your mapped destination path, guaranteed to be positioned
		// WITHIN the given target path (which is prefixed to the generated
		// RELATIVE path!)
		//
		// Example:
		// given
		//   CWD = 	  C:/a/b/c
		//   TARGET = T:/t
		// we then get for these inputs:
		//   C:/a/b/c/d1  -> d1        (leftover: <nil>)     -> d1             -> T:/t/d1
		//   C:/a/b/d     -> d         (leftover: c)         -> _/c            -> T:/t/_/c
		//   C:/a/e/f     -> e/f       (leftover: b/c)       -> __/e/f         -> T:/t/__/e/f
		//   C:/x/y/z     -> x/y/z     (leftover: a/b/c)     -> ___/x/y/z      -> T:/t/___/x/y/z
		//   D:/a/b/c     -> D:/a/b/c  (leftover: C:/a/b/c)  -> ____/D:/a/b/c  -> T:/t/____/D_/a/b/c
		//   C:/a         ->           (leftover: b/c)       -> __             -> T:/t/__
		//   C:/a/b       ->           (leftover: c)         -> _              -> T:/t/_
		//   C:/x         -> x         (leftover: a/b/c)     -> ___/x          -> T:/t/___/x
		//   D:/a         -> D:/a      (leftover: C:/a/b/c)  -> ____/D:/a      -> T:/t/____/D_/a
		// thus every position in the directory tree *anywhere in the system* gets encoded to its own
		// unique subdirectory path within the "target path" directory tree -- of course, ASSUMING
		// you don't have any underscore-only leading directories in any of (relative) paths you feed
		// this mapper... ;-)
		const char* common_prefix = output_path_mapping_spec[idx].abs_cwd_as_mapping_source;
		const char* sep = common_prefix;
		const char* prefix_end = common_prefix;

		do
		{
			sep = find_next_dirsep(sep);
			// match common prefix:
			size_t cpfxlen = sep - common_prefix;
			if (strncmp(common_prefix, srcpath, cpfxlen) != 0)
			{
				// failed to match; the common prefix is our previous match length.
				break;
			}
			// check edge case: srcpath has a longer part name at the end of the match,
			// e.g. `b` vs. `bb`: `/a/b[/...]` vs. `/a/b[b/...]`
			if (*sep == 0 && srcpath[cpfxlen] != '/')
			{
				// failed to match; the common prefix is our previous match length.
				break;
			}
			prefix_end = sep;
		} while (*sep);

		// count the path parts left over from the CWD:
		char dotdot[PATH_MAX] = "";
		int ddpos = 0;

		// each left-over part is represented by a single _
		while (*sep)
		{
			dotdot[ddpos++] = '_';
			sep = find_next_dirsep(sep);
		}

		if (ddpos > 0)
			dotdot[ddpos++] = '/';
		dotdot[ddpos] = 0;

		// append the remainder of the source path then, cleaning it up
		// to get rid of drive colons and other 'illegal' chars, replacing them with _.
		size_t common_prefix_length = prefix_end - common_prefix;
		const char* remaining_inpath_part = srcpath + common_prefix_length;
		// skip leading '/' separators in remaining_inpath_part as they will only clutter the output
		while (*remaining_inpath_part == '/')
			remaining_inpath_part++;

		char appendedpath[PATH_MAX];
		int rv = fz_snprintf(appendedpath, sizeof(appendedpath), "%s/%s%s", output_path_mapping_spec[idx].abs_target_path, dotdot, remaining_inpath_part);
		if (rv <= 0 || rv >= dstsiz)
		{
			appendedpath[sizeof(appendedpath) - 1] = 0;
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot map file path to a sane sized absolute path: dstsize: %zu, srcpath: %s, dstpath: %s", dstsiz, srcpath, appendedpath);
		}

		// sanitize the appended part: lingering drive colons, wildcards, etc. will be replaced by _:
		fz_sanitize_path_ex(appendedpath, "^$!", "_", 0, strlen(output_path_mapping_spec[idx].abs_target_path));

		if (dst_len == 0 || dst_len > strlen(appendedpath))
		{
			ASSERT(dstsiz >= PATH_MAX);
			strncpy(dst, appendedpath, dstsiz);
			dst[dstsiz - 1] = 0;
			dst_len = strlen(appendedpath);
		}
	}



	std::string normalizePath(const std::string& path) {
		char* tmp = realpath(path.c_str(), nullptr);
		if (!tmp) {
			throw IOError::fromSystemError(
					"Failed to normalize path \"" + path + "\"", PISTIS_EX_HERE
			);
		} else {
			std::string normalized(tmp);
			free((void*)tmp);
			return std::string(normalized);
		}
	}

	std::string relativePath(const std::string& path,
				 const std::string& base) {
		static const std::string THIS_DIR = ".";
		static const std::string PARENT_DIR = "..";

		if (isAbsolute(path) != isAbsolute(base)) {
			return relativePath(absolutePath(path), absolutePath(base));
		} else {
			std::vector<std::string> pathComponents = split(path);
			std::vector<std::string> baseComponents = split(base);
			uint32_t i = 0;

			while ((i != pathComponents.size()) && (i != baseComponents.size()) &&
			 (pathComponents[i] == baseComponents[i])) {
				++i;
			}

			if ((i == baseComponents.size()) && (i == pathComponents.size())) {
				return THIS_DIR;
			} else {
				std::vector<std::string> newComponents;
				for (uint32_t j = i; j != baseComponents.size(); ++j) {
					newComponents.push_back(PARENT_DIR);
				}
				while (i != pathComponents.size()) {
					newComponents.push_back(pathComponents[i]);
					++i;
				}
				return joinSequence(newComponents.begin(), newComponents.end());
			}
		}
	}

	std::tuple<std::string, std::string> splitFile(const std::string& path) {
		static const std::string ROOT_DIR("/");
		size_t i = path.rfind('/');
		if (i == std::string::npos) {
			return std::make_tuple(std::string(), path);
		} else if (!i) {
			return std::make_tuple(ROOT_DIR, path.substr(1));
		} else {
			size_t j = path.find_last_not_of('/', i - 1);
			if (j == std::string::npos) {
				return std::make_tuple(ROOT_DIR, path.substr(i + 1));
			}
			return std::make_tuple(path.substr(0, j + 1), path.substr(i + 1));
		}
	}

	std::tuple<std::string, std::string> splitExtension(const std::string& path) {
		size_t i = path.size();
		while ((i > 1) && (path[i - 1] != '/')) {
			--i;
			if ((path[i] == '.') && (path[i - 1] != '/') &&
					(path[i - 1] != '.')) {
				return std::make_tuple(path.substr(0, i), path.substr(i));
			}
		}
		return std::make_tuple(path, std::string());
	}

