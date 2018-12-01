/* SSL support via OpenSSL library.
   Copyright (C) 2000-2012, 2015, 2018 Free Software Foundation, Inc.
   Originally contributed by Christian Fraenkel.
 */
#include "wget.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#if OPENSSL_VERSION_NUMBER >= 0x00907000
#include <openssl/conf.h>
#include <openssl/engine.h>
#endif

#include "utils.h"
#include "connect.h"
#include "url.h"
#include "ssl.h"

/* Application-wide SSL context.  This is common to all SSL connections.  */
static SSL_CTX *ssl_ctx;

/* Initialize the SSL's PRNG using various methods.
 * PRNG(Pseudo Random Noise Generation), 即伪随机噪声生成，用于生成各种密码学操作中所需的随机数。
 */
static void init_prng(void)
{
    char namebuf[256];
    const char *random_file;

    /* Seed from a file specified by the user.  This will be the file
       specified with --random-file, $RANDFILE, if set, or ~/.rnd, if it exists.  */
    if (opt.random_file) {
        random_file = opt.random_file;
    } else {
        /* Get the random file name using RAND_file_name. */
        namebuf[0] = '\0';
		/* const char *RAND_file_name(char *buf, size_t num);
		   RAND_file_name() generates a default path for the random seed file.
		   @buf points to a buffer of size @num in which to store the filename.
		   The seed file is $RANDFILE if that environment variable is set, $HOME/.rnd otherwise.
		   If $HOME is not set either, or num is too small for the path name, an error occurs.

		   RAND_file_name() returns a pointer to buf on success, and NULL on error.
		 */
        random_file = RAND_file_name(namebuf, sizeof(namebuf));
    }

	log_info("random_file: %s", random_file);
    if (random_file && *random_file)
        /* Seed at most 16k (apparently arbitrary value borrowed from curl) from random file. */
		/* int RAND_load_file(const char *filename, long max_bytes);
		   RAND_load_file() reads a number of bytes from file @filename and adds them to the PRNG. 
		   If @max_bytes is non-negative, up to to max_bytes are read; starting with OpenSSL 0.9.5,
		   if max_bytes is -1, the complete file is read.

		   RAND_load_file() returns the number of bytes read.
		 */
        RAND_load_file (random_file, 16384);

#if 0 /* don't do this by default */
    {
        int maxrand = 500;

        /* Still not random enough, presumably because neither /dev/random
           nor EGD were available.  Try to seed OpenSSL's PRNG with libc
           PRNG.  This is cryptographically weak and defeats the purpose
           of using OpenSSL, which is why it is highly discouraged.  */
        logprintf (LOG_NOTQUIET, _("WARNING: using a weak random seed.\n"));

        while (RAND_status () == 0 && maxrand-- > 0) {
        unsigned char rnd = random_number (256);
        RAND_seed (&rnd, sizeof (rnd));
        }
    }
#endif
}

/* Print errors in the OpenSSL error stack. */
static void print_errors (void)
{
    unsigned long err;
    while ((err = ERR_get_error ()) != 0) {
        logprintf (LOG_NOTQUIET, "OpenSSL: %s\n", ERR_error_string (err, NULL));
    }
}

/* Convert keyfile type as used by options.h to a type as accepted by
   SSL_CTX_use_certificate_file and SSL_CTX_use_PrivateKey_file.

   (options.h intentionally doesn't use values from openssl/ssl.h so
   it doesn't depend specifically on OpenSSL for SSL functionality.)  */
static int key_type_to_ssl_type (enum keyfile_type type)
{
    switch (type) {
    case keyfile_pem:
        return SSL_FILETYPE_PEM;
    case keyfile_asn1:
        return SSL_FILETYPE_ASN1;
    default:
        abort();
    }
}

/* SSL has been initialized */
static int ssl_true_initialized = 0;

/* Create an SSL Context and set default paths etc.  Called the first
   time an HTTP download is attempted.

   Returns true on success, false otherwise.

   On Ubuntu 14.04
   $ openssl 
   OpenSSL> version
   OpenSSL 1.0.1f 6 Jan 2014
  */
