/*
 * Copyright (c) 2016 Jeff Spaulding <sarnet@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>

#define MAX_SOCKETS 10
#define BUFSIZE 1024

void
print_help()
{
        printf("Usage:\n");
        printf("fwctl [-d] [-p <port number>] [-a <bind address>]\n");
	printf("  -d will keep the program from daemonizing.\n");
}

void
print_version()
{
        printf("fwctl version 0.1\n");
        printf("Copyright (c) 2016 Jeff Spaulding\n");
}

int
handle_message(int sock, char *buf, int len, const struct sockaddr *to,
	       socklen_t *addrlen)
{
	FILE *pipe;
	char msg[4];
	char state;

	buf[len] = 0;

	if (strncmp(buf, "FUP", 4) == 0) {
		/* Close the firewall */
		pipe = popen("/usr/local/sbin/fwkids -u", "r");
		state = fgetc(pipe);
		pclose(pipe);

		if (state == '1') {
			strncpy(msg, "ACK", 4);
		} else {
			strncpy(msg, "ERR", 4);
		}
	} else if (strncmp(buf, "FDN", 4) == 0) {
		/* Open the firewall */
		pipe = popen("/usr/local/sbin/fwkids -d", "r");
		state = fgetc(pipe);
		pclose(pipe);

		if (state == '1') {
			strncpy(msg, "ACK", 4);
		} else {
			strncpy(msg, "ERR", 4);
		}
	} else if (strncmp(buf, "FST", 4) == 0) {
		/* Return status */
		pipe = popen("/usr/local/sbin/fwkids -s", "r");
		state = fgetc(pipe);
		pclose(pipe);

		if (state == '1') {
			strncpy(msg, "SUP", 4);
		} else if (state == '2') {
			strncpy(msg, "SDN", 4);
		} else {
			strncpy(msg, "ERR", 4);
		}
	} else {
		/* Unrecognized packet */
		strncpy(msg, "NAK", 4);
	}

	if (sendto(sock, msg, 4, 0, to, *addrlen) == -1) {
		warn("Error sending reply.");
		return 0;
	}

	return 1;
}

int
main(int argc, char *argv[])
{
	int ch;
	int i;
	int error_flag = 0;
	int debug_flag = 0;

	int sock[MAX_SOCKETS];
	int num_sockets = 0;

	int gai_err, save_error;
	const char *error_cause = NULL;

	char addrstr[512] = "";
	char portstr[20] = "2287";

	struct addrinfo ai_hints, *addr0, *addr1;

	char buf[BUFSIZE];
	int recvlen;
	struct sockaddr_storage from;
	socklen_t from_len = sizeof(from);

	struct pollfd listeners[MAX_SOCKETS];
	int ready_sockets;

	/* Set up variables */
	memset(&ai_hints, 0, sizeof(ai_hints));
	ai_hints.ai_family = PF_UNSPEC;
	ai_hints.ai_socktype = SOCK_DGRAM;
	ai_hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;

	/* Parse options */
	while ((ch = getopt(argc, argv, "p:a:v")) != -1) {
                switch (ch) {
                case 'v':
                        print_version();
                        return 0;
                case 'p':
			strncpy(portstr, optarg, sizeof(portstr));
                        break;
                case 'a':
			strncpy(addrstr, optarg, sizeof(addrstr));
                        break;
		case 'd':
			debug_flag = 1;
			break;
                default:
                        print_help();
                        return -1;
                }
        }

	if (! debug_flag) {
		if (daemon(0, 0))
			err(1, "Could not damonize");
	}

	/* Set up sockets */
	if (addrstr[0] == 0 || portstr[0] == 0) {
		print_help();
		return -1;
	}


	gai_err = getaddrinfo(addrstr, portstr, &ai_hints, &addr1);
	if (gai_err) {
		errx(1, "%s", gai_strerror(gai_err));
	}

	sock[0] = -1;

	for (addr0 = addr1; addr0; addr0 = addr0->ai_next) {
		sock[num_sockets] = socket(addr0->ai_family, addr0->ai_socktype,
			      addr0->ai_protocol);

		if (sock[num_sockets] == -1) {
			error_cause = "Error creating socket";
			warn("%s", error_cause);
			continue;
		}

		if (bind(sock[num_sockets], addr0->ai_addr, addr0->ai_addrlen) == -1) {
			error_cause = "Error binding socket";
			save_error = errno;
			close(sock[num_sockets]);
			errno = save_error;
			warn("%s", error_cause);
			continue;
		}

		num_sockets++;
	}

	if (sock[0] == -1)
		err(1, "%s", error_cause);

	if (num_sockets < 1)
		errx(1, "Couldn't open any sockets!");

	/* Listen */

	for (i = 0; i < num_sockets; i++) {
		listeners[i].fd = sock[i];
		listeners[i].events = POLLIN;
	}


	while(1) {
		ready_sockets = poll(listeners, num_sockets, 1000);

		if (ready_sockets < 0) {
			warn("%s", "Poll error");
			error_flag = 1;
			break;
		}

		if (ready_sockets == 0)
			continue;

		for (i = 0; i < num_sockets; i++) {
			if (listeners[i].revents & (POLLERR|POLLNVAL)) {
				warn("Poll error on %d\n", i);
				error_flag = 1;
				break;
			}

			if (listeners[i].revents & POLLIN) {
				listeners[i].revents = 0;
				recvlen = recvfrom(sock[i], buf, BUFSIZE, 0,
						   (struct sockaddr*) &from,
						   &from_len);

				if (recvlen < 1) {
					warn("%s", "UDP Receive error");
					error_flag = 1;
					break;
				} else {
					if (! handle_message(sock[i], buf, recvlen,
							   (struct sockaddr*) &from,
							   &from_len)) {
						error_flag = 1;
						break;
					}
				}
			}
		}

		if (error_flag)
			break;
	}

	/* Close down */

	for (i = 0; i < num_sockets; i++) {
		close(sock[i]);
	}

	freeaddrinfo(addr1);

	if (error_flag)
		return -1;
	else
		return 0;
}
