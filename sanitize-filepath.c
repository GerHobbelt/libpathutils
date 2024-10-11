
#include "mupdf/fitz.h"
#include "mupdf/helpers/dir.h"
#include "mupdf/helpers/system-header-files.h"
#include "utf.h"

#ifdef _MSC_VER
#include <direct.h> /* for mkdir */
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <sys/stat.h>


/**
 * Normalize a given path, i.e. resolve the ./ and ../ directories that may be part of it.
 * Also UNIXify the path by replacing \ backslashes with / slashes, which work on all platforms.
 *
 * When path is NULL, the path is assumed to already be present in the dstpath buffer and
 * will be modified in place.
 *
 * Throws an exception when the path is buggy, e.g. contains a ../ which cannot be resolved,
 * e.g. C:\..\b0rk
 */
void fz_normalize_path(fz_context* ctx, char* dstpath, size_t dstpath_bufsize, const char* path)
{
	// as we normalize a path, it can only become *shorter* (or equal in length).
	// Thus we copy the source path to the buffer verbatim first.
	if (!dstpath)
		fz_throw(ctx, FZ_ERROR_GENERIC, "fz_normalize_path: dstpath cannot be NULL.");
	// copy source path, if it isn't already in the work buffer:
	size_t len;
	if (path) {
		len = strlen(path);
		if (dstpath_bufsize < len + 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "fz_normalize_path: buffer overrun.");
		if (path != dstpath)
			memmove(dstpath, path, len + 1);
	}
	else {
		len = strlen(dstpath);
	}

	// unixify MSDOS path:
	char* e = strchr(dstpath, '\\');
	while (e)
	{
		*e = '/';
		e = strchr(e, '\\');
	}

	int can_have_mswin_drive_letter = 1;

	// Sanitize the *entire* path, including the Windows Drive/Share part:
	e = dstpath;
	if (e[0] == '/' && e[1] == '/' && strchr(".?", e[2]) != NULL && e[3] == '/')
	{
		// skip //?/ and //./ UNC leaders
		e += 4;
	}
	else if (e[0] == '/' && e[1] == '/')
	{
		// skip //<server>... UNC path starter (which cannot contain Windows drive letters as-is)
		e += 2;
		can_have_mswin_drive_letter = 0;
	}

	if (can_have_mswin_drive_letter)
	{
		// See if we have a Windows drive as part of the path: keep that one intact!
		if (isalpha(e[0]) && e[1] == ':')
		{
			*e = toupper(e[0]);
			e += 2;
		}
	}

	char* start = e;

	// now find ./ and ../ directories and resolve each, if possible:
	char* p = start;
	e = strchr(*p ? p + 1 : p, '/');  // skip root /, if it (may) exist
	while (e)
	{
		if (e == p) {
			// matched an extra / (as in the second / of  '//'): that one can be killed
			//memmove(p, e + 1, len + 1 - (e + 1 - dstpath));  -->
			memmove(p, e + 1, len - (e - dstpath));
			len -= e + 1 - p;
		}
		else if (strncmp(p, ".", e - p) == 0) {
			// matched a ./ directory: that one can be killed
			//memmove(p, e + 1, len + 1 - (e + 1 - dstpath));  -->
			memmove(p, e + 1, len - (e - dstpath));
			len -= e + 1 - p;
		}
		else if (strncmp(p, "..", e - p) == 0) {
			// matched a ../ directory: that can only be killed when there's a parent available
			// which is not itself already a ../
			if (p - 2 >= start) {
				// scan back to previous /
				p -= 2;
				while (p > start && p[-1] != '/')
					p--;
				// make sure we have backtracked over a directory that is not itself named ../
				if (strncmp(p, "../", 3) != 0) {
					//memmove(p, e + 1, len + 1 - (e + 1 - dstpath));  -->
					memmove(p, e + 1, len - (e - dstpath));
					len -= e + 1 - p;
				}
				else {
					// otherwise, we're stuck with this ../ that we currently have.
					p = e + 1;
				}
			}
			else {
				// no parent available at all...
				// hence we're stuck with this ../ that we currently have.
				p = e + 1;
			}
		}
		else {
			// we now have a directory that's not ./ nor ../
			// hence keep as is
			p = e + 1;
		}
		e = strchr(p, '/');
	}

	// we now have the special case, where the path ends with a directory named . or .., which does not come with a trailing /
	if (strcmp(p, ".") == 0) {
		// matched a ./ directory: that one can be killed
		if (p - 1 > start) {
			// keep this behaviour of no / at end (unless we were processing the fringe case '/.'):
			p--;
		}
		*p = 0;
	}
	else if (strcmp(p, "..") == 0) {
		// matched a ../ directory: that can only be killed when there's a parent available
		// which is not itself already a ../
		if (p - 2 >= start) {
			// scan back to previous /
			p -= 2;
			while (p > start && p[-1] != '/')
				p--;
			// make sure we have backtracked over a directory that is not itself named ../
			if (strncmp(p, "../", 3) != 0) {
				if (p - 1 > start) {
					// keep this behaviour of no / at end (unless we were processing the fringe case '/..', which is illegal BTW):
					p--;
				}
				else if (p - 1 == start) {
					fz_throw(ctx, FZ_ERROR_GENERIC, "fz_normalize_path: illegal /.. path.");
				}
				*p = 0;
			}
			// otherwise, we're stuck with this ../ that we currently have.
		}
		// else: no parent available at all...
		// hence we're stuck with this ../ that we currently have.
	}
}


static uint64_t calchash(const char* s) {
	uint64_t x = 0x33333333CCCCCCCCULL;
	const uint8_t* p = (const uint8_t*)s;

	// hash/reduction inspired by Xorshift64
	for (; *p; p++) {
		uint64_t c = *p;
		x ^= c << 26;
		x += c;

		x ^= x << 13;
		x ^= x >> 7;
		x ^= x << 17;
	}

	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;

	return x;
}


#include "../../scripts/legal_codepoints_bitmask.inc"


static int has_prefix(const char* s, const char* prefix)
{
	size_t len = strlen(prefix);
	return fz_strncasecmp(s, prefix, len) == 0;
}

static int has_postfix(const char* s, const char* postfix)
{
	size_t len = strlen(postfix);
	size_t slen = strlen(s);
	if (slen < len)
		return 0;
	return fz_strcasecmp(s + slen - len, postfix) == 0;
}

static int has_basename(const char* s, const char* prefix)
{
	size_t len = strlen(prefix);
	return (fz_strncasecmp(s, prefix, len) == 0) &&
		(!s[len] || s[len] == '.');
}

static int has_infix(const char* s, const char* infix)
{
	size_t len = strlen(infix);
	size_t slen = strlen(s);
	if (slen < len)
		return 0;
	for (size_t i = 0; i < slen - len; i++)
	{
		if (has_prefix(s + i, infix))
			return 1;
	}
	return 0;
}



// Detect 'reserved' and other 'dangerous' file/directory names.
//
// DO NOT accept any of these file / directory *basenames*: 
//
// - CON, PRN, AUX, NUL, COM0, COM1, COM2, COM3, COM4, COM5, COM6, COM7, COM8, COM9, 
//   LPT0, LPT1, LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, and LPT9
//
// - while on UNIXes a directory named `dev` might be ill-advised. 
//   (Writing to a **device** such as `/dev/null` is perfectly okay there; we're interested 
//   and focusing on *regular files and directories* here though, so giving them names such 
//   as `dev`, `stdout`, `stdin`, `stderr` or `null` might be somewhat... *ill-advised*. 😉
//
// - of course, Mac/OSX has it's own set of reserved names and aliases, e.g. all filenames 
//   starting with `._` are "extended attributes": when you have filename `"xyz"`, OSX will 
//   create an additional file named `"._xyz"` when you set any *extended attributes*. 
//   There's also the `__MACOSX` directory that you never see -- unless you look at the same 
//   store by way of a MSWindows or Linux box.
//
// - it's ill-advised to start (or end!) any file or directory name with `"."` or `"~"`: 
//   the former is used to declare the file/directory "hidden" in UNIX, while the latter 
//   MAY be treated as a shorthand for the user home directory or signal the file is a 
//   "backup/temporary file" when appended at the end.
//
// - NTFS has a set of reserved names of its own, all of which start with `"$"`, while antique 
//   MSDOS reserved filenames *end* with a `"$"`, so you'ld do well to forego `"$"` anywhere 
//   it shows up in your naming. 
//   To put the scare into you, there's a MSWindows hack which will *crash and corrupt* the system 
//   by simply *addressing* a (non-existing) reserved filename `"$i30:$bitmap"` on any NTFS drive 
//   (CVE-2021-28312): https://www.bleepingcomputer.com/news/security/microsoft-fixes-windows-10-bug-that-can-corrupt-ntfs-drives/
//
// - OneDrive also [mentions several reserved filenames](https://support.microsoft.com/en-us/office/restrictions-and-limitations-in-onedrive-and-sharepoint-64883a5d-228e-48f5-b3d2-eb39e07630fa#invalidcharacters): 
//   "*These names aren't allowed for files or folders: `.lock`, `CON`, `PRN`, `AUX`, `NUL`, 
//   `COM0` - `COM9`, `LPT0` - `LPT9`, `_vti_`, `desktop.ini`, any filename starting with `~$`.*" 
//   It also rejects `".ds_store"`, which is a hidden file on Mac/OSX. 
//   [Elsewhere](https://support.microsoft.com/en-us/office/onedrive-can-rename-files-with-invalid-characters-99333564-c2ed-4e78-8936-7c773e958881) 
//   it mentions these again, but with a broader scope: 
//   "*These other characters have special meanings when used in file names in OneDrive, SharePoint, 
//   Windows and macOS, such as `"*"` for wildcards, `"\"` in file name paths, **and names containing** 
//   `.lock`, `CON`, or `_vti_`.*" They also strongly discourage the use of `#%&` characters in filenames.
//
// - Furthermore it's a **bad idea** to start any filename with `"-"` or `"--"` as this clashes 
//   very badly with standard UNIX commandline options; only a few tools "fix" this by allowing 
//   a special `"--"` commandline option.
//
// - [GoogleDrive](https://developers.google.com/style/filenames) doesn't mention any explicit 
//   restrictions, but the published *guidelines* rather suggest limiting oneself to plain ASCII only, 
//   with all the other restrictions mentioned in this larger list. 
//   Google is (as usual for those buggers) *extremely vague* on this subject, but [some hints](https://www.googlecloudcommunity.com/gc/Workspace-Q-A/File-and-restrictions-for-Google-Drive/td-p/509595) 
//   have been discovered: path length *probably* limited to 32767, directory depth max @ 21 is 
//   mentioned (probably gleaned from [here](https://support.google.com/a/users/answer/7338880?hl=en#shared_drives_file_folder_limits), 
//   however I tested it on my own GoogleDrive account and at the time of this writing (May 2023) 
//   Google didn't object to creating a tree depth >= 32, nor a filename <= 500 characters.)
//
static int is_reserved_filename(const char* s)
{
	if (has_infix(s, "_vti_"))
		return 1;
	if (has_postfix(s, "$"))
		return 1;
	if (has_postfix(s, "~"))
		return 1;

	switch (tolower(s[0]))
	{
	case 'a':
		return has_basename(s, "aux");

	case 'd':
		return has_basename(s, "dev") ||
			has_prefix(s, "desktop.ini");

	case 'c':
		return has_basename(s, "con") ||
			(has_prefix(s, "com") && isdigit(s[3]));

	case 'l':
		return has_prefix(s, "lpt") && isdigit(s[3]);

	case 'n':
		return has_basename(s, "nul") ||
			has_basename(s, "null");

	case 'p':
		return has_basename(s, "prn");

	case 's':
		return has_basename(s, "stdin") ||
			has_basename(s, "stdout") ||
			has_basename(s, "stderr");

	case '_':
		return has_basename(s, "__macosx");

	case '~':
		return 1;

	case '.':
		return has_basename(s + 1, "lock");
		has_basename(s + 1, "ds_store");

	case '$':
		return 1;

	case '-':
		return 1;
	}

	return 0;
}

const char* rigorously_clean_fname(char* s)
{
	// - keep up to two 'extension' dots.
	// - nuke anything that's not an ASCII alphanumeric
	// - modify in place
	char* d = s;
	while (*s)
	{
		char c = *s;
		if (isalnum(c))
		{
			*d++ = c;
		}
		else if (c == '.' && d > s)
		{
			*d++ = c;
		}
		// else: discard!
	}

	// trim off trailing dots:
	while (d > s && d[1] == '.')
		d--;

	*d = 0;

	// now nuke all but the last two dots:
	int dot_count = 0;
	while (d > s)
	{
		char c = *--d;
		if (c == '.')
		{
			dot_count++;
			if (dot_count > 2)
				*d = '_';
		}
	}

	return d;
}


/**
 * Sanitize a given path, i.e. replace any "illegal" characters in the path, using generic
 * OS/filesystem heuristics. "Illegal" characters are replaced with an _ underscore.
 *
 * When path is NULL, the path is assumed to already be present in the dstpath buffer and
 * will be modified in place.
 *
 * The path is assumed to have been normalized already, hence little intermezzos like '/./'
 * shall not exist in the input. IFF they do, they will be sanitized to '/_/'.
 *
 * Path elements (filename, directory names) are restricted to a maximum length of 255 characters
 * *each* (Linux FS limit).
 *
 * The entire path will be reduced to the given `dstpath_bufsize`;
 * reductions are done by first reducing the filename length, until it is less
 * or equal to 13 characters, after which directory elements are reduced, one after the other,
 * until we've reached the desired total path length.
 *
 * Returns 0 when no sanitization has taken place. Returns 1 when only the filename part
 * of the path has been sanitized. Returns 2 or higher when any of the directory parts have
 * been sanitized: this is useful when calling code wishes to speed up their FS I/O by
 * selectively executing `mkdirp` only when the directory elements are sanitized and thus
 * "may be new".
 */
int fz_sanitize_path(fz_context* ctx, char* dstpath, size_t dstpath_bufsize, const char* path)
{
	// as we normalize a path, it can only become *shorter* (or equal in length).
	// Thus we copy the source path to the buffer verbatim first.
	if (!dstpath)
		fz_throw(ctx, FZ_ERROR_GENERIC, "fz_sanitize_path: dstpath cannot be NULL.");

	// copy source path, if it isn't already in the work buffer:
	size_t len;
	if (path) {
		len = strlen(path);
		if (dstpath_bufsize < len + 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "fz_sanitize_path: buffer overrun.");
		if (path != dstpath)
			memmove(dstpath, path, len + 1);
	}
	else {
		len = strlen(dstpath);
	}

	return fz_sanitize_path_ex(dstpath, NULL, NULL, 0, dstpath_bufsize);
}


