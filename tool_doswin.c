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
static CurlSanitizeCode truncate_dryrun(const char *path,
                                    const size_t truncate_pos);
#ifdef MSDOS
static CurlSanitizeCode msdosify(char **const sanitized, const char *file_name,
                             int flags);
#endif
static CurlSanitizeCode rename_if_reserved_dos_device_name(char **const sanitized,
                                                       const char *file_name,
                                                       int flags);
#endif /* !UNITTESTS (static declarations used if no unit tests) */

static size_t get_max_sanitized_len(const char *file_name, int flags);

// move src to dst, where the copy in dst will overlap the source in src
static void strmov(char *dst, char *src) {
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
CurlSanitizeCode curl_sanitize_file_name(char **const sanitized, const char *file_name, int flags)
{
  char *p, *target;
  size_t len;
  CurlSanitizeCode sc;
  size_t max_sanitized_len;

  if(!sanitized)
    return CURL_SANITIZE_ERR_BAD_ARGUMENT;

  *sanitized = NULL;

  if(!file_name)
    return CURL_SANITIZE_ERR_BAD_ARGUMENT;

  len = strlen(file_name);
  max_sanitized_len = get_max_sanitized_len(file_name, flags);

  if(len > max_sanitized_len) {
    if(!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
       truncate_dryrun(file_name, max_sanitized_len))
      return CURL_SANITIZE_ERR_INVALID_PATH;

    len = max_sanitized_len;
  }

  target = malloc(len + 1);
  if(!target)
    return CURL_SANITIZE_ERR_OUT_OF_MEMORY;

  strncpy(target, file_name, len);
  target[len] = '\0';

  if (flags & CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH) {
	flags |= CURL_SANITIZE_ALLOW_PATH;
	flags &= ~CURL_SANITIZE_ALLOW_COLONS;

	// do not tolerate absolute and UNC paths:
	if(!strncmp(target, "\\\\?\\", 4))
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
  if((flags & CURL_SANITIZE_ALLOW_PATH) && !(flags & CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH) && !strncmp(target, "\\\\?\\", 4))
    /* Skip the literal path prefix \\?\ */
    p = target + 4;
  else
#endif
    p = target;

  /* replace control characters and other banned characters */
  bool s_o_p = TRUE;
  bool dot = FALSE;
  for(; *p; ++p) {
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
			for ( ; p[i] == '/' || p[i] == '\\'; i++)
				;
			if (i > 1)
				strmov(p + 1, p + i);

			// remove trailing spaces and periods per path segment
			char *q;
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
  if(!(flags & CURL_SANITIZE_ALLOW_PATH) && len) {
    char *clip = NULL;

    p = &target[len];
    do {
      --p;
      if(*p != ' ' && *p != '.')
        break;
      clip = p;
    } while(p != target);

    if(clip) {
      *clip = '\0';
      len = clip - target;
    }
  }

#ifdef MSDOS
  sc = msdosify(&p, target, flags);
  free(target);
  if(sc)
    return sc;
  target = p;
  len = strlen(target);

  if(len > max_sanitized_len) {
    free(target);
    return CURL_SANITIZE_ERR_INVALID_PATH;
  }
#endif

  if (!(flags & CURL_SANITIZE_ALLOW_RESERVED)) {
    sc = rename_if_reserved_dos_device_name(&p, target, flags);
    free(target);
    if(sc)
      return sc;
    target = p;
    len = strlen(target);

    if(len > max_sanitized_len) {
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
static size_t get_max_sanitized_len(const char *file_name, int flags)
{
  size_t max_sanitized_len;

  if((flags & CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH) && !(flags & CURL_SANITIZE_ALLOW_TRUNCATE)) {
    return 32767-1;
  }

  if((flags & CURL_SANITIZE_ALLOW_PATH)) {
#ifdef UNITTESTS
    if(file_name[0] == '\\' && file_name[1] == '\\')
      max_sanitized_len = 32767-1;
    else
      max_sanitized_len = 259;
#elif defined(MSDOS)
    max_sanitized_len = PATH_MAX-1;
#else
    /* Paths that start with \\ may be longer than PATH_MAX (MAX_PATH) on any
       version of Windows. Starting in Windows 10 1607 (build 14393) any path
       may be longer than PATH_MAX if the user has opted-in and the application
       supports it. */
    if((file_name[0] == '\\' && file_name[1] == '\\') ||
       curlx_verify_windows_version(10, 0, 14393, PLATFORM_WINNT,
                                    VERSION_GREATER_THAN_EQUAL))
      max_sanitized_len = 32767-1;
    else
      max_sanitized_len = PATH_MAX-1;
#endif
  }
  else
#ifdef UNITTESTS
    max_sanitized_len = 255;
#else
    /* The maximum length of a filename.
       FILENAME_MAX is often the same as PATH_MAX, in other words it is 260 and
       does not discount the path information therefore we shouldn't use it. */
    max_sanitized_len = (PATH_MAX-1 > 255) ? 255 : PATH_MAX-1;
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
CurlSanitizeCode truncate_dryrun(const char *path, const size_t truncate_pos)
{
  size_t len;

  if(!path)
    return CURL_SANITIZE_ERR_BAD_ARGUMENT;

  len = strlen(path);

  if(truncate_pos > len)
    return CURL_SANITIZE_ERR_BAD_ARGUMENT;

  if(!len || !truncate_pos)
    return CURL_SANITIZE_ERR_INVALID_PATH;

  if(strpbrk(&path[truncate_pos - 1], "\\/:"))
    return CURL_SANITIZE_ERR_INVALID_PATH;

  /* C:\foo can be truncated but C:\foo:ads cannot */
  if(truncate_pos > 1) {
    const char *p = &path[truncate_pos - 1];
    do {
      --p;
      if(*p == ':')
        return CURL_SANITIZE_ERR_INVALID_PATH;
    } while(p != path && *p != '\\' && *p != '/');
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
CurlSanitizeCode msdosify(char **const sanitized, const char *file_name,
                      int flags)
{
  char dos_name[PATH_MAX];
  static const char illegal_chars_dos[] = ".+, ;=[]" /* illegal in DOS */
    "|<>/\\\":?*"; /* illegal in DOS & W95 */
  static const char *illegal_chars_w95 = &illegal_chars_dos[8];
  int idx, dot_idx;
  const char *s = file_name;
  char *d = dos_name;
  const char *const dlimit = dos_name + sizeof(dos_name) - 1;
  const char *illegal_aliens = illegal_chars_dos;
  size_t len = sizeof(illegal_chars_dos) - 1;

  if(!sanitized)
    return CURL_SANITIZE_ERR_BAD_ARGUMENT;

  *sanitized = NULL;

  if(!file_name)
    return CURL_SANITIZE_ERR_BAD_ARGUMENT;

  if(strlen(file_name) > PATH_MAX-1 &&
     (!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
      truncate_dryrun(file_name, PATH_MAX-1)))
    return CURL_SANITIZE_ERR_INVALID_PATH;

  /* Support for Windows 9X VFAT systems, when available. */
  if(_use_lfn(file_name)) {
    illegal_aliens = illegal_chars_w95;
    len -= (illegal_chars_w95 - illegal_chars_dos);
  }

  /* Get past the drive letter, if any. */
  if(s[0] >= 'A' && s[0] <= 'z' && s[1] == ':') {
    *d++ = *s++;
    *d = ((flags & (SANITIZE_ALLOW_COLONS|SANITIZE_ALLOW_PATH))) ? ':' : '_';
    ++d; ++s;
  }

  for(idx = 0, dot_idx = -1; *s && d < dlimit; s++, d++) {
    if(memchr(illegal_aliens, *s, len)) {

      if((flags & (SANITIZE_ALLOW_COLONS|SANITIZE_ALLOW_PATH)) && *s == ':')
        *d = ':';
      else if((flags & CURL_SANITIZE_ALLOW_PATH) && (*s == '/' || *s == '\\'))
        *d = *s;
      /* Dots are special: DOS does not allow them as the leading character,
         and a filename cannot have more than a single dot. We leave the
         first non-leading dot alone, unless it comes too close to the
         beginning of the name: we want sh.lex.c to become sh_lex.c, not
         sh.lex-c.  */
      else if(*s == '.') {
        if((flags & CURL_SANITIZE_ALLOW_PATH) && idx == 0 &&
           (s[1] == '/' || s[1] == '\\' ||
            (s[1] == '.' && (s[2] == '/' || s[2] == '\\')))) {
          /* Copy "./" and "../" verbatim.  */
          *d++ = *s++;
          if(d == dlimit)
            break;
          if(*s == '.') {
            *d++ = *s++;
            if(d == dlimit)
              break;
          }
          *d = *s;
        }
        else if(idx == 0)
          *d = '_';
        else if(dot_idx >= 0) {
          if(dot_idx < 5) { /* 5 is a heuristic ad-hoc'ery */
            d[dot_idx - idx] = '_'; /* replace previous dot */
            *d = '.';
          }
          else
            *d = '-';
        }
        else
          *d = '.';

        if(*s == '.')
          dot_idx = idx;
      }
      else if(*s == '+' && s[1] == '+') {
        if(idx - 2 == dot_idx) { /* .c++, .h++ etc. */
          *d++ = 'x';
          if(d == dlimit)
            break;
          *d   = 'x';
        }
        else {
          /* libg++ etc.  */
          if(dlimit - d < 4) {
            *d++ = 'x';
            if(d == dlimit)
              break;
            *d   = 'x';
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
    if(*s == '/' || *s == '\\') {
      idx = 0;
      dot_idx = -1;
    }
    else
      idx++;
  }
  *d = '\0';

  if(*s) {
    /* dos_name is truncated, check that truncation requirements are met,
       specifically truncating a filename suffixed by an alternate data stream
       or truncating the entire filename is not allowed. */
    if(!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) || strpbrk(s, "\\/:") ||
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
CurlSanitizeCode rename_if_reserved_dos_device_name(char **const sanitized,
                                                const char *file_name,
                                                int flags)
{
  char *p, *base, *target;
  size_t t_len, max_sanitized_len;
#ifdef MSDOS
  struct_stat st_buf;
#endif

  if(!sanitized)
    return CURL_SANITIZE_ERR_BAD_ARGUMENT;

  *sanitized = NULL;

  if(!file_name)
    return CURL_SANITIZE_ERR_BAD_ARGUMENT;

  max_sanitized_len = get_max_sanitized_len(file_name, flags);

  t_len = strlen(file_name);
  if(t_len > max_sanitized_len) {
    if(!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
       truncate_dryrun(file_name, max_sanitized_len))
      return CURL_SANITIZE_ERR_INVALID_PATH;

    t_len = max_sanitized_len;
  }

  target = malloc(t_len + 1);
  if(!target)
    return CURL_SANITIZE_ERR_OUT_OF_MEMORY;

  strncpy(target, file_name, t_len);
  target[t_len] = '\0';

#ifndef MSDOS
  if((flags & CURL_SANITIZE_ALLOW_PATH) &&
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
  for(p = target; p; p = (p == target && target != base ? base : NULL)) {
    size_t p_len;
    int x = (curl_strnequal(p, "CON", 3) ||
             curl_strnequal(p, "PRN", 3) ||
             curl_strnequal(p, "AUX", 3) ||
             curl_strnequal(p, "NUL", 3)) ? 3 :
            (curl_strnequal(p, "CLOCK$", 6)) ? 6 :
            (curl_strnequal(p, "COM", 3) || curl_strnequal(p, "LPT", 3)) ?
              (('1' <= p[3] && p[3] <= '9') ? 4 : 3) : 0;

    if(!x)
      continue;

    /* the devices may be accessible with an extension or ADS, for
       example CON.AIR and 'CON . AIR' and CON:AIR access console */

    for(; p[x] == ' '; ++x)
      ;

    if(p[x] == '.') {
      p[x] = '_';
      continue;
    }
    else if(p[x] == ':') {
      if(!(flags & (CURL_SANITIZE_ALLOW_COLONS|CURL_SANITIZE_ALLOW_PATH))) {
        p[x] = '_';
        continue;
      }
      ++x;
    }
    else if(p[x]) /* no match */
      continue;

    /* p points to 'CON' or 'CON ' or 'CON:' (if colon/path allowed), etc.
       Prepend a '_'. */

    p_len = strlen(p);

    if(t_len == max_sanitized_len) {
      --p_len;
      --t_len;
      if(!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
         truncate_dryrun(target, t_len)) {
        free(target);
        return CURL_SANITIZE_ERR_INVALID_PATH;
      }
      target[t_len] = '\0';
    }
    else {
      p = realloc(target, t_len + 2);
      if(!p) {
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
  if(base && ((stat(base, &st_buf)) == 0) && (S_ISCHR(st_buf.st_mode))) {
    /* Prepend a '_' */
    size_t blen = strlen(base);
    if(blen) {
      size_t t_len = strlen(target);

      if(t_len == max_sanitized_len) {
        --blen;
        --t_len;
        if(!(flags & CURL_SANITIZE_ALLOW_TRUNCATE) ||
           truncate_dryrun(target, t_len)) {
          free(target);
          return CURL_SANITIZE_ERR_INVALID_PATH;
        }
        target[t_len] = '\0';
      }
      else {
        p = realloc(target, t_len + 2);
        if(!p) {
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

  *sanitized = target;
  return CURL_SANITIZE_ERR_OK;
}

#if defined(MSDOS) && (defined(__DJGPP__) || defined(__GO32__))

/*
 * Disable program default argument globbing. We do it on our own.
 */
char **__crt0_glob_function(char *arg)
{
  (void)arg;
  return (char **)0;
}

#endif /* MSDOS && (__DJGPP__ || __GO32__) */

#ifdef _WIN32

#if !defined(CURL_WINDOWS_UWP) && \
  !defined(CURL_DISABLE_CA_SEARCH) && !defined(CURL_CA_SEARCH_SAFE)
/* Search and set the CA cert file for Windows.
 *
 * Do not call this function if Schannel is the selected SSL backend. We allow
 * setting CA location for Schannel only when explicitly specified by the user
 * via CURLOPT_CAINFO / --cacert.
 *
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
CURLcode FindWin32CACert(struct OperationConfig *config,
                         const TCHAR *bundle_file)
{
  CURLcode result = CURLE_OK;
    DWORD res_len, written;
    TCHAR *buf;

    res_len = SearchPath(NULL, bundle_file, NULL, 0, NULL, NULL);
    if(!res_len)
      return CURLE_OK;

    buf = malloc(res_len * sizeof(TCHAR));
    if(!buf)
      return CURLE_OUT_OF_MEMORY;

    written = SearchPath(NULL, bundle_file, NULL, res_len, buf, NULL);
    if(written) {
      if (written >= res_len) {
        // unexpected fault!
        DEBUGASSERT(!"Unexpected fault in SearchPath API usage");
        result = CURLE_OUT_OF_MEMORY;
      }
      else {
        char *mstr = curlx_convert_tchar_to_UTF8(buf);
        Curl_safefree(config->cacert);
        if(mstr)
          config->cacert = strdup(mstr);
        curlx_unicodefree(mstr);
        if(!config->cacert)
          result = CURLE_OUT_OF_MEMORY;
      }
    }
    Curl_safefree(buf);

  return result;
}
#endif

/* Get a list of all loaded modules with full paths.
 * Returns slist on success or NULL on error.
 */
struct curl_slist *GetLoadedModulePaths(void)
{
  HANDLE hnd = INVALID_HANDLE_VALUE;
  MODULEENTRY32 mod = {0};
  struct curl_slist *slist = NULL;

  mod.dwSize = sizeof(MODULEENTRY32);

  do {
    hnd = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
  } while(hnd == INVALID_HANDLE_VALUE && GetLastError() == ERROR_BAD_LENGTH);

  if(hnd == INVALID_HANDLE_VALUE)
    goto error;

  if(!Module32First(hnd, &mod))
    goto error;

  do {
    char *path; /* points to stack allocated buffer */
    struct curl_slist *temp;

#ifdef UNICODE
    /* sizeof(mod.szExePath) is the max total bytes of wchars. the max total
       bytes of multibyte chars will not be more than twice that. */
    char buffer[sizeof(mod.szExePath) * 2];
    if(!WideCharToMultiByte(CP_ACP, 0, mod.szExePath, -1,
                            buffer, sizeof(buffer), NULL, NULL))
      goto error;
    path = buffer;
#else
    path = mod.szExePath;
#endif
    temp = curl_slist_append(slist, path);
    if(!temp)
      goto error;
    slist = temp;
  } while(Module32Next(hnd, &mod));

  goto cleanup;

error:
  curl_slist_free_all(slist);
  slist = NULL;
cleanup:
  if(hnd != INVALID_HANDLE_VALUE)
    CloseHandle(hnd);
  return slist;
}

bool tool_term_has_bold;

#ifndef CURL_WINDOWS_UWP
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
  if(InterlockedExchange(&TerminalSettings.valid, (LONG)FALSE))
    SetConsoleMode(TerminalSettings.hStdOut, TerminalSettings.dwOutputMode);
}

/* This is the console signal handler.
 * The system calls it in a separate thread.
 */
static BOOL WINAPI signal_handler(DWORD type)
{
  if(type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT)
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
  if(TerminalSettings.hStdOut == INVALID_HANDLE_VALUE ||
     !GetConsoleMode(TerminalSettings.hStdOut,
                     &TerminalSettings.dwOutputMode) ||
     !curlx_verify_windows_version(10, 0, 16299, PLATFORM_WINNT,
                                   VERSION_GREATER_THAN_EQUAL))
    return;

  if((TerminalSettings.dwOutputMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
    tool_term_has_bold = true;
  else {
    /* The signal handler is set before attempting to change the console mode
       because otherwise a signal would not be caught after the change but
       before the handler was installed. */
    (void)InterlockedExchange(&TerminalSettings.valid, (LONG)TRUE);
    if(SetConsoleCtrlHandler(signal_handler, TRUE)) {
      if(SetConsoleMode(TerminalSettings.hStdOut,
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
  if(curlx_verify_windows_version(6, 0, 0, PLATFORM_WINNT,
                                  VERSION_GREATER_THAN_EQUAL))
    tool_isVistaOrGreater = true;
  else
    tool_isVistaOrGreater = false;

  QueryPerformanceFrequency(&tool_freq);

#ifndef CURL_WINDOWS_UWP
  init_terminal();
#endif

  return CURLE_OK;
}

#endif /* _WIN32 */

#endif /* _WIN32 || MSDOS */