bool ssl_init (void)
{
    SSL_METHOD const *meth;
    long ssl_options = 0;
    char *ciphers_string = NULL;
    log_info("OPENSSL_VERSION_NUMBER: 0x%x", OPENSSL_VERSION_NUMBER); // 0x1000106f
#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    int ssl_proto_version = 0;
#endif

	/* OPENSSL_API_COMPAT
	   Various functions get deprecated as other interfaces get added, but are still available
	   in a default build. The include files support setting the OPENSSL_API_COMPAT define that
	   will hide functions that are deprecated in the selected version. To select the 1.1.0
	   version use -DOPENSSL_API_COMPAT=0x10100000L.
	 */
#ifdef OPENSSL_API_COMPAT
	log_info("OPENSSL_API_COMPAT: 0x%x", OPENSSL_API_COMPAT);
#endif

#if OPENSSL_VERSION_NUMBER >= 0x00907000
    if (ssl_true_initialized == 0) {
#if OPENSSL_API_COMPAT < 0x10100000L
        OPENSSL_config(NULL);
#endif
        ssl_true_initialized = 1;
    }
#endif

    if (ssl_ctx)
        /* The SSL has already been initialized. */
        return true;

    /* Init the PRNG.  If that fails, bail out.  */
    init_prng();
	/* RAND_add, RAND_seed, RAND_status, RAND_event, RAND_screen - add entropy to the PRNG
	   RAND_status() returns 1 if the PRNG has been seeded with enough data, 0 otherwise.
	 */
    if (RAND_status() != 1) {
        log_error("Could not seed PRNG; consider using --random-file.\n");
        goto error;
    }

#if OPENSSL_VERSION_NUMBER >= 0x00907000
	/* void OPENSSL_load_builtin_modules(void);
	   OPENSSL_load_builtin_modules() adds all the standard OpenSSL configuration modules to
	   the internal list. They can then be used by the OpenSSL configuration code.

	   NOTES
	   If the simple configuration function OPENSSL_config() is called then
	   OPENSSL_load_builtin_modules() is called automatically.

	   Applications which use the configuration functions directly will need to call
	   OPENSSL_load_builtin_modules() themselves before any other configuration code.

	   Applications should call OPENSSL_load_builtin_modules() to load all configuration modules
	   instead of adding modules selectively: otherwise functionality may be missing from the
	   application if an when new modules are added.
	*/
    OPENSSL_load_builtin_modules();

	/* NOTES
	   OPENSSL_config calls ENGINE_load_builtin_engines, so there is no need to call
	   ENGINE_load_builtin_engines if you call OPENSSL_config(indeed, doing so seems to result in a memory leak).
	  */
    ENGINE_load_builtin_engines();

	/* int CONF_modules_load_file(const char *filename, const char *appname, unsigned long flags);
	   int CONF_modules_load(const CONF *cnf, const char *appname, unsigned long flags);

	   The function CONF_modules_load_file() configures OpenSSL using file @filename
	   and application name @appname. If @filename is NULL the standard OpenSSL configuration file is used.
	   If @appname is NULL the standard OpenSSL application name openssl_conf is used.
	   The behaviour can be cutomized using flags.

	   CONF_modules_load() is idential to CONF_modules_load_file() except it read configuration information from @cnf.

	   These functions return 1 for success and a zero or negative value for failure.
	  */
    CONF_modules_load_file(NULL, NULL, CONF_MFLAGS_DEFAULT_SECTION | CONF_MFLAGS_IGNORE_MISSING_FILE);
#endif

#if OPENSSL_API_COMPAT >= 0x10100000L
	/* int OPENSSL_init_ssl(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings);
	   DESCRIPTION
	   During normal operation OpenSSL (libssl and libcrypto) will allocate various resources at start up that must,
	   subsequently, be freed on close down of the library. Additionally some resources are allocated on a per thread
	   basis (if the application is multi-threaded), and these resources must be freed prior to the thread closing.

	   As of version 1.1.0 OpenSSL will automatically allocate all resources that it needs so no explicit initialisation
	   is required. Similarly it will also automatically deinitialise as required.

	   However, there may be situations when explicit initialisation is desirable or needed, for example when some
	   non-default initialisation is required. The function OPENSSL_init_ssl() can be used for this purpose.
	   Calling this function will explicitly initialise BOTH libcrypto and libssl.
	   To explicitly initialise ONLY libcrypto see the OPENSSL_init_crypto function.

	   Numerous internal OpenSSL functions call OPENSSL_init_ssl(). Therefore, in order to perform
	   non-default initialisation, OPENSSL_init_ssl() MUST be called by application code prior to any other OpenSSL function calls.

	   RETURN VALUES
	   The function OPENSSL_init_ssl() returns 1 on success or 0 on error.

	   HISTORY
	   The OPENSSL_init_ssl() function was added in OpenSSL 1.1.0.
	 */
    OPENSSL_init_ssl(0, NULL);
#else
	/* int SSL_library_init(void);
	   SSL_library_init() registers the available SSL/TLS ciphers and digests.

	   SSL_library_init() must be called before any other action takes place.
	   SSL_library_init() is not reentrant.

	   SSL_library_init() always returns "1", so it is safe to discard the return value.
	 */
    SSL_library_init();

	/* void SSL_load_error_strings(void);
	   
	  void ERR_load_crypto_strings(void);
	  void ERR_free_strings(void);

	  ERR_load_crypto_strings() registers the error strings for all libcrypto functions.
	  SSL_load_error_strings() does the same, but also registers the libssl error strings.
	  One of these functions should be called before generating textual error messages.
	  However, this is not required when memory usage is an issue.

	   ERR_free_strings() frees all previously loaded error strings.
	 */
    SSL_load_error_strings();
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	/* #define SSLeay_add_all_algorithms() OpenSSL_add_all_algorithms()
	   OpenSSL_add_all_algorithms() adds all algorithms to the table (digests and ciphers).
	 */
    SSLeay_add_all_algorithms();
	/* #define SSLeay_add_ssl_algorithms()     SSL_library_init() */
    SSLeay_add_ssl_algorithms();
#endif

	log_info("secure_protocol: %d", opt.secure_protocol); // 0x1000106f
    switch (opt.secure_protocol) {
#if !defined OPENSSL_NO_SSL2 && OPENSSL_VERSION_NUMBER < 0x10100000L
    case secure_protocol_sslv2:
        meth = SSLv2_client_method();
        break;
#endif

#ifndef OPENSSL_NO_SSL3_METHOD
    case secure_protocol_sslv3:
        meth = SSLv3_client_method();
        break;
#endif

    case secure_protocol_auto:
    case secure_protocol_pfs:
        meth = SSLv23_client_method();
        ssl_options |= SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
        break;
    case secure_protocol_tlsv1:
#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        meth = TLS_client_method();
        ssl_proto_version = TLS1_VERSION;
#else
        meth = TLSv1_client_method();
#endif
    break;

#if OPENSSL_VERSION_NUMBER >= 0x10001000
    case secure_protocol_tlsv1_1:
#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        meth = TLS_client_method();
        ssl_proto_version = TLS1_1_VERSION;
#else
        meth = TLSv1_1_client_method();
#endif
        break;

    case secure_protocol_tlsv1_2:
#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        meth = TLS_client_method();
        ssl_proto_version = TLS1_2_VERSION;
#else
        meth = TLSv1_2_client_method();
#endif
        break;

    case secure_protocol_tlsv1_3:
#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L) && defined TLS1_3_VERSION
        meth = TLS_client_method();
        ssl_proto_version = TLS1_3_VERSION;
#else
        log_error("Your OpenSSL version is too old to support TLS 1.3");
        goto error;
#endif
        break;
#else
    case secure_protocol_tlsv1_1:
        log_error("Your OpenSSL version is too old to support TLSv1.1");
        goto error;

    case secure_protocol_tlsv1_2:
        log_error("Your OpenSSL version is too old to support TLSv1.2");
        goto error;
#endif

    default:
        log_error("OpenSSL: unimplemented 'secure-protocol' option value %d", opt.secure_protocol);
        log_error("Please report this issue to bug-wget@gnu.org\n");
        abort();
    }

    /* The type cast below accommodates older OpenSSL versions (0.9.8)
       where SSL_CTX_new() is declared without a "const" argument. */
    ssl_ctx = SSL_CTX_new((SSL_METHOD *) meth);
    if (!ssl_ctx)
        goto error;

    if (ssl_options)
		/* long SSL_CTX_set_options(SSL_CTX *ctx, long options);
		   SSL_CTX_set_options() adds the options set via bitmask in options to ctx.
		   Options already set before are not cleared!
		 */
        SSL_CTX_set_options(ssl_ctx, ssl_options);

#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
	log_info("ssl_proto_version: %d", ssl_proto_version);
    if (ssl_proto_version)
		/** int SSL_CTX_set_min_proto_version(SSL_CTX *ctx, int version);
			int SSL_CTX_set_max_proto_version(SSL_CTX *ctx, int version);
			int SSL_CTX_get_min_proto_version(SSL_CTX *ctx);
			int SSL_CTX_get_max_proto_version(SSL_CTX *ctx);

			int SSL_set_min_proto_version(SSL *ssl, int version);
			int SSL_set_max_proto_version(SSL *ssl, int version);
			int SSL_get_min_proto_version(SSL *ssl);
			int SSL_get_max_proto_version(SSL *ssl);

			The functions get or set the minimum and maximum supported protocol versions for the ctx or ssl.
			This works in combination with the options set via SSL_CTX_set_options(3) that also make it
			possible to disable specific protocol versions. Use these functions instead of disabling specific
			protocol versions.

			Setting the minimum or maximum version to 0, will enable protocol versions down to the lowest version,
			or up to the highest version supported by the library, respectively.

			Getters return 0 in case ctx or ssl have been configured to automatically use the lowest or highest
			version supported by the library.

			Currently supported versions are:
				SSL3_VERSION,
				TLS1_VERSION,
				TLS1_1_VERSION,
				TLS1_2_VERSION,
				TLS1_3_VERSION for TLS and DTLS1_VERSION, DTLS1_2_VERSION for DTLS.

			Return Values
			These setter functions return 1 on success and 0 on failure.
			The getter functions return the configured version or 0 for auto-configuration of lowest or highest protocol, respectively.
		 */
        SSL_CTX_set_min_proto_version(ssl_ctx, ssl_proto_version);
#endif

    /* OpenSSL ciphers: https://www.openssl.org/docs/apps/ciphers.html
     *
     * Rules:
     *  1. --ciphers overrides everything
     *  2. We allow RSA key exchange by default (secure_protocol_auto)
     *  3. We disallow RSA key exchange if PFS was requested (secure_protocol_pfs)
     */
    if (!opt.tls_ciphers_string) {
        if (opt.secure_protocol == secure_protocol_auto)
            ciphers_string = "HIGH:!aNULL:!RC4:!MD5:!SRP:!PSK";
        else if (opt.secure_protocol == secure_protocol_pfs)
            ciphers_string = "HIGH:!aNULL:!RC4:!MD5:!SRP:!PSK:!kRSA";
    } else {
        ciphers_string = opt.tls_ciphers_string;
    }

	log_info("ciphers_string: %s", ciphers_string);
	/* int SSL_CTX_set_cipher_list(SSL_CTX *ctx, const char *control);
	   int SSL_set_cipher_list(SSL *ssl, const char *control);

	   SSL_CTX_set_cipher_list() sets the list of available cipher suites for @ctx using the @control string.
	   The list of cipher suites is inherited by all ssl objects created from ctx.
	   SSL_set_cipher_list() sets the list of cipher suites only for ssl.

	   The control string consists of one or more control words separated by colon characters (‘:’).
	   Space (‘ ’), semicolon (‘;’), and comma (‘,’) characters can also be used as separators.
	   Each control words selects a set of cipher suites and can take one of the following optional prefix characters:
	   No prefix:
	   Those of the selected cipher suites that have not been made available yet are added to the end of
	   the list of available cipher suites, preserving their order.

	   Prefixed minus sign (‘-’):
	   Those of the selected cipher suites that have been made available earlier are moved back from the list of
	   available cipher suites to the beginning of the list of unavailable cipher suites, also preserving their order.

	   Prefixed plus sign (‘+’):
	   Those of the selected cipher suites have been made available earlier are moved to end of the list of available
	   cipher suites, reducing their priority, but preserving the order among themselves.

	   Prefixed exclamation mark (‘!’):
	   The selected cipher suites are permanently deleted, no matter whether they had earlier been made available or not,
	   and can no longer be added or re-added by later words.

	   The following special words can only be used without a prefix:
	   DEFAULT
	   An alias for ALL:! aNULL:!eNULL. It can only be used as the first word.
	*/
    if (ciphers_string && !SSL_CTX_set_cipher_list(ssl_ctx, ciphers_string)) {
        log_error("OpenSSL: Invalid cipher list: %s\n", ciphers_string);
        goto error;
    }

	/** int SSL_CTX_set_default_verify_paths(SSL_CTX *ctx);
		SSL_CTX_set_default_verify_paths() specifies that the default locations from which CA certificates are loaded
		should be used. There is one default directory and one default file.
		The default CA certificates directory is called "certs" in the default OpenSSL directory.
		Alternatively the SSL_CERT_DIR environment variable can be defined to override this location.
		The default CA certificates file is called "cert.pem" in the default OpenSSL directory.
		Alternatively the SSL_CERT_FILE environment variable can be defined to override this location.
	 */
    SSL_CTX_set_default_verify_paths(ssl_ctx);

	/** int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile, const char *CApath);
		SSL_CTX_load_verify_locations() specifies the locations for @ctx, at which CA certificates
		for verification purposes are located. The certificates available via @CAfile and @CApath are trusted.

		int SSL_CTX_set_default_verify_dir(SSL_CTX *ctx);
		is similar to SSL_CTX_set_default_verify_paths() except that just the default directory is used.

		int SSL_CTX_set_default_verify_file(SSL_CTX *ctx);
		is similar to SSL_CTX_set_default_verify_paths() except that just the default file is used.
	 */
    SSL_CTX_load_verify_locations(ssl_ctx, opt.ca_cert, opt.ca_directory);

	log_info("crl_file: %s", opt.crl_file);
    if (opt.crl_file) {
        X509_STORE *store = SSL_CTX_get_cert_store(ssl_ctx);
        X509_LOOKUP *lookup;

        if (!(lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file()))
                || (!X509_load_crl_file(lookup, opt.crl_file, X509_FILETYPE_PEM)))
            goto error;

        X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    }

    /* SSL_VERIFY_NONE instructs OpenSSL not to abort SSL_connect if the
       certificate is invalid.  We verify the certificate separately in
       ssl_check_certificate, which provides much better diagnostics
       than examining the error stack after a failed SSL_connect.  */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);

	log_info("cert_file: %s", opt.cert_file);
	log_info("cert_type: %d", opt.cert_type);
	log_info("private_key: %s", opt.private_key);
    /* Use the private key from the cert file unless otherwise specified. */
    if (opt.cert_file && !opt.private_key) {
        opt.private_key = xstrdup(opt.cert_file);
        opt.private_key_type = opt.cert_type;
    }

    /* Use cert from private key file unless otherwise specified. */
    if (opt.private_key && !opt.cert_file) {
        opt.cert_file = xstrdup(opt.private_key);
        opt.cert_type = opt.private_key_type;
    }

    if (opt.cert_file)
        if (SSL_CTX_use_certificate_file(ssl_ctx, opt.cert_file,
                key_type_to_ssl_type(opt.cert_type)) != 1)
            goto error;

    if (opt.private_key)
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, opt.private_key,
                key_type_to_ssl_type(opt.private_key_type)) != 1)
            goto error;

    /* Since fd_write unconditionally assumes partial writes (and
       handles them correctly), allow them in OpenSSL.  */
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

    /* The OpenSSL library can handle renegotiations automatically, so
       tell it to do so.  */
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);

    return true;

