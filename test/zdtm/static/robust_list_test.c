#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "zdtmtst.h"

const char *test_doc = "Test robust futex list checkpoint/restore";
const char *test_author = "Your Name <your@email.com>";

static pthread_mutex_t mutex;
static volatile int ready = 0;

void *thread_fn(void *arg)
{
	int ret;

	test_msg("[child] Locking mutex...\n");

	ret = pthread_mutex_lock(&mutex);
	if (ret != 0) {
		fail("[child] Failed to lock mutex: %s\n", strerror(ret));
	}

	test_msg("[child] Mutex locked. Going to sleep (pause)...\n");
	ready = 1;

	pause();

	return NULL;
}

int main(int argc, char **argv)
{
	pthread_mutexattr_t attr;
	pthread_t tid;
	int ret;

	test_init(argc, argv);
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE);

	pthread_mutex_init(&mutex, &attr);

	pthread_create(&tid, NULL, thread_fn, NULL);

	while (!ready)
		usleep(1000);

	test_msg("[main] Child locked mutex and is sleeping. Ready for CRIU dump.\n");
	test_msg("[main] Calling test_daemon...\n");
	test_daemon();
	test_msg("[main] Returned from test_daemon. Trying to lock mutex...\n");
	test_msg("[main] Back from restore. Trying to lock mutex...\n");

	ret = pthread_mutex_lock(&mutex);
	if (ret == EOWNERDEAD) {
		test_msg("[main] Mutex owner died. Making mutex consistent.\n");
		pthread_mutex_consistent(&mutex);
	} else if (ret != 0) {
		fail("[main] Failed to lock mutex after restore: %s\n", strerror(ret));
	} else {
		test_msg("[main] Locked mutex successfully after restore.\n");
	}

	pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
	pthread_mutexattr_destroy(&attr);

	pass();
	return 0;
}
