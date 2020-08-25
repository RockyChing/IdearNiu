#include "../../../include/common_header.h"
#include "../../../include/log_ext.h"

#include "socket/ssl_util.h"


static const char *g_ca_crt_test = \
{
    \
	"-----BEGIN CERTIFICATE-----\r\n"
	"MIIDWjCCAkICCQDp6EvRsn2rQTANBgkqhkiG9w0BAQsFADBsMQswCQYDVQQGEwJD\r\n" \
	"TjELMAkGA1UECAwCQkoxCzAJBgNVBAcMAkJKMQswCQYDVQQKDAJIRDEMMAoGA1UE\r\n" \
	"CwwDZGV2MQswCQYDVQQDDAJjYTEbMBkGCSqGSIb3DQEJARYMY2FAd29ybGQuY29t\r\n" \
	"MB4XDTIwMDgyNTAxMzA1MloXDTMwMDgyMzAxMzA1MlowcjELMAkGA1UEBhMCQ04x\r\n" \
	"CzAJBgNVBAgMAkJKMQswCQYDVQQHDAJCSjELMAkGA1UECgwCSEQxDDAKBgNVBAsM\r\n" \
	"A2RldjEOMAwGA1UEAwwFaGVsbG8xHjAcBgkqhkiG9w0BCQEWD2hlbGxvQHdvcmxk\r\n" \
	"LmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKZaOsAzlC7JuX1w\r\n" \
	"WK2s6NgxbR0CLRyAgOT7HuNQoypjeYglsTkBfRUlF1/PaK6tDQzho0t4f0z/PtzT\r\n" \
	"+LR9VGKyA0cOwcE+jReEgEXjHUuMHiycBuX2ZWRsdzYkDOuNrhVdSJxua8LoJl64\r\n" \
	"jx6CI8IFB8c8OkRVs0Y8YK5t4p5NI3n88BSUtNVDNhItRgNOmLeWBn4EatEQs72l\r\n" \
	"WJNGslFomViNyL1TBMF1XlYO4LiO6QyJjqqHbrcF7E19wj0RL8o6W5vMAiAysVmT\r\n" \
	"VW58aTlBSFAzvRYPIPjU1H9s4O6eVRny5CAjUoqe+OxoMpsz1HIg3jwmU4KnbR/q\r\n" \
	"dDuxszkCAwEAATANBgkqhkiG9w0BAQsFAAOCAQEAly5nCeqUGNVYw6H3ymBLw5qN\r\n" \
	"cQMmGheeK1r7RavufN065FQqA73jGTwlmMP1A0cBwA7LEfCY6UM8wCwscyNcv79R\r\n" \
	"Y8Z2jNQ9eg7CNKlw6jE5PHrlZkBMB8DGlPoT8uyURiPxDNL7+b3uIiwcjBH/RlwV\r\n" \
	"Nhc08Mh5MFqf1MO+tlEgVXCo5/zp96DAQdioXDBpfRghbSRPzMvov3WHRgug6OY9\r\n" \
	"jmpG5AD1OraGDBWPrg+H5sjj+v4j/1gJRnvyeudPaKPaL+taHdAsZkal5mkk883c\r\n" \
	"p0CbTvzwjCeayUnzl+Gnfb6KChTXu+Tc04eI8rBwNTh3qBW/pxyWMwpgTUZOeQ==\r\n" \
	"-----END CERTIFICATE-----"
};

struct ssl_network g_ssl_network;



int main(int argc, char *argv[])
{
	struct ssl_network *pNetwork = &g_ssl_network;
	char buff[1024];
	memset(pNetwork, 0, sizeof(struct ssl_network));
	pNetwork->remote = "bing.com";
	pNetwork->port = 443;
	pNetwork->ca_crt = g_ca_crt_test;
	pNetwork->ca_crt_len = strlen(pNetwork->ca_crt);

	if (ssl_connect(pNetwork)) {
		return -1;
	}

	ssl_send(pNetwork->ssl_fd, pNetwork->ca_crt, pNetwork->ca_crt_len, 1000);
	memset(buff, 0, sizeof(buff));
	ssl_recv(pNetwork->ssl_fd, buff, sizeof(buff), 1000);
	ssl_disconnect(pNetwork->ssl_fd);
	printf("RESP:\n%s\n\n", buff);
	return 0;
}

