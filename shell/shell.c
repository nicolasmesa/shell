#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#define ERROR -1
#define SPACE 0
#define BN 1
#define PIPE 2

#define INITIAL_WORD_SIZE 20

void printError(char *msg){
	if(strcmp(msg, "") == 0)
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
	while((c = getchar()) != EOF && ( c == ' ' || c == '\t'))
		;

	if(c == '\n'){
		return BN;	
	}

	if(c == '|'){
		return PIPE;
	}


	start = malloc(sizeof(char) * buffSize);

        if(!start){
                printError("");
		return ERROR;
        }

	buff = (char *)start;

	*(buff++) = c;

	while((c = getchar()) != EOF && c != '\n' && c != ' ' && c != '|'){
		if((length = buff - start) >= buffSize - 2){
			buffSize *= 2;
			start = (char *)realloc(start, buffSize);

			/* Fix identation */
			if(start == NULL){
				printError("");
				return ERROR;
			}

			buff = start + length;
		}
		
		*(buff++) = c;
	}


	*(buff) = '\0';

	*buffer = start;

	if(c == '\n'){
                return BN;
        }


	if(c == '|'){
		return PIPE;
	}

	return SPACE;
}

int main(int argc, char **argv)
{
	char * word = NULL;
	int flag;

	while((flag = getWord(&word)) != BN || word != NULL){
		if(word != NULL){
                        printf("%s\n", word);
                        free(word);
                        word = NULL;
                }

		if(flag == BN)
			break;

		if(flag == PIPE){
			printf("Pipe\n");
		}

	}

	return 0;
}
