#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

/**
 * Enum to help with weird pipe semantics
 */
#define PIPE_OUT 0
#define PIPE_IN 1

/**
 * Struct to represent a command and it's arguments.
 */
typedef struct {
	/** The name of the executable */
	char* progname;

	/**
	 * IO redirection
	 * redirect[i] should be used as fd i in the child
	 * a value of -1 represents no redirect
	 */
	int redirect[2];

	/** The arguments, must be NULL-terminated */
	char *args[];
} cmd_struct;

/**
 * Struct to represent a pipeline of commands. The intention is that cmd[i]'s
 * output goes to cmd[i+1]'s input.
 */
typedef struct {
	/* The total number of commands. */
	int n_cmds;
	/* The commands themselves. */
	cmd_struct* cmds[];
} pipeline_struct;

/**
 * Parses str into a freshly allocated cmd_sruct and returns a pointer to it.
 * The redirects in the returned cmd_struct will be set to -1, ie no redirect.
 */
cmd_struct* parse_command(char *str);

/**
 * Parses str into a freshly allocated pipeline_struct and returns a pointer to
 * it. All cmd_structs will also be freshly allocated, and have their redirects
 * set to -1, ie no redirect.
 */
pipeline_struct* parse_pipeline(char *str);

/**
 * Checks for a builtin command and if there is one, it executes the appropriate
 * action.
 */
bool handle_command(cmd_struct* command);

/**
 * Closes a set of file descriptors representing pipes based on the index of the command in the pipeline.
 */
void close_fd_set(int **fd_set, int cmd_index, int n_cmds, bool close_all);

/**
 * Executes a command and it's arguments, optionally redirecting stdin to
 * stdin_redirect, and stdout to stdout_redirect. If either of the redirect
 * arguments is -1, nothing will be redirected.
 */
void execvp_redirect_io(cmd_struct* command, int stdin_fd, int stdout_fd);

// for debugging
void print_command(cmd_struct* command);

#endif
