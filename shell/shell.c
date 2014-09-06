#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#define ERROR -1
#define SPACE 0
#define BN 1
#define PIPE 2

#define INITIAL_WORD_SIZE 20

#define MAX_ARGS 20

struct command {
	char *command;
	char *args[MAX_ARGS + 1];
	char *path;
	int numArgs;
	struct command *pipeCommand;
};

void printError(char *msg)
{
	if (strcmp(msg, "") == 0)
		msg = strerror(errno);

	printf("error: %s", msg);
}

int getWord(char **buffer)
{
	char c;
	size_t buffSize = INITIAL_WORD_SIZE, length = 0;
	char *start;
	char *buff;


	/* Skip preceding white spaces */
	while ((c = getchar()) != EOF && (c == ' ' || c == '\t'))
		;

	if (c == '\n')
		return BN;

	if (c == '|')
		return PIPE;


	start = malloc(sizeof(char) * buffSize);

	if (!start) {
		printError("");
		return ERROR;
	}

	buff = (char *)start;

	*(buff++) = c;

	while ((c = getchar()) != EOF && c != '\n' && c != ' ' && c != '|') {
		length = buff - start;
		if (length >= buffSize - 2) {
			buffSize *= 2;
			start = (char *)realloc(start, buffSize);

			/* Fix identation */
			if (start == NULL) {
				printError("");
				return ERROR;
			}

			buff = start + length;
		}

		*(buff++) = c;
	}


	*(buff) = '\0';

	*buffer = start;

	if (c == '\n')
		return BN;


	if (c == '|')
		return PIPE;

	return SPACE;
}


int getCommand(struct command **commandPtr)
{
	char *word;
	int flag, pipeFlag;
	struct command *command = (struct command *)malloc(sizeof(struct command));

	word = NULL;

	if (command == NULL) {
		printError("");
		return ERROR;
	}

	flag = getWord(&word);

	if (flag == ERROR) {
		return ERROR;
	}

	if (word == NULL) {
		return BN;
	}

	command->command = word;
	command->numArgs = 0;
	command->pipeCommand = NULL;

	command->args[command->numArgs++] = word;

	*commandPtr = command;

	if (flag == BN)
		return BN;

	if (flag == PIPE) {
		command->args[command->numArgs] = NULL;
		return getCommand(&(command->pipeCommand));
	}
	
	word = NULL;
	while ((flag = getWord(&word)) != BN || word != NULL) {
		if (flag == ERROR)
			continue;

		if (word != NULL) {
			command->args[command->numArgs++] = word;
			word = NULL;
		}

		if (flag == BN)
			break;

		if (flag == PIPE) {
			pipeFlag = getCommand(&(command->pipeCommand));	

			if(pipeFlag == ERROR)
				return ERROR;

			break;
		}

		word = NULL;

		/* Verify max ars */
	}

	command->args[command->numArgs] = NULL;

	return 0;
}

int main(int argc, char **argv)
{
	struct command *command, *commands;
	int flag;
	int i;

	printf("$");
	command = NULL;

	while ((flag = getCommand(&command)) != -1){
		if (command == NULL) {
			printf("$");
			continue;
		}

		for (commands = command; commands; commands = commands->pipeCommand) {
			printf("Command: %s\n", commands->command);
			printf("Args: \n");

			for (i = 0; i < commands->numArgs; i++)
				printf("\t-%s\n", commands->args[i]);

			if (commands->pipeCommand)
				printf("--------Pipe------\n");
		}

		printf("$");

		/*memory leak */
		command = NULL;
	}

	return 0;
}
