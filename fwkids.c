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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <sys/wait.h>

const char *version = "0.1";

char *cmd_fwup[]   = {"pfctl", "-a", "kids", "-f",
			    "/etc/pf.anchor.kids-blocked", NULL};
char *cmd_fwdown[] = {"pfctl", "-a", "kids", "-F", "rules", NULL};
char *cmd_fwstat[] = {"pfctl", "-a", "kids", "-s", "rules", NULL};

void
print_help()
{
	printf("fwkids %s\n", version);
	printf("Usage:\n");
	printf("fwkids [-u] [-d] [-s]\n");
	printf("  u - bring up firewall\n");
	printf("  d - bring down firewall\n");
	printf("  s - return firewall status\n");
	printf("Exactly one option must be used.\n");
}

int
main(int argc, char **argv)
{
	pid_t pid;
	int pipefd[2];
	char buf[128];
	ssize_t chars_read;
	int newlines = 0;
	int i;
	int status;
	int failed = 0;

	int ch;
	int uflag, dflag, sflag;

	uflag = dflag = sflag = 0;

	while ((ch = getopt(argc, argv, "udsh")) != -1) {
		switch(ch) {
		case 'u':
			if (dflag | sflag) {
				print_help();
				return 1;
			}

			uflag = 1;
			break;
		case 'd':
			if (uflag | sflag) {
				print_help();
				return 1;
			}

			dflag = 1;
			break;
		case 's':
			if (uflag | dflag) {
				print_help();
				return 1;
			}

			sflag = 1;
			break;
		default:
		case 'h':
			print_help();
			exit(1);
		}
	}

	if (dflag + uflag + sflag != 1) {
		print_help();
		return 1;
	}

	if (pipe(pipefd) != 0) {
		err(1, "Cannot open pipe.");
	}

	if ((pid = fork()) == 0) {
		/* child */
		dup2(pipefd[1], 1);
		close(pipefd[0]);
		if (uflag) {
			execv("/sbin/pfctl", cmd_fwup);
		} else if (dflag) {
			execv("/sbin/pfctl", cmd_fwdown);
		} else {
			execv("/sbin/pfctl", cmd_fwstat);
		}

		err(1, "Could not open pfctl");

	} else {
		/* parent */
		close(pipefd[1]);

		if (sflag) {

			while ((chars_read = read(
					pipefd[0], buf, sizeof(buf))) > 0) {
				for (i = 0; i < chars_read; i++) {
					if (buf[i] == '\n') {
						newlines++;
					}
				}
			}
			/* So, here's the idea.
			   We read the output of pfctl.  It should return one
			   newline per rule.  If we get no newlines, the anchor
			   is empty. If we do get any number of newlines, the
			   anchor has rules in it.  Since the rules are
			   blocking rules, having newlines means the anchor is
			   loaded and the firewall is up. */

			if (newlines > 0) {
				printf("1");
			} else {
				printf("2");
			}
		} else if (uflag) {
			/* Read pipe to end, we don't care about output.
			 * There shouldn't be any anyway. */
			while ((chars_read = read(
					pipefd[0], buf, sizeof(buf))) > 0) {
			}

			if (wait(&status) == -1) {
				err(1, "Error waiting on child process");
			} else {
				if (WIFEXITED(status)) {
					if (WEXITSTATUS(status) != 0) {
						failed = 1;
						printf("0");
					} else {
						/* success */
						printf("1");
					}
				}
			}
		} else {
			/* dflag */

			/* Read pipe to end, discard output. */
			while ((chars_read = read(
					pipefd[0], buf, sizeof(buf))) > 0) {
			}

			if (wait(&status) == -1) {
				err(1, "Error waiting on child process");
			} else {
				if (WIFEXITED(status)) {
					if (WEXITSTATUS(status) != 0) {
						failed = 1;
						printf("0");
					} else {
						/* success */
						printf("1");
					}
				}
			}
		}

		close(pipefd[0]);
	}

	if (failed) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}
