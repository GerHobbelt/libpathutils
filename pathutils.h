
/* curl_sanitize_file_name flags */

#define CURL_SANITIZE_ALLOW_COLONS              (1<<0)  /* Allow colons */
#define CURL_SANITIZE_ALLOW_PATH                (1<<1)  /* Allow path separators and colons */
#define CURL_SANITIZE_ALLOW_ONLY_RELATIVE_PATH  (1<<2)  /* Allow only relative path specs; implies CURL_SANITIZE_ALLOW_PATH */
#define CURL_SANITIZE_ALLOW_RESERVED            (1<<3)  /* Allow reserved device names */
#define CURL_SANITIZE_ALLOW_DOTFILES            (1<<4)  /* Allow UNIX 'hidden' dotfiles (and dotdirectories), e.g. '.gitattributes' */
#define CURL_SANITIZE_ALLOW_TRUNCATE            (1<<5)  /* Allow truncating a long filename */

typedef enum {
	CURL_SANITIZE_ERR_OK = 0,           /* 0 - OK */
	CURL_SANITIZE_ERR_INVALID_PATH,     /* 1 - the path is invalid */
	CURL_SANITIZE_ERR_BAD_ARGUMENT,     /* 2 - bad function parameter */
	CURL_SANITIZE_ERR_OUT_OF_MEMORY,    /* 3 - out of memory */
	CURL_SANITIZE_ERR_LAST /* never use! */
} CurlSanitizeCode;

CurlSanitizeCode curl_sanitize_file_name(char **const sanitized, const char *file_name, int flags);

#ifdef  __cplusplus
} /* end of extern "C" */
#endif




/* sanitize a local file for writing, return TRUE on success */
bool tool_sanitize_output_file_path(struct per_transfer *per);

/* create a local file for writing, return TRUE on success */
bool tool_create_output_file(struct OutStruct *outs,
							 struct per_transfer *per);



bool sanitize_with_extreme_prejudice; /* Sanitize URLs with extreme prejudice, i.e.
																				 accept some pretty shoddy input and make
																				 the best of it.
																				 Output filenames are also sanitized with extreme
																				 prejudice: on all platform we will ensure
																				 that the generated filenames are 'sane',
																				 i.e. non-hidden (UNIX dotfiles) and without
																				 any crufty chracters that may thwart your
																				 fileesystem. */



