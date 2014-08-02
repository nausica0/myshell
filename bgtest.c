#include <stdio.h>
#include <stdlib.h>

int main()
{
	int i;
	for (i = 1; i <= 10; i++) {
		printf("background job...(%d)\n", i);
		fflush(stdout);
		sleep(1);
	}

	return 0;
}

