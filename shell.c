#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include "utils.h"

#define PATH_MAX 4096

static char* const prompt = "psh> ";
static char* const exit_cmd = "exit";

extern char **environ;

ssize_t prompt_for_input(char **buf, size_t *len) {
	char current_dir[PATH_MAX];
	getcwd(current_dir, PATH_MAX);

	printf("psh [%s]> ", current_dir);
	return getline(buf, len, stdin);
}

bool should_exit(char *cmd) {
	if (!cmd || strlen(cmd) != 4)
		return false;

	int i;

	for (i = 0; i < 4; i++) {
		if (cmd[i] != exit_cmd[i])
			return false;
	}

	return true;
}

void fork_exec(cmd_struct* command) {
	switch (fork()) {
	case -1:
		fprintf(stderr, "Error executing %s\n", command->progname);
	case 0:
		execvp_redirect_io(command, -1, -1);
	default:
		if (wait(NULL) == -1) {
			fprintf(stderr, "error: wait\n");
			return;
		}
	}
}

void close_fail(int fd, char* reason) {
	if (close(fd) == -1) {
		perror(reason);
		exit(1);
	}
}

void exec_pipeline(pipeline_struct *pipeline) {
	int i = 0;
	int **pfd = (int**) malloc(sizeof(int*) * (pipeline->n_cmds - 1));

	for (i = 0; i < pipeline->n_cmds - 1; i++)
		pfd[i] = (int*) calloc(sizeof(2*sizeof(int)), 1);

	int stdin_next = -1;
	pid_t *children = calloc(sizeof(pid_t) * pipeline->n_cmds, 1);

	for (i = 0; i < pipeline->n_cmds; i++) {
		int stdin_redirect, stdout_redirect = -1;
		cmd_struct* command = pipeline->cmds[i];

		/** For n processes, we need n-1 pipelines */
		if (i < pipeline->n_cmds - 1) {
			if (pipe(pfd[i]) == -1) {
				perror("pipe");
				exit(1);
			}
		}

		/**
		 * There are three cases to consider while building an execution
		 * pipeline: The first process, middle processes, and the last process.
		 * 1) The first process doesn't need to have it's stdin fd redirected,
		 *    but will need to have it's stdout redirected to the write end of
		 *    the first pipe. Since stdin_next is initialized to -1, this is
		 *    handled implicitly.
		 * 2) The middle processes need both their stdin fd and stdout fd
		 *    redirected. This is handled implcitly.
		 * 3) The last process doesn't need it's stdout redirected. This is
		 *    handled explicitly with the if statement below.
		 */
		stdin_redirect = stdin_next;

		if (i != pipeline->n_cmds - 1) {
			stdin_next = pfd[i][0];
			stdout_redirect = pfd[i][1];
		}

		pid_t child = fork();

		switch (child) {
			case -1:
				break;
			case 0:
				/**
				 * bash uses a bitmap of file descriptors to close for each
				 * process (see execute_cmd.c fd_bitmap and close_fd_bitmap).
				 * we just calculate and manually close after the fork()
				 */
				close_fd_set(pfd, i, pipeline->n_cmds, false);
				execvp_redirect_io(command, stdin_redirect, stdout_redirect);
				break;
			default:
				children[i] = child;
				break;
		}
	}

	close_fd_set(pfd, pipeline->n_cmds-1, pipeline->n_cmds, true);

	for (i = 0; i < pipeline->n_cmds; i++)
		waitpid(children[i], NULL, 0);

	free(children);

	for (i = 0; i < pipeline->n_cmds - 1; i++)
		free(pfd[i]);

	free(pfd);
}

int main() {
	char *buff = NULL;
	size_t len;

	for(;;) {
		prompt_for_input(&buff, &len);
		pipeline_struct* pipeline = parse_pipeline(buff);

		if (pipeline->n_cmds <= 0)
			continue;

		cmd_struct* cur_cmd = pipeline->cmds[0];

		if (should_exit(cur_cmd->progname))
			return 0;

		if (handle_command(cur_cmd))
			continue;

		if (pipeline->n_cmds == 1) {
			fork_exec(cur_cmd);
			continue;
		}

		exec_pipeline(pipeline);
	}

	return 0;
}
