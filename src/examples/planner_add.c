#include <stdio.h>
#include <syscall.h>

int main(int argc, char *argv[])
{
	printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n");

	return 0;
}