error:
    if (ssl_ctx)
        SSL_CTX_free (ssl_ctx);
    print_errors();
    return false;
}

struct openssl_transport_context {
    SSL *conn;                    /* SSL connection handle */
    SSL_SESSION *sess;            /* SSL session info */
    char *last_error;             /* last error printed with openssl_errstr */
};

struct openssl_read_args {
    int fd;
    struct openssl_transport_context *ctx;
    char *buf;
    int bufsize;
    int retval;
};

static int openssl_read(int fd, char *buf, int bufsize, void *arg)
{
    struct openssl_transport_context *ctx = (struct openssl_transport_context*) arg;
	SSL *conn = ctx->conn;
	int ret;

	do {
		/** int SSL_read_ex(SSL *ssl, void *buf, size_t num, size_t *readbytes);
			int SSL_read(SSL *ssl, void *buf, int num);

			int SSL_peek_ex(SSL *ssl, void *buf, size_t num, size_t *readbytes);
			int SSL_peek(SSL *ssl, void *buf, int num);

			SSL_read_ex() and SSL_read() try to read @num bytes from the specified @ssl into the buffer @buf.
			On success SSL_read_ex() will store the number of bytes actually read in *@readbytes.

			SSL_peek_ex() and SSL_peek() are identical to SSL_read_ex() and SSL_read() respectively except
			no bytes are actually removed from the underlying BIO during the read, so that a subsequent
			call to SSL_read_ex() or SSL_read() will yield at least the same bytes.

			SSL_read_ex() and SSL_peek_ex() were added in OpenSSL 1.1.1.

			RETURN VALUES
			SSL_read_ex() and SSL_peek_ex() will return 1 for success or 0 for failure.
			Success means that 1 or more application data bytes have been read from the SSL connection.
			Failure means that no bytes could be read from the SSL connection.
			Failures can be retryable (e.g. we are waiting for more bytes to be delivered by the network) or
			non-retryable (e.g. a fatal network error).
			In the event of a failure call SSL_get_error(3) to find out the reason which indicates whether
			the call is retryable or not.

			For SSL_read() and SSL_peek() the following return values can occur:
			> 0
				The read operation was successful. The return value is the number of bytes actually read from
				the TLS/SSL connection.

			<= 0
				The read operation was not successful, because either the connection was closed, an error occurred
				or action must be taken by the calling process. Call SSL_get_error(3) with the return value ret to
				find out the reason.

				Old documentation indicated a difference between 0 and -1, and that -1 was retryable.
				You should instead call SSL_get_error() to find out if it's retryable.
		 */
		ret = SSL_read(conn, buf, bufsize);
		/** int SSL_get_error(const SSL *ssl, int ret);
			SSL_get_error() returns a result code (suitable for the C "switch" statement) for a preceding call to
			SSL_connect(), SSL_accept(), SSL_do_handshake(), SSL_read_ex(), SSL_read(), SSL_peek_ex(), SSL_peek(),
			SSL_write_ex() or SSL_write() on ssl. The value returned by that TLS/SSL I/O function must be passed
			to SSL_get_error() in parameter ret.

			SSL_ERROR_SYSCALL
			An I/O error occurred. Issue the sock_errno function to determine the cause of the error.
		 */
	} while (ret == -1 && SSL_get_error(conn, ret) == SSL_ERROR_SYSCALL && errno == EINTR);

    if (ret < 0) {
        return -1;
    }

    return ret;
}

