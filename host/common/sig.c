#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <signal.h>

#include "sig.h"

/* Whether we've had a `ctrl-c`. */
volatile bool bl_sig_killed;

static void bl_sig__ctrl_c_handler(int sig)
{
	if (sig == SIGINT) {
		bl_sig_killed = true;
	}
}

static bool bl_sig__signal_handler(void)
{
	struct sigaction act = {
		.sa_handler = bl_sig__ctrl_c_handler,
	};
	int ret;

	ret = sigemptyset(&act.sa_mask);
	if (ret == -1) {
		fprintf(stderr, "sigemptyset call failed: %s\n",
				strerror(errno));
		return false;
	}

	ret = sigaddset(&act.sa_mask, SIGINT);
	if (ret == -1) {
		fprintf(stderr, "sigaddset call failed: %s\n",
				strerror(errno));
		return false;
	}

	ret = sigaction(SIGINT, &act, NULL);
	if (ret == -1) {
		fprintf(stderr, "Failed to set SIGINT handler: %s\n",
				strerror(errno));
		return false;
	}

	return true;
}

bool bl_sig_init(void)
{
	bl_sig_killed = false;

	if (!bl_sig__signal_handler()) {
		return false;
	}

	return true;
}
