#include <stdio.h>

int main(int argc, char **argv){
	char c;

	while((c = getchar()) != EOF)
		putchar(c);

	printf("\n My name is : %s\n\n\n------------------\n", argv[0]);

	return 0;
}