/**
	Replace any path-invalid characters from the input path.
	This API assumes a rather strict rule set for file naming to ensure resulting paths
	are valid on UNIX, Windows and Mac OSX platforms' default file systems (ext2, HFS, NTFS).

	You may additionally specify a set of characters, which should be replaced as well,
	and a replacement string for each occurrence or run of occurrences.

	When `f` is specified as part of the replacement sequence, this implies any `printf`/`fz_format_output_path`
	format string, e.g. `%[+-][0-9]*d`, in its entirety. You may want to specify `%` in the set if you only
	wish to replace/'nuke' the leading `%` in each such format specifier. Use `f` to strip out all `printf`-like
	format parts from a filename/path to make it generically safe to use.

	These additional arguments, `set` and `replace_single`, may be NULL	or NUL(0), in which case they are not active.

	The `replacement_single` string is used as a map: each character in the `set` is replaced by its corresponding
	character in `replacement_single`. When `replacement_single` is shorter than `set`, any of the later `set` members
	will be replaced by the last `replacement_single` character.

	When `start_at_offset` is not zero (i.e. start-of-string), only the part of the path at & following the
	`start_at_offset` position is sanitized. It is assumed that the non-zero offset ALWAYS points past any critical
	UNC or MSWindows Drive designation parts of the path.

	Modifies the `path` string in place.

		Returns 0 when no sanitization has taken place. Returns 1 when only the filename part
		of the path has been sanitized. Returns 2 or higher when any of the directory parts have
		been sanitized: this is useful when calling code wishes to speed up their FS I/O by
		selectively executing `mkdirp` only when the directory elements are sanitized and thus
		"may be new".
*/
int
fz_sanitize_path_ex(char* path, const char* set, const char* replace_single, size_t start_at_offset, size_t maximum_path_length)
{
	if (!path)
		return 0;
	if (!replace_single || !*replace_single)
		replace_single = "_";
	if (!set)
		set = "";

	size_t repl_map_len = strlen(replace_single);
	int has_printf_format_repl_idx1;
	{
		const char* m = strchr(set, 'f');
		if (m)
		{
			has_printf_format_repl_idx1 = m - set;
			// pick last in map when we're out-of-bounds:
			if (has_printf_format_repl_idx1 >= repl_map_len)
				has_printf_format_repl_idx1 = repl_map_len - 1;
			has_printf_format_repl_idx1++;
		}
		else
		{
			has_printf_format_repl_idx1 = 0;
		}
	}

	if (start_at_offset)
	{
		size_t l = strlen(path);
		if (l <= start_at_offset)
		{
			// nothing to process!
			return 0;
		}

		path += start_at_offset;
	}

	// unixify MSDOS/MSWIN/UNC path:
	char* e = strchr(path, '\\');
	while (e)
	{
		*e = '/';
		e = strchr(e + 1, '\\');
	}

	e = path;

	if (start_at_offset == 0)
	{
		int can_have_mswin_drive_letter = 1;

		// Sanitize the *entire* path, including the Windows Drive/Share part:
		//
		// check if path is a UNC path. It may legally start with `\\.\` or `\\?\` before a Windows drive/share+COLON:
		if (e[0] == '/' && e[1] == '/')
		{
			if (strchr(".?", e[2]) != NULL && e[3] == '/')
			{
				// skip //?/ and //./ UNC path leaders
				e += 4;
				can_have_mswin_drive_letter = -1;
			}
			else
			{
				// skip //<server>... UNC path starter (which cannot contain Windows drive letters as-is)
				char* p = e + 2;
				while (isalnum(*p) || strchr("_-$", *p))
					p++;
				if (p > e && *p == '/' && p[1] != '/')
					p++;
				else
				{
					// no legal UNC domain name, hence no UNC at all: assume it's a simple UNIX '/' root directory
					p = e + 1;
				}
				e = p;
				can_have_mswin_drive_letter = 0;
			}
		}
		else if (e[0] == '.')
		{
			can_have_mswin_drive_letter = 0;

			if (e[1] == '/')
			{
				// skip ./ relative path at start, replace it by NIL: './' is superfluous!
				size_t len = strlen(e + 2);
				memmove(e, e + 2, len + 1);
			}
			else if (e[1] == '.' && e[2] == '/')
			{
				// skip any chain of ../ relative path elements at start, as this may be a valid relative path
				e += 3;

				while (e[0] == '.' && e[1] == '.' && e[2] == '/')
				{
					e += 3;
				}
			}
		}
		else if (e[0] == '/')
		{
			// basic UNIX '/' root path:
			e++;
		}

		if (can_have_mswin_drive_letter)
		{
			// See if we have a Windows drive as part of the path: keep that one intact!
			if (isalpha(e[0]) && e[1] == ':')
			{
				// while users can specify relative paths within drives, e.g. `C:path`, they CANNOT do so when this is a UNC path.
				if (e[2] != '/' && can_have_mswin_drive_letter < 0)
				{
					// not a legal UNC path after all
				}
				else
				{
					e[0] = toupper(e[0]);
					e += 2;
				}
			}
		}

		// when we get here, `e` will point PAST the previous '/', so we can ditch any leading '/' below, before we sanitize the remainder.
	}
	else
	{
		// when the caller specified an OFFSET, we assume we're pointing PAST the previous '/', but we better make sure
		// as we MAY have been pointed AT the separating '/' instead...
		if (e[0] == '/')
		{
			if (e[-1] != '/')
			{
				// skip this '/' directory separator before we start for real:
				e++;
			}
		}
	}

	char* e_start = e;
	char* cur_segment_start = e;
	char* d = e;

	// skip leading surplus '/'
	while (e[0] == '/')
		e++;

	// calculate a simple & fast hash of the remaining path:
	uint32_t hash = calchash(e);

	char* p = e;

	// now go and scan/clean the rest of the path spec:
	int repl_seq_count = 0;

	while (*p)
	{
		while (repl_seq_count > 1)
		{
			if (d[-1] != d[-2])
				break;
			d--;
			repl_seq_count--;
		}
		// ^^^ note that we do this step-by-step this way, slowly eating the
		// replacement series. The benefit of this approach, however, is that
		// we don't have to bother the rest of the code/loop with this,
		// making the code easier to review. Performance is still good enough.

		char c = *p++;
		if (c == '/')
		{
			*d = 0;
			if (is_reserved_filename(cur_segment_start))
			{
				// previous part of the path isn't allowed: replace by a hash-based name instead.
				char buf[32];
				size_t max_width = p - cur_segment_start - 1;

				const char* old_cleaned_fname = rigorously_clean_fname(cur_segment_start);
				size_t fnlen = strlen(old_cleaned_fname);

				int rslen = max_width;
				rslen -= 4 + 8;
				if (rslen > fnlen)
					rslen = fnlen;

				if (rslen >= 0)
				{
					snprintf(buf, sizeof(buf), "_H%08X_%s", (unsigned int)hash, old_cleaned_fname + fnlen - rslen);
					buf[sizeof(buf) - 1] = 0;
				}
				else
				{
					snprintf(buf, sizeof(buf), "H%08X", (unsigned int)hash);
					buf[max_width] = 0;
				}
				strcpy(cur_segment_start, buf);
				d = cur_segment_start + strlen(cur_segment_start);
			}
			else
			{
				size_t sslen = strlen(cur_segment_start);

				// limit to UNIX fname length limit:
				if (sslen > 255)
				{
					char buf[256];

					const char* old_cleaned_fname = rigorously_clean_fname(cur_segment_start);
					size_t fnlen = strlen(old_cleaned_fname);

					int rslen = 255 - 10;
					if (rslen > fnlen)
						rslen = fnlen;

					snprintf(buf, sizeof(buf), "_H%08X_%s", (unsigned int)hash, old_cleaned_fname + fnlen - rslen);
					buf[sizeof(buf) - 1] = 0;
					strcpy(cur_segment_start, buf);
					d = cur_segment_start + strlen(cur_segment_start);
				}
			}

			// keep path separators intact at all times.
			*d++ = c;

			cur_segment_start = d;

			// skip superfluous addititional '/' separators:
			while (*p == '/')
				p++;

			repl_seq_count = 0;
			continue;
		}

		// custom set replacements for printf-style format bits, anyone?
		if (has_printf_format_repl_idx1 && c == '%')
		{
			// replace any %[+/-/ ][0-9.*][dlfgespuizxc] with a single replacement character
			while (*p && strchr("+- .0123456789*", *p))
				p++;
			if (*p && strchr("lz", *p))
				p++;
			if (*p && strchr("dlfgespuizxc", *p))
				p++;

			// custom 1:1 replacement: set -> replace_single.
			*d++ = replace_single[has_printf_format_repl_idx1 - 1];
			repl_seq_count++;
			continue;
		}

		// custom set replacements, anyone?
		const char* m = strchr(set, c);
		if (m)
		{
			// custom replacement for fz_format / printf formatters:
			if (c == '#')
			{
				// replace ream of '#'s with a single replacement character
				while (*p == '#')
					p++;
			}

			// custom 1:1 replacement: set -> replace_single.
			int idx = m - set;
			// pick last in map when we're out-of-bounds:
			if (idx >= repl_map_len)
				idx = repl_map_len - 1;
			*d++ = replace_single[idx];
			repl_seq_count++;
			continue;
		}

		// regular sanitation of the path follows
		//
		// reject ASCII control chars.
		// also reject any non-UTF8-legal byte (the red slots in https://en.wikipedia.org/wiki/UTF-8#Codepage_layout)
		//    if (c < ' ' || c == 0x7F /* DEL */ || strchr("\xC0\xC1\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF", c))
		// in fact, check for *proper UTF8 encoding* and replace all illegal code points:
		if (c > 0x7F || c < 0) {
			// 0x80 and higher character codes: UTF8
			int u;
			int l = fz_chartorune_unsafe(&u, p - 1);
			if (u == Runeerror) {
				// bad UTF8 is to be discarded!
				*d++ = '_';
				repl_seq_count++;
				continue;
			}

			// Only accept Letters and Numbers in the BMP:
			if (u <= 65535)
			{
				int pos = u / 32;
				int bit = u % 32;
				uint32_t mask = 1U << bit;
				if (legal_codepoints_bitmask[pos] & mask)
				{
					// Unicode BMP: L (Letter) or N (Number)
					// --> keep Unicode UTF8 codepoint:
					p--;
					for (; l > 0; l--)
						*d++ = *p++;
					repl_seq_count = 0;
					continue;
				}
			}

			// undesirable UTF8 codepoint is to be discarded!
			*d++ = '_';
			p += l - 1;
			repl_seq_count++;
			continue;
		}
		else if (c < ' ' || c == 0x7F /* DEL */)
		{
			*d++ = '_';
			repl_seq_count++;
			continue;
		}
		else if (strchr("`\"*@=|;?:", c))
		{
			// replace NTFS-illegal character path characters.
			// replace some shell-scripting-risky character path characters.
			// replace the usual *wildcards* as well.
			*d++ = '_';
			repl_seq_count++;
			continue;
		}
		else
		{
			static const char* braces = "{}[]<>";
			const char* b = strchr(braces, c);

			if (b)
			{
				// replace some shell-scripting-risky brace types as well: all braces are transmuted to `()` for safety.
				int idx = b - braces;
				*d++ = "()"[idx % 2];
				repl_seq_count++;
				continue;
			}
			else if (c == '.')
			{
				// a dot at the END of a path element is not accepted; neither do we tolerate ./ and ../ as we have
				// skipped any legal ones already before.
				//
				// we assume the path has been normalized (relative or absolute) before it was sent here, hence
				// we'll accept UNIX dotfiles (one leading '.' dot for an otherwise sane filename/dirname), but
				// otherwise dots can only sit right smack in the middle of an otherwise legal filename/dirname:
				if (!p[0] || p[0] == '.' || p[0] == '/')
				{
					*d++ = '_';
					repl_seq_count++;
					continue;
				}
				// we're looking at a plain-as-Jim vanilla '.' dot here: pass as-is:
				*d++ = c;
				repl_seq_count = 0;
				continue;
			}
			else if (c == '$')
			{
				// a dollar is only accepted in the middle of an element in order to prevent NTFS special filename attacks.
				if (!p[0] || p[0] == '/')
				{
					*d++ = '_';
					repl_seq_count++;
					continue;
				}
				else if (d == e_start || strchr(":/", d[-1]))
				{
					*d++ = '_';
					repl_seq_count++;
					continue;
				}

				// we're looking at a plain-as-Jim vanilla '$' dollar here: pass as-is:
				*d++ = c;
				repl_seq_count = 0;
				continue;
			}
			else if (isalnum(c))
			{
				// we're looking at a plain-as-Jim vanilla character here: pass as-is:
				*d++ = c;
				repl_seq_count = 0;
				continue;
			}
			else if (c == '-')
			{
				// Furthermore it's a **bad idea** to start any filename with `"-"` or `"--"` 
				// as this clashes very badly with standard UNIX commandline options; 
				// only a few tools "fix" this by allowing a special `"--"` commandline option.
				//
				// We also don't like filenames which end with '-', but that's more an aesthetical thing...
				//
				// Hence we nuke such filenames:
				if (!p[0] || p[0] == '/')
				{
					*d++ = '_';
					repl_seq_count++;
					continue;
				}
				else if (d == cur_segment_start)
				{
					*d++ = '_';
					repl_seq_count++;
					continue;
				}

				// we're looking at a plain-as-Jim vanilla '-' dash character here: pass as-is:
				*d++ = c;
				repl_seq_count = 0;
				continue;
			}
			else if (strchr("_()", c))
			{
				// we're looking at a plain-as-Jim vanilla character here: pass as-is:
				*d++ = c;
				repl_seq_count = 0;
				continue;
			}
			else
			{
				// anything else is considered illegal / bad for our FS health:
				*d++ = '_';
				repl_seq_count++;
				continue;
			}
		}
	}

	// and print the sentinel
	*d = 0;

	return 0;
}

// Note:
// both abspaths are assumed to be FILES (not directories): the filename
// at the end of `relative_to_abspath` therefor is not considered; only the
// directory part of that path is compared against.
char* fz_mk_relative_path(fz_context* ctx, char* dst, size_t dstsiz, const char* abspath, const char* relative_to_abspath)
{
	// tactic:
	// - determine common prefix
	// - see what's left and walk up that dirchain, producing '..' elements.
	// - construct the relative path by merging all.
	// When the prefix length is zero or down to drive-root directory (MS Windows)
	// there's not much 'relative' to walk and the result therefor is the abs.path
	// itself.
	size_t len_of_cmp_dirpath = fz_strrcspn(relative_to_abspath, ":/\\");
	size_t len_of_cmp_drivespec = fz_strrcspn(relative_to_abspath, "?:") + 1;   // Match UNC and classic paths both
	size_t common_prefix_len = 0;

	for (size_t pos = 0; pos < len_of_cmp_dirpath + 1; pos++)
	{
		char c1 = abspath[pos];
		char c2 = relative_to_abspath[pos];

		// check for MSWin vs. UNIXy directory separator style mashups before ringing the bell:
		if (c1 == '\\')
			c1 = '/';
		if (c2 == '\\')
			c2 = '/';
		if (c1 != c2)
			break;

		// when we have a match and it's a directory '/', we have a preliminary common_prefix:
		if (c1 == '/')
		{
			common_prefix_len = pos;
		}
	}

	char dotdot[PATH_MAX] = "";
	char* d = dotdot;
	// find out how many segments we have to walk up the dirtree:
	if (relative_to_abspath[common_prefix_len] && len_of_cmp_dirpath > common_prefix_len)
	{
		const char* sep = relative_to_abspath + common_prefix_len;
		do
		{
			*d++ = '.';
			*d++ = '.';
			*d++ = '/';
			sep = strpbrk(sep + 1, "\\/");
		} while (sep);
		*d++ = '.';
		*d++ = '.';
		*d++ = '/';
		*d = 0;
		ASSERT((d - dotdot) < sizeof(dotdot));
	}
	// is this a 'sensible' common prefix, i.e. one where both paths are on the same drive?
	// (We're okay with having to walk the chain all the way back to the drive/root-directory, though...)
	if (common_prefix_len >= len_of_cmp_drivespec)
	{
		const char* remainder = abspath + common_prefix_len;
		if (remainder[0])
		{
			// skip initial '/'
			remainder++;
		}
		// when, despite assumptions, we happen to be looking at the fringe case where `abspath` was
		// a subpath (parent) of `relative_to_abspath`, then we wish to output without a trailing '/'
		// anyway, i.e. a path that only consists of '..' directory elements:
		if (!remainder[0])
		{
			if (d != dotdot)
			{
				// end on a '.', not a '/':
				d[-1] = 0;
			}
		}
		fz_snprintf(dst, dstsiz, "%s%s", dotdot, remainder);
	}
	else
	{
		fz_strncpy_s(ctx, dst, abspath, dstsiz);
	}
	return dst;
}

void fz_mk_absolute_path_using_absolute_base(fz_context* ctx, char* dst, size_t dstsiz, const char* source_path, const char* abs_optional_base_file_path)
{
	ASSERT(source_path);
	ASSERT(abs_optional_base_file_path);

	if (fz_is_absolute_path(source_path))
	{
		fz_strncpy_s(ctx, dst, source_path, dstsiz);
		fz_normalize_path(ctx, dst, dstsiz, dst);
		fz_sanitize_path(ctx, dst, dstsiz, dst);
		return;
	}

	// relative paths are assumed relative to the *base file path*:
	//
	// NOTE: the 'base file path' is assumed to be a FILE path, NOT a DIRECTORY, so we need to get rid of that
	// 'useless' base filename: we do that by appending a '../' element and have the sanitizer deal with it
	// afterwards: the '../' will eat the preceding element, which, in this case, will be the 'base file name'.
	fz_snprintf(dst, dstsiz, "%s/../%s", abs_optional_base_file_path, source_path);
	fz_normalize_path(ctx, dst, dstsiz, dst);
	fz_sanitize_path(ctx, dst, dstsiz, dst);

#if 0 // as abs_optional_base_path is already to be absolute and sanitized, we don't need fz_realpath() in here
	char abspath[PATH_MAX];
	if (!fz_realpath(dst, abspath))
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot process relative (based) file path to a sane absolute path: rel.path: %q, abs.base: %q, dst: %q", source_path, abs_optional_base_file_path, dst);
	}
	fz_normalize_path(ctx, abspath, sizeof(abspath), abspath);
	fz_sanitize_path(ctx, abspath, sizeof(abspath), abspath);
	fz_strncpy_s(ctx, dst, abspath, dstsiz);
#endif
}