static int openssl_write(int fd, char *buf, int bufsize, void *arg)
{
    int ret = 0;
    struct openssl_transport_context *ctx = (struct openssl_transport_context *) arg;
    SSL *conn = ctx->conn;

    do {
		/** int SSL_write(SSL *ssl, const void *buf, int num);
			SSL_write() writes @num bytes from the buffer @buf into the specified @ssl connection.
			When an SSL_write() operation has to be repeated because of SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE,
			it must be repeated with the same arguments.

			When calling SSL_write() with num=0 bytes to be sent the behaviour is undefined.

			The following return values can occur:
			> 0
				The write operation was successful, the return value is the number of bytes actually written
				to the TLS/SSL connection.

			<= 0
				The write operation was not successful, because either the connection was closed, an error occurred or
				action must be taken by the calling process. Call SSL_get_error() with the return value ret to find out
				the reason.

				SSLv2 (deprecated) does not support a shutdown alert protocol, so it can only be detected, whether the
				underlying connection was closed. It cannot be checked, why the closure happened.

				Old documentation indicated a difference between 0 and -1, and that -1 was retryable.
				You should instead call SSL_get_error() to find out if it's retryable.
		 */
        ret = SSL_write(conn, buf, bufsize);
    } while (ret == -1 && SSL_get_error(conn, ret) == SSL_ERROR_SYSCALL && errno == EINTR);

    return ret;
}

