

/*!
 * \brief   leptDebugGenFilepath()
 *
 * \param[in]    template_printf_fmt_str
 *
 * <pre>
 * Notes:
 * 
 * - leptonica APIs will always follow-up with a call to genPathname() afterwards, before using the generated path.
 *   This implies that leptDebugGenFilepath() MAY safely generate an unsafe.relative/incomplete path string.
 * 
 * - the returned string is stored in a cache array and will remain owned by the leptDebug code: callers MUST NOT free/release the returned string.
 *
 *
 *
 * </pre>
 */
char *
leptDebugGenFilepath(const char* path_fmt_str, ...)
{
	leptCreateDiagnoticsSpecInstance();

	updateStepId();

	/* if (diag_spec.must_regenerate_path)   -- we don't know the filename part so we have to the work anyway! */
	{
		char* fn = NULL;
		size_t fn_len = 0;

		if (path_fmt_str && path_fmt_str[0]) {
			va_list va;
			va_start(va, path_fmt_str);
			fn = string_vasprintf(path_fmt_str, va);
			va_end(va);

			convertSepCharsInPath(fn, UNIX_PATH_SEPCHAR);
			if (getPathRootLength(fn) != 0) {
				L_WARNING("The intent of %s() is to generate full paths from RELATIVE paths; this is not: '%s'\n", __func__, fn);
			}

			fn_len = strlen(fn);
		}

		const char* bp = leptDebugGetFileBasePath();
		size_t bp_len = strlen(bp);

		// sorta like leptDebugGetStepIdAsString(), but with filepath elements thrown in:
		size_t bufsize = L_MAX_STEPS_DEPTH * 4 * sizeof(diag_spec.steps.vals[0]) + 5 + 5;  // rough upper limit estimate...
		//bufsize += bp_len;
		bufsize += fn_len;
		for (int i = 0; i < diag_spec.steps.actual_depth; i++) {
			const char* str = sarrayGetString(diag_spec.step_paths, i, L_NOCOPY);
			bufsize += strlen(str);
		}
		char* buf = (char*)LEPT_MALLOC(bufsize);
		if (!buf) {
			return (char*)ERROR_PTR("could not allocate string buffer", __func__, NULL);
		}

		{
			char* p = buf;
			char* e = buf + bufsize;
			int max_level = diag_spec.steps.actual_depth + 1;
			int w = diag_spec.step_width + 1;
			for (int i = 0; i < max_level; i++) {
				const char* str = sarrayGetString(diag_spec.step_paths, i, L_NOCOPY);
				unsigned int v = diag_spec.steps.vals[i];    // MSVC complains about feeding a l_atomic into a variadic function like printf() (because I was compiling in forced C++ mode, anyway). This hotfixes that.
				assert(e - p > w + 3 + strlen(str));
				if (str && *str) {
					int n = snprintf(p, e - p, "%s-%0*u/", str, w, v);
					assert(n > 0);
					p += n;
				}
				else {
					// don't just produce numbered directories,
					// instead append the depth number to the previous dir:
					--p;
					int n = snprintf(p, e - p, ".%0*u/", w, v);
					assert(n > 0);
					p += n;
				}
			}

			char* fn_part = p;

			// drop the trailing '/':
			{
				--p;

				// the last level is not used as a directory, but as a filename PREFIX:
				*p++ = '.';

				// inject unique number in the last path element, just after the file prefix:
				int l = snprintf(p, e - p, "%04u.", leptDebugGetForeverIncreasingIdValue());
				assert(l < e - p);
				assert(l > 0);
				assert(p[l] == 0);
				p += l;

				// drop that last '.' if there's no filename suffix specified:
				if (fn_len == 0) {
					--p;
					*p = 0;
				}
			}
			assert(e - p > 2 + fn_len);
			if (fn_len > 0) {
				strcpy(p, fn);
			}
			else {
				*p = 0;
			}

			assert(strlen(buf) + 1 < bufsize);

			// now we sanitize the mother: '..' anywhere becomes '__' and non-ASCII, non-UTF8 is gentrified to '_' as well.
			p = buf;
			while (*p) {
				unsigned char c = p[0];
				if (c == '.' && p[1] == '.') {
					p[0] = '_';
					p[1] = '_';
					p += 2;
					continue;
				}
				else if (c == '.' && p > buf && p[-1] == '/') {
					// dir/.dotfile --> dir/_dotfile  :: unhide UNIX-style 'hidden' files
					p[0] = '_';
					p += 1;
					continue;
				}
				else if (c == '.' && (p[1] == '/' || p[1] == '\\' || p[1] == 0)) {
					// dirs & files cannot end in a dot
					p[0] = '_';
					p += 1;
					continue;
				}
				else if (c <= ' ') {   // replace spaces and low-ASCII chars
					p[0] = '_';
				}
				else if (strchr("$~%^&*?|;:'\"<>`", c)) {
					p[0] = '_';
				}
				else if (c == '\\') {
					if (p < fn_part) {
						p[0] = '/';
					}
					else {
						p[0] = '_';
					}
				}
				else if (c == '/' && p >= fn_part) {
					p[0] = '_';
				}
				p++;
			}
		}

		char* np = pathSafeJoin(bp, buf);

		stringDestroy(&fn);
		stringDestroy(&buf);

		sarrayAddString(diag_spec.last_generated_paths, np, L_INSERT);

		return np;
	}
}