int fz_is_absolute_path(const char* path)
{
	// anything absolute is either a UNIX root directory '/', an UNC path or prefix '//yadayada' or a classic MSWindows drive letter.
	return (path && (
		(path[0] == '/') ||
		(path[0] == '\\') ||
		(isalpha(path[0]) && path[1] == ':' && strchr("/\\", path[2]) != NULL)
		));
}

#if defined(_WIN32)

static int fz_is_UNC_path(const wchar_t* path)
{
	// anything with prefix '//yadayada'
	return (path && (
		(path[0] == '/') ||
		(path[0] == '\\')
		) && (
			(path[1] == '/') ||
			(path[1] == '\\')
			) && (
				isalnum(path[2]) ||
				path[2] == '$' ||
				path[2] == '.' ||
				path[2] == '?'
				));
}

static wchar_t* fz_UNC_wfullpath_from_name(fz_context* ctx, const char* path)
{
	if (!path) {
		errno = EINVAL;
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return NULL;
	}

	// don't use PATH_MAX fixed-sized arrays as the input path MAY be larger than this!
	size_t slen = strlen(path);
	size_t wlen = slen + 5;
	wchar_t* wpath = fz_malloc(ctx, wlen * sizeof(wpath[0]));
	wchar_t* wbuf = fz_malloc(ctx, wlen * sizeof(wbuf[0]));

	if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen))
	{
	err:
		fz_copy_ephemeral_system_error(ctx, GetLastError(), NULL);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		fz_free(ctx, wbuf);
		fz_free(ctx, wpath);
		return NULL;
	}
	for (;;) {
		size_t reqd_len = GetFullPathNameW(wpath, wlen - 4, wbuf + 4, NULL);

		if (!reqd_len)
		{
			fz_copy_ephemeral_system_error(ctx, GetLastError(), NULL);
			ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
			goto err;
		}
		if (reqd_len < wlen - 4) {
			// buffer was large enough; path has been rewritten in wbuf[4]+
			break;
		}
		wlen = reqd_len + 7;
		wbuf = fz_realloc(ctx, wbuf, wlen * sizeof(wbuf[0]));
	}

	const wchar_t* fp = wbuf + 4;
	// Is full path an UNC path already? If not, make it so:
	if (!fz_is_UNC_path(wbuf + 4))
	{
		wbuf[0] = '\\';
		wbuf[1] = '\\';
		wbuf[2] = '?';
		wbuf[3] = '\\';
	}
	else
	{
		size_t dlen = wcslen(wbuf + 4) + 1;
		memmove(wbuf, wbuf + 4, dlen * sizeof(wbuf[0]));
	}
	fz_free(ctx, wpath);
	return wbuf;
}

#endif

int fz_chdir(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	wchar_t* wname = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wname)
	{
		return -1;
	}

	// remove trailing / dir separator, if any...
	size_t len = wcslen(wname);
	if (wname[len - 1] == '\\')
	{
		if (wname[len - 2] != ':')
			wname[len - 1] = 0;
	}

	if (_wchdir(wname))
#else
	if (chdir(path))
#endif
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);

		if (fz_ctx_get_rtl_errno(ctx) == ENOENT)
		{
			fz_freplace_ephemeral_system_error(ctx, 0, "chdir: Unable to locate the directory: %s", path);
		}
#if defined(_WIN32)
		fz_free(ctx, wname);
#endif
		return -1;
	}

#if defined(_WIN32)
	fz_free(ctx, wname);
#endif
	return 0;
}

void fz_mkdir_for_file(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	char* buf = fz_strdup(ctx, path);
	ASSERT(buf);

	// unixify MSDOS path:
	char* e = strchr(buf, '\\');
	while (e)
	{
		*e = '/';
		e = strchr(e, '\\');
	}
	e = strrchr(buf, '/');

	if (e)
	{
		// strip off the *filename*: keep the (nested?) path intact for recursive mkdir:
		*e = 0;

		int rv = fz_mkdirp_utf8(ctx, buf);
		if (rv)
		{
			rv = fz_ctx_get_rtl_errno(ctx);
			if (rv != EEXIST)
			{
				const char* errmsg = fz_ctx_get_system_errormsg(ctx);
				fz_info(ctx, "mkdirp --> mkdir(%s) --> (%d) %s\n", buf, rv, errmsg);
			}
		}
	}

	fz_free(ctx, buf);
#else
	char* buf = fz_strdup(ctx, path);
	ASSERT(buf);

	// unixify MSDOS path:
	char* e = strchr(buf, '\\');
	while (e)
	{
		*e = '/';
		e = strchr(e, '\\');
	}
	e = strrchr(buf, '/');

	if (e)
	{
		// strip off the *filename*: keep the (nested?) path intact for recursive mkdir:
		*e = 0;

		// construct the path:
		for (e = strchr(buf, '/'); e; e = strchr(e + 1, '/'))
		{
			*e = 0;
			// do not mkdir UNIX or MSDOS/WIN/Network root directory:
			if (e == buf || e[-1] == ':' || e[-1] == '/')
			{
				*e = '/';
				continue;
			}
			// just bluntly attempt to create the directory: we don't care if it fails.
			int rv = mkdir(buf);
			if (rv)
			{
				fz_copy_ephemeral_errno(ctx);
				ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
				rv = fz_ctx_get_rtl_errno(ctx);
				if (rv != EEXIST)
				{
					const char* errmsg = fz_ctx_get_system_errormsg(ctx);
					fz_info(ctx, "mkdirp --> mkdir(%s) --> (%d) %s\n", buf, rv, errmsg);
				}
			}
			*e = '/';
		}
	}
	fz_free(ctx, buf);
#endif
}


int64_t
fz_stat_ctime(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	struct _stat info;
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
		return 0;

	int n = _wstat(wpath, &info);
	if (n)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		fz_free(ctx, wpath);
		return 0;
	}

	fz_free(ctx, wpath);
	return info.st_ctime;
#else
	struct stat info;
	if (stat(path, &info))
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return 0;
	}
	return info.st_ctime;
#endif
}

int64_t
fz_stat_mtime(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	struct _stat info;
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
		return 0;

	int n = _wstat(wpath, &info);
	if (n)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		fz_free(ctx, wpath);
		return 0;
	}

	fz_free(ctx, wpath);
	return info.st_mtime;
#else
	struct stat info;
	if (stat(path, &info))
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return 0;
	}
	return info.st_mtime;
#endif
}

int64_t
fz_stat_atime(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	struct _stat info;
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
		return 0;

	int n = _wstat(wpath, &info);
	if (n)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		fz_free(ctx, wpath);
		return 0;
	}

	fz_free(ctx, wpath);
	return info.st_atime;
#else
	struct stat info;
	if (stat(path, &info))
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return 0;
	}
	return info.st_atime;
#endif
}

int64_t
fz_stat_size(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	struct _stat info;
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
		return 0;

	int n = _wstat(wpath, &info);
	if (n)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		fz_free(ctx, wpath);
		return -1;
	}

	fz_free(ctx, wpath);

	// directories should be reported as size=0 or 1...

	if (info.st_mode & _S_IFDIR)
		return 0;
	if (info.st_mode & _S_IFREG)
		return info.st_size;
	return 0;
#else
	struct stat info;
	if (stat(path, &info))
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return -1;
	}
	if (info.st_mode & _S_IFDIR)
		return 0;
	if (info.st_mode & _S_IFREG)
		return info.st_size;
	return 0;
#endif
}

int
fz_path_is_regular_file(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	struct _stat info;
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
		return FALSE;

	int n = _wstat(wpath, &info);
	if (n)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		fz_free(ctx, wpath);
		return FALSE;
	}

	fz_free(ctx, wpath);

	// directories should be reported as size=0 or 1...

	return (info.st_mode & _S_IFREG) && !(info.st_mode & _S_IFDIR);
#else
	struct stat info;
	if (stat(path, &info))
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return FALSE;
	}

	return (info.st_mode & _S_IFREG) && !(info.st_mode & _S_IFDIR);
#endif
}

int
fz_path_is_directory(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	struct _stat info;
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
		return FALSE;

	int n = _wstat(wpath, &info);
	if (n)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		fz_free(ctx, wpath);
		return FALSE;
	}

	fz_free(ctx, wpath);

	// directories should be reported as size=0 or 1...

	return !!(info.st_mode & _S_IFDIR);
#else
	struct stat info;
	if (stat(path, &info))
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return FALSE;
	}

	return !!(info.st_mode & _S_IFDIR);
#endif
}


FILE*
fz_fopen_utf8(fz_context* ctx, const char* path, const char* mode)
{
#if defined(_WIN32)
	wchar_t* wmode;
	FILE* file;
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
	{
		return NULL;
	}

	wmode = fz_wchar_from_utf8(ctx, mode);
	if (wmode == NULL)
	{
		fz_free(ctx, wpath);
		return NULL;
	}

	file = _wfopen(wpath, wmode);
	if (!file)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
	}

	fz_free(ctx, wmode);
	fz_free(ctx, wpath);

	return file;
#else
	if (path == NULL)
	{
		errno = EINVAL;
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return NULL;
	}

	if (mode == NULL)
	{
		errno = EINVAL;
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return NULL;
	}

	FILE* rv = fopen(path, mode);
	if (!rv)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return NULL;
	}
	return rv;
#endif
}

int
fz_remove_utf8(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	int n;
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
	{
		return -1;
	}

	n = _wremove(wpath);
	if (n)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		n = -1;
	}

	return n;
#else
	if (path == NULL)
	{
		errno = EINVAL;
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return -1;
	}

	int rv = remove(path);
	if (rv)
	{
		fz_copy_ephemeral_errno(ctx);
		ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
		return -1;
	}
	return 0;
#endif
}

int
fz_mkdirp_utf8(fz_context* ctx, const char* path)
{
#if defined(_WIN32)
	wchar_t* wpath = fz_UNC_wfullpath_from_name(ctx, path);

	if (!wpath)
	{
		return -1;
	}

	// as this is now an UNC path, we can only mkdir beyond the drive letter:
	wchar_t* p = wpath + 4;
	wchar_t* q = wcschr(p, ':');
	if (!q)
		q = p;
	wchar_t* d = wcschr(q, '\\'); // drive rootdir
	if (d)
		d = wcschr(d + 1, '\\'); // first path level
	if (d == NULL)
	{
		// apparently, there's only one directory name above the drive letter specified. We still need to mkdir that one, though.
		d = q + wcslen(q);
	}

	int rv = 0;

	for (;;)
	{
		wchar_t c = *d;
		*d = 0;

		int n = _wmkdir(wpath);
		int e = errno;
		if (n && e != EEXIST)
		{
			fz_copy_ephemeral_errno(ctx);
			ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
			rv = -1;
		}

		*d = c;
		// did we reach the end of the *original* path spec?
		if (!c)
			break;
		q = wcschr(d + 1, '\\');
		if (q)
			d = q;
		else
			d += wcslen(d);  // make sure the sentinel-patching doesn't damage the last part of the original path spec
	}

	fz_free(ctx, wpath);
	return rv;
#else
	char* pname;

	pname = fz_strdup_no_throw(ctx, path);
	if (pname == NULL)
	{
		errno = ENOMEM;
		return -1;
	}

	// we can only mkdir beyond the (optional) root directory:
	char* p = pname + 1;
	char* d = strchr(p, '/'); // drive rootdir
	if (d == NULL)
	{
		// apparently, there's only one directory name above the drive letter specified. We still need to mkdir that one, though.
		d = p + strlen(p);
	}

	for (;;)
	{
		char c = *d;
		*d = 0;

		int n = mkdir(pname);
		int e = errno;
		if (n && e != EEXIST && e != EACCES)
		{
			fz_copy_ephemeral_errno(ctx);
			ASSERT(fz_ctx_get_system_errormsg(ctx) != NULL);
			fz_free(ctx, pname);
			return -1;
		}
		*d = c;
		// did we reach the end of the *original* path spec?
		if (!c)
			break;
		q = strchr(d + 1, '/');
		if (q)
			d = q;
		else
			d += strlen(d);  // make sure the sentinel-patching doesn't damage the last part of the original path spec
	}

	fz_free(ctx, pname);
	return 0;
#endif
}


int
fz_mkdir(fz_context* ctx, const char* path)
{
#ifdef _WIN32
	int ret;
	wchar_t* wpath = fz_wchar_from_utf8(ctx, path);

	if (wpath == NULL)
		return -1;

	ret = _wmkdir(wpath);

	fz_free(ctx, wpath);

	return ret;
#else
	return mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
}


#pragma message("TODO: code review re ephemeral errorcode handling")




































//---------------------------------------------------------------------------------















/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include "tool_setup.h"

#ifdef HAVE_FCNTL_H
 /* for open() */
#include <fcntl.h>
#endif

#include <sys/stat.h>

#include "curlx.h"

#include "tool_cfgable.h"
#include "tool_msgs.h"
#include "tool_cb_wrt.h"
#include "tool_dirhie.h"
#include "tool_getenv.h"
#include "tool_operate.h"
#include "tool_doswin.h"
#include "escape.h"
#include "sendf.h" /* for infof function prototype */

#include "memdebug.h" /* keep this as LAST include */

#ifdef _WIN32
#include <direct.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifdef _WIN32
#define OPENMODE S_IREAD | S_IWRITE
#else
#define OPENMODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
#endif

static char* find_beyond_all(char* s, const char* set)
{
	while (*set) {
		char* p = strrchr(s, *set);
		if (p)
			s = p + 1;
		set++;
	}
	return s;
}

// Produce a filename extension based on the mimetype
// reported by the server response. As this bit can be adversarial as well, we keep our
// sanity about it by restricting the length of the extension.
static char* get_file_extension_for_response_content_type(char* fname, struct per_transfer* per) {
	struct OperationConfig* config;

	DEBUGASSERT(per);
	config = per->config;
	DEBUGASSERT(config);

	CURL* curl = per->curl;
	DEBUGASSERT(curl);

	char* ctype = NULL;
	curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ctype);

	// construct a sane extension from the mime type if the filename doesn't have a porper extension yet.
	if (ctype) {
		static const char* mime_categories[] = {
				"text",
				"image",
				"video",
				"audio",
				"application"
		};

		char new_ext[16] = "";

		// TODO: map known mime types, e.g. text/javascript, to extension, without using the heuristic code below.

		for (int i = 0; i < sizeof(mime_categories) / sizeof(mime_categories[0]); i++) {
			const int cat_len = strlen(mime_categories[i]);
			if (!strncmp(mime_categories[i], ctype, cat_len)) {
				const char* mime_ext = ctype + cat_len;
				if (*mime_ext == '/') {
					mime_ext++;
					// remove possible 'x-' and 'vnd.' prefixes:
					if (!strncmp("x-", mime_ext, 2)) {
						mime_ext += 2;
					}
					if (!strncmp("vnd.", mime_ext, 4)) {
						mime_ext += 4;
					}

					// strip off ';charset=...' bits and such-like:
					strncpy(new_ext, mime_ext, sizeof(new_ext));
					new_ext[sizeof(new_ext) - 1] = 0;

					mime_ext = new_ext;
					char* m_end = strchr(mime_ext, ';');
					if (m_end) {
						while (m_end > mime_ext) {
							if (isspace(m_end[-1])) {
								m_end--;
								continue;
							}
							*m_end = 0;
							break;
						}
					}

					char* m_last = strrchr(mime_ext, '-');
					if (m_last) {
						mime_ext = m_last + 1;
					}
					m_end = strchr(mime_ext, '.');
					if (m_end) {
						m_end[0] = 0;
					}
					size_t cp_len = strlen(mime_ext);
					if (cp_len > sizeof(new_ext) - 1) {
						// 'extension' too long: forget it!
						new_ext[0] = 0;
						break;
					}
					if (mime_ext != new_ext) {
						memmove(new_ext, mime_ext, strlen(mime_ext) + 1);
					}
					break;
				}
			}
		}

		// sanitize new_ext: we are only interested in derived extensions containing letters and numbers:
		// e.g. 'html, 'mp3', ...
		for (const char* p = new_ext; *p; p++) {
			if (*p >= '0' && *p <= '9')
				continue;
			if (*p >= 'a' && *p <= 'z')
				continue;
			if (*p >= 'A' && *p <= 'Z')
				continue;
			// bad character encountered: nuke the entire extension!
			new_ext[0] = 0;
			break;
		}

		strlwr(new_ext);    // lowercase extension for convenience

		// do we have a non-empty filename extension?
		if (new_ext[0]) {
			return strdup(new_ext);
		}
	}

	return NULL;
}