static int openssl_poll(int fd, double timeout, int wait_for, void *arg)
{
    struct openssl_transport_context *ctx = (struct openssl_transport_context *) arg;
    SSL *conn = ctx->conn;
	/** int SSL_pending(const SSL *ssl);
		int SSL_has_pending(const SSL *s); // added in OpenSSL 1.1.0.

		Data is received in whole blocks known as records from the peer.
		A whole record is processed (e.g. decrypted) in one go and is buffered by OpenSSL until it is read by
		the application via a call to SSL_read_ex(3) or SSL_read(3).

		RETURN VALUES
		SSL_pending() returns the number of buffered and processed application data bytes that are pending
		and are available for immediate read. SSL_has_pending() returns 1 if there is buffered record data
		in the SSL object and 0 otherwise.
 	 */
    if (SSL_pending(conn))
        return 1;
    if (timeout == 0)
        return 1;

    return select_fd(fd, timeout, wait_for);
}

static int openssl_peek(int fd, char *buf, int bufsize, void *arg)
{
    int ret;
    struct openssl_transport_context *ctx = (struct openssl_transport_context *) arg;
    SSL *conn = ctx->conn;
    if (!openssl_poll(fd, 0.0, WAIT_FOR_READ, arg))
        return 0;

    do {
        ret = SSL_peek(conn, buf, bufsize);
    } while (ret == -1 && SSL_get_error(conn, ret) == SSL_ERROR_SYSCALL && errno == EINTR);

    return ret;
}

static const char *openssl_errstr(int fd, void *arg)
{
    struct openssl_transport_context *ctx = (struct openssl_transport_context *) arg;
    unsigned long errcode;
    char *errmsg = NULL;
    int msglen = 0;

    /* If there are no SSL-specific errors, just return NULL. */
    if ((errcode = ERR_get_error()) == 0)
        return NULL;

    /* Get rid of previous contents of ctx->last_error, if any.  */
    xfree(ctx->last_error);

    /** Iterate over OpenSSL's error stack and accumulate errors in the
        last_error buffer, separated by "; ".  This is better than using
        a static buffer, which *always* takes up space (and has to be
        large, to fit more than one error message), whereas these
        allocations are only performed when there is an actual error.
     */
    for (; ;) {
		/** char *ERR_error_string(unsigned long e, char *buf);
			ERR_error_string() generates a human-readable string representing the error code @e,
			and places it at @buf. @buf must be at least 256 bytes long. If @buf is NULL,
			the error string is placed in a static buffer.
			Note that this function is not thread-safe and does no checks on the size of the buffer.

			The string will have the following format:
			error:[error code]:[library name]:[function name]:[reason string]
			error code is an 8 digit hexadecimal number.
			library name, function name and reason string are ASCII text.
		 */
        const char *str = ERR_error_string(errcode, NULL);
        int len = strlen(str);

        /* Allocate space for the existing message, plus two more chars
           for the "; " separator and one for the terminating \0.  */
        errmsg = xrealloc(errmsg, msglen + len + 2 + 1);
        memcpy(errmsg + msglen, str, len);
        msglen += len;

        /* Get next error and bail out if there are no more. */
        errcode = ERR_get_error();
        if (errcode == 0)
            break;

        errmsg[msglen++] = ';';
        errmsg[msglen++] = ' ';
    }
    errmsg[msglen] = '\0';

    /* Store the error in ctx->last_error where openssl_close will
       eventually find it and free it.  */
    ctx->last_error = errmsg;

    return errmsg;
}

static void openssl_close (int fd, void *arg)
{
    struct openssl_transport_context *ctx = arg;
    SSL *conn = ctx->conn;

	/** int SSL_shutdown(SSL *ssl);
		SSL_shutdown() shuts down an active TLS/SSL connection. It sends the close_notify shutdown alert to the peer.

		RETURN VALUES
		0
			The shutdown is not yet finished: the close_notify was sent but the peer did not send it back yet.
			Call SSL_read() to do a bidirectional shutdown. The output of SSL_get_error(3) may be misleading,
			as an erroneous SSL_ERROR_SYSCALL may be flagged even though no error occurred.

		1
			The shutdown was successfully completed. The close_notify alert was sent and the peer's close_notify
			alert was received.

		<0
			The shutdown was not successful. Call SSL_get_error(3) with the return value ret to find out the reason.
			It can occur if an action is needed to continue the operation for non-blocking BIOs.
			It can also occur when not all data was read using SSL_read().
	 */
    SSL_shutdown(conn);

	/** void SSL_free(SSL *ssl);
		SSL_free() decrements the reference count of ssl, and removes the SSL structure pointed to by ssl
		and frees up the allocated memory if the reference count has reached 0.
		If ssl is NULL nothing is done.
	 */
    SSL_free(conn);
    xfree(ctx->last_error);
    xfree(ctx);

    close(fd);
    log_info("Closed %d/SSL 0x%0*lx\n", fd, PTR_FORMAT(conn));
}

/* openssl_transport is the singleton that describes the SSL transport
   methods provided by this file.  */
static struct transport_implementation openssl_transport = {
    .reader = openssl_read,
    .writer = openssl_write,
    .poller = openssl_poll,
    .peeker = openssl_peek,
    .errstr = openssl_errstr,
    .closer = openssl_close
};

struct scwt_context {
    SSL *ssl;
    int result;
};

static void ssl_connect_with_timeout_callback(void *arg)
{
    struct scwt_context *ctx = (struct scwt_context *)arg;
	/** int SSL_connect(SSL *ssl);
		SSL_connect() initiates the TLS/SSL handshake with a server.
		The communication channel must already have been set and assigned to the ssl by setting an underlying BIO.

		The behaviour of SSL_connect() depends on the underlying BIO:
		1) If the underlying BIO is blocking, SSL_connect() will only return once the handshake has been finished or an error occurred.
		2) If the underlying BIO is non-blocking, SSL_connect() will also return when the underlying BIO
		   could not satisfy the needs of SSL_connect() to continue the handshake, indicating the problem
		   by the return value -1. In this case a call to SSL_get_error() with the return value of SSL_connect()
		   will yield SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE . The calling process then must repeat the call
		   after taking appropriate action to satisfy the needs of SSL_connect(). The action depends on the underlying
		   BIO . When using a non-blocking socket, nothing is to be done, but select() can be used to check for the
		   required condition. When using a buffering BIO , like a BIO pair, data must be written into or retrieved
		   out of the BIO before being able to continue.

		RETURN VALUES
		0
			The TLS/SSL handshake was successfully completed, a TLS/SSL connection has been established.

		1
			The TLS/SSL handshake was not successful but was shut down controlled and by the specifications of the TLS/SSL protocol.
			Call SSL_get_error() with the return value ret to find out the reason.

		<0
			The TLS/SSL handshake was not successful, because a fatal error occurred either at the protocol level or
			a connection failure occurred. The shutdown was not clean. It can also occur of action is need to continue
			the operation for non-blocking BIOs. Call SSL_get_error() with the return value ret to find out the reason.
	 */
    ctx->result = SSL_connect(ctx->ssl);
}

