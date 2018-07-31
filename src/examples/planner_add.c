#include <stdio.h>
#include <syscall.h>
#include <string.h>

bool check_time_format(char *time);

int main(int argc, char *argv[])
{
	printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n- - - - - - - - -\n");

	if (argc < 3)
	{
		printf("Wrong number of arguments!\n");
		exit(-1);
	}

	char buf[128], *ptr = buf;
	int len = argc - 3;
	memset(buf, 0, sizeof buf);

	for (int i = 1; i < argc - 1; i++)
	{
		len += strlen(argv[i]);
		if (len > 127)
		{
			printf("Too long line [name args]!\n");
			exit(-1);
		}

		memcpy(ptr, argv[i], strlen(argv[i]));
		ptr += strlen(argv[i]);
		if (i != argc - 2) { *ptr = ' '; ptr++; }
		else *ptr = '\0';
	}

	if (planner_add(buf, argv[argc - 1]) < 0)
	{
		printf("Wrong arguments! Use this format: [name args] [YYYY:MM:DD:hh:mm:ss]\nNOTE: length of arg 'name' must be > 3 symbols.\n");
		exit(-1);
	}

	return 0;
}