/* sanitize a local file for writing, return TRUE on success */
bool tool_sanitize_output_file_path(struct per_transfer* per)
{
	struct GlobalConfig* global;
	struct OperationConfig* config;
	char* fname = per->outfile;
	char* aname = NULL;

	DEBUGASSERT(per);
	config = per->config;
	DEBUGASSERT(config);

	CURL* curl = per->curl;
	DEBUGASSERT(curl);

	global = config->global;

	const char* outdir = config->output_dir;
	int outdir_len = (outdir ? strlen(outdir) : 0);
	int starts_with_outdir = (outdir && strncmp(outdir, fname, outdir_len) == 0 && strchr("\\/", fname[outdir_len]));

	if (starts_with_outdir) {
		fname += outdir_len + 1; // skip path separator as well
	}

	const char* __hidden_prefix = "";

	if (config->sanitize_with_extreme_prejudice) {
		// config->failwithbody ?

		__hidden_prefix = "___";

		// - if filename is empty (or itself a directory), then we create a filename after the fact.
		// - if the filename is 'hidden' (i.e. starts with a '.'), the filename is *unhiddden.
		// - if the filename does not have an extension, an extension will be added, based on the mimetype
		//   reported by the server response. As this bit can be adversarial as well, we keep our
		//   sanity about it by restricting the length of the extension.

		// unescape possibly url-escaped filename for our convenience:
		{
			size_t len = strlen(fname);
			char* fn2 = NULL;
			if (CURLE_OK != Curl_urldecode(fname, len, &fn2, &len, SANITIZE_CTRL)) {
				errorf(global, "failure during filename sanitization: out of memory?\n");
				return FALSE;
			}

			if (CURL_SANITIZE_ERR_OK != curl_sanitize_file_name(&fname, fn2, CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH)) {
				errorf(global, "failure during filename sanitization: out of memory?\n");
				return FALSE;
			}
		}
	}
	else {   // !config->sanitize_with_extreme_prejudice

		// prep for free(fname) + free(per->outfile) afterwards: prevent double free when we travel this branch.
		//per->outfile = NULL;

		// - if the filename does not have an extension, an extension will be added, based on the mimetype
		//   reported by the server response. As this bit can be adversarial as well, we keep our
		//   sanity about it by restricting the length of the extension.
	}

	char* fn = find_beyond_all(fname, "\\/:");
	int fn_offset = (int)(fn - fname);

	bool empty = !*fn;
	bool hidden = (*fn == '.');

	// We would like to derive a 'sane' filename extension from the server-reported mime-type
	// when our current filename has NO extension.
	// We ALSO benefit from doing this when the actual filename has a 'nonsense extension',
	// which can happen due to the filename having been derived off the request URL, where
	// you might get something like:
	//     https://dl.acm.org/doi/abs/10.1145/3532342.3532351
	// and you would thus end up with thee 'nonsense filename':
	//     3532342.3532351
	// where appending a 'sane' mime-type based extension MIGHT help.
	//
	// However, we MUST defer fixing that until we have the MIME-type info from the
	// server's response headers or similar server-side info, which is info we currently
	// do NOT YET have. Until that time, we live with the fact that this here file don't have
	// a preferred file extension yet. ;-)

	aname = aprintf("%s%s%.*s%s%s", (starts_with_outdir ? outdir : ""), (starts_with_outdir ? "/" : ""),
		fn_offset, fname, (hidden ? __hidden_prefix : ""), (empty ? "__download__" : fn));
	if (!aname) {
		errorf(global, "out of memory\n");
		if (fname != per->outfile)
			free(fname);
		return FALSE;
	}
	if (fname != per->outfile)
		free(fname);
	free(per->outfile);
	per->outfile = aname;

	if (!aname || !*aname) {
		warnf(global, "Remote filename has no length");
		return FALSE;
	}

	return TRUE;
}


/*
 * Return an allocated new path with file extension affixed (or not, if it wasn't deemed necessary). Return NULL on error.
 *
 * Do note that the file path given has already been sanitized, so no need for that again!
 */
static char* tool_affix_most_suitable_filename_extension(char* fname, struct per_transfer* per)
{
	struct GlobalConfig* global;
	struct OperationConfig* config;
	char* aname = NULL;

	DEBUGASSERT(per);
	config = per->config;
	DEBUGASSERT(config);

	CURL* curl = per->curl;
	DEBUGASSERT(curl);

	global = config->global;

	const char* __unknown__ext = NULL;

	if (config->sanitize_with_extreme_prejudice) {
		// config->failwithbody ?

		__unknown__ext = "unknown";
	}

	char* fn = find_beyond_all(fname, "\\/:");

	bool hidden = (*fn == '.');
	const char* ext = strrchr(fn + hidden, '.');

	int fn_length = (ext ? (ext - fname) : INT_MAX);

	// We would like to derive a 'sane' filename extension from the server-reported mime-type
	// when our current filename has NO extension.
	// We ALSO benefit from doing this when the actual filename has a 'nonsense extension',
	// which can happen due to the filename having been derived off the request URL, where
	// you might get something like:
	//     https://dl.acm.org/doi/abs/10.1145/3532342.3532351
	// and you would thus end up with thee 'nonsense filename':
	//     3532342.3532351
	// where appending a 'sane' mime-type based extension MIGHT help:

	if (ext) {
		ext++;    // skip dot

		// sanitize ext: we are only interested in derived extensions containing letters and numbers:
		// e.g. 'html, 'mp3', ...
		for (const char* p = ext; *p; p++) {
			if (*p >= '0' && *p <= '9')
				continue;
			if (*p >= 'a' && *p <= 'z')
				continue;
			if (*p >= 'A' && *p <= 'Z')
				continue;
			// bad character encountered: nuke the entire extension!
			ext = NULL;
			break;
		}
		if (!ext[0])
			ext = NULL;
	}

	const char* new_ext = get_file_extension_for_response_content_type(fn, per);

	// when the server gave us a sensible extension through MIME-type info or otherwise, that one prevails over
	// the current ëxtension"the filename may or may not have:
	if (!ext || !new_ext) {
		// when we could not determine a proper & *sane* filename extension from the mimetype, we simply resolve to '.unknown' / *empty*, depending on configuration.
		if (!new_ext) {
			ext = __unknown__ext;
		}
		else {
			ext = new_ext;
		}

		if (!ext)
			ext = "";

		fn_length = INT_MAX;
	}
	else {
		// we already have an extension for the filename, but the mime-type derived one might be 'saner'.
		// There are now 3 scenarios to consider:
		// 1. both extensions are the same (case-*IN*sensitive comparison, because '.PDF' == '.pdf' for our purposes!)
		// 2. the filename extension is 'sane', the mime-derived one isn't so much: keep the filename ext as-is.
		// 3. the filename extension is less 'sane' than the mime-derived one. Append the mime-ext, i.e.
		//    treat the filename extension as part of the filename instead.
		//    e.g. filename="3532342.3532351" --> ext="3532351", mimee-ext="html" --> new filename="3532342.3532351.html"

		if (curl_strequal(new_ext, ext)) {
			ext = new_ext;
		}
		else {
			static const char* preferred_extensions[] = {
					"html",
					"js",
					"css",
					NULL
			};
			bool mime_ext_is_preferred = FALSE;
			for (const char** pe = preferred_extensions; *pe && !mime_ext_is_preferred; pe++) {
				mime_ext_is_preferred = (0 == strcmp(*pe, new_ext));
			}
			DEBUGASSERT(*ext);
			if (!(
				mime_ext_is_preferred ||
				(strlen(ext) >= strlen(new_ext))
				)) {
				// 2. no-op, ergo: keep extension as-is

				free((void*)new_ext);
				ext = new_ext = strdup(ext);
				if (!new_ext) {
					errorf(global, "out of memory\n");
					return NULL;
				}
				strlwr((char*)ext);    // lowercase extension for convenience
			}
			else {
				// 3. drop file ext; use mime ext.
				fn_length = INT_MAX;
				ext = new_ext;
			}
		}
	}

	// also fix 
	aname = aprintf("%.*s%s%s", fn_length, fname, (*ext ? "." : ""), ext);
	if (!aname) {
		errorf(global, "out of memory\n");
		free((void*)new_ext);
		return NULL;
	}
	free((void*)new_ext);

	return aname;
}


/* create/open a local file for writing, return TRUE on success */
bool tool_create_output_file(struct OutStruct* outs,
	struct per_transfer* per)
{
	struct GlobalConfig* global;
	struct OperationConfig* config;
	FILE* file = NULL;
	file_clobber_mode_t clobber_mode;
	int duplicate = 1;
	char* fname = outs->filename;

	if (!fname || !*fname) {
		fname = per->outfile;
	}

	DEBUGASSERT(outs);
	DEBUGASSERT(per);
	config = per->config;
	DEBUGASSERT(config);

	CURL* curl = per->curl;
	DEBUGASSERT(curl);

	global = config->global;

	clobber_mode = config->file_clobber_mode;

	if (config->sanitize_with_extreme_prejudice) {
		// never clobber generated download filenames:
		clobber_mode = CLOBBER_NEVER;
	}

	if (!fname || !*fname) {
		warnf(global, "Remote filename has no length");
		return FALSE;
	}

	// NOW is the time to check server response headers for MIME info, etc.
	// to help affix a most proper filename extension:
	fname = tool_affix_most_suitable_filename_extension(fname, per);
	if (!fname) {
		return FALSE;
	}

	if (outs->is_cd_filename && clobber_mode != CLOBBER_ALWAYS) {
		/* default behaviour: don't overwrite existing files */
		clobber_mode = CLOBBER_NEVER;
	}
	else {
		/* default behaviour: open file for writing (overwrite!) *UNLESS* --noclobber is set */
	}

	if (config->create_dirs) {
		CURLcode result = create_dir_hierarchy(fname, global);
		/* create_dir_hierarchy shows error upon CURLE_WRITE_ERROR */
		if (result) {
			warnf(global, "Failed to create the path directories to file %s: %s", fname,
				strerror(errno));
			free(fname);
			return FALSE;
		}
	}

	if (clobber_mode != CLOBBER_NEVER) {
		/* open file for writing */
		file = fopen(fname, "wb");
	}
	else {
		int fd;
		int fn_ext_pos = 0;
		char* fn = find_beyond_all(fname, "\\/:");
		bool hidden = (*fn == '.');
		char* fn_ext = strrchr(fn + hidden, '.');

		if (!fn_ext) {
			/* filename has no extension */
			fn_ext_pos = strlen(fname);
		}
		else {
			fn_ext_pos = fn_ext - fname;
		}

		fn_ext = strdup(fname + fn_ext_pos);
		if (!fn_ext) {
			errorf(global, "out of memory");
			free(fname);
			return FALSE;
		}

		do {
			fd = open(fname, O_CREAT | O_WRONLY | O_EXCL | O_BINARY, OPENMODE);
			/* Keep retrying in the hope that it is not interrupted sometime */
		} while (fd == -1 && errno == EINTR);
		if (fd == -1) {
			int next_num = 1;
			size_t len = strlen(fname);
			size_t newlen = len + 13; /* nul + 1-11 digits + dot */
			char* newname = NULL;

			/* Guard against wraparound in new filename */
			if (newlen < len) {
				errorf(global, "overflow in filename generation");
				free(fn_ext);
				free(fname);
				return FALSE;
			}

			bool has_risky_filename = hidden;

			while (fd == -1 && /* have not successfully opened a file */
				(errno == EEXIST || errno == EISDIR) &&
				/* because we keep having files that already exist */
				next_num < 100 /* and we have not reached the retry limit */) {
				free(newname);
				newname = aprintf("%.*s%s.%02d%s", fn_ext_pos, fname, (has_risky_filename ? "__hidden__" : ""), next_num, fn_ext);
				if (!newname) {
					errorf(global, "out of memory");
					free(fn_ext);
					free(fname);
					return FALSE;
				}
				next_num++;
				do {
					fd = open(newname, O_CREAT | O_WRONLY | O_EXCL | O_BINARY, OPENMODE);
					/* Keep retrying in the hope that it is not interrupted sometime */
				} while (fd == -1 && errno == EINTR);
			}

			free(fname);
			fname = newname;
		}

		Curl_safefree(fn_ext);

		/* An else statement to not overwrite existing files and not retry with
			 new numbered names (which would cover
			 config->file_clobber_mode == CLOBBER_DEFAULT && outs->is_cd_filename)
			 is not needed because we would have failed earlier, in the while loop
			 and `fd` would now be -1 */
		if (fd != -1) {
			file = fdopen(fd, "wb");
			if (!file)
				close(fd);
		}
	}

	if (!file) {
		warnf(global, "Failed to open the file %s: %s", outs->filename,
			strerror(errno));
		free(fname);
		return FALSE;
	}

	if (outs->alloc_filename)
		free(outs->filename);
	if (fname == per->outfile)
		per->outfile = NULL;
	outs->filename = fname;
	outs->alloc_filename = TRUE;

	if (fname != per->outfile) {
		free(per->outfile);
		per->outfile = strdup(fname);
		if (!per->outfile) {
			errorf(global, "out of memory\n");
			fclose(file);
			return FALSE;
		}
	}

	Curl_infof(per->curl, "Data will be written to output file: %s", per->outfile);

	outs->s_isreg = TRUE;
	outs->fopened = TRUE;
	outs->stream = file;
	outs->bytes = 0;
	outs->init = 0;
	return TRUE;
}

/*
** callback for CURLOPT_WRITEFUNCTION
*/