/**
 * SNI: Server Name Indication,改善服务器与客户端 SSL (Secure Socket Layer)和 TLS (Transport Layer Security) 
 * 的一个扩展。主要解决一台服务器只能使用一个证书(一个域名)的缺点.
 * 随着服务器对虚拟主机的支持，一个服务器上可以为多个域名提供服务，因此SNI须得到支持才能满足需求。
 */
static const char *_sni_hostname(const char *hostname)
{
    size_t len = strlen(hostname);

    char *sni_hostname = xmemdup(hostname, len + 1);

    /* Remove trailing dot(s) to fix #47408.
     * Regarding RFC 6066 (SNI): The hostname is represented as a byte
     * string using ASCII encoding without a trailing dot. */
    while (len && sni_hostname[--len] == '.')
        sni_hostname[len] = 0;

    return sni_hostname;
}

/* Perform the SSL handshake on file descriptor FD, which is assumed
   to be connected to an SSL server.  The SSL handle provided by
   OpenSSL is registered with the file descriptor FD using
   fd_register_transport, so that subsequent calls to fd_read,
   fd_write, etc., will use the corresponding SSL functions.

 * Returns true on success, false on failure.  */
bool ssl_connect_wget(int fd, const char *hostname, int *continue_session)
{
    SSL *conn;
    struct scwt_context scwt_ctx;
    struct openssl_transport_context *ctx;

    log_debug("Initiating SSL handshake.\n");

    assert(ssl_ctx != NULL);
    conn = SSL_new(ssl_ctx);
    if (!conn)
        goto error;

#if OPENSSL_VERSION_NUMBER >= 0x0090806fL && !defined(OPENSSL_NO_TLSEXT)
    /* If the SSL library was built with support for ServerNameIndication
       then use it whenever we have a hostname.  If not, don't, ever. */
    if (!is_valid_ip_address(hostname)) {
        const char *sni_hostname = _sni_hostname(hostname);
        long rc = SSL_set_tlsext_host_name(conn, sni_hostname);
        xfree(sni_hostname);
        if (rc == 0) {
            log_error("Failed to set TLS server-name indication.");
            goto error;
        }
    }
#endif

    if (continue_session) {
        /* attempt to resume a previous SSL session */
        ctx = (struct openssl_transport_context *) fd_transport_context(*continue_session);
        if (!ctx || !ctx->sess || !SSL_set_session(conn, ctx->sess))
            goto error;
    }

	/** int SSL_set_fd(SSL *ssl, int fd);
		int SSL_set_rfd(SSL *ssl, int fd);
		int SSL_set_wfd(SSL *ssl, int fd);
		SSL_set_fd() sets the file descriptor @fd as the input/output facility for the TLS/SSL (encrypted) side of @ssl.
		@fd will typically be the socket file descriptor of a network connection.

		RETURN VALUES
		0
			The operation failed. Check the error stack to find out why.

		1
			The operation succeeded.
	 */
    if (!SSL_set_fd(conn, fd))
        goto error;

	/** void SSL_set_connect_state(SSL *ssl);
		void SSL_set_accept_state(SSL *ssl);

		SSL_set_connect_state() sets ssl to work in client mode.
		SSL_set_accept_state() sets ssl to work in server mode.
	 */
    SSL_set_connect_state(conn);

    /* Re-seed the PRNG before the SSL handshake */
    init_prng();
	/* RAND_status() return 1 if the PRNG has been seeded with enough data, 0 otherwise. */
    if (RAND_status() != 1) {
        log_error("WARNING: Could not seed PRNG. Consider using --random-file.\n");
        goto error;
    }

    scwt_ctx.ssl = conn;
	ssl_connect_with_timeout_callback(&scwt_ctx);
    if (scwt_ctx.result < 0) {
        log_error("SSL handshake timed out.\n");
        goto timeout;
    }

	/** int SSL_in_init(const SSL *s);
		int SSL_in_before(const SSL *s);
		int SSL_is_init_finished(const SSL *s);

		int SSL_in_connect_init(SSL *s);
		int SSL_in_accept_init(SSL *s);

		OSSL_HANDSHAKE_STATE SSL_get_state(const SSL *ssl);

		SSL_in_init() returns 1 if the SSL/TLS state machine is currently processing or awaiting handshake messages, or 0 otherwise.
		SSL_in_before() returns 1 if no SSL/TLS handshake has yet been initiated, or 0 otherwise.
		SSL_is_init_finished() returns 1 if the SSL/TLS connection is in a state where fully protected
		application data can be transferred or 0 otherwise.
		Note that in some circumstances (such as when early data is being transferred) SSL_in_init(), SSL_in_before() and SSL_is_init_finished() can all return 0.

		SSL_in_connect_init() returns 1 if s is acting as a client and SSL_in_init() would return 1, or 0 otherwise.
		SSL_in_accept_init() returns 1 if s is acting as a server and SSL_in_init() would return 1, or 0 otherwise.
		SSL_in_connect_init() and SSL_in_accept_init() are implemented as macros.

		SSL_get_state() returns a value indicating the current state of the handshake state machine.
 	 */
    if (scwt_ctx.result <= 0 || !SSL_is_init_finished(conn))
        goto error;

	/** SSL_SESSION *SSL_get_session(const SSL *ssl);
		SSL_SESSION *SSL_get0_session(const SSL *ssl);
		SSL_SESSION *SSL_get1_session(SSL *ssl);

		Retrieve TLS/SSL session data.
		SSL_get_session() returns a pointer to the SSL_SESSION actually used in @ssl.
		The reference count of the SSL_SESSION is not incremented, so that the pointer can become invalid
		by other operations.

		SSL_get0_session() is the same as SSL_get_session().

		SSL_get1_session() is the same as SSL_get_session(), but the reference count of the SSL_SESSION is incremented by one.
		The ssl session contains all information required to re-establish the connection without a new handshake.
	 */
    ctx = xnew0(struct openssl_transport_context);
    ctx->conn = conn;
    ctx->sess = SSL_get0_session (conn);
    if (!ctx->sess)
        log_error("WARNING: Could not save SSL session data for socket %d\n", fd);

    /* Register FD with Wget's transport layer, i.e. arrange that our
       functions are used for reading, writing, and polling.  */
    fd_register_transport(fd, &openssl_transport, ctx);
    log_debug("Handshake successful; connected socket %d to SSL handle 0x%0*lx\n", fd, PTR_FORMAT (conn));
    return true;

error:
    log_error("SSL handshake failed.\n");
    print_errors();
timeout:
    if (conn)
        SSL_free(conn);
    return false;
}

