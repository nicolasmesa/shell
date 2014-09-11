#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>
#include <sys/wait.h>

#define ERROR -1
#define SPACE 0
#define BN 1
#define PIPE 2

#define INITIAL_WORD_SIZE 20

#define MAX_ARGS 20

#define S_FIRST 1
#define S_PIPE 2
#define S_COMMAND 3
#define S_ERROR 4

int commandState;

struct command {
	char *command;
	char *args[MAX_ARGS + 1];
	char *path;
	int numArgs;
	struct command *pipeCommand;
};

struct pathNode {
	char *pathText;
	struct pathNode *next;
} *path;

void printError(char *msg)
{
	if (strcmp(msg, "") == 0)
		msg = strerror(errno);

	fprintf(stderr, "error: %s\n", msg);
}

void ignoreRemainingCharacters(void)
{
	while (getchar() != '\n')
		;
}

int getWord(char **buffer)
{
	char c;
	size_t buffSize = INITIAL_WORD_SIZE, length = 0;
	char *start;
	char *buff;


	/* Skip preceding white spaces  and tabs*/
	while ((c = getchar()) != EOF && (c == ' ' || c == '\t'))
		;

	if (c == '\n')
		return BN;

	if (c == '|') {
		if (commandState == S_PIPE) {
			printError("Can't have two pipes in a row");
			commandState = S_ERROR;
		} else if (commandState == S_FIRST) {
			printError("Unexpected \"|\"");
			commandState = S_ERROR;
		} else {
			commandState = PIPE;
		}

		if (commandState == S_ERROR)
			ignoreRemainingCharacters();


		return PIPE;
	}


	start = malloc(sizeof(char) * buffSize);

	if (!start) {
		printError("");
		return ERROR;
	}

	buff = (char *)start;

	*(buff++) = c;

	commandState = S_COMMAND;

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


	if (c == '|') {
		commandState = S_PIPE;
		return PIPE;
	}

	return SPACE;
}

void addPathToCommand(struct command *command)
{
	if (*(command->command) == '/') {
		command->path = strdup(command->command);
		return;
	}

	struct pathNode *node;
	char *route;
	int found = 0, acc, totalLength;

	for (node = path; node; node = node->next) {
		totalLength = strlen(command->command) + strlen(node->pathText);
		route = malloc(sizeof(char) * (totalLength + 2));

		if (route == NULL) {
			printError("");
			break;
		}

		strcpy(route, node->pathText);
		strcat(route, "/");
		strcat(route, command->command);

		acc = access(route, X_OK);

		if (acc == 0) {
			command->path = strdup(route);
			free(route);
			found = 1;
			break;
		} else if (acc == -1 && errno != EACCES && errno != ENOENT) {
			printError("");
		}

		free(route);
	}


	if (found == 0)
		command->path = strdup(command->command);

	if (command->path == NULL)
		printError("error: Problem in the strduph\n");
}


int getCommand(struct command **commandPtr)
{
	char *word;
	int flag, pipeFlag;
	struct command *command =
			(struct command *)malloc(sizeof(struct command));

	word = NULL;

	if (command == NULL) {
		printError("");
		return ERROR;
	}

	flag = getWord(&word);

	if (flag == ERROR)
		return ERROR;

	if (word == NULL)
		return BN;

	command->command = word;
	command->numArgs = 0;
	command->pipeCommand = NULL;
	command->path = NULL;
	addPathToCommand(command);

	command->args[command->numArgs++] = word;
	*commandPtr = command;

	if (flag == BN) {
		command->args[command->numArgs] = NULL;
		return BN;
	}

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

			if (pipeFlag == ERROR)
				return ERROR;

			break;
		}

		word = NULL;

		if (command->numArgs > MAX_ARGS - 1) {
			commandState = S_ERROR;
			fprintf(stderr, "Can't have more than %d arguments\n",
				 MAX_ARGS);

			ignoreRemainingCharacters();
			break;
		}
	}

	command->args[command->numArgs] = NULL;

	return 0;
}

void printPath(void)
{
	struct pathNode *node;

	for (node = path; node; node = node->next)
		printf("%s%s", node->pathText, node->next != NULL ? ":" : "");

	printf("\n");
}

void addPath(char *pathText)
{
	struct pathNode *node;

	if (path == NULL) {
		path = malloc(sizeof(struct pathNode));
		node = path;
	} else {
		for (node = path; node->next; node = node->next)
			;

		node->next = malloc(sizeof(struct pathNode));
		node = node->next;
	}

	if (node == NULL) {
		printError("");
		return;
	}

	node->next = NULL;
	node->pathText = strdup(pathText);

	if (node->pathText == NULL)
		printError("");
}

