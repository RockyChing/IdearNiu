#include <stdio.h>

#include <sockets.h>
#include <log_ext.h>

int sockfd = -1;


int main(int argc, char *arv[])
{
	sockfd = socket_create(1, 0);
	log_info("New socket: %d", sockfd);

	socket_close(sockfd);
	return 0;
}
