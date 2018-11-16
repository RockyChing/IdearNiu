enum CHECK_CERT_MODES {
	CHECK_CERT_OFF,
	CHECK_CERT_ON,
	CHECK_CERT_QUIET
};

struct options {
	int verbose;                  /* Are we verbose?  (First set to -1,
	                               hence not boolean.) */
	int ntry;                     /* Number of tries per URL */
	bool ignore_length;           /* Do we heed content-length at all?  */
	bool relative_only;           /* Follow only relative links. */
	bool no_parent;               /* Restrict access to the parent
	                               directory.  */
	bool dirstruct;               /* Do we build the directory structure
	                               as we go along? */
	bool no_dirstruct;            /* Do we hate dirstruct? */
	int cut_dirs;                 /* Number of directory components to cut. */
	bool add_hostdir;             /* Do we add hostname directory? */
	bool protocol_directories;    /* Whether to prepend "http"/"ftp" to dirs. */
	bool noclobber;               /* Disables clobbering of existing data. */
	bool unlink_requested;        /* remove file before clobbering */
	char *dir_prefix;             /* The top of directory tree */
	char *lfilename;              /* Log filename */
	char *choose_config;          /* Specified config file */
	bool noconfig;                /* Ignore all config files? */
	bool force_html;              /* Is the input file an HTML file? */

	char *default_page;           /* Alternative default page (index file) */


	char **accepts;               /* List of patterns to accept. */
	char **rejects;               /* List of patterns to reject. */
	const char **excludes;        /* List of excluded FTP directories. */
	const char **includes;        /* List of FTP directories to
	                               follow. */
	bool ignore_case;             /* Whether to ignore case when
	                               matching dirs and files */

	void *(*regex_compile_fun)(const char *);             /* Function to compile a regex. */

	char **domains;               /* See host.c */

	char **follow_tags;           /* List of HTML tags to recursively follow. */
	char **ignore_tags;           /* List of HTML tags to ignore if recursing. */


	bool always_rest;             /* Always use REST. */
	wgint start_pos;              /* Start position of a download. */
	char *ftp_user;               /* FTP username */
	char *ftp_passwd;             /* FTP password */
	bool netrc;                   /* Whether to read .netrc. */
	bool ftp_glob;                /* FTP globbing */

	char *http_user;              /* HTTP username. */
	char *http_passwd;            /* HTTP password. */
	bool http_keep_alive;         /* whether we use keep-alive */

	char *ftp_proxy, *https_proxy;
	char **no_proxy;
	char *base_href;
	char *progress_type;          /* progress indicator type. */
	int  show_progress;           /* Show only the progress bar */
	bool noscroll;                /* Don't scroll the filename in the progressbar */
	char *proxy_user; /*oli*/
	char *proxy_passwd;

	double read_timeout;          /* The read/write timeout. */
	double dns_timeout;           /* The DNS timeout. */
	double connect_timeout;       /* The connect timeout. */

	bool random_wait;             /* vary from 0 .. wait secs by random()? */
	double wait;                  /* The wait period between retrievals. */
	double waitretry;             /* The wait period between retries. - HEH */

	wgint limit_rate;             /* Limit the download rate to this
	                               many bps. */
	SUM_SIZE_INT quota;           /* Maximum file size to download and
	                               store. */

	bool server_response;         /* Do we print server response? */
	bool save_headers;            /* Do we save headers together with
	                               file? */
	bool content_on_error;        /* Do we output the content when the HTTP
	                               status code indicates a server error */

	bool debug;                   /* Debugging on/off */

	bool timestamping;            /* Whether to use time-stamping. */
	bool if_modified_since;       /* Whether to use conditional get requests.  */

	int backups;                  /* Are numeric backups made? */

	char *useragent;              /* User-Agent string, which can be set
	                               to something other than Wget. */
	char *referer;                /* Naughty Referer, which can be
	                               set to something other than
	                               NULL. */

	bool htmlify;                 /* Do we HTML-ify the OS-dependent
	                               listings? */

	char *dot_style;
	wgint dot_bytes;              /* How many bytes in a printing
	                               dot. */
	int dots_in_line;             /* How many dots in one line. */
	int dot_spacing;              /* How many dots between spacings. */

	bool delete_after;            /* Whether the files will be deleted
	                               after download. */



#ifdef HAVE_SSL
	enum {
		secure_protocol_auto,
		secure_protocol_sslv2,
		secure_protocol_sslv3,
		secure_protocol_tlsv1,
		secure_protocol_tlsv1_1,
		secure_protocol_tlsv1_2,
		secure_protocol_tlsv1_3,
		secure_protocol_pfs
	} secure_protocol;            /* type of secure protocol to use. */
	int check_cert;               /* whether to validate the server's cert */
	char *cert_file;              /* external client certificate to use. */
	char *private_key;            /* private key file (if not internal). */
	enum keyfile_type {
		keyfile_pem,
		keyfile_asn1
	} cert_type;                  /* type of client certificate file */
  	enum keyfile_type
    private_key_type;           /* type of private key file */

	char *ca_directory;           /* CA directory (hash files) */
	char *ca_cert;                /* CA certificate file to use */
	char *crl_file;               /* file with CRLs */

	char *pinnedpubkey;           /* Public key (PEM/DER) file, or any number
	                               of base64 encoded sha256 hashes preceded by
	                               \'sha256//\' and separated by \';\', to verify
	                               peer against */

	char *random_file;            /* file with random data to seed the PRNG */
	char *egd_file;               /* file name of the egd daemon socket */
	char *tls_ciphers_string;
#endif /* HAVE_SSL */

	bool cookies;                 /* whether cookies are used. */
	char *cookies_input;          /* file we're loading the cookies from. */
	char *cookies_output;         /* file we're saving the cookies to. */
	bool keep_badhash;            /* Keep files with checksum mismatch. */
	bool keep_session_cookies;    /* whether session cookies should be
	                               saved and loaded. */

	char *post_data;              /* POST query string */
	char *post_file_name;         /* File to post */
	char *method;                 /* HTTP Method to use in Header */
	char *body_data;              /* HTTP Method Data String */
	char *body_file;              /* HTTP Method File */

	enum {
		restrict_unix,
		restrict_vms,
		restrict_windows
	} restrict_files_os;          /* file name restriction ruleset. */
	bool restrict_files_ctrl;     /* non-zero if control chars in URLs
	                               are restricted from appearing in
	                               generated file names. */
	bool restrict_files_nonascii; /* non-zero if bytes with values greater
                                   than 127 are restricted. */
	enum {
		restrict_no_case_restriction,
		restrict_lowercase,
		restrict_uppercase
	} restrict_files_case;        /* file name case restriction. */
	bool preserve_perm;           /* whether remote permissions are used
                                   or that what is set by umask. */
	enum {
		prefer_ipv4,
		prefer_ipv6,
		prefer_none
  	} prefer_family;              /* preferred address family when more
                                   than one type is available */

	bool content_disposition;     /* Honor HTTP Content-Disposition header. */
	bool auth_without_challenge;  /* Issue Basic authentication creds without
	                               waiting for a challenge. */


	bool trustservernames;

	bool useservertimestamps;     /* Update downloaded files' timestamps to
	                               match those on server? */
	bool report_bps;              /*Output bandwidth in bits format*/


};

extern struct options opt;
