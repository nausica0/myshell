#include <stdio.h>
#include <fcntl.h>

int main()
{
	int fd, stdout_fd;

	fd = creat("ttt", 0664);
	printf("printf before dup2, fd=%d\n", fd);
	stdout_fd = dup(1);
	dup2(fd, 1);
	printf("printf after dup2\n");
	close(fd);
	printf("printf after close(fd)\n");
	dup2(stdout_fd, 1);
	printf("printf after dup2(stdout_fd)\n");

	return;
}
