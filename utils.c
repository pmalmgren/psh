#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

#define TOKEN_SEP " \t\n\r"
#define MAX_LEN 1024

char* next_non_empty(char **line) {
    char *tok;

    while ((tok = strsep(line, TOKEN_SEP)) && !*tok);

    return tok;
}

void print_command(cmd_struct* command) {
    char** arg = command->args;
  	 int i = 0;

  	 fprintf(stderr, "progname: %s\n", command->progname);

    for (i = 0, arg = command->args; *arg; ++arg, ++i) {
  	    fprintf(stderr, " args[%d]: %s\n", i, *arg);
  	}
}

cmd_struct* parse_command(char* str) {
    /* Copy the input line in case the caller wants it later. */
    char* copy = strndup(str, MAX_LEN);
    char* token;
    int i = 0;

    /*
    * Being lazy (Rule 0) and allocating way too much memory for the args array.
    * Using calloc to ensure it's zero-initialised, which is important because
    * execvp expects a NULL-terminated array of arguments.
    */
    cmd_struct* ret = calloc(sizeof(cmd_struct) + MAX_LEN * sizeof(char*), 1);

    while ((token = next_non_empty(&copy))) {
        ret->args[i++] = token;
    }

    ret->progname = ret->args[0];
    ret->redirect[0] = ret->redirect[1] = -1;
    return ret;
}

pipeline_struct* parse_pipeline(char* str) {
    char* copy = strndup(str, MAX_LEN);
    char* cmd_str;
    int n_cmds = 0;
    int i = 0;
    pipeline_struct* ret;

    /* count pipe characters */
    for (char* cur = copy; *cur; cur++) {
        if (*cur == '|') ++n_cmds;
    }

    ++n_cmds; /* There is one more command than there are pipe characters. */

    ret = calloc(sizeof(pipeline_struct) + n_cmds * sizeof(cmd_struct*), 1);
    ret->n_cmds = n_cmds;

    while ((cmd_str = strsep(&copy, "|"))) {
        ret->cmds[i++] = parse_command(cmd_str);
    }

    return ret;
}

void execvp_redirect_io(cmd_struct* command, int stdin_fd, int stdout_fd) {
    if (stdin_fd != -1 && stdin_fd != STDIN_FILENO) {
        if (dup2(stdin_fd, STDIN_FILENO) == -1) {
            char* error = calloc(sizeof(char*) * 1024, 1);
            sprintf(error, "%s stdin dup2 fd %d", command->progname, stdin_fd);
            perror(error);
            exit(1);
        }

        fprintf(stderr, "Command %s redirecting STDIN to %d\n", command->progname, command->redirect[0]);

        if (close(stdin_fd) == -1) {
            perror("close");
            exit(1);
        }
    } else {
        fprintf(stderr, "Command %s not redirecting STDIN. stdin_fd == %d\n", command->progname, stdin_fd);
    }

    if (stdout_fd != -1 && stdout_fd != STDOUT_FILENO) {
        if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
            perror("stdout dup2");
            exit(1);
        }

        fprintf(stderr, "Command %s redirecting STDOUT to %d\n", command->progname, stdout_fd);

        if (close(stdout_fd) == -1) {
            perror("close");
            exit(1);
        }
    } else {
        fprintf(stderr, "Command %s not redirecting STDOUT. stdout_fd == %d\n", command->progname, stdout_fd);
    }

    execvp(command->progname, command->args);
}

bool handle_command(cmd_struct* command) {
    if (!command->progname) {
        return true;
    }

    if (strcmp(command->progname, "cd") == 0) {
        if (command->args[1]) {
            chdir(command->args[1]);
        }
        return true;
    }

    return false;
}