#define ASTERISK_EXCLUDES_DOT   /* mandated by rfc2818 */

/* Return true is STRING (case-insensitively) matches PATTERN, false
   otherwise.  The recognized wildcard character is "*", which matches
   any character in STRING except ".".  Any number of the "*" wildcard
   may be present in the pattern.

   This is used to match of hosts as indicated in rfc2818: "Names may
   contain the wildcard character * which is considered to match any
   single domain name component or component fragment. E.g., *.a.com
   matches foo.a.com but not bar.foo.a.com. f*.com matches foo.com but
   not bar.com [or foo.bar.com]."

   If the pattern contain no wildcards, pattern_match(a, b) is
   equivalent to !strcasecmp(a, b).  */
static bool pattern_match(const char *pattern, const char *string)
{
    const char *p = pattern, *n = string;
    char c;
    for (; (c = tolower(*p++)) != '\0'; n++)
    if (c == '*') {
        for (c = tolower(*p); c == '*'; c = tolower(*++p))
            ;
        for (; *n != '\0'; n++)
            if (tolower(*n) == c && pattern_match(p, n))
                return true;
#ifdef ASTERISK_EXCLUDES_DOT
            else if (*n == '.')
                return false;
#endif
        return c == '\0';
    } else {
        if (c != tolower (*n))
        return false;
    }

    return *n == '\0';
}

static char *_get_rfc2253_formatted(X509_NAME *name)
{
    int len;
    char *out = NULL;
    BIO* b;

    if ((b = BIO_new(BIO_s_mem()))) {
        if (X509_NAME_print_ex(b, name, 0, XN_FLAG_RFC2253) >= 0 && (len = BIO_number_written(b)) > 0) {
            out = xmalloc (len + 1);
            BIO_read (b, out, len);
            out[len] = 0;
        }
        BIO_free (b);
    }

    return out ? out : xstrdup("");
}

/*
 * Heavily modified from:
 * https://www.owasp.org/index.php/Certificate_and_Public_Key_Pinning#OpenSSL
 */
static bool pkp_pin_peer_pubkey(X509 *cert, const char *pinnedpubkey)
{
    /* Scratch */
    int len1 = 0, len2 = 0;
    char *buff1 = NULL, *temp = NULL;

    /* Result is returned to caller */
    bool result = false;

    /* if a path wasn't specified, don't pin */
    if (!pinnedpubkey)
        return true;

    if (!cert)
        return result;

    /* Begin Gyrations to get the subjectPublicKeyInfo */
    /* Thanks to Viktor Dukhovni on the OpenSSL mailing list */
    /* https://groups.google.com/group/mailing.openssl.users/browse_thread/thread/d61858dae102c6c7 */
    len1 = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), NULL);
    if (len1 < 1)
        goto cleanup; /* failed */

    /* https://www.openssl.org/docs/crypto/buffer.html */
    buff1 = temp = OPENSSL_malloc(len1);
    if (!buff1)
        goto cleanup; /* failed */

    /* https://www.openssl.org/docs/crypto/d2i_X509.html */
    len2 = i2d_X509_PUBKEY (X509_get_X509_PUBKEY (cert), (unsigned char **) &temp);

    /*
     * These checks are verifying we got back the same values as when we
     * sized the buffer. It's pretty weak since they should always be the
     * same. But it gives us something to test.
     */
    if ((len1 != len2) || !temp || ((temp - buff1) != len1))
        goto cleanup; /* failed */

    /* End Gyrations */

    /* The one good exit point */
    result = wg_pin_peer_pubkey(pinnedpubkey, buff1, len1);

cleanup:
    /* https://www.openssl.org/docs/crypto/buffer.html */
    if (NULL != buff1)
        OPENSSL_free(buff1);

    return result;
}

/* Verify the validity of the certificate presented by the server.
   Also check that the "common name" of the server, as presented by
   its certificate, corresponds to HOST.  (HOST typically comes from
   the URL and is what the user thinks he's connecting to.)

   This assumes that ssl_connect_wget has successfully finished, i.e. that
   the SSL handshake has been performed and that FD is connected to an
   SSL handle.

   If opt.check_cert is true (the default), this returns 1 if the
   certificate is valid, 0 otherwise.  If opt.check_cert is 0, the
   function always returns 1, but should still be called because it
   warns the user about any problems with the certificate. */
