#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
		execvp_redirect_io(command);
	default:
		if (wait(NULL) == -1) {
			fprintf(stderr, "error: wait\n");
			return;
		}
	}
}

void exec_pipeline(pipeline_struct *pipeline) {
	int i = 0;
	int pfd[2];

	// prev_wd keeps track of the previous fork()'s write descriptor, which needs to be closed
	int prev_wfd = -1;
	pid_t* child_pids = calloc(sizeof(pid_t) * pipeline->n_cmds, 1);
	cmd_struct* cur_cmd = pipeline->cmds[0];
	cmd_struct* next_cmd;

	/** create pipes */
	for (i = 0; i < pipeline->n_cmds; i++) {
		/** the first command has it's stdout redirected to the head of the
		 *  pipe and the stdin closed
		 */
		cur_cmd = pipeline->cmds[i];

		if (i < pipeline->n_cmds - 1)
			next_cmd = pipeline->cmds[i+1];
		else
			next_cmd = NULL;

		if (next_cmd) {
			if (pipe(pfd) == -1) {
				perror("pipe");
				exit(1);
			}
			cur_cmd->redirect[1] = pfd[1];
			next_cmd->redirect[0] = pfd[0];
			printf("pfd[1]=%d -> ()==========() pfd[0]=%d -> \n", pfd[1], pfd[0]);
		}

		pid_t child_pid;
		child_pid = fork();

		switch (child_pid) {
		case -1:
			perror("fork");
			exit(1);
		case 0:
			if (next_cmd) {
				if (close(pfd[0]) == -1) {
					perror("close pipe");
					exit(1);
				}
				fprintf(stderr, "closed read fd %d in %s\n", pfd[0], cur_cmd->progname);
			}

			if (prev_wfd != -1) {
				if (close(prev_wfd) == -1) {
					perror("close previous write file descriptor");
					exit(1);
				}
				fprintf(stderr, "closed write fd %d in %s\n", prev_wfd, cur_cmd->progname);
			}
			execvp_redirect_io(cur_cmd);
		default:
			prev_wfd = pfd[1];
			child_pids[i] = child_pid;
		}
	}

	for (i = 0; i < pipeline->n_cmds; i++) {
		cur_cmd = pipeline->cmds[i];

		if (cur_cmd->redirect[0] != -1) {
			if (close(cur_cmd->redirect[0]) == -1) {
				perror("close parent");
				exit(1);
			} else {
				fprintf(stderr, "parent closed %d\n", cur_cmd->redirect[0]);
			}
		}

		if (cur_cmd->redirect[1] != -1) {
			if (close(cur_cmd->redirect[1]) == -1) {
				perror("close parent");
				exit(1);
			} else {
				fprintf(stderr, "parent closed %d\n", cur_cmd->redirect[1]);
			}
		}

		if (i < pipeline->n_cmds -1) {
			if (wait(NULL) == -1) {
				fprintf(stderr, "error: wait\n");
				return;
			}
		}
	}
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
