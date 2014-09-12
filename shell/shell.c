#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>
#include <sys/wait.h>

/* ERROR: means there an error that will make the
	the shell exit
   SPACE: means that there was a space after a word
   BN: means that there was a \n after a word
   PIPE: means that there was a pipe after a word */
#define ERROR -1
#define SPACE 0
#define BN 1
#define PIPE 2

/* Space for this many characters is set aside for
each word. If this value is reached, realloc is called
and the value is increased just for that word */
#define INITIAL_WORD_SIZE 20

/* Maximum number of arguments. Feel free to change it */
#define MAX_ARGS 20

/* S_FIRTS: means that the current command is the first one
	(no pipes before it)
   S_PIPE: Means that there is a pipe before the next command
   S_COMMAND: Means that a command is being parsed
   S_ERROR: means an error was found (for expmple two consecutive pipes
	with no command in between them*/
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
		/* check if length of the word (so far) is greater than
		the allocated buffer -2 (missing the \0 at the end. If
		it is reallocate the buffer multiplying its length times
		two*/
		if (length >= buffSize - 2) {
			buffSize *= 2;
			start = (char *)realloc(start, buffSize);

			/* Check realloc error */
			if (start == NULL) {
				printError("");
				return ERROR;
			}

			buff = start + length;
		}

		*(buff++) = c;
	}


	*buff = '\0';

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
	/* Absolute path check */
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

		/* Concatenate the path with a / and the command. There
		could be a // if the path is ended with a / but execv
		still works with this problem (/bin//ls) */
		sprintf(route, "%s/%s", node->pathText, command->command);

		/* Check if the program exists and is executable by the user */
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

	/* If it was not found, let execv try in the CWD and if it still
	doesn't find it, let execv handle the error */
	if (found == 0)
		command->path = strdup(command->command);

	if (command->path == NULL)
		printError("error: Problem in the strdup\n");
}

/* Fills the command structure with the
appropriate data*/
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

	/* If \n is found, the NULL is appended to numArgs to let execv
	know that no more arguments are present */
	if (flag == BN) {
		command->args[command->numArgs] = NULL;
		return BN;
	}

	if (flag == PIPE) {
		command->args[command->numArgs] = NULL;
		return getCommand(&(command->pipeCommand));
	}

	word = NULL;
	/* Iterate through each word and add it to the args
	array */
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

/* Iterate through path list and print it */
void printPath(void)
{
	struct pathNode *node;

	for (node = path; node; node = node->next)
		printf("%s%s", node->pathText, node->next != NULL ? ":" : "");

	printf("\n");
}

/* Add a new pathNode with the text pathText to the end of the list */
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

/* Delete path containing pathText from the list */
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

/* Closes unnecessary file descriptors and duplicates the write portion
of the pipe to STDOUT. This function handles the piping for the command
that is at the left side of the pipe */
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

/* Closes unnecessary file descriptors and duplicates the read portion
of the pipe to STDIN. This function handles the piping for the command
that is at the right side of the pipe */
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

/* Function to close the 2 descriptors in the array fd */
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

/* Verifies if the command should be executed by the shell (cd, path, exit)
and executes them. If not, it forks and executes the command(s) */
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
		/* Check if there is another command piped after this one
		and if it is handle the ouput of this command to be the
		input of the other command */
		if (command->pipeCommand != NULL)
			handleNewPipeFd(fd);

		/* If there was a pipe before, handle the input by
		making it the read part of the pipe instead of
		STDIN*/
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

/* Free all of the command's properties and other pipe commands */
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

	/* S_FIRST means that it is the first command
	that will be executed. (No pipes before it) */
	commandState = S_FIRST;

	while ((flag = getCommand(&command)) != -1) {
		if (commandState == S_PIPE) {
			printError("Can't end command with a pipe");
			commandState = S_ERROR;
		}

		/* If there was an error, no the command is freed. No
		error message is shown because it has already been shown
		by the getCommand function */
		if (commandState == S_ERROR) {
			freeCommand(command);
			command = NULL;
			printf("$");
			commandState = S_FIRST;
			continue;
		}

		/* Handle \n commands. Simply put the prompt again and
		set the state of the command to S_FIRST*/
		if (command == NULL) {
			printf("$");
			commandState = S_FIRST;
			continue;
		}

		execCommand(command, NULL);


		while ((pid = wait(&status)) != -1)
			;

		/* Check why the wait function failed and if it was
		not because there are no more child processes to wait
		for, an error message is displayed */
		if (errno != ECHILD)
			printError("");


		printf("$");

		freeCommand(command);
		command = NULL;
		commandState = S_FIRST;
	}

	return 0;
}
