#ifndef CONVERT_H
#define CONVERT_H

enum convert_options {
	CO_NOCONVERT = 0,             /* don't convert this URL */
	CO_CONVERT_TO_RELATIVE,       /* convert to relative, e.g. to
	                               "../../otherdir/foo.gif" */
	CO_CONVERT_BASENAME_ONLY,     /* convert the file portion only (basename)
	                               leaving the rest of the URL unchanged */
	CO_CONVERT_TO_COMPLETE,       /* convert to absolute, e.g. to
	                               "http://orighost/somedir/bar.jpg". */
	CO_NULLIFY_BASE               /* change to empty string. */
};

struct url;

/* A structure that defines the whereabouts of a URL, i.e. its
   position in an HTML document, etc.  */

struct urlpos {
	struct url *url;              /* the URL of the link, after it has
	                               been merged with the base */
	char *local_name;             /* local file to which it was saved
	                               (used by convert_links) */

	/* reserved for special links such as <base href="..."> which are
	 used when converting links, but ignored when downloading.  */
	unsigned int ignore_when_downloading  :1;

	/* Information about the original link: */

	unsigned int link_relative_p  :1; /* the link was relative */
	unsigned int link_complete_p  :1; /* the link was complete (had host name) */
	unsigned int link_base_p  :1;     /* the url came from <base href=...> */
	unsigned int link_inline_p    :1; /* needed to render the page */
	unsigned int link_css_p   :1;     /* the url came from CSS */
	unsigned int link_noquote_html_p :1; /* from HTML, but doesn't need " */
	unsigned int link_expect_html :1; /* expected to contain HTML */
	unsigned int link_expect_css  :1; /* expected to contain CSS */

	unsigned int link_refresh_p   :1; /* link was received from
	                                   <meta http-equiv=refresh content=...> */
	int refresh_timeout;              /* for reconstructing the refresh. */

	/* Conversion requirements: */
	enum convert_options convert;     /* is conversion required? */

	/* URL's position in the buffer. */
	int pos, size;

	struct urlpos *next;              /* next list element */
};

/* downloaded_file() takes a parameter of this type and returns this type. */
typedef enum {
	/* Return enumerators: */
	FILE_NOT_ALREADY_DOWNLOADED = 0,

	/* Return / parameter enumerators: */
	FILE_DOWNLOADED_NORMALLY,
	FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED,

	/* Parameter enumerators: */
	CHECK_FOR_FILE
} downloaded_file_t;

downloaded_file_t downloaded_file(downloaded_file_t, const char *);

#endif /* CONVERT_H */