size_t tool_write_cb(char* buffer, size_t sz, size_t nmemb, void* userdata)
{
	size_t rc;
	struct per_transfer* per = userdata;
	struct OutStruct* outs = &per->outs;
	struct OperationConfig* config = per->config;
	size_t bytes = sz * nmemb;
	bool is_tty = config->global->isatty;
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO console_info;
	intptr_t fhnd;
#endif

#ifdef DEBUGBUILD
	{
		char* tty = curl_getenv("CURL_ISATTY");
		if (tty) {
			is_tty = TRUE;
			curl_free(tty);
		}
	}

	if (config->show_headers) {
		if (bytes > (size_t)CURL_MAX_HTTP_HEADER) {
			warnf(config->global, "Header data size exceeds single call write "
				"limit");
			return CURL_WRITEFUNC_ERROR;
		}
	}
	else {
		if (bytes > (size_t)CURL_MAX_WRITE_SIZE) {
			warnf(config->global, "Data size exceeds single call write limit");
			return CURL_WRITEFUNC_ERROR;
		}
	}

	{
		/* Some internal congruency checks on received OutStruct */
		bool check_fails = FALSE;
		if (outs->filename) {
			/* regular file */
			if (!*outs->filename)
				check_fails = TRUE;
			if (!outs->s_isreg)
				check_fails = TRUE;
			if (outs->fopened && !outs->stream)
				check_fails = TRUE;
			if (!outs->fopened && outs->stream)
				check_fails = TRUE;
			if (!outs->fopened && outs->bytes)
				check_fails = TRUE;
		}
		else {
			/* standard stream */
			if (!outs->stream || outs->s_isreg || outs->fopened)
				check_fails = TRUE;
			if (outs->alloc_filename || outs->is_cd_filename || outs->init)
				check_fails = TRUE;
		}
		if (check_fails) {
			warnf(config->global, "Invalid output struct data for write callback");
			return CURL_WRITEFUNC_ERROR;
		}
	}
#endif

	if (!outs->stream && !tool_create_output_file(outs, per))
		return CURL_WRITEFUNC_ERROR;

	if (is_tty && (outs->bytes < 2000) && !config->terminal_binary_ok) {
		/* binary output to terminal? */
		if (memchr(buffer, 0, bytes)) {
			warnf(config->global, "Binary output can mess up your terminal. "
				"Use \"--output -\" to tell curl to output it to your terminal "
				"anyway, or consider \"--output <FILE>\" to save to a file.");

			if (config->output_dir) {
				warnf(config->global, "\n");
				warnf(config->global, "By the way: you specified --output-dir "
					"but output is still written to stdout as you apperently did not "
					"specify an --output or --remote-name-all option. Might be you "
					"wanted to do that?");
			}
			config->synthetic_error = TRUE;
			return CURL_WRITEFUNC_ERROR;
		}
	}

#ifdef _WIN32
	fhnd = _get_osfhandle(fileno(outs->stream));
	/* if Windows console then UTF-8 must be converted to UTF-16 */
	if (isatty(fileno(outs->stream)) &&
		GetConsoleScreenBufferInfo((HANDLE)fhnd, &console_info)) {
		wchar_t* wc_buf;
		DWORD wc_len, chars_written;
		unsigned char* rbuf = (unsigned char*)buffer;
		DWORD rlen = (DWORD)bytes;

#define IS_TRAILING_BYTE(x) (0x80 <= (x) && (x) < 0xC0)

		/* attempt to complete an incomplete UTF-8 sequence from previous call.
			 the sequence does not have to be well-formed. */
		if (outs->utf8seq[0] && rlen) {
			bool complete = false;
			/* two byte sequence (lead byte 110yyyyy) */
			if (0xC0 <= outs->utf8seq[0] && outs->utf8seq[0] < 0xE0) {
				outs->utf8seq[1] = *rbuf++;
				--rlen;
				complete = true;
			}
			/* three byte sequence (lead byte 1110zzzz) */
			else if (0xE0 <= outs->utf8seq[0] && outs->utf8seq[0] < 0xF0) {
				if (!outs->utf8seq[1]) {
					outs->utf8seq[1] = *rbuf++;
					--rlen;
				}
				if (rlen && !outs->utf8seq[2]) {
					outs->utf8seq[2] = *rbuf++;
					--rlen;
					complete = true;
				}
			}
			/* four byte sequence (lead byte 11110uuu) */
			else if (0xF0 <= outs->utf8seq[0] && outs->utf8seq[0] < 0xF8) {
				if (!outs->utf8seq[1]) {
					outs->utf8seq[1] = *rbuf++;
					--rlen;
				}
				if (rlen && !outs->utf8seq[2]) {
					outs->utf8seq[2] = *rbuf++;
					--rlen;
				}
				if (rlen && !outs->utf8seq[3]) {
					outs->utf8seq[3] = *rbuf++;
					--rlen;
					complete = true;
				}
			}

			if (complete) {
				WCHAR prefix[3] = { 0 };  /* UTF-16 (1-2 WCHARs) + NUL */

				if (MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)outs->utf8seq, -1,
					prefix, sizeof(prefix) / sizeof(prefix[0]))) {
					DEBUGASSERT(prefix[2] == L'\0');
					if (!WriteConsoleW(
						(HANDLE)fhnd,
						prefix,
						prefix[1] ? 2 : 1,
						&chars_written,
						NULL)) {
						return CURL_WRITEFUNC_ERROR;
					}
				}
				/* else: UTF-8 input was not well formed and OS is pre-Vista which
					 drops invalid characters instead of writing U+FFFD to output.  */

				memset(outs->utf8seq, 0, sizeof(outs->utf8seq));
			}
		}

		/* suppress an incomplete utf-8 sequence at end of rbuf */
		if (!outs->utf8seq[0] && rlen && (rbuf[rlen - 1] & 0x80)) {
			/* check for lead byte from a two, three or four byte sequence */
			if (0xC0 <= rbuf[rlen - 1] && rbuf[rlen - 1] < 0xF8) {
				outs->utf8seq[0] = rbuf[rlen - 1];
				rlen -= 1;
			}
			else if (rlen >= 2 && IS_TRAILING_BYTE(rbuf[rlen - 1])) {
				/* check for lead byte from a three or four byte sequence */
				if (0xE0 <= rbuf[rlen - 2] && rbuf[rlen - 2] < 0xF8) {
					outs->utf8seq[0] = rbuf[rlen - 2];
					outs->utf8seq[1] = rbuf[rlen - 1];
					rlen -= 2;
				}
				else if (rlen >= 3 && IS_TRAILING_BYTE(rbuf[rlen - 2])) {
					/* check for lead byte from a four byte sequence */
					if (0xF0 <= rbuf[rlen - 3] && rbuf[rlen - 3] < 0xF8) {
						outs->utf8seq[0] = rbuf[rlen - 3];
						outs->utf8seq[1] = rbuf[rlen - 2];
						outs->utf8seq[2] = rbuf[rlen - 1];
						rlen -= 3;
					}
				}
			}
		}

		if (rlen) {
			/* calculate buffer size for wide characters */
			wc_len = (DWORD)MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)rbuf, (int)rlen,
				NULL, 0);
			if (!wc_len)
				return CURL_WRITEFUNC_ERROR;

			wc_buf = (wchar_t*)malloc(wc_len * sizeof(wchar_t));
			if (!wc_buf)
				return CURL_WRITEFUNC_ERROR;

			wc_len = (DWORD)MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)rbuf, (int)rlen,
				wc_buf, (int)wc_len);
			if (!wc_len) {
				free(wc_buf);
				return CURL_WRITEFUNC_ERROR;
			}

			if (!WriteConsoleW(
				(HANDLE)fhnd,
				wc_buf,
				wc_len,
				&chars_written,
				NULL)) {
				free(wc_buf);
				return CURL_WRITEFUNC_ERROR;
			}
			free(wc_buf);
		}

		rc = bytes;
	}
	else
#endif
		rc = fwrite(buffer, sz, nmemb, outs->stream);

	if (bytes == rc)
		/* we added this amount of data to the output */
		outs->bytes += bytes;

	if (config->readbusy) {
		config->readbusy = FALSE;
		curl_easy_pause(per->curl, CURLPAUSE_CONT);
	}

	if (config->nobuffer) {
		/* output buffering disabled */
		int res = fflush(outs->stream);
		if (res)
			return CURL_WRITEFUNC_ERROR;
	}

	return rc;
}










//--------------------------------------------------------------------------------------------















/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include "tool_setup.h"

#if defined(_WIN32) || defined(MSDOS)

#if defined(HAVE_LIBGEN_H) && defined(HAVE_BASENAME)
#  include <libgen.h>
#endif

#ifdef _WIN32
#  include <stdlib.h>
#  include <tlhelp32.h>
#  include "tool_cfgable.h"
#  include "tool_libinfo.h"
#endif

#include "tool_bname.h"
#include "tool_doswin.h"

#include "curlx.h"
#include "memdebug.h" /* keep this as LAST include */

#ifdef _WIN32
#  undef  PATH_MAX
#  define PATH_MAX MAX_PATH
#endif

#ifndef S_ISCHR
#  ifdef S_IFCHR
#    define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#  else
#    define S_ISCHR(m) (0) /* cannot tell if file is a device */
#  endif
#endif

#ifdef _WIN32
#  define _use_lfn(f) (1)   /* long filenames always available */
#elif !defined(__DJGPP__) || (__DJGPP__ < 2)  /* DJGPP 2.0 has _use_lfn() */
#  define _use_lfn(f) (0)  /* long filenames never available */
#elif defined(__DJGPP__)
#  include <fcntl.h>                /* _use_lfn(f) prototype */
#endif

#ifndef UNITTESTS
static CurlSanitizeCode truncate_dryrun(const char* path,
	const size_t truncate_pos);
#ifdef MSDOS
static CurlSanitizeCode msdosify(char** const sanitized, const char* file_name,
	int flags);
#endif
static CurlSanitizeCode rename_if_reserved_dos_device_name(char** const sanitized,
	const char* file_name,
	int flags);
#endif /* !UNITTESTS (static declarations used if no unit tests) */

static size_t get_max_sanitized_len(const char* file_name, int flags);

// move src to dst, where the copy in dst will overlap the source in src
static void strmov(char* dst, char* src) {
	if (dst < src) {
		// use memmove() as that one explicitly supports overlapping memory areas:
		memmove(dst, src, strlen(src) + 1);
	}
}

/*
Sanitize a file or path name.

All banned characters are replaced by underscores, for example:
f?*foo => f__foo
f:foo::$DATA => f_foo__$DATA
f:\foo:bar => f__foo_bar
f:\foo:bar => f:\foo:bar   (flag CURL_SANITIZE_ALLOW_PATH)

This function was implemented according to the guidelines in 'Naming Files,
Paths, and Namespaces' section 'Naming Conventions'.
https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx

Flags
-----
CURL_SANITIZE_ALLOW_COLONS:     Allow colons.
Without this flag colons are sanitized.

CURL_SANITIZE_ALLOW_PATH:       Allow path separators.
Without this flag path separators are sanitized.

CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH:  Allow only a relative path spec.
Absolute and UNC paths are sanitized to relative path form. Implies CURL_SANITIZE_ALLOW_PATH.

CURL_SANITIZE_ALLOW_RESERVED:   Allow reserved device names.
Without this flag a reserved device name is renamed (COM1 => _COM1) unless it
is in a \\ prefixed path (eg: \\?\, \\.\, UNC) where the names are always allowed.

CURL_SANITIZE_ALLOW_TRUNCATE:   Allow truncating a long filename.
Without this flag if the sanitized filename or path will be too long an error
occurs. With this flag the filename --and not any other parts of the path-- may
be truncated to at least a single character. A filename followed by an
alternate data stream (ADS) cannot be truncated in any case.

Success: (CURL_SANITIZE_ERR_OK) *sanitized points to a sanitized copy of file_name.
Failure: (!= CURL_SANITIZE_ERR_OK) *sanitized is NULL.
*/
CurlSanitizeCode curl_sanitize_file_name(char** const sanitized, const char* file_name, int flags)
{
	char* p, * target;
	size_t len;
	CurlSanitizeCode sc;
	size_t max_sanitized_len;

	if (!sanitized)
		return CURL_SANITIZE_ERR_BAD_ARGUMENT;

	*sanitized = NULL;

	if (!file_name)
		return CURL_SANITIZE_ERR_BAD_ARGUMENT;

	len = strlen(file_name);
	max_sanitized_len = get_max_sanitized_len(file_name, flags);

	if (len > max_sanitized_len) {
		if (!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
			truncate_dryrun(file_name, max_sanitized_len))
			return CURL_SANITIZE_ERR_INVALID_PATH;

		len = max_sanitized_len;
	}

	target = malloc(len + 1);
	if (!target)
		return CURL_SANITIZE_ERR_OUT_OF_MEMORY;

	strncpy(target, file_name, len);
	target[len] = '\0';

	if (flags & CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH) {
		flags |= CURL_SANITIZE_ALLOW_PATH;
		flags &= ~CURL_SANITIZE_ALLOW_COLONS;

		// do not tolerate absolute and UNC paths:
		if (!strncmp(target, "\\\\?\\", 4))
			/* Skip the literal path prefix \\?\ */
			p = target + 4;
		else
			p = target;

		while (*p == '/' || *p == '\\') {
			p++;
		}

		// strip off the leading 'root path' bit:
		strmov(target, p);
	}

#ifndef MSDOS
	if ((flags & CURL_SANITIZE_ALLOW_PATH) && !(flags & CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH) && !strncmp(target, "\\\\?\\", 4))
		/* Skip the literal path prefix \\?\ */
		p = target + 4;
	else
#endif
		p = target;

	/* replace control characters and other banned characters */
	bool s_o_p = TRUE;
	bool dot = FALSE;
	for (; *p; ++p) {
		if (*p == '.') {
			if (s_o_p && !(flags & CURL_SANITIZE_ALLOW_DOTFILES)) {
				// dotfiles are not allowed!
				//
				// Incidentally: make sure this is not some crappy attempt to slip a '..' path in: encode it entirely.
				*p = '_';
				dot = TRUE;
				s_o_p = (p[1] == '.');
				continue;
			}

			// only accept a dot at the start or middle of a *larger* file/dir name, never at the end of the file/dirname.
			if (!dot && p[1] && p[1] != '/' && p[1] != '\\') {
				dot = TRUE;
				s_o_p = FALSE;
				continue;
			}

			if (dot) {
				// previous character was a '.' as well: sanitize them all!
				p[-1] = '_';
			}
			*p = '_';
			s_o_p = FALSE;
			continue;
		}

		dot = FALSE;

		if (*p == '/' || *p == '\\') {
			if (flags & CURL_SANITIZE_ALLOW_PATH) {
				*p = '/';	// convert to UNIX-style path separator

				// and when we've hit our first path separator like that, we do no longer tolerate colons in the path either!
				flags &= ~CURL_SANITIZE_ALLOW_COLONS;

				s_o_p = TRUE;

				// replace multiple-'/' sequences with a single '/' iff this is to be a path 
				int i = 1;
				for (; p[i] == '/' || p[i] == '\\'; i++)
					;
				if (i > 1)
					strmov(p + 1, p + i);

				// remove trailing spaces and periods per path segment
				char* q;
				for (q = p - 1; q >= target; q--) {
					if (*q != ' ' && *q != '.')
						break;
				}
				q++;

				if (q != p) {
					strmov(q, p);
					p = q;
				}
				continue;
			}

			*p = '_';
			s_o_p = FALSE;
			continue;
		}

		if ((1 <= *p && *p <= 31) || (*p == 0x7F)) {
			*p = '_';
			s_o_p = FALSE;
			continue;
		}

		// replace ':', but strip it off when it's the last thing in the path part, e.g. 'http://' --> 'https/'
		if (!(flags & (CURL_SANITIZE_ALLOW_COLONS)) && *p == ':') {
			if (s_o_p || p[1] != '/')
				*p = '_';
			else {
				int i = 2;
				for (; p[i] == '/' || p[i] == '\\'; i++)
					;
				strmov(p, p + i - 1);
				p--;
			}
			s_o_p = FALSE;
			continue;
		}

		if (strchr("|<>\"&'~`?*$^;#%", *p)) {
			*p = '_';
			s_o_p = FALSE;
			continue;
		}

		// remove leading spaces and dashes per path segment:
		//
		// we remove dashes (`-`) to prevent creating file/dir-names which would otherwise mimmick commandline options, e.g. `-2` --> `_2`
		if (s_o_p) {
			if (*p == ' ' || *p == '-' || (*p == '.' && !(flags & CURL_SANITIZE_ALLOW_DOTFILES))) {
				*p = '_';

				// replace a series of any of these at Start-Of-Part (SOP), if any:
				p++;
				while (*p == ' ' || *p == '-' || (*p == '.' && !(flags & CURL_SANITIZE_ALLOW_DOTFILES)))
					*p++ = '_';
				p--;
				s_o_p = FALSE;
				continue;
			}
		}
		s_o_p = FALSE;
	}

	// remove trailing spaces and periods if not allowing paths
	if (!(flags & CURL_SANITIZE_ALLOW_PATH) && len) {
		char* clip = NULL;

		p = &target[len];
		do {
			--p;
			if (*p != ' ' && *p != '.')
				break;
			clip = p;
		} while (p != target);

		if (clip) {
			*clip = '\0';
			len = clip - target;
		}
	}

#ifdef MSDOS
	sc = msdosify(&p, target, flags);
	free(target);
	if (sc)
		return sc;
	target = p;
	len = strlen(target);

	if (len > max_sanitized_len) {
		free(target);
		return CURL_SANITIZE_ERR_INVALID_PATH;
	}
#endif

	if (!(flags & CURL_SANITIZE_ALLOW_RESERVED)) {
		sc = rename_if_reserved_dos_device_name(&p, target, flags);
		free(target);
		if (sc)
			return sc;
		target = p;
		len = strlen(target);

		if (len > max_sanitized_len) {
			free(target);
			return CURL_SANITIZE_ERR_INVALID_PATH;
		}
	}

	*sanitized = target;
	return CURL_SANITIZE_ERR_OK;
}

/*
Return the maximum allowed length of the (to-be-)sanitized 'file_name'.

This is a supporting function for any function that returns a sanitized
filename.
*/
static size_t get_max_sanitized_len(const char* file_name, int flags)
{
	size_t max_sanitized_len;

	if ((flags & CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH) && !(flags & CURL_SANITIZE_ALLOW_TRUNCATE)) {
		return 32767 - 1;
	}

	if ((flags & CURL_SANITIZE_ALLOW_PATH)) {
#ifdef UNITTESTS
		if (file_name[0] == '\\' && file_name[1] == '\\')
			max_sanitized_len = 32767 - 1;
		else
			max_sanitized_len = 259;
#elif defined(MSDOS)
		max_sanitized_len = PATH_MAX - 1;
#else
		/* Paths that start with \\ may be longer than PATH_MAX (MAX_PATH) on any
			 version of Windows. Starting in Windows 10 1607 (build 14393) any path
			 may be longer than PATH_MAX if the user has opted-in and the application
			 supports it. */
		if ((file_name[0] == '\\' && file_name[1] == '\\') ||
			curlx_verify_windows_version(10, 0, 14393, PLATFORM_WINNT,
				VERSION_GREATER_THAN_EQUAL))
			max_sanitized_len = 32767 - 1;
		else
			max_sanitized_len = PATH_MAX - 1;
#endif
	}
	else
#ifdef UNITTESTS
		max_sanitized_len = 255;
#else
		/* The maximum length of a filename.
			 FILENAME_MAX is often the same as PATH_MAX, in other words it is 260 and
			 does not discount the path information therefore we shouldn't use it. */
		max_sanitized_len = (PATH_MAX - 1 > 255) ? 255 : PATH_MAX - 1;
#endif

	return max_sanitized_len;
}