bool ssl_check_certificate(int fd, const char *host)
{
    X509 *cert;
    GENERAL_NAMES *subjectAltNames;
    char common_name[256];
    long vresult;
    bool success = true;
    bool alt_name_checked = false;
    bool pinsuccess = opt.pinnedpubkey == NULL;

    /* If the user has specified --no-check-cert, we still want to warn
       him about problems with the server's certificate.  */
    const char *severity = opt.check_cert ? "ERROR" : "WARNING";

    struct openssl_transport_context *ctx = fd_transport_context(fd);
    SSL *conn = ctx->conn;
    assert(conn != NULL);

    /* The user explicitly said to not check for the certificate.  */
    if (opt.check_cert == CHECK_CERT_QUIET && pinsuccess)
        return success;

    cert = SSL_get_peer_certificate(conn);
    if (!cert) {
        log_warn("%s: No certificate presented by %s.", severity, host);
        success = false;
        goto no_cert; /* must bail out since CERT is NULL */
    }

    if (1) { //IF_DEBUG
        char *subject = _get_rfc2253_formatted(X509_get_subject_name(cert));
        char *issuer = _get_rfc2253_formatted(X509_get_issuer_name(cert));
        log_info("certificate:\n  subject: %s\n  issuer:  %s", subject, issuer);
        xfree(subject);
        xfree(issuer);
    }

    vresult = SSL_get_verify_result(conn);
    if (vresult != X509_V_OK) {
        char *issuer = _get_rfc2253_formatted(X509_get_issuer_name (cert));
        log_warn("%s: cannot verify %s's certificate, issued by %s:", severity, host, issuer);
        xfree(issuer);

        /* Try to print more user-friendly (and translated) messages for
        the frequent verification errors.  */
        switch (vresult) {
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
            log_warn("  Unable to locally verify the issuer's authority.");
            break;
        case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
            log_warn("  Self-signed certificate encountered.");
            break;
        case X509_V_ERR_CERT_NOT_YET_VALID:
            log_warn("  Issued certificate not yet valid.");
            break;
        case X509_V_ERR_CERT_HAS_EXPIRED:
            log_warn("  Issued certificate has expired.");
            break;
        default:
            /* For the less frequent error strings, simply provide the
               OpenSSL error message.  */
            log_warn("  %s", X509_verify_cert_error_string(vresult));
        }
        success = false;
        /* Fall through, so that the user is warned about *all* issues
           with the cert (important with --no-check-certificate.)  */
    }

    /* Check that HOST matches the common name in the certificate.
       The following remains to be done:

       - When matching against common names, it should loop over all
         common names and choose the most specific one, i.e. the last
         one, not the first one, which the current code picks.

       - Ensure that ASN1 strings from the certificate are encoded as
         UTF-8 which can be meaningfully compared to HOST.  */
    subjectAltNames = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
    if (subjectAltNames) {
        /* Test subject alternative names */

        /* SNI hostname must not have a trailing dot */
        const char *sni_hostname = _sni_hostname(host);

        /* Do we want to check for dNSNAmes or ipAddresses (see RFC 2818)?
        * Signal it by host_in_octet_string. */
        ASN1_OCTET_STRING *host_in_octet_string = a2i_IPADDRESS(sni_hostname);

        int numaltnames = sk_GENERAL_NAME_num(subjectAltNames);
        int i;
        for (i=0; i < numaltnames; i++) {
            const GENERAL_NAME *name = sk_GENERAL_NAME_value(subjectAltNames, i);
            if (name) {
                if (host_in_octet_string) {
                    if (name->type == GEN_IPADD) {
                        /* Check for ipAddress */
                        /* TODO: Should we convert between IPv4-mapped IPv6
                         * addresses and IPv4 addresses? */
                        alt_name_checked = true;
                        if (!ASN1_STRING_cmp(host_in_octet_string, name->d.iPAddress))
                            break;
                    }
                } else if (name->type == GEN_DNS)  {
                    /* dNSName should be IA5String (i.e. ASCII), however who
                     * does trust CA? Convert it into UTF-8 for sure. */
                    unsigned char *name_in_utf8 = NULL;

                    /* Check for dNSName */
                    alt_name_checked = true;

                    if (0 <= ASN1_STRING_to_UTF8(&name_in_utf8, name->d.dNSName)) {
                        /* Compare and check for NULL attack in ASN1_STRING */
                        if (pattern_match((char *)name_in_utf8, sni_hostname) &&
                                (strlen((char *)name_in_utf8) == (size_t) ASN1_STRING_length(name->d.dNSName))) {
                            OPENSSL_free (name_in_utf8);
                            break;
                        }

                        OPENSSL_free (name_in_utf8);
                    }
                }
            }
        }

        sk_GENERAL_NAME_pop_free(subjectAltNames, GENERAL_NAME_free);
        if (host_in_octet_string)
            ASN1_OCTET_STRING_free(host_in_octet_string);

        if (alt_name_checked == true && i >= numaltnames) {
            log_warn("%s: no certificate subject alternative name matches"
                    "trequested host name %s.",severity, sni_hostname);
            success = false;
        }

        xfree(sni_hostname);
    }

    if (alt_name_checked == false) {
        /* Test commomName */
        X509_NAME *xname = X509_get_subject_name(cert);
        common_name[0] = '\0';
        X509_NAME_get_text_by_NID(xname, NID_commonName, common_name, sizeof(common_name));

        if (!pattern_match(common_name, host)) {
            log_warn("%s: certificate common name %s doesn't match requested host name %s.", severity, common_name, host);
            success = false;
        } else {
            /* We now determine the length of the ASN1 string. If it
             * differs from common_name's length, then there is a \0
             * before the string terminates.  This can be an instance of a
             * null-prefix attack.
             *
             * https://www.blackhat.com/html/bh-usa-09/bh-usa-09-archives.html#Marlinspike
             * */
            int i = -1, j;
            X509_NAME_ENTRY *xentry;
            ASN1_STRING *sdata;

            if (xname) {
                for (;;) {
                    j = X509_NAME_get_index_by_NID(xname, NID_commonName, i);
                    if (j == -1) break;
                    i = j;
                }
            }

            xentry = X509_NAME_get_entry(xname,i);
            sdata = X509_NAME_ENTRY_get_data(xentry);
            if (strlen(common_name) != (size_t) ASN1_STRING_length(sdata)) {
                log_warn("%s: certificate common name is invalid (contains a NUL character).\n"
                        "This may be an indication that the host is not who it claims to be\n"
                        "(that is, it is not the real %s).", severity, (host));
                success = false;
            }
        }
    }

    pinsuccess = pkp_pin_peer_pubkey(cert, opt.pinnedpubkey);
    if (!pinsuccess) {
        log_warn("The public key does not match pinned public key!");
        success = false;
    }

    if (success)
        log_info("X509 certificate successfully verified and matches host %s", host);
    X509_free (cert);

no_cert:
    if (opt.check_cert == CHECK_CERT_ON && !success)
        log_warn("To connect to %s insecurely, use `--no-check-certificate'.", host);

    /* never return true if pinsuccess fails */
    return !pinsuccess ? false : (opt.check_cert == CHECK_CERT_ON ? success : true);
}

