#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

#include "freezer.h"

#define FRZR_PATH_LEN 128
#define FRZR_PATH_PREFIX_LEN 13
static char frzr_path[FRZR_PATH_LEN] = "/dev/freezer/";

static const char *frzr_states[] = {
	[FRZR_THAWED]        = "THAWED",
	[FRZR_FROZEN]        = "FROZEN",
	[FRZR_FREEZING]      = "FREEZING",
	[FRZR_UNINITIALIZED] = "UNINITIALIZED"
};


static int
frzr_setpath(const char *id, int prfx_len)
{
	const int sufx_len = sizeof(frzr_path) - prfx_len;
	int ret;

	ret = snprintf(frzr_path + prfx_len,  sufx_len, "%s", id);
	if (ret < 0) {
		perror("snprintf");
		exit(1);
	} else if (ret >= sufx_len) {
		fprintf(stderr, "Couldn't write the whole id:%s\n", id);
		exit(1);
	}

	return ret;
}

void
frzr_init(pid_t pid, const char *id)
{
	int ret, fd, len, prfx_len;
	char buff[128];


	prfx_len = FRZR_PATH_PREFIX_LEN;
	prfx_len += frzr_setpath(id, prfx_len);

	ret = mkdir(frzr_path, 0755);
	if (ret < 0) {
		perror(frzr_path);
		exit(1);
	}

	frzr_setpath("/tasks", prfx_len);
	fd = open(frzr_path, O_WRONLY);
	if (fd == -1){
		perror(frzr_path);
		exit(1);
	}

	len = snprintf(buff, sizeof(buff), "%d", pid);
	if (len < 0) {
		perror("snprintf");
		exit(1);
	} else if (len >= sizeof(buff)) {
		fprintf(stderr, "Couldn't write whole pid:%d !?!?\n", pid);
		exit(1);
	}

	ret = write(fd, buff, len);
	if (ret != len){
		perror("write");
		exit(1);
	}

	close(fd);

}

void
frzr_shut(const char *id)
{
	int ret;
	int prfx_len;

	prfx_len = FRZR_PATH_PREFIX_LEN;
	prfx_len += frzr_setpath(id, prfx_len);

	ret = rmdir(frzr_path);
	if (ret < 0) {
		perror(frzr_path);
		exit(1);
	}
}

void
frzr_setstate(const char *id, int state)
{
	int prfx_len, fd, ret, len, retry;
	const char *state_str;

	assert(state == FRZR_THAWED || state == FRZR_FROZEN);

	prfx_len = FRZR_PATH_PREFIX_LEN;
	prfx_len += frzr_setpath(id, prfx_len);
	prfx_len += frzr_setpath("/freezer.state", prfx_len);

	fd = open(frzr_path, O_WRONLY);
	if (fd == -1){
		perror(frzr_path);
		exit(1);
	}

//	printf("%s: going to freeze %s\n", __func__, id); fflush(stdout);
	state_str = frzr_states[state];
	len = strlen(state_str);
	retry = 2;
write_state:
	retry--;
	ret = write(fd, state_str, len);
	if (ret != len){
		printf("%s: freezing %s failed... retries %d\n", __func__, id, retry); fflush(stdout);
		if (retry)
			goto write_state;
		perror("frzr_setstate, write");
		exit(1);
	}
	
//	printf("%s: done\n", __func__); fflush(stdout);

	close(fd);
}

int
frzr_getstate(const char *id)
{
	int prfx_len, fd, ret, i;
	char buff[128];

	prfx_len = FRZR_PATH_PREFIX_LEN;
	prfx_len += frzr_setpath(id, prfx_len);
	prfx_len += frzr_setpath("/freezer.state", prfx_len);

	fd = open(frzr_path, O_RDONLY);
	if (fd == -1){
		if (errno == ENOENT)
			return FRZR_UNINITIALIZED;
		perror(frzr_path);
		exit(1);
	}

	ret = read(fd, buff, sizeof(buff));
	if (ret < 0){
		perror("read");
		exit(1);
	}

	if (buff[ret-1] == '\n')
		buff[ret-1] = '\0';

	for (i=0; i<FRZR_STATES_END__; i++) {
		if (strncmp(frzr_states[i], buff, sizeof(buff)) == 0)
			goto end;
	}

	printf("Unknown state: %s\n", buff);
	assert(0);
end:
	close(fd);
	return i;
}

#ifdef FREEZER_TEST
int main(int argc, const char *argv[])
{
	int pid;

	void myatexit(void) {
	}
	atexit(myatexit);

	pid = fork();
	if (pid < 0){
		perror("fork");
		exit(1);
	}

	/* Child */
	if (pid == 0){
		int i=0;
		for (;;){
			printf("[%4d] Pizza!\n", i++);
			sleep(1);
		}
	}

	/* main */
	frzr_init(pid, "PIZZA");
	printf("current state: %s\n", frzr_states[frzr_getstate("PIZZA")]);
	sleep(5);

	frzr_setstate("PIZZA", FRZR_FROZEN);
	printf("current state: %s\n", frzr_states[frzr_getstate("PIZZA")]);
	sleep(5);

	frzr_setstate("PIZZA", FRZR_THAWED);
	printf("current state: %s\n", frzr_states[frzr_getstate("PIZZA")]);
	sleep(1);

	printf("Killing: %d\n", pid);
	kill(pid, SIGTERM);
	wait(NULL);
	frzr_shut("PIZZA");

	return 0;
}
#endif /* FREEZER_TEST */
