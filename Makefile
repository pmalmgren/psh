all: shell

clean:
	rm -vf shell utils.o

CFLAGS := -Wall -Wextra -std=c11 -pedantic -Werror

shell: shell.o utils.o