/*
Test if truncating a path to a file will leave at least a single character in
the filename. Filenames suffixed by an alternate data stream cannot be
truncated. This performs a dry run, nothing is modified.

Good truncate_pos 9:    C:\foo\bar  =>  C:\foo\ba
Good truncate_pos 6:    C:\foo      =>  C:\foo
Good truncate_pos 5:    C:\foo      =>  C:\fo
Bad* truncate_pos 5:    C:foo       =>  C:foo
Bad truncate_pos 5:     C:\foo:ads  =>  C:\fo
Bad truncate_pos 9:     C:\foo:ads  =>  C:\foo:ad
Bad truncate_pos 5:     C:\foo\bar  =>  C:\fo
Bad truncate_pos 5:     C:\foo\     =>  C:\fo
Bad truncate_pos 7:     C:\foo\     =>  C:\foo\
Error truncate_pos 7:   C:\foo      =>  (pos out of range)
Bad truncate_pos 1:     C:\foo\     =>  C

* C:foo is ambiguous, C could end up being a drive or file therefore something
	like C:superlongfilename cannot be truncated.

Returns
SANITIZE_ERR_OK: Good -- 'path' can be truncated
SANITIZE_ERR_INVALID_PATH: Bad -- 'path' cannot be truncated
!= CURL_SANITIZE_ERR_OK && != CURL_SANITIZE_ERR_INVALID_PATH: Error
*/
CurlSanitizeCode truncate_dryrun(const char* path, const size_t truncate_pos)
{
	size_t len;

	if (!path)
		return CURL_SANITIZE_ERR_BAD_ARGUMENT;

	len = strlen(path);

	if (truncate_pos > len)
		return CURL_SANITIZE_ERR_BAD_ARGUMENT;

	if (!len || !truncate_pos)
		return CURL_SANITIZE_ERR_INVALID_PATH;

	if (strpbrk(&path[truncate_pos - 1], "\\/:"))
		return CURL_SANITIZE_ERR_INVALID_PATH;

	/* C:\foo can be truncated but C:\foo:ads cannot */
	if (truncate_pos > 1) {
		const char* p = &path[truncate_pos - 1];
		do {
			--p;
			if (*p == ':')
				return CURL_SANITIZE_ERR_INVALID_PATH;
		} while (p != path && *p != '\\' && *p != '/');
	}

	return CURL_SANITIZE_ERR_OK;
}

/* The functions msdosify, rename_if_dos_device_name and __crt0_glob_function
 * were taken with modification from the DJGPP port of tar 1.12. They use
 * algorithms originally from DJTAR.
 */

 /*
 Extra sanitization MS-DOS for file_name.

 This is a supporting function for curl_sanitize_file_name.

 Warning: This is an MS-DOS legacy function and was purposely written in a way
 that some path information may pass through. For example drive letter names
 (C:, D:, etc) are allowed to pass through. For sanitizing a filename use
 curl_sanitize_file_name.

 Success: (SANITIZE_ERR_OK) *sanitized points to a sanitized copy of file_name.
 Failure: (!= CURL_SANITIZE_ERR_OK) *sanitized is NULL.
 */
#if defined(MSDOS) || defined(UNITTESTS)
CurlSanitizeCode msdosify(char** const sanitized, const char* file_name,
	int flags)
{
	char dos_name[PATH_MAX];
	static const char illegal_chars_dos[] = ".+, ;=[]" /* illegal in DOS */
		"|<>/\\\":?*"; /* illegal in DOS & W95 */
	static const char* illegal_chars_w95 = &illegal_chars_dos[8];
	int idx, dot_idx;
	const char* s = file_name;
	char* d = dos_name;
	const char* const dlimit = dos_name + sizeof(dos_name) - 1;
	const char* illegal_aliens = illegal_chars_dos;
	size_t len = sizeof(illegal_chars_dos) - 1;

	if (!sanitized)
		return CURL_SANITIZE_ERR_BAD_ARGUMENT;

	*sanitized = NULL;

	if (!file_name)
		return CURL_SANITIZE_ERR_BAD_ARGUMENT;

	if (strlen(file_name) > PATH_MAX - 1 &&
		(!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
			truncate_dryrun(file_name, PATH_MAX - 1)))
		return CURL_SANITIZE_ERR_INVALID_PATH;

	/* Support for Windows 9X VFAT systems, when available. */
	if (_use_lfn(file_name)) {
		illegal_aliens = illegal_chars_w95;
		len -= (illegal_chars_w95 - illegal_chars_dos);
	}

	/* Get past the drive letter, if any. */
	if (s[0] >= 'A' && s[0] <= 'z' && s[1] == ':') {
		*d++ = *s++;
		*d = ((flags & (SANITIZE_ALLOW_COLONS | SANITIZE_ALLOW_PATH))) ? ':' : '_';
		++d; ++s;
	}

	for (idx = 0, dot_idx = -1; *s && d < dlimit; s++, d++) {
		if (memchr(illegal_aliens, *s, len)) {

			if ((flags & (SANITIZE_ALLOW_COLONS | SANITIZE_ALLOW_PATH)) && *s == ':')
				*d = ':';
			else if ((flags & CURL_SANITIZE_ALLOW_PATH) && (*s == '/' || *s == '\\'))
				*d = *s;
			/* Dots are special: DOS does not allow them as the leading character,
				 and a filename cannot have more than a single dot. We leave the
				 first non-leading dot alone, unless it comes too close to the
				 beginning of the name: we want sh.lex.c to become sh_lex.c, not
				 sh.lex-c.  */
			else if (*s == '.') {
				if ((flags & CURL_SANITIZE_ALLOW_PATH) && idx == 0 &&
					(s[1] == '/' || s[1] == '\\' ||
						(s[1] == '.' && (s[2] == '/' || s[2] == '\\')))) {
					/* Copy "./" and "../" verbatim.  */
					*d++ = *s++;
					if (d == dlimit)
						break;
					if (*s == '.') {
						*d++ = *s++;
						if (d == dlimit)
							break;
					}
					*d = *s;
				}
				else if (idx == 0)
					*d = '_';
				else if (dot_idx >= 0) {
					if (dot_idx < 5) { /* 5 is a heuristic ad-hoc'ery */
						d[dot_idx - idx] = '_'; /* replace previous dot */
						*d = '.';
					}
					else
						*d = '-';
				}
				else
					*d = '.';

				if (*s == '.')
					dot_idx = idx;
			}
			else if (*s == '+' && s[1] == '+') {
				if (idx - 2 == dot_idx) { /* .c++, .h++ etc. */
					*d++ = 'x';
					if (d == dlimit)
						break;
					*d = 'x';
				}
				else {
					/* libg++ etc.  */
					if (dlimit - d < 4) {
						*d++ = 'x';
						if (d == dlimit)
							break;
						*d = 'x';
					}
					else {
						memcpy(d, "plus", 4);
						d += 3;
					}
				}
				s++;
				idx++;
			}
			else
				*d = '_';
		}
		else
			*d = *s;
		if (*s == '/' || *s == '\\') {
			idx = 0;
			dot_idx = -1;
		}
		else
			idx++;
	}
	*d = '\0';

	if (*s) {
		/* dos_name is truncated, check that truncation requirements are met,
			 specifically truncating a filename suffixed by an alternate data stream
			 or truncating the entire filename is not allowed. */
		if (!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) || strpbrk(s, "\\/:") ||
			truncate_dryrun(dos_name, d - dos_name))
			return CURL_SANITIZE_ERR_INVALID_PATH;
	}

	*sanitized = strdup(dos_name);
	return (*sanitized ? CURL_SANITIZE_ERR_OK : CURL_SANITIZE_ERR_OUT_OF_MEMORY);
}
#endif /* MSDOS || UNITTESTS */

/*
Rename file_name if it is a reserved dos device name.

This is a supporting function for curl_sanitize_file_name.

Warning: This is an MS-DOS legacy function and was purposely written in a way
that some path information may pass through. For example drive letter names
(C:, D:, etc) are allowed to pass through. For sanitizing a filename use
curl_sanitize_file_name.

This function does not rename reserved device names in special paths, where a
'file_name' starts with \\ and 'flags' contains CURL_SANITIZE_ALLOW_PATH. For
example C:\COM1 is surely unintended and would be renamed in any case, but
\\.\COM1 wouldn't be renamed when UNC paths are allowed.

Success: (SANITIZE_ERR_OK) *sanitized points to a sanitized copy of file_name.
Failure: (!= CURL_SANITIZE_ERR_OK) *sanitized is NULL.
*/
CurlSanitizeCode rename_if_reserved_dos_device_name(char** const sanitized,
	const char* file_name,
	int flags)
{
	char* p, * base, * target;
	size_t t_len, max_sanitized_len;
#ifdef MSDOS
	struct_stat st_buf;
#endif

	if (!sanitized)
		return CURL_SANITIZE_ERR_BAD_ARGUMENT;

	*sanitized = NULL;

	if (!file_name)
		return CURL_SANITIZE_ERR_BAD_ARGUMENT;

	max_sanitized_len = get_max_sanitized_len(file_name, flags);

	t_len = strlen(file_name);
	if (t_len > max_sanitized_len) {
		if (!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
			truncate_dryrun(file_name, max_sanitized_len))
			return CURL_SANITIZE_ERR_INVALID_PATH;

		t_len = max_sanitized_len;
	}

	target = malloc(t_len + 1);
	if (!target)
		return CURL_SANITIZE_ERR_OUT_OF_MEMORY;

	strncpy(target, file_name, t_len);
	target[t_len] = '\0';

#ifndef MSDOS
	if ((flags & CURL_SANITIZE_ALLOW_PATH) &&
		file_name[0] == '\\' && file_name[1] == '\\') {
		*sanitized = target;
		return CURL_SANITIZE_ERR_OK;
	}
#endif

	base = tool_basename(target);

	/* Rename reserved device names that are known to be accessible without \\.\
		 Examples: CON => _CON, CON.EXT => CON_EXT, CON:ADS => CON_ADS
		 https://web.archive.org/web/2014/http://support.microsoft.com/kb/74496
		 https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx

		 Windows API has some weird interpretations to find legacy names. It may
		 check the beginning of the path and/or the basename so rename both.
		 */
	for (p = target; p; p = (p == target && target != base ? base : NULL)) {
		size_t p_len;
		int x = (curl_strnequal(p, "CON", 3) ||
			curl_strnequal(p, "PRN", 3) ||
			curl_strnequal(p, "AUX", 3) ||
			curl_strnequal(p, "NUL", 3)) ? 3 :
			(curl_strnequal(p, "CLOCK$", 6)) ? 6 :
			(curl_strnequal(p, "COM", 3) || curl_strnequal(p, "LPT", 3)) ?
			(('1' <= p[3] && p[3] <= '9') ? 4 : 3) : 0;

		if (!x)
			continue;

		/* the devices may be accessible with an extension or ADS, for
			 example CON.AIR and 'CON . AIR' and CON:AIR access console */

		for (; p[x] == ' '; ++x)
			;

		if (p[x] == '.') {
			p[x] = '_';
			continue;
		}
		else if (p[x] == ':') {
			if (!(flags & (CURL_SANITIZE_ALLOW_COLONS | CURL_SANITIZE_ALLOW_PATH))) {
				p[x] = '_';
				continue;
			}
			++x;
		}
		else if (p[x]) /* no match */
			continue;

		/* p points to 'CON' or 'CON ' or 'CON:' (if colon/path allowed), etc.
			 Prepend a '_'. */

		p_len = strlen(p);

		if (t_len == max_sanitized_len) {
			--p_len;
			--t_len;
			if (!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
				truncate_dryrun(target, t_len)) {
				free(target);
				return CURL_SANITIZE_ERR_INVALID_PATH;
			}
			target[t_len] = '\0';
		}
		else {
			p = realloc(target, t_len + 2);
			if (!p) {
				free(target);
				return CURL_SANITIZE_ERR_OUT_OF_MEMORY;
			}
			target = p;
			p = target + (t_len - p_len);
		}

		/* prepend '_' to target or base (basename within target) */
		memmove(p + 1, p, p_len + 1);
		p[0] = '_';
		++p_len;
		++t_len;

		base = tool_basename(target);
	}

	/* This is the legacy portion from rename_if_dos_device_name that checks for
		 reserved device names. It only works on MS-DOS. On Windows XP the stat
		 check errors with EINVAL if the device name is reserved. On Windows
		 Vista/7/8 it sets mode S_IFREG (regular file or device). According to
		 MSDN stat doc the latter behavior is correct, but that does not help us
		 identify whether it is a reserved device name and not a regular
		 filename. */
#ifdef MSDOS
	if (base && ((stat(base, &st_buf)) == 0) && (S_ISCHR(st_buf.st_mode))) {
		/* Prepend a '_' */
		size_t blen = strlen(base);
		if (blen) {
			size_t t_len = strlen(target);

			if (t_len == max_sanitized_len) {
				--blen;
				--t_len;
				if (!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
					truncate_dryrun(target, t_len)) {
					free(target);
					return CURL_SANITIZE_ERR_INVALID_PATH;
				}
				target[t_len] = '\0';
			}
			else {
				p = realloc(target, t_len + 2);
				if (!p) {
					free(target);
					return CURL_SANITIZE_ERR_OUT_OF_MEMORY;
				}
				target = p;
				base = target + (t_len - blen);
			}

			memmove(base + 1, base, blen + 1);
			base[0] = '_';
			++blen;
			++t_len;
		}
	}
#endif

	* sanitized = target;
	return CURL_SANITIZE_ERR_OK;
}

#if defined(MSDOS) && (defined(__DJGPP__) || defined(__GO32__))

/*
 * Disable program default argument globbing. We do it on our own.
 */
char** __crt0_glob_function(char* arg)
{
	(void)arg;
	return (char**)0;
}

#endif /* MSDOS && (__DJGPP__ || __GO32__) */

#ifdef _WIN32

/*
 * Function to find CACert bundle on a Win32 platform using SearchPath.
 *
 * The order of the directories it searches is:
 *  1. application's directory
 *  2. current working directory
 *  3. Windows System directory (e.g. C:\Windows\System32)
 *  4. Windows Directory (e.g. C:\Windows)
 *  5. all directories along %PATH%
 *
 * For WinXP and later search order actually depends on registry value:
 * HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\SafeProcessSearchMode
 */

CURLcode FindWin32CACert(struct OperationConfig* config,
	curl_sslbackend backend,
	const TCHAR* bundle_file)
{
	CURLcode result = CURLE_OK;

#ifdef CURL_WINDOWS_APP
	(void)config;
	(void)backend;
	(void)bundle_file;
#else
	/* Search and set cert file only if libcurl supports SSL.
	 *
	 * If Schannel is the selected SSL backend then these locations are
	 * ignored. We allow setting CA location for schannel only when explicitly
	 * specified by the user via CURLOPT_CAINFO / --cacert.
	 */
	if (feature_ssl && backend != CURLSSLBACKEND_SCHANNEL) {

		DWORD res_len, written;
		TCHAR* buf;

		res_len = SearchPath(NULL, bundle_file, NULL, 0, NULL, NULL);
		if (!res_len)
			return CURLE_OK;

		buf = malloc(res_len * sizeof(TCHAR));
		if (!buf)
			return CURLE_OUT_OF_MEMORY;

		written = SearchPath(NULL, bundle_file, NULL, res_len, buf, NULL);
		if (written) {
			if (written >= res_len) {
				// unexpected fault!
				DEBUGASSERT(!"Unexpected fault in SearchPath API usage");
				result = CURLE_OUT_OF_MEMORY;
			}
			else {
				char* mstr = curlx_convert_tchar_to_UTF8(buf);
				Curl_safefree(config->cacert);
				if (mstr)
					config->cacert = strdup(mstr);
				curlx_unicodefree(mstr);
				if (!config->cacert)
					result = CURLE_OUT_OF_MEMORY;
			}
		}
		Curl_safefree(buf);
	}
#endif

	return result;
}


/* Get a list of all loaded modules with full paths.
 * Returns slist on success or NULL on error.
 */
struct curl_slist* GetLoadedModulePaths(void)
{
	HANDLE hnd = INVALID_HANDLE_VALUE;
	MODULEENTRY32 mod = { 0 };
	struct curl_slist* slist = NULL;

	mod.dwSize = sizeof(MODULEENTRY32);

	do {
		hnd = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
	} while (hnd == INVALID_HANDLE_VALUE && GetLastError() == ERROR_BAD_LENGTH);

	if (hnd == INVALID_HANDLE_VALUE)
		goto error;

	if (!Module32First(hnd, &mod))
		goto error;

	do {
		char* path; /* points to stack allocated buffer */
		struct curl_slist* temp;

#ifdef UNICODE
		/* sizeof(mod.szExePath) is the max total bytes of wchars. the max total
			 bytes of multibyte chars will not be more than twice that. */
		char buffer[sizeof(mod.szExePath) * 2];
		if (!WideCharToMultiByte(CP_ACP, 0, mod.szExePath, -1,
			buffer, sizeof(buffer), NULL, NULL))
			goto error;
		path = buffer;
#else
		path = mod.szExePath;
#endif
		temp = curl_slist_append(slist, path);
		if (!temp)
			goto error;
		slist = temp;
	} while (Module32Next(hnd, &mod));