void deletePath(char *pathText)
{
	struct pathNode *node, *previous = NULL;
	int found = 0;

	for (node = path; node; node = node->next) {
		if (strcmp(node->pathText, pathText) == 0) {
			found = 1;
			break;
		}

		previous = node;
	}

	if (found == 1) {
		if (previous == NULL)
			path = node->next;
		else
			previous->next = node->next;

		free(node);
	} else {
		printf("error: Entry \"%s\" not found in path\n", pathText);
	}
}

void handlePathCommand(struct command *command)
{
	if (command->numArgs == 1) {
		printPath();
		return;
	}

	if (strcmp("+", command->args[1]) == 0) {
		if (command->numArgs > 2)
			addPath(command->args[2]);
		else
			printError("Need to specify the path to add");
	} else if (strcmp("-", command->args[1]) == 0) {
		if (command->numArgs > 2)
			deletePath(command->args[2]);
		else
			printError("Need to secify path to delete");
	} else {
		printf("error: Unknown argument \"%s\"\n", command->args[1]);
	}
}

void handleNewPipeFd(int *fd)
{
	int returnVal;

	returnVal = close(fd[0]);

	if (returnVal == -1) {
		printError("");
		exit(errno);
	}

	returnVal = dup2(fd[1], 1);

	if (returnVal == -1) {
		printError("");
		exit(errno);
	}

	returnVal = close(fd[1]);

	if (returnVal == -1) {
		printError("");
		exit(errno);
	}
}

void handleOldPipeFd(int *fd)
{
	int returnVal;

	returnVal = close(fd[1]);

	if (returnVal == -1) {
		printError("");
		exit(errno);
	}

	returnVal = dup2(fd[0], 0);

	if (returnVal == -1) {
		printError("");
		exit(errno);
	}

	returnVal = close(fd[0]);

	if (returnVal == -1) {
		printError("");
		exit(errno);
	}
}

void closePipeDescriptors(int *fd)
{
	int returnVal;

	returnVal = close(fd[0]);

	if (returnVal == -1)
		printError("");


	returnVal = close(fd[1]);

	if (returnVal == -1)
		printError("");
}

void execCommand(struct command *command, int *pipeBeforeFd)
{
	int fd[2], pipeReturn = 0;

	if (strcmp("cd", command->command) == 0 && command->numArgs > 1) {
		if (chdir(command->args[1]) == -1)
			printError("");

		return;
	}

	if (strcmp("exit", command->command) == 0)
		exit(0);

	if (strcmp("path", command->command) == 0) {
		handlePathCommand(command);
		return;
	}

	if (command->pipeCommand != NULL)
		pipeReturn = pipe(fd);

	if (pipeReturn == -1) {
		printError("");
		return;
	}


	int pid = fork();

	if (pid < 0) {
		printError("");
		return;
	}

	if (pid == 0) {
		if (command->pipeCommand != NULL)
			handleNewPipeFd(fd);

		if (pipeBeforeFd != NULL)
			handleOldPipeFd(pipeBeforeFd);

		execv(command->path, command->args);
		printError("");
		exit(errno);
	} else {
		if (pipeBeforeFd != NULL)
			closePipeDescriptors(pipeBeforeFd);

		if (command->pipeCommand != NULL)
			execCommand(command->pipeCommand, fd);
	}
}

void freeCommand(struct command *command)
{
	int i;

	if (command == NULL)
		return;

	if (command->pipeCommand != NULL)
		freeCommand(command->pipeCommand);

	for (i = 0; i < command->numArgs; i++)
		free(command->args[i]);

	free(command);
}

int main(int argc, char **argv)
{
	struct command *command;
	int flag, status, pid;

	printf("$");
	command = NULL;
	path = NULL;
	commandState = S_FIRST;

	while ((flag = getCommand(&command)) != -1) {
		if (commandState == S_PIPE) {
			printError("Can't end command with a pipe");
			commandState = S_ERROR;
		}

		if (commandState == S_ERROR) {
			freeCommand(command);
			command = NULL;
			printf("$");
			commandState = S_FIRST;
			continue;
		}

		if (command == NULL) {
			printf("$");
			commandState = S_FIRST;
			continue;
		}

		execCommand(command, NULL);

		/* Check error */
		while ((pid = wait(&status)) != -1)
			;

		if (errno != ECHILD)
			printError("");


		printf("$");

		freeCommand(command);
		command = NULL;
		commandState = S_FIRST;
	}

	return 0;
}
