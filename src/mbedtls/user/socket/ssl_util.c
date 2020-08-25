#include "../../../include/common_header.h"
#include "../../../include/log_ext.h"

#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "ssl_util.h"


#define FORCE_SSL_VERIFY

#define TLS_AUTH_MODE_CA        0
#define TLS_AUTH_MODE_PSK       1

/* config authentication mode */
#ifndef TLS_AUTH_MODE
#define TLS_AUTH_MODE           TLS_AUTH_MODE_CA
#endif


typedef struct _TLSDataParams {
    mbedtls_ssl_context ssl;          /**< mbed TLS control context. */
    mbedtls_net_context fd;           /**< mbed TLS network context. */
    mbedtls_ssl_config conf;          /**< mbed TLS configuration context. */
    mbedtls_x509_crt cacertl;         /**< mbed TLS CA certification. */
    mbedtls_x509_crt clicert;         /**< mbed TLS Client certification. */
    mbedtls_pk_context pkey;          /**< mbed TLS Client key. */
} TLSDataParams_t;


static void x509_crt_dump(const mbedtls_x509_crt *crt)
{
	char buf[512];
	log_info("x509_crt info:");
	printf("version: %d\n", crt->version);
	printf("valid from: %04d-%02d-%d %02d:%02d:%02d\n", crt->valid_from.year, crt->valid_from.mon, crt->valid_from.day,
				crt->valid_from.hour, crt->valid_from.min, crt->valid_from.sec);
	printf("valid to: %04d-%02d-%d %02d:%02d:%02d\n", crt->valid_to.year, crt->valid_to.mon, crt->valid_to.day,
				crt->valid_to.hour, crt->valid_to.min, crt->valid_to.sec);

	memset(buf, 0, sizeof(buf));
	mbedtls_x509_dn_gets(buf, sizeof(buf), &crt->issuer);
	printf("issuer: %s\n", buf);

	memset(buf, 0, sizeof(buf));
	mbedtls_x509_dn_gets(buf, sizeof(buf), &crt->subject);
	printf("subject: %s\n", buf);

	memset(buf, 0, sizeof(buf));
	mbedtls_x509_serial_gets(buf, sizeof(buf), &crt->serial);
}

static void _ssl_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    ((void) level);

    if (NULL != ctx) {
        printf("%s(%04d): %s", file, line, str);
    }
}

static int _ssl_random(void *p_rng, unsigned char *output, size_t output_len)
{
    uint32_t rnglen = output_len;
    uint8_t   rngoffset = 0;

    while (rnglen > 0) {
        *(output + rngoffset) = (unsigned char) (((unsigned int)rand() << 16) + rand());
        rngoffset++;
        rnglen--;
    }
    return 0;
}

static int _ssl_client_init(mbedtls_ssl_context *ssl,
                            mbedtls_net_context *tcp_fd,
                            mbedtls_ssl_config *conf,
                            mbedtls_x509_crt *crt509_ca, const char *ca_crt, size_t ca_len,
                            mbedtls_x509_crt *crt509_cli, const char *cli_crt, size_t cli_len,
                            mbedtls_pk_context *pk_cli, const char *cli_key, size_t key_len,  const char *cli_pwd, size_t pwd_len
                           )
{
#if (TLS_AUTH_MODE == TLS_AUTH_MODE_CA)
    int ret = -1;
#endif

    /*
     * 0. Initialize the RNG and the session data
     */
#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold((int)0);
#endif

    mbedtls_net_init(tcp_fd);
    mbedtls_ssl_init(ssl);
    mbedtls_ssl_config_init(conf);

#if (TLS_AUTH_MODE == TLS_AUTH_MODE_CA)
    mbedtls_x509_crt_init(crt509_ca);

    /*verify_source->trusted_ca_crt==NULL
     * 0. Initialize certificates
     */

    log_info("Loading the CA root certificate ...");
    if (NULL != ca_crt) {
        if (0 != (ret = mbedtls_x509_crt_parse(crt509_ca, (const unsigned char *)ca_crt, ca_len))) {
            log_error(" failed ! x509parse_crt returned -0x%04x", -ret);
            return ret;
        }
    }
    log_info(" ok (%d skipped)", ret);


    /* Setup Client Cert/Key */
#if defined(MBEDTLS_CERTS_C)
    mbedtls_x509_crt_init(crt509_cli);
    mbedtls_pk_init(pk_cli);
#endif
    if (cli_crt != NULL && cli_key != NULL) {
#if defined(MBEDTLS_CERTS_C)
        log_info("start prepare client cert .");
        ret = mbedtls_x509_crt_parse(crt509_cli, (const unsigned char *) cli_crt, cli_len);
#else
        {
            ret = 1;
            log_error("MBEDTLS_CERTS_C not defined.");
        }
#endif
        if (ret != 0) {
            log_error(" failed!  mbedtls_x509_crt_parse returned -0x%x\n", -ret);
            return ret;
        }

#if defined(MBEDTLS_CERTS_C)
        log_info("start mbedtls_pk_parse_key[%s]", cli_pwd);
        ret = mbedtls_pk_parse_key(pk_cli, (const unsigned char *) cli_key, key_len, (const unsigned char *) cli_pwd, pwd_len);
#else
        {
            ret = 1;
            log_error("MBEDTLS_CERTS_C not defined.");
        }
#endif

        if (ret != 0) {
            log_error(" failed\n  !  mbedtls_pk_parse_key returned -0x%x\n", -ret);
            return ret;
        }
    }
#endif /* #if (TLS_AUTH_MODE == TLS_AUTH_MODE_CA) */

    return 0;
}

