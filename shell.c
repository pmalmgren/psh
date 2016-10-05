#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

static char* const prompt = "psh> ";
static char* const exit_cmd = "exit";

ssize_t prompt_for_input(char **buf, size_t *len) {
	fputs(prompt, stdout);
	return getline(buf, len, stdin);
}

int should_exit(char *cmd) {
	if (strlen(cmd) != 5)
		return 1;

	int i;
	for (i = 0; i < 4; i++) {
		if (cmd[i] != exit_cmd[i])
			return 1;
	}

	return 0;
}

int main() {
	char *buff = NULL;
	size_t len;

	do {
		prompt_for_input(&buff, &len);
	} while (should_exit(buff) != 0);

	return 0;
}
