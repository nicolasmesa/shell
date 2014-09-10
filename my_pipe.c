#include <stdio.h>
#include <stdlib.h>

int main(){
	int fd[2];
	int pid;
	char *args[] = {"ls","-al", 0};
	char *args2[] = {"grep", "my_pipe", 0};

	pipe(fd);

	pid = fork();

	if(pid){
		dup2(fd[1], 1);
		execv("/bin/ls", args);
		printf("Error\n");
	}else{
		pid = fork();

		if(pid){
			dup2(fd[0], 0);
			execv("/bin/grep", args2);
			printf("Error p2"); 
		}else{
			while(wait() != -1)
				;

			printf("Parent exits\n");
		}
	}
}