/**
 * @brief This function connects to the specific SSL server with TLS, and returns a value that indicates whether the connection is create successfully or not. Call #NewNetwork() to initialize network structure before calling this function.
 * @param[in] ptlsData is the the network structure pointer.
 * @param[in] addr is the Server Host name or IP address.
 * @param[in] port is the Server Port.
 * @param[in] ca_crt is the Server's CA certification.
 * @param[in] ca_crt_len is the length of Server's CA certification.
 * @param[in] client_crt is the client certification.
 * @param[in] client_crt_len is the length of client certification.
 * @param[in] client_key is the client key.
 * @param[in] client_key_len is the length of client key.
 * @param[in] client_pwd is the password of client key.
 * @param[in] client_pwd_len is the length of client key's password.
 * @sa #NewNetwork();
 * @return If the return value is 0, the connection is created successfully. If the return value is -1, then calling lwIP #socket() has failed. If the return value is -2, then calling lwIP #connect() has failed. Any other value indicates that calling lwIP #getaddrinfo() has failed.
 */
static int _TLSConnectNetwork(TLSDataParams_t *ptls_data, const char *addr, const char *port,
                              const char *ca_crt, size_t ca_crt_len,
                              const char *client_crt,   size_t client_crt_len,
                              const char *client_key,   size_t client_key_len,
                              const char *client_pwd, size_t client_pwd_len)
{
	char e_buf[0xFF] = { 0, };
    int ret = -1, verify_result;
    /*
     * 0. Init
     */
    if (0 != (ret = _ssl_client_init(&(ptls_data->ssl), &(ptls_data->fd), &(ptls_data->conf),
                                     &(ptls_data->cacertl), ca_crt, ca_crt_len,
                                     &(ptls_data->clicert), client_crt, client_crt_len,
                                     &(ptls_data->pkey), client_key, client_key_len, client_pwd, client_pwd_len))) {
        log_error(" failed ! ssl_client_init returned -0x%04x", -ret);
        return ret;
    }

    /*
     * 1. Start the connection
     */
    log_info("Connecting to /%s/%s...", addr, port);
    if (0 != (ret = mbedtls_net_connect(&(ptls_data->fd), addr, port, MBEDTLS_NET_PROTO_TCP))) {
        log_error(" failed ! net_connect returned -0x%04x", -ret);
        return ret;
    }
    log_info(" ok");

    /*
     * 2. Setup stuff
     */
    log_info("  . Setting up the SSL/TLS structure...");
    if ((ret = mbedtls_ssl_config_defaults(&(ptls_data->conf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        log_error(" failed! mbedtls_ssl_config_defaults returned %d", ret);
        return ret;
    }

    mbedtls_ssl_conf_max_version(&ptls_data->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_min_version(&ptls_data->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);

    log_info(" ok");

    /* OPTIONAL is not optimal for security, but makes interop easier in this simplified example */
    if (ca_crt != NULL && false) { /* skip ssl_verify */
#if defined(FORCE_SSL_VERIFY)
        mbedtls_ssl_conf_authmode(&(ptls_data->conf), MBEDTLS_SSL_VERIFY_REQUIRED);
#else
        mbedtls_ssl_conf_authmode(&(ptls_data->conf), MBEDTLS_SSL_VERIFY_OPTIONAL);
#endif
    } else {
        mbedtls_ssl_conf_authmode(&(ptls_data->conf), MBEDTLS_SSL_VERIFY_NONE);
    }

#if (TLS_AUTH_MODE == TLS_AUTH_MODE_CA)
    mbedtls_ssl_conf_ca_chain(&(ptls_data->conf), &(ptls_data->cacertl), NULL);

    if ((ret = mbedtls_ssl_conf_own_cert(&(ptls_data->conf), &(ptls_data->clicert), &(ptls_data->pkey))) != 0) {
        log_error(" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n", ret);
        return ret;
    }
#elif (TLS_AUTH_MODE == TLS_AUTH_MODE_PSK)
	
#endif /* #elif (TLS_AUTH_MODE == TLS_AUTH_MODE_PSK) */

    mbedtls_ssl_conf_rng(&(ptls_data->conf), _ssl_random, NULL);
    mbedtls_ssl_conf_dbg(&(ptls_data->conf), _ssl_debug, NULL);

    if ((ret = mbedtls_ssl_setup(&(ptls_data->ssl), &(ptls_data->conf))) != 0) {
        log_error("failed! mbedtls_ssl_setup returned %d", ret);
        return ret;
    }

    mbedtls_ssl_set_hostname(&(ptls_data->ssl), addr);
    mbedtls_ssl_set_bio(&(ptls_data->ssl), &(ptls_data->fd), mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

/* setup sessoin if sessoin ticket enabled */
#if defined(TLS_SAVE_TICKET)
    if (NULL == saved_session) {
        do {
            int len = TLS_MAX_SESSION_BUF;
            unsigned char *save_buf = HAL_Malloc(TLS_MAX_SESSION_BUF);
	        char key_buf[KEY_MAX_LEN] = {0};

            if (save_buf ==  NULL) {
                printf(" malloc failed\r\n");
                break;
            }

            saved_session = HAL_Malloc(sizeof(mbedtls_ssl_session));

            if (saved_session == NULL) {
                printf(" malloc failed\r\n");
                HAL_Free(save_buf);
                save_buf =  NULL;
                break;
            }

            memset(save_buf, 0x00, TLS_MAX_SESSION_BUF);
            memset(saved_session, 0x00, sizeof(mbedtls_ssl_session));
	        HAL_Snprintf(key_buf,KEY_MAX_LEN -1, KV_SESSION_KEY_FMT, addr, port);
            ret = HAL_Kv_Get(key_buf, save_buf, &len);

            if (ret != 0 || len == 0) {
                printf(" kv get failed len=%d,ret = %d\r\n", len, ret);
                HAL_Free(saved_session);
                HAL_Free(save_buf);
                save_buf = NULL;
                saved_session = NULL;
                break;
            }
            ret = ssl_deserialize_session(saved_session, save_buf, len);
            if (ret < 0) {
                printf("ssl_deserialize_session err,ret = %d\r\n", ret);
                HAL_Free(saved_session);
                HAL_Free(save_buf);
                save_buf = NULL;
                saved_session = NULL;
                break;
            }
            HAL_Free(save_buf);
        } while (0);
    }

    if (NULL != saved_session) {
        mbedtls_ssl_set_session(&(ptls_data->ssl), saved_session);
        printf("use saved session!!\r\n");
    }
#endif /* #if defined(TLS_SAVE_TICKET) */

    /*
      * 4. Handshake
      */
    log_info("Performing the SSL/TLS handshake...");

    while ((ret = mbedtls_ssl_handshake(&(ptls_data->ssl))) != 0) {
        if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
			mbedtls_strerror(ret, e_buf, sizeof(e_buf));
            log_error("mbedtls_ssl_handshake failed: %s", e_buf);

#if defined(TLS_SAVE_TICKET)
            if (saved_session != NULL) {
                mbedtls_ssl_session_free(saved_session);
                HAL_Free(saved_session);
                saved_session = NULL;
            }
#endif
            return ret;
        }
    }
    log_info(" ok");

#if defined(TLS_SAVE_TICKET)
    do {
        size_t real_session_len = 0;
        mbedtls_ssl_session *new_session = NULL;

        new_session = HAL_Malloc(sizeof(mbedtls_ssl_session));
        if (NULL == new_session) {
            break;
        }

        memset(new_session, 0x00, sizeof(mbedtls_ssl_session));

        ret = mbedtls_ssl_get_session(&(pTlsData->ssl), new_session);
        if (ret != 0) {
            HAL_Free(new_session);
            break;
        }
        if (saved_session == NULL) {
            ret = 1;
        } else if (new_session->ticket_len != saved_session->ticket_len) {
            ret = 1;
        } else {
            ret = memcmp(new_session->ticket, saved_session->ticket, new_session->ticket_len);
        }
        if (ret != 0) {
            unsigned char *save_buf = HAL_Malloc(TLS_MAX_SESSION_BUF);
            if (save_buf ==  NULL) {
                mbedtls_ssl_session_free(new_session);
                HAL_Free(new_session);
                new_session = NULL;
                break;
            }
            memset(save_buf, 0x00, sizeof(TLS_MAX_SESSION_BUF));
            ret = ssl_serialize_session(new_session, save_buf, TLS_MAX_SESSION_BUF, &real_session_len);
            printf("mbedtls_ssl_get_session_session return 0x%04x real_len=%d\r\n", ret, (int)real_session_len);
            if (ret == 0) {
		        char key_buf[KEY_MAX_LEN] = {0};
		        HAL_Snprintf(key_buf,KEY_MAX_LEN -1, KV_SESSION_KEY_FMT, addr, port);
                ret = HAL_Kv_Set(key_buf, (void *)save_buf, real_session_len, 1);

                if (ret < 0) {
                    printf("save ticket to kv failed ret =%d ,len = %d\r\n", ret, (int)real_session_len);
                }
            }
            HAL_Free(save_buf);
        }
        mbedtls_ssl_session_free(new_session);
        HAL_Free(new_session);
    } while (0);
    if (saved_session != NULL) {
        mbedtls_ssl_session_free(saved_session);
        HAL_Free(saved_session);
        saved_session = NULL;
    }
#endif

	const mbedtls_x509_crt *peer_crt = mbedtls_ssl_get_peer_cert(&ptls_data->ssl);
	if (!peer_crt) {
		log_error("! fail ! server certificate null");
		return -1;
	}

	x509_crt_dump(peer_crt);

    /*
     * 5. Verify the server certificate
     */
    log_info("  . Verifying peer X.509 certificate..");
    ret = mbedtls_ssl_get_verify_result(&(ptls_data->ssl));
	log_info("certificate verification result: 0x%02x", ret);

	verify_result = ret;
#if defined(FORCE_SSL_VERIFY)
	if ((verify_result & MBEDTLS_X509_BADCERT_EXPIRED) != 0) {
		log_error("! fail ! ERROR_CERTIFICATE_EXPIRED");
		return -1;
	}

	if ((verify_result & MBEDTLS_X509_BADCERT_REVOKED) != 0) {
		log_error("! fail ! server certificate has been revoked");
		return -1;
	}

	if ((verify_result & MBEDTLS_X509_BADCERT_CN_MISMATCH) != 0) {
		log_error("! fail ! CN mismatch");
		return -1;
	}

	if ((verify_result & MBEDTLS_X509_BADCERT_NOT_TRUSTED) != 0) {
		log_error("! fail ! self-signed or not signed by a trusted CA");
		return -1;
	}
#endif
    return 0;
}

/**
 * @ca_crt_len: value is 'strlen(ca_crt) + 1', refer to mbedtls_x509_crt_parse
 */
static uintptr_t ssl_establish(const char *host, uint16_t port,
		const char *ca_crt, size_t ca_crt_len)
{
	char port_str[6];
    const char *alter = host;
	TLSDataParams_t *ptls_data;

	if (NULL == host || NULL == ca_crt) {
        log_error("%s failed: null!", __func__);
        return 0;
    }

	if (0 == port || ca_crt_len == 0) {
        log_error("%s failed: param invalid!", __func__);
        return 0;
    }

	ptls_data = xmalloc(sizeof(TLSDataParams_t));
    if (NULL == ptls_data) {
        return (uintptr_t) NULL;
    }
    memset(ptls_data, 0x0, sizeof(TLSDataParams_t));
	sprintf(port_str, "%u", port);

	if (0 != _TLSConnectNetwork(ptls_data, alter, port_str, ca_crt, ca_crt_len, NULL, 0, NULL, 0, NULL, 0)) {
        ssl_disconnect(ptls_data);
        xfree((void *)ptls_data);
        return (uintptr_t) NULL;
    }

	return (uintptr_t) ptls_data;
}

int ssl_connect(struct ssl_network *network)
{
	network->ssl_fd = ssl_establish(network->remote, network->port, network->ca_crt, network->ca_crt_len + 1);
	if (network->ssl_fd) {
		return 0;
	}

	return -1;
}

ssize_t ssl_recv(uintptr_t fd, void *buf, size_t len, uint32_t timeout)
{
	TLSDataParams_t *ptls_data = (TLSDataParams_t *) fd;
	uint32_t readLen = 0;
    static int net_status = 0;
    int ret = -1;
    char err_str[0xff];

    mbedtls_ssl_conf_read_timeout(&(ptls_data->conf), timeout);
    while (readLen < len) {
        ret = mbedtls_ssl_read(&(ptls_data->ssl), (unsigned char *)(buf + readLen), (len - readLen));
        if (ret > 0) {
            readLen += ret;
            net_status = 0;
        } else if (ret == 0) {
            /* if ret is 0 and net_status is -2, indicate the connection is closed during last call */
            return (net_status == -2) ? net_status : readLen;
        } else {
            if (MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY == ret) {
                mbedtls_strerror(ret, err_str, sizeof(err_str));
                log_error("ssl recv error: code = %d, err_str = '%s'", ret, err_str);
                net_status = -2; /* connection is closed */
                break;
            } else if ((MBEDTLS_ERR_SSL_TIMEOUT == ret)
                       || (MBEDTLS_ERR_SSL_CONN_EOF == ret)
                       || (MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED == ret)
                       || (MBEDTLS_ERR_SSL_NON_FATAL == ret)) {
                /* read already complete */
                /* if call mbedtls_ssl_read again, it will return 0 (means EOF) */

                return readLen;
            } else {
                mbedtls_strerror(ret, err_str, sizeof(err_str));
                log_error("ssl recv error: code = %d, err_str = '%s'", ret, err_str);
                net_status = -1;
                return -1; /* Connection error */
            }
        }
    }

    return (readLen > 0) ? readLen : net_status;
}

size_t ssl_send(uintptr_t fd, const void *buf, size_t len, uint32_t timeout)
{
	TLSDataParams_t *ptls_data = (TLSDataParams_t *) fd;
	size_t writtenLen = 0;
	int ret = -1;

	while (writtenLen < len) {
		ret = mbedtls_ssl_write(&(ptls_data->ssl), (unsigned char *)(buf + writtenLen), (len - writtenLen));
		if (ret > 0) {
			writtenLen += ret;
			continue;
		} else if (ret == 0) {
			log_error("ssl write timeout");
			return 0;
		} else {
			char err_str[0xff];
			mbedtls_strerror(ret, err_str, sizeof(err_str));
			log_error("ssl write fail, code=%d, str=%s", ret, err_str);
			return -1; /* Connnection error */
		}
	}

	return writtenLen;
}

int ssl_disconnect(uintptr_t fd)
{
	TLSDataParams_t *ptls_data = (TLSDataParams_t *) fd;
	mbedtls_ssl_close_notify(&(ptls_data->ssl));
    mbedtls_net_free(&(ptls_data->fd));

#if (TLS_AUTH_MODE == TLS_AUTH_MODE_CA)
    mbedtls_x509_crt_free(&(ptls_data->cacertl));
    if (ptls_data->pkey.pk_info != NULL) {
        log_info("need release client crt&key");
#if defined(MBEDTLS_CERTS_C)
        mbedtls_x509_crt_free(&(ptls_data->clicert));
        mbedtls_pk_free(&(ptls_data->pkey));
#endif
    }
#endif /* #if (TLS_AUTH_MODE == TLS_AUTH_MODE_CA) */
    mbedtls_ssl_free(&(ptls_data->ssl));
    mbedtls_ssl_config_free(&(ptls_data->conf));
    log_info("ssl_disconnect");
	return 0;
}

