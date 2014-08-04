#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


/* 상수 정의 */
#define MAXLINE 1024
#define MAXARGS 128

/* 전역 변수 정의 */
char prompt[] = "myshell> ";
const char delim[] = " ";


/* 함수 선언 */
void myshell_error(char *err_msg);
void process_cmd(char *cmdline);
int parse_line(char *cmdline, char **argv);

// 내장 명령어 처리 함수
int list_files(int argc, char **argv);
int list_files_long(int argc, char **argv);
int copy_file(int argc, char **argv);
int remove_files(int argc, char **argv);
int move_files(int argc, char **argv);
int change_directory(int argc, char **argv);
int make_directory(int argc, char **argv);
int remove_directory(int argc, char **argv);
int quit_shell(int argc, char **argv);


/* 내장 명령어 목록 자료구조 */
typedef struct {
	char *cmd;
	int (*func)(int argc, char **argv);
} Builtin_cmd;

Builtin_cmd cmds[] = {
	{ "ls",		list_files },
	{ "ll",		list_files_long },
	{ "cp",		copy_file },
	{ "rm",		remove_file },
	{ "mv",		move_file },
	{ "cd",		change_directory },
	{ "mkdir",	make_directory },
	{ "rmdir",	remove_directory },
	{ "quit",	quit_shell },
	{ NULL,		NULL }
};


/*
 * main - MyShell's main routine
 */
int main()
{
	char cmdline[MAXLINE];

	/* 명령어 처리 루프: 셸 명령어를 읽고 처리한다. */
	while (1) {
		// 프롬프트 출력
		printf("%s", prompt);
		fflush(stdout);

		// 명령 라인 읽기
		if (fgets(cmdline, MAXLINE, stdin) == NULL) {
			myshell_error("command line read error");
		}

		// 명령 라인 처리
		process_cmd(cmdline);

		fflush(stdout);
		fflush(stdout);
	}

	/* 프로그램이 도달하지 못한다. */
	return 0;
}


/*
 * process_cmd
 *
 * 명령 라인을 인자 (argument) 배열로 변환한다.
 * 내장 명령 처리 함수를 수행한다.
 * 내장 명령이 아니면 자식 프로세스를 생성하여 지정된 프로그램을 실행한다.
 * 파이프(|)를 사용하는 경우에는 두 개의 자식 프로세스를 생성한다.
 */
void process_cmd(char *cmdline)
{
	int pipe_flag = 0;
	int argc;
	char *argv[MAXARGS];

	// 명령 라인을 해석하여 인자 (argument) 배열로 변환한다.
	argc = parse_line(cmdline);
	if (argc == 0) {
		return;
	}

#ifdef HW_STAGE1
	/* 명령 라인 처리 결과를 출력한다. */
#endif

	/* 내장 명령 목록을 검색하여 명령을 수행한다. */
	cmd_index = 0;
	while (cmds[cmd_index].cmd != NULL) {
		if (strcmp(argv[0], cmds[cmd_index].cmd) == 0) {
			ret_val = cmds[cmd.index].func(argc, argv);
			break;
		}

		cmd_index++;
	}

	return;
}


/*
 * parse_line
 *
 * 명령 라인을 인자(argument) 배열로 변환한다.
 * 인자의 개수(argc)를 리턴한다.
 */
int parse_line(char *cmdline, char **argv)
{
	int argc = 0;
	char *tok;

	argv[argc] = NULL;

	tok = strtok(cmdline, delim);
	while (tok) {
		argv[argc++] = tok;
		tok = strtok(NULL, delim);
	}

	return argc;
}


/*
 * myshell_error
 *
 * 주어진 문자열(error message)을 출력하고 프로그램을 종료한다.
 */
void myshell_error(char *err_msg)
{
	fprintf(stderr, "%s\n", err_msg);
	exit(1);
}