	goto cleanup;

error:
	curl_slist_free_all(slist);
	slist = NULL;
cleanup:
	if (hnd != INVALID_HANDLE_VALUE)
		CloseHandle(hnd);
	return slist;
}

bool tool_term_has_bold;

#ifndef CURL_WINDOWS_APP
/* The terminal settings to restore on exit */
static struct TerminalSettings {
	HANDLE hStdOut;
	DWORD dwOutputMode;
	LONG valid;
} TerminalSettings;

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static void restore_terminal(void)
{
	if (InterlockedExchange(&TerminalSettings.valid, (LONG)FALSE))
		SetConsoleMode(TerminalSettings.hStdOut, TerminalSettings.dwOutputMode);
}

/* This is the console signal handler.
 * The system calls it in a separate thread.
 */
static BOOL WINAPI signal_handler(DWORD type)
{
	if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT)
		restore_terminal();
	return FALSE;
}

static void init_terminal(void)
{
	TerminalSettings.hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	/*
	 * Enable VT (Virtual Terminal) output.
	 * Note: VT mode flag can be set on any version of Windows, but VT
	 * processing only performed on Win10 >= version 1709 (OS build 16299)
	 * Creator's Update. Also, ANSI bold on/off supported since then.
	 */
	if (TerminalSettings.hStdOut == INVALID_HANDLE_VALUE ||
		!GetConsoleMode(TerminalSettings.hStdOut,
			&TerminalSettings.dwOutputMode) ||
		!curlx_verify_windows_version(10, 0, 16299, PLATFORM_WINNT,
			VERSION_GREATER_THAN_EQUAL))
		return;

	if ((TerminalSettings.dwOutputMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
		tool_term_has_bold = true;
	else {
		/* The signal handler is set before attempting to change the console mode
			 because otherwise a signal would not be caught after the change but
			 before the handler was installed. */
		(void)InterlockedExchange(&TerminalSettings.valid, (LONG)TRUE);
		if (SetConsoleCtrlHandler(signal_handler, TRUE)) {
			if (SetConsoleMode(TerminalSettings.hStdOut,
				(TerminalSettings.dwOutputMode |
					ENABLE_VIRTUAL_TERMINAL_PROCESSING))) {
				tool_term_has_bold = true;
				atexit(restore_terminal);
			}
			else {
				SetConsoleCtrlHandler(signal_handler, FALSE);
				(void)InterlockedExchange(&TerminalSettings.valid, (LONG)FALSE);
			}
		}
	}
}
#endif

LARGE_INTEGER tool_freq;
bool tool_isVistaOrGreater;

CURLcode win32_init(void)
{
	/* curlx_verify_windows_version must be called during init at least once
		 because it has its own initialization routine. */
	if (curlx_verify_windows_version(6, 0, 0, PLATFORM_WINNT,
		VERSION_GREATER_THAN_EQUAL))
		tool_isVistaOrGreater = true;
	else
		tool_isVistaOrGreater = false;

	QueryPerformanceFrequency(&tool_freq);

#ifndef CURL_WINDOWS_APP
	init_terminal();
#endif

	return CURLE_OK;
}

#endif /* _WIN32 */

#endif /* _WIN32 || MSDOS */










//--------------------------------------------------------------------------------------------

















//
// produce destination paths in a /tmp/... directory tree, where:
//
// - all output files are NUMBERED in the order in which they are produced,
//   so as to produce an intelligible sequence.
// - the last directory in the generated output path is based on the source file,
//   so as multiple runs with different test files each produce their own
//   output image sequence in separate directories to keep it manageable and
//   intelligible.
//

static char dstdirname[PATH_MAX];

static void register_source_filename_for_dst_path(const char* name)
{
	// rough hash of file path:
	uint32_t hash = 0x003355AA;
	for (int i = 0; name[i]; i++) {
		hash <<= 5;
		uint8_t c = name[i];
		hash += c;
		uint32_t c2 = c;
		hash += c2 << 17;
		hash ^= hash >> 21;
	}

	const char* fn = strrchr(name, '/');
	if (!fn)
		fn = strrchr(name, '\\');
	if (!fn)
		fn = name;
	else
		fn++;
	const char* ext = strrchr(fn, '.');
	size_t len = ext - fn;
	if (len >= sizeof(dstdirname))
		len = sizeof(dstdirname) - 1;

	memcpy(dstdirname, fn, len);
	dstdirname[len] = 0;

	// sanitize:
	int dot_allowed = 0;
	for (int i = 0; dstdirname[i]; i++) {
		int c = dstdirname[i];
		if (isalnum(c)) {
			dot_allowed = 1;
			continue;
		}
		if (strchr("-_.", c) && dot_allowed) {
			dot_allowed = 0;
			continue;
		}
		dot_allowed = 0;
		dstdirname[i] = '_';
	}
	for (int i = strlen(dstdirname) - 1; i >= 0; i--) {
		int c = dstdirname[i];
		if (!strchr("-_.", c)) {
			break;
		}
		dstdirname[i] = 0;
	}

	// append path hash:
	hash &= 0xFFFF;
	size_t offset = strlen(dstdirname);
	if (offset >= sizeof(dstdirname) - 7)
		offset = sizeof(dstdirname) - 7;
	snprintf(dstdirname + offset, 6, "_H%04X", hash);
	dstdirname[sizeof(dstdirname) - 1] = 0;
}


static int index = 0;

static const char* mk_dst_filename(const char* name)
{
	static char dstpath[PATH_MAX];

	snprintf(dstpath, sizeof(dstpath), "/tmp/lept/binarization/%s/%03d-%s", dstdirname, index, name);
	index++;
	return dstpath;
}















//--------------------------------------------------------------------------------------------
















#pragma once

#include "crow/settings.h"

#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <cstring>
#include <cctype>
#include <functional>
#include <string>
#include <sstream>
#include <unordered_map>
#include <random>
#include <algorithm>


#if defined(CROW_CAN_USE_CPP17) && !defined(CROW_FILESYSTEM_IS_EXPERIMENTAL)
#include <filesystem>
#endif

// TODO(EDev): Adding C++20's [[likely]] and [[unlikely]] attributes might be useful
#if defined(__GNUG__) || defined(__clang__)
#define CROW_LIKELY(X) __builtin_expect(!!(X), 1)
#define CROW_UNLIKELY(X) __builtin_expect(!!(X), 0)
#else
#define CROW_LIKELY(X) (X)
#define CROW_UNLIKELY(X) (X)
#endif

namespace crow
{
	/// @cond SKIP
	namespace black_magic
	{
#ifndef CROW_MSVC_WORKAROUND
		/// Out of Range Exception for const_str
		struct OutOfRange
		{
			OutOfRange(unsigned /*pos*/, unsigned /*length*/) {}
		};
		/// Helper function to throw an exception if i is larger than len
		constexpr unsigned requires_in_range(unsigned i, unsigned len)
		{
			return i >= len ? throw OutOfRange(i, len) : i;
		}

		/// A constant string implementation.
		class const_str
		{
			const char* const begin_;
			unsigned size_;

		public:
			template<unsigned N>
			constexpr const_str(const char(&arr)[N]) :
				begin_(arr), size_(N - 1)
			{
				static_assert(N >= 1, "not a string literal");
			}
			constexpr char operator[](unsigned i) const
			{
				return requires_in_range(i, size_), begin_[i];
			}

			constexpr operator const char* () const
			{
				return begin_;
			}

			constexpr const char* begin() const { return begin_; }
			constexpr const char* end() const { return begin_ + size_; }

			constexpr unsigned size() const
			{
				return size_;
			}
		};

		constexpr unsigned find_closing_tag(const_str s, unsigned p)
		{
			return s[p] == '>' ? p : find_closing_tag(s, p + 1);
		}

		/// Check that the CROW_ROUTE string is valid
		constexpr bool is_valid(const_str s, unsigned i = 0, int f = 0)
		{
			return i == s.size() ? f == 0 :
				f < 0 || f >= 2 ? false :
				s[i] == '<' ? is_valid(s, i + 1, f + 1) :
				s[i] == '>' ? is_valid(s, i + 1, f - 1) :
				is_valid(s, i + 1, f);
		}

		constexpr bool is_equ_p(const char* a, const char* b, unsigned n)
		{
			return *a == 0 && *b == 0 && n == 0 ? true :
				(*a == 0 || *b == 0) ? false :
				n == 0 ? true :
				*a != *b ? false :
				is_equ_p(a + 1, b + 1, n - 1);
		}

		constexpr bool is_equ_n(const_str a, unsigned ai, const_str b, unsigned bi, unsigned n)
		{
			return ai + n > a.size() || bi + n > b.size() ? false :
				n == 0 ? true :
				a[ai] != b[bi] ? false :
				is_equ_n(a, ai + 1, b, bi + 1, n - 1);
		}

		constexpr bool is_int(const_str s, unsigned i)
		{
			return is_equ_n(s, i, "<int>", 0, 5);
		}

		constexpr bool is_uint(const_str s, unsigned i)
		{
			return is_equ_n(s, i, "<uint>", 0, 6);
		}

		constexpr bool is_float(const_str s, unsigned i)
		{
			return is_equ_n(s, i, "<float>", 0, 7) ||
				is_equ_n(s, i, "<double>", 0, 8);
		}

		constexpr bool is_str(const_str s, unsigned i)
		{
			return is_equ_n(s, i, "<str>", 0, 5) ||
				is_equ_n(s, i, "<string>", 0, 8);
		}

		constexpr bool is_path(const_str s, unsigned i)
		{
			return is_equ_n(s, i, "<path>", 0, 6);
		}
#endif
		template<typename T>
		struct parameter_tag
		{
			static const int value = 0;
		};
#define CROW_INTERNAL_PARAMETER_TAG(t, i) \
    template<>                            \
    struct parameter_tag<t>               \
    {                                     \
        static const int value = i;       \
    }
		CROW_INTERNAL_PARAMETER_TAG(int, 1);
		CROW_INTERNAL_PARAMETER_TAG(char, 1);
		CROW_INTERNAL_PARAMETER_TAG(short, 1);
		CROW_INTERNAL_PARAMETER_TAG(long, 1);
		CROW_INTERNAL_PARAMETER_TAG(long long, 1);
		CROW_INTERNAL_PARAMETER_TAG(unsigned int, 2);
		CROW_INTERNAL_PARAMETER_TAG(unsigned char, 2);
		CROW_INTERNAL_PARAMETER_TAG(unsigned short, 2);
		CROW_INTERNAL_PARAMETER_TAG(unsigned long, 2);
		CROW_INTERNAL_PARAMETER_TAG(unsigned long long, 2);
		CROW_INTERNAL_PARAMETER_TAG(double, 3);
		CROW_INTERNAL_PARAMETER_TAG(std::string, 4);
#undef CROW_INTERNAL_PARAMETER_TAG
		template<typename... Args>
		struct compute_parameter_tag_from_args_list;

		template<>
		struct compute_parameter_tag_from_args_list<>
		{
			static const int value = 0;
		};

		template<typename Arg, typename... Args>
		struct compute_parameter_tag_from_args_list<Arg, Args...>
		{
			static const int sub_value =
				compute_parameter_tag_from_args_list<Args...>::value;
			static const int value =
				parameter_tag<typename std::decay<Arg>::type>::value ? sub_value * 6 + parameter_tag<typename std::decay<Arg>::type>::value : sub_value;
		};

		static inline bool is_parameter_tag_compatible(uint64_t a, uint64_t b)
		{
			if (a == 0)
				return b == 0;
			if (b == 0)
				return a == 0;
			int sa = a % 6;
			int sb = a % 6;
			if (sa == 5) sa = 4;
			if (sb == 5) sb = 4;
			if (sa != sb)
				return false;
			return is_parameter_tag_compatible(a / 6, b / 6);
		}

		static inline unsigned find_closing_tag_runtime(const char* s, unsigned p)
		{
			return s[p] == 0 ? throw std::runtime_error("unmatched tag <") :
				s[p] == '>' ? p :
				find_closing_tag_runtime(s, p + 1);
		}

		static inline uint64_t get_parameter_tag_runtime(const char* s, unsigned p = 0)
		{
			return s[p] == 0 ? 0 :
				s[p] == '<' ? (
					std::strncmp(s + p, "<int>", 5) == 0 ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 1 :
					std::strncmp(s + p, "<uint>", 6) == 0 ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 2 :
					(std::strncmp(s + p, "<float>", 7) == 0 ||
						std::strncmp(s + p, "<double>", 8) == 0) ?
					get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 3 :
					(std::strncmp(s + p, "<str>", 5) == 0 ||
						std::strncmp(s + p, "<string>", 8) == 0) ?
					get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 4 :
					std::strncmp(s + p, "<path>", 6) == 0 ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 5 :
					throw std::runtime_error("invalid parameter type")) :
				get_parameter_tag_runtime(s, p + 1);
		}
#ifndef CROW_MSVC_WORKAROUND
		constexpr uint64_t get_parameter_tag(const_str s, unsigned p = 0)
		{
			return p == s.size() ? 0 :
				s[p] == '<' ? (
					is_int(s, p) ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 1 :
					is_uint(s, p) ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 2 :
					is_float(s, p) ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 3 :
					is_str(s, p) ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 4 :
					is_path(s, p) ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 5 :
					throw std::runtime_error("invalid parameter type")) :
				get_parameter_tag(s, p + 1);
		}
#endif

		template<typename... T>
		struct S
		{
			template<typename U>
			using push = S<U, T...>;
			template<typename U>
			using push_back = S<T..., U>;
			template<template<typename... Args> class U>
			using rebind = U<T...>;
		};

		// Check whether the template function can be called with specific arguments
		template<typename F, typename Set>
		struct CallHelper;
		template<typename F, typename... Args>
		struct CallHelper<F, S<Args...>>
		{
			template<typename F1, typename... Args1, typename = decltype(std::declval<F1>()(std::declval<Args1>()...))>
			static char __test(int);

			template<typename...>
			static int __test(...);

			static constexpr bool value = sizeof(__test<F, Args...>(0)) == sizeof(char);
		};

		// Check Tuple contains type T
		template<typename T, typename Tuple>
		struct has_type;

		template<typename T>
		struct has_type<T, std::tuple<>> : std::false_type
		{};

		template<typename T, typename U, typename... Ts>
		struct has_type<T, std::tuple<U, Ts...>> : has_type<T, std::tuple<Ts...>>
		{};

		template<typename T, typename... Ts>
		struct has_type<T, std::tuple<T, Ts...>> : std::true_type
		{};

		// Find index of type in tuple
		template<class T, class Tuple>
		struct tuple_index;

		template<class T, class... Types>
		struct tuple_index<T, std::tuple<T, Types...>>
		{
			static const int value = 0;
		};

		template<class T, class U, class... Types>
		struct tuple_index<T, std::tuple<U, Types...>>
		{
			static const int value = 1 + tuple_index<T, std::tuple<Types...>>::value;
		};

		// Extract element from forward tuple or get default
#ifdef CROW_CAN_USE_CPP14
		template<typename T, typename Tup>
		typename std::enable_if<has_type<T&, Tup>::value, typename std::decay<T>::type&&>::type
			tuple_extract(Tup& tup)
		{
			return std::move(std::get<T&>(tup));
		}
#else
		template<typename T, typename Tup>
		typename std::enable_if<has_type<T&, Tup>::value, T&&>::type
			tuple_extract(Tup& tup)
		{
			return std::move(std::get<tuple_index<T&, Tup>::value>(tup));
		}
#endif

		template<typename T, typename Tup>
		typename std::enable_if<!has_type<T&, Tup>::value, T>::type
			tuple_extract(Tup&)
		{
			return T{};
		}

		// Kind of fold expressions in C++11
		template<bool...>
		struct bool_pack;
		template<bool... bs>
		using all_true = std::is_same<bool_pack<bs..., true>, bool_pack<true, bs...>>;

		template<int N>
		struct single_tag_to_type
		{};

		template<>
		struct single_tag_to_type<1>
		{
			using type = int64_t;
		};

		template<>
		struct single_tag_to_type<2>
		{
			using type = uint64_t;
		};

		template<>
		struct single_tag_to_type<3>
		{
			using type = double;
		};

		template<>
		struct single_tag_to_type<4>
		{
			using type = std::string;
		};

		template<>
		struct single_tag_to_type<5>
		{
			using type = std::string;
		};


		template<uint64_t Tag>
		struct arguments
		{
			using subarguments = typename arguments<Tag / 6>::type;
			using type =
				typename subarguments::template push<typename single_tag_to_type<Tag % 6>::type>;
		};

		template<>
		struct arguments<0>
		{
			using type = S<>;
		};

		template<typename... T>
		struct last_element_type
		{
			using type = typename std::tuple_element<sizeof...(T) - 1, std::tuple<T...>>::type;
		};


		template<>
		struct last_element_type<>
		{};


		// from http://stackoverflow.com/questions/13072359/c11-compile-time-array-with-logarithmic-evaluation-depth
		template<class T>
		using Invoke = typename T::type;

		template<unsigned...>
		struct seq
		{
			using type = seq;
		};

		template<class S1, class S2>
		struct concat;

		template<unsigned... I1, unsigned... I2>
		struct concat<seq<I1...>, seq<I2...>> : seq<I1..., (sizeof...(I1) + I2)...>
		{};

		template<class S1, class S2>
		using Concat = Invoke<concat<S1, S2>>;

		template<unsigned N>
		struct gen_seq;
		template<unsigned N>
		using GenSeq = Invoke<gen_seq<N>>;

		template<unsigned N>
		struct gen_seq : Concat<GenSeq<N / 2>, GenSeq<N - N / 2>>
		{};

		template<>
		struct gen_seq<0> : seq<>
		{};
		template<>
		struct gen_seq<1> : seq<0>
		{};

		template<typename Seq, typename Tuple>
		struct pop_back_helper;

		template<unsigned... N, typename Tuple>
		struct pop_back_helper<seq<N...>, Tuple>
		{
			template<template<typename... Args> class U>
			using rebind = U<typename std::tuple_element<N, Tuple>::type...>;
		};

		template<typename... T>
		struct pop_back //: public pop_back_helper<typename gen_seq<sizeof...(T)-1>::type, std::tuple<T...>>
		{
			template<template<typename... Args> class U>
			using rebind = typename pop_back_helper<typename gen_seq<sizeof...(T) - 1>::type, std::tuple<T...>>::template rebind<U>;
		};

		template<>
		struct pop_back<>
		{
			template<template<typename... Args> class U>
			using rebind = U<>;
		};

		// from http://stackoverflow.com/questions/2118541/check-if-c0x-parameter-pack-contains-a-type
		template<typename Tp, typename... List>
		struct contains : std::true_type
		{};

		template<typename Tp, typename Head, typename... Rest>
		struct contains<Tp, Head, Rest...> : std::conditional<std::is_same<Tp, Head>::value, std::true_type, contains<Tp, Rest...>>::type
		{};

		template<typename Tp>
		struct contains<Tp> : std::false_type
		{};

		template<typename T>
		struct empty_context
		{};

		template<typename T>
		struct promote
		{
			using type = T;
		};

#define CROW_INTERNAL_PROMOTE_TYPE(t1, t2) \
    template<>                             \
    struct promote<t1>                     \
    {                                      \
        using type = t2;                   \
    }

		CROW_INTERNAL_PROMOTE_TYPE(char, int64_t);
		CROW_INTERNAL_PROMOTE_TYPE(short, int64_t);
		CROW_INTERNAL_PROMOTE_TYPE(int, int64_t);
		CROW_INTERNAL_PROMOTE_TYPE(long, int64_t);
		CROW_INTERNAL_PROMOTE_TYPE(long long, int64_t);
		CROW_INTERNAL_PROMOTE_TYPE(unsigned char, uint64_t);
		CROW_INTERNAL_PROMOTE_TYPE(unsigned short, uint64_t);
		CROW_INTERNAL_PROMOTE_TYPE(unsigned int, uint64_t);
		CROW_INTERNAL_PROMOTE_TYPE(unsigned long, uint64_t);
		CROW_INTERNAL_PROMOTE_TYPE(unsigned long long, uint64_t);
		CROW_INTERNAL_PROMOTE_TYPE(float, double);
#undef CROW_INTERNAL_PROMOTE_TYPE

		template<typename T>
		using promote_t = typename promote<T>::type;

	} // namespace black_magic

	namespace detail
	{

		template<class T, std::size_t N, class... Args>
		struct get_index_of_element_from_tuple_by_type_impl
		{
			static constexpr auto value = N;
		};

		template<class T, std::size_t N, class... Args>
		struct get_index_of_element_from_tuple_by_type_impl<T, N, T, Args...>
		{
			static constexpr auto value = N;
		};

		template<class T, std::size_t N, class U, class... Args>
		struct get_index_of_element_from_tuple_by_type_impl<T, N, U, Args...>
		{
			static constexpr auto value = get_index_of_element_from_tuple_by_type_impl<T, N + 1, Args...>::value;
		};
	} // namespace detail

	namespace utility
	{
		template<class T, class... Args>
		T& get_element_by_type(std::tuple<Args...>& t)
		{
			return std::get<detail::get_index_of_element_from_tuple_by_type_impl<T, 0, Args...>::value>(t);
		}

		template<typename T>
		struct function_traits;

#ifndef CROW_MSVC_WORKAROUND
		template<typename T>
		struct function_traits : public function_traits<decltype(&T::operator())>
		{
			using parent_t = function_traits<decltype(&T::operator())>;
			static const size_t arity = parent_t::arity;
			using result_type = typename parent_t::result_type;
			template<size_t i>
			using arg = typename parent_t::template arg<i>;
		};
#endif

		template<typename ClassType, typename R, typename... Args>
		struct function_traits<R(ClassType::*)(Args...) const>
		{
			static const size_t arity = sizeof...(Args);

			typedef R result_type;

			template<size_t i>
			using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
		};

		template<typename ClassType, typename R, typename... Args>
		struct function_traits<R(ClassType::*)(Args...)>
		{
			static const size_t arity = sizeof...(Args);

			typedef R result_type;

			template<size_t i>
			using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
		};

		template<typename R, typename... Args>
		struct function_traits<std::function<R(Args...)>>
		{
			static const size_t arity = sizeof...(Args);

			typedef R result_type;

			template<size_t i>
			using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
		};
		/// @endcond

		inline static std::string base64encode(const unsigned char* data, size_t size, const char* key = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
		{
			std::string ret;
			ret.resize((size + 2) / 3 * 4);
			auto it = ret.begin();
			while (size >= 3)
			{
				*it++ = key[(static_cast<unsigned char>(*data) & 0xFC) >> 2];
				unsigned char h = (static_cast<unsigned char>(*data++) & 0x03) << 4;
				*it++ = key[h | ((static_cast<unsigned char>(*data) & 0xF0) >> 4)];
				h = (static_cast<unsigned char>(*data++) & 0x0F) << 2;
				*it++ = key[h | ((static_cast<unsigned char>(*data) & 0xC0) >> 6)];
				*it++ = key[static_cast<unsigned char>(*data++) & 0x3F];

				size -= 3;
			}
			if (size == 1)
			{
				*it++ = key[(static_cast<unsigned char>(*data) & 0xFC) >> 2];
				unsigned char h = (static_cast<unsigned char>(*data++) & 0x03) << 4;
				*it++ = key[h];
				*it++ = '=';
				*it++ = '=';
			}
			else if (size == 2)
			{
				*it++ = key[(static_cast<unsigned char>(*data) & 0xFC) >> 2];
				unsigned char h = (static_cast<unsigned char>(*data++) & 0x03) << 4;
				*it++ = key[h | ((static_cast<unsigned char>(*data) & 0xF0) >> 4)];
				h = (static_cast<unsigned char>(*data++) & 0x0F) << 2;
				*it++ = key[h];
				*it++ = '=';
			}
			return ret;
		}

		inline static std::string base64encode(std::string data, size_t size, const char* key = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
		{
			return base64encode((const unsigned char*)data.c_str(), size, key);
		}

		inline static std::string base64encode_urlsafe(const unsigned char* data, size_t size)
		{
			return base64encode(data, size, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
		}

		inline static std::string base64encode_urlsafe(std::string data, size_t size)
		{
			return base64encode((const unsigned char*)data.c_str(), size, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
		}

		inline static std::string base64decode(const char* data, size_t size)
		{
			// We accept both regular and url encoding here, as there does not seem to be any downside to that.
			// If we want to distinguish that we should use +/ for non-url and -_ for url.

			// Mapping logic from characters to [0-63]
			auto key = [](char c) -> unsigned char {
				if ((c >= 'A') && (c <= 'Z')) return c - 'A';
				if ((c >= 'a') && (c <= 'z')) return c - 'a' + 26;
				if ((c >= '0') && (c <= '9')) return c - '0' + 52;
				if ((c == '+') || (c == '-')) return 62;
				if ((c == '/') || (c == '_')) return 63;
				return 0;
				};

			// Not padded
			if (size % 4 == 2)             // missing last 2 characters
				size = (size / 4 * 3) + 1; // Not subtracting extra characters because they're truncated in int division
			else if (size % 4 == 3)        // missing last character
				size = (size / 4 * 3) + 2; // Not subtracting extra characters because they're truncated in int division

			// Padded
			else if (data[size - 2] == '=') // padded with '=='
				size = (size / 4 * 3) - 2;  // == padding means the last block only has 1 character instead of 3, hence the '-2'
			else if (data[size - 1] == '=') // padded with '='
				size = (size / 4 * 3) - 1;  // = padding means the last block only has 2 character instead of 3, hence the '-1'

			// Padding not needed
			else
				size = size / 4 * 3;

			std::string ret;
			ret.resize(size);
			auto it = ret.begin();

			// These will be used to decode 1 character at a time
			unsigned char odd;  // char1 and char3
			unsigned char even; // char2 and char4

			// Take 4 character blocks to turn into 3
			while (size >= 3)
			{
				// dec_char1 = (char1 shifted 2 bits to the left) OR ((char2 AND 00110000) shifted 4 bits to the right))
				odd = key(*data++);
				even = key(*data++);
				*it++ = (odd << 2) | ((even & 0x30) >> 4);
				// dec_char2 = ((char2 AND 00001111) shifted 4 bits left) OR ((char3 AND 00111100) shifted 2 bits right))
				odd = key(*data++);
				*it++ = ((even & 0x0F) << 4) | ((odd & 0x3C) >> 2);
				// dec_char3 = ((char3 AND 00000011) shifted 6 bits left) OR (char4)
				even = key(*data++);
				*it++ = ((odd & 0x03) << 6) | (even);

				size -= 3;
			}
			if (size == 2)
			{
				// d_char1 = (char1 shifted 2 bits to the left) OR ((char2 AND 00110000) shifted 4 bits to the right))
				odd = key(*data++);
				even = key(*data++);
				*it++ = (odd << 2) | ((even & 0x30) >> 4);
				// d_char2 = ((char2 AND 00001111) shifted 4 bits left) OR ((char3 AND 00111100) shifted 2 bits right))
				odd = key(*data++);
				*it++ = ((even & 0x0F) << 4) | ((odd & 0x3C) >> 2);
			}
			else if (size == 1)
			{
				// d_char1 = (char1 shifted 2 bits to the left) OR ((char2 AND 00110000) shifted 4 bits to the right))
				odd = key(*data++);
				even = key(*data++);
				*it++ = (odd << 2) | ((even & 0x30) >> 4);
			}
			return ret;
		}

		inline static std::string base64decode(const std::string& data, size_t size)
		{
			return base64decode(data.data(), size);
		}

		inline static std::string base64decode(const std::string& data)
		{
			return base64decode(data.data(), data.length());
		}

		inline static std::string normalize_path(const std::string& directoryPath)
		{
			std::string normalizedPath = directoryPath;
			std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
			if (!normalizedPath.empty() && normalizedPath.back() != '/')
				normalizedPath += '/';
			return normalizedPath;
		}

		inline static void sanitize_filename(std::string& data, char replacement = '_')
		{
			if (data.length() > 255)
				data.resize(255);

			static const auto toUpper = [](char c) {
				return ((c >= 'a') && (c <= 'z')) ? (c - ('a' - 'A')) : c;
				};
			// Check for special device names. The Windows behavior is really odd here, it will consider both AUX and AUX.txt
			// a special device. Thus we search for the string (case-insensitive), and then check if the string ends or if
			// is has a dangerous follow up character (.:\/)
			auto sanitizeSpecialFile = [](std::string& source, unsigned ofs, const char* pattern, bool includeNumber, char replacement) {
				unsigned i = ofs;
				size_t len = source.length();
				const char* p = pattern;
				while (*p)
				{
					if (i >= len) return;
					if (toUpper(source[i]) != *p) return;
					++i;
					++p;
				}
				if (includeNumber)
				{
					if ((i >= len) || (source[i] < '1') || (source[i] > '9')) return;
					++i;
				}
				if ((i >= len) || (source[i] == '.') || (source[i] == ':') || (source[i] == '/') || (source[i] == '\\'))
				{
					source.erase(ofs + 1, (i - ofs) - 1);
					source[ofs] = replacement;
				}
				};
			bool checkForSpecialEntries = true;
			for (unsigned i = 0; i < data.length(); ++i)
			{
				// Recognize directory traversals and the special devices CON/PRN/AUX/NULL/COM[1-]/LPT[1-9]
				if (checkForSpecialEntries)
				{
					checkForSpecialEntries = false;
					switch (toUpper(data[i]))
					{
					case 'A':
						sanitizeSpecialFile(data, i, "AUX", false, replacement);
						break;
					case 'C':
						sanitizeSpecialFile(data, i, "CON", false, replacement);
						sanitizeSpecialFile(data, i, "COM", true, replacement);
						break;
					case 'L':
						sanitizeSpecialFile(data, i, "LPT", true, replacement);
						break;
					case 'N':
						sanitizeSpecialFile(data, i, "NUL", false, replacement);
						break;
					case 'P':
						sanitizeSpecialFile(data, i, "PRN", false, replacement);
						break;
					case '.':
						sanitizeSpecialFile(data, i, "..", false, replacement);
						break;
					}
				}

				// Sanitize individual characters
				unsigned char c = data[i];
				if ((c < ' ') || ((c >= 0x80) && (c <= 0x9F)) || (c == '?') || (c == '<') || (c == '>') || (c == ':') || (c == '*') || (c == '|') || (c == '\"'))
				{
					data[i] = replacement;
				}
				else if ((c == '/') || (c == '\\'))
				{
					if (CROW_UNLIKELY(i == 0)) //Prevent Unix Absolute Paths (Windows Absolute Paths are prevented with `(c == ':')`)
					{
						data[i] = replacement;
					}
					else
					{
						checkForSpecialEntries = true;
					}
				}
			}
		}

		inline static std::string random_alphanum(std::size_t size)
		{
			static const char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
			std::random_device dev;
			std::mt19937 rng(dev());
			std::uniform_int_distribution<std::mt19937::result_type> dist(0, sizeof(alphabet) - 2);
			std::string out;
			out.reserve(size);
			for (std::size_t i = 0; i < size; i++)
				out.push_back(alphabet[dist(rng)]);
			return out;
		}

		inline static std::string join_path(std::string path, const std::string& fname)
		{
#if defined(CROW_CAN_USE_CPP17) && !defined(CROW_FILESYSTEM_IS_EXPERIMENTAL)
			return (std::filesystem::path(path) / fname).string();
#else
			if (!(path.back() == '/' || path.back() == '\\'))
				path += '/';
			path += fname;
			return path;
#endif
		}

		/**
		 * @brief Checks two string for equality.
		 * Always returns false if strings differ in size.
		 * Defaults to case-insensitive comparison.
		 */
		inline static bool string_equals(const std::string& l, const std::string& r, bool case_sensitive = false)
		{
			if (l.length() != r.length())
				return false;

			for (size_t i = 0; i < l.length(); i++)
			{
				if (case_sensitive)
				{
					if (l[i] != r[i])
						return false;
				}
				else
				{
					if (std::toupper(l[i]) != std::toupper(r[i]))
						return false;
				}
			}

			return true;
		}

		template<typename T, typename U>
		inline static T lexical_cast(const U& v)
		{
			std::stringstream stream;
			T res;

			stream << v;
			stream >> res;

			return res;
		}

		template<typename T>
		inline static T lexical_cast(const char* v, size_t count)
		{
			std::stringstream stream;
			T res;

			stream.write(v, count);
			stream >> res;

			return res;
		}


		/// Return a copy of the given string with its
		/// leading and trailing whitespaces removed.
		inline static std::string trim(const std::string& v)
		{
			if (v.empty())
				return "";

			size_t begin = 0, end = v.length();

			size_t i;
			for (i = 0; i < v.length(); i++)
			{
				if (!std::isspace(v[i]))
				{
					begin = i;
					break;
				}
			}

			if (i == v.length())
				return "";

			for (i = v.length(); i > 0; i--)
			{
				if (!std::isspace(v[i - 1]))
				{
					end = i;
					break;
				}
			}

			return v.substr(begin, end - begin);
		}
	} // namespace utility
} // namespace crow










//--------------------------------------------------------------------------------------------

















