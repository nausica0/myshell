#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


/* 컴파일 옵션 매크로 정의 */
//#define HW_STAGE1


/* 상수 정의 */
#define MAXLINE 1024
#define MAXARGS 128
#define MAXPATH 1024

#define DEFAULT_DIR_MODE	0775
#define FD_STDIN			0
#define FD_STDOUT			1


/* 전역 변수 정의 */
char prompt[] = "myshell> ";
const char delim[] = " \t\n";
int bg_flag, pipe_flag;
char *pargv[MAXARGS];		// argv for pipe processing


/* 전역 변수 선언 */


/* 함수 선언 */
void myshell_error(char *err_msg);
void process_cmd(char *cmdline);
int parse_line(char *cmdline, char **argv);
int builtin_cmd(int argc, char **argv);

// 내장 명령어 처리 함수
int list_files(int argc, char **argv);
void print_long_format(char *filename, struct stat *statbuf);
int copy_file(int argc, char **argv);
int remove_file(int argc, char **argv);
int move_file(int argc, char **argv);
int change_directory(int argc, char **argv);
int print_working_directory(void);
int make_directory(int argc, char **argv);
int remove_directory(int argc, char **argv);
int xcopy_file(int argc, char **argv);



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
	int argc;
	char *argv[MAXARGS];
#ifndef HW_STAGE1
	pid_t pid, pipe_pid;
	int pipefd[2];
	int status, ret;
#endif

	// background와 pipe flag를 초기화
	bg_flag = pipe_flag = 0;

	// 명령 라인을 해석하여 인자 (argument) 배열로 변환한다.
	argc = parse_line(cmdline, argv);
	if (argc == 0) {
		// 종료된 background 프로세스를 wait하고 리턴한다.
		ret = waitpid(-1, &status, WNOHANG);
		if (ret > 0) {
			printf("process %d is terminated\n", ret);
		}
		return;
	}

#ifdef HW_STAGE1
	/* 명령 라인 처리 결과를 출력한다. */
	printf("argc = %d\n", argc);
	int i;
	for (i = 0; i < argc; i++) {
		printf("argv[%d] = %s\n", i, argv[i]);
	}
#else

	/* 내장 명령 처리 함수를 수행한다. */
	if (builtin_cmd(argc, argv) == 0) {
		// 종료된 background 프로세스를 wait하고 리턴한다.
		ret = waitpid(-1, &status, WNOHANG);
		if (ret > 0) {
			printf("process %d is terminated\n", ret);
		}
		return;
	}


	/*
	 * 자식 프로세스를 생성하여 프로그램을 실행한다.
	 */

	// pipe flag가 설정되어 있으면 파이프 생성
	if (pipe_flag) {
		if (pipe(pipefd) == -1) {
			fprintf(stderr,"pipe error\n");
			exit(1);
		}
	}

	// 자식 프로세스 생성
	if ((pid = fork()) < 0) {
		fprintf(stderr, "fork error\n");
		return;
	}	
	
	/*
	 * 자식 프로세스 (Child  process)
	 */
	if (pid == 0) {
		/*
		// 새로운 프로세스 그룹 설정
		if (setpgid(0, 0) < 0) {
			fprintf(stderr, "setpgid error (1)\n");
			exit(1);
		}
printf("pid in first child = %d, pgid = %d\n", getpid(), getpgid(getpid()));
*/
		// 파이프의 첫번째 프로그램을 위한 파이프 설정
		if (pipe_flag) {
			// 파이프의 read를 닫는다.
			close(pipefd[0]);

			// 파이프 출력을 stdout으로 복제
			if (dup2(pipefd[1], FD_STDOUT) < 0) {
				fprintf(stderr, "pipefd duplication error\n");
				exit(1);
			}
		}

fprintf(stderr, "argv[0] in first child = %s\n", argv[0]);
		// 프로그램 실행
		if (execvp(argv[0], argv) < 0) {
			fprintf(stderr, "%s: Command not found\n", argv[0]);
			exit(1);
		}
	}


	/*
	 * 부모 프로세스 (Parent process)
	 */
	// pipe_flag가 설정되어 있으면, 새로운 자식 프로세스를 하나 더 
	// 생성하여 파이프로 연결한다.
	if (pipe_flag) {
		// 새로운 자식 프로세스 생성
		if ((pipe_pid = fork()) < 0) {
			fprintf(stderr, "fork error\n");
			exit(1);
		}	

		// 자식 프로세스
		// 파이프의 파이프의 두번째 프로그램
		if (pipe_pid == 0) {
			/*
printf("pid in second child = %d\n", pid);
			// 첫번째 자식의 프로세스 그룹에 포함
			if (setpgid(0, pid) < 0) {
				fprintf(stderr, "setpgid error (2), %d\n", errno);
				exit(1);
			}
			*/

			// 파이프의 write를 닫는다.
			close(pipefd[1]);

			// 파이프 입력을 stdin으로 복제
			if (dup2(pipefd[0], FD_STDIN) < 0) {
				fprintf(stderr, "pipefd duplication error\n");
				exit(1);
			}	

printf("pargv[0] in second child = %s\n", pargv[0]);
			// 파이프의 두번째 프로그램 실행
			if (execvp(pargv[0], pargv) < 0) {
				fprintf(stderr, "%s: Command not found\n", pargv[0]);
				exit(1);
			}
		}	// 두번째 자식 프로세스 (파이프)
	}


	// myshell 부모 프로세스는 파이프를 닫는다.
	if (pipe_flag) {
		close(pipefd[0]);
		close(pipefd[1]);
	}

	// foreground 실행이면 자식 프로세스가 종료할 때까지 기다린다.
	if (!bg_flag) {
#if 0
		ret = waitpid(pid, &status, 0);
		while (pid != ret) {
			if (ret > 0) {
				printf("process %d is terminated\n", ret);
				ret = waitpid(pid, &status, 0);
			} else {
				fprintf(stderr, "wait error\n");
				return;
			}
		}
#endif
#if 1
		ret = waitpid(pid, &status, 0);
		if (ret < 0) {
			fprintf(stderr, "wait error\n");
			return;
		}

		// 파이프 실행이면 자식 프로세스 그룹의 모든 프로세스를 기다린다.
		if (pipe_flag) {
			ret = waitpid(pipe_pid, &status, 0);
			if (ret < 0) {
				fprintf(stderr, "wait error (pipe)\n");
				return;
			}
			/*
			//while ((ret = waitpid(-pid, &status, 0))) {
			while ((ret = waitpid(-1, &status, 0))) {
				if (ret < 0) {
					fprintf(stderr, "wait error\n");
					return;
				}
			}
			*/
		}
#endif
	} else {
		printf("[bg] %d : %s\n", pid, cmdline);
    }
#endif	// HW_STAGE1

	return;
}


/*
 * parse_line
 *
 * 명령 라인을 인자(argument) 배열로 변환한다.
 * 인자의 개수(argc)를 리턴한다.
 * 파이프와 백그라운드 실행을 해석하고 flag와 관련 변수를 설정한다.
 */
int parse_line(char *cmdline, char **argv)
{
	int argc, targc = 0;
	char *tok, **targv;

	targv = argv;	// 인자 배열 임시 포인터
	tok = strtok(cmdline, delim);
	targv[targc] = tok;

	while (tok) {
		// pipe flag 검사 (중복된 파이프 허용 안됨)
		if (!pipe_flag && (!strcmp(targv[targc], "|"))) {
			targv[targc] = NULL;
			pipe_flag = 1;
			argc = targc;		// 첫번째 명령의 인자 개수 결정
			targc = -1;
			targv = pargv;		// 두번째 명령을 위한 인자 배열
		}
		tok = strtok(NULL, delim);
		targv[++targc] = tok;
	}

	// background flag 검사
	//if (!pipe_flag && (targc > 0) && (!strcmp(targv[targc-1], "&"))) {
	if ((targc > 0) && (!strcmp(targv[targc-1], "&"))) {
		targc--;
		targv[targc] = NULL;
		bg_flag = 1;
	}

	// 첫번째 명령의 인자 개수 리턴
	if (pipe_flag) {
		// 두번째 명령이 없으면 파이프 무시
		if (targc == 0) {
			pipe_flag = 0;
		}
		return argc;
	} else {
		return targc;
	}
}


/*
 * builtin_cmd
 *
 * 내장 명령을 수행한다.
 * 내장 명령이 아니면 1을 리턴한다.
 */
int builtin_cmd(int argc, char **argv)
{
	char *cmd = argv[0];

	if ((!strcmp(cmd, "quit") || (!strcmp(cmd, "exit")))) {
		exit(0);
	}

	if (!strcmp(cmd, "ls")) {
		list_files(argc, argv);
		return 0;
	}

	if (!strcmp(cmd, "ll")) {
		list_files(argc, argv);
		return 0;
	}

	if (!strcmp(cmd, "cp")) {
		copy_file(argc, argv);
		return 0;
	}

	if (!strcmp(cmd, "rm")) {
		remove_file(argc, argv);
		return 0;
	}

	if (!strcmp(cmd, "mv")) {
		move_file(argc, argv);
		return 0;
	}

	if (!strcmp(cmd, "cd")) {
		change_directory(argc, argv);
		return 0;
	}

	if (!strcmp(cmd, "pwd")) {
		print_working_directory();
		return 0;
	}

	if (!strcmp(cmd, "mkdir")) {
		make_directory(argc, argv);
		return 0;
	}

	if (!strcmp(cmd, "rmdir")) {
		remove_directory(argc, argv);
		return 0;
	}

	// 내장 명령어가 아님.
	return 1;
}



int list_files(int argc, char **argv)
{
	char *dirname, current_dir[] = ".";
	char pathname[MAXPATH];
	DIR *dp;
	struct dirent *d_entry;
	int lflag = 0;
	struct stat statbuf;

	// 명령 인자 개수를 확인
	if (argc == 1) {
		dirname = current_dir;
	} else if (argc == 2) {
		dirname = argv[1];
	} else {
		fprintf(stderr, "Usage: %s [dir name]\n", argv[0]);
		return 1;
	}

	// 명령어 종류 확인 ls / ll
	if (!strcmp(argv[0], "ll")) {
		lflag = 1;
	}

	dp = opendir(dirname);
	if (dp == NULL) {
		fprintf(stderr, "directory open error\n");
		return 1;
	}

	d_entry = readdir(dp);
	while (d_entry != NULL) {
		// '.'으로 시작하는 파일 제외
		if (d_entry->d_name[0] != '.') {
			if (lflag == 0) {	// ls 명령
				printf("%s\n", d_entry->d_name);
			} else {			// ll 명령
				sprintf(pathname, "%s/%s", dirname, d_entry->d_name);
				if (stat(pathname, &statbuf)) {
					fprintf(stderr, "file information error\n");
					closedir(dp);
					return 1;
				}
				print_long_format(d_entry->d_name, &statbuf);
			}
		}
		d_entry = readdir(dp);
	}

	closedir(dp);

	return 0;
}


void print_long_format(char *filename, struct stat *statbuf)
{
	char timestr[30];

	// 디렉터리 / 파일
	if (S_ISDIR(statbuf->st_mode)) {
		printf("d");
	} else {
		printf("-");
	}

	// 파일 접근 모드
	printf("%c%c%c%c%c%c%c%c%c ", 
		(statbuf->st_mode & S_IRUSR) ? 'r' : '-',
		(statbuf->st_mode & S_IWUSR) ? 'w' : '-',
		(statbuf->st_mode & S_IXUSR) ? 'x' : '-',
		(statbuf->st_mode & S_IRGRP) ? 'r' : '-',
		(statbuf->st_mode & S_IWGRP) ? 'w' : '-',
		(statbuf->st_mode & S_IXGRP) ? 'x' : '-',
		(statbuf->st_mode & S_IROTH) ? 'r' : '-',
		(statbuf->st_mode & S_IWOTH) ? 'w' : '-',
		(statbuf->st_mode & S_IXOTH) ? 'x' : '-');

	// 하드 링크 개수
	printf("%d ", statbuf->st_nlink);

	// 사용자 id와 그룹 id
	printf("%4d ", statbuf->st_uid);
	printf("%4d ", statbuf->st_gid);

	// 파일 크기
	printf("%8d ", (int) statbuf->st_size);

	// 파일 수정 시간
	ctime_r(&statbuf->st_mtime, timestr);
	timestr[16] = 0;
	printf("%s ", (char *) &(timestr[4]));

	// 파일 이름
	printf("%s\n", filename);

	return;
}

int copy_file(int argc, char **argv)
{
	char *in_file, *out_file;
	FILE *in, *out;	
	int c;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <src_file> <dst_file>\n", argv[0]);
		return 1;
	}

	in_file = argv[1];
	out_file = argv[2];

	if ( (in = fopen(in_file,"r")) == NULL) {
		fprintf(stderr,"Cannot open %s for reading\n",in_file);
		return 1;
	}
	if ( (out = fopen(out_file,"w")) == NULL) {
		fprintf(stderr,"Cannot open %s for writing\n",out_file);
		return 1;
	}

	while ( (c = getc(in)) != EOF)
		putc(c,out);

	fclose(in);
	fclose(out);

	return 0;
}

int remove_file(int argc, char **argv)
{
	char *filename;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file_name>\n", argv[0]);
		return 1;
	}

	filename = argv[1];

	//ret = remove(filename);
	ret = unlink(filename);
	if (ret != 0) {
		fprintf(stderr, "remove file error\n");
	}

	return ret;
}

int move_file(int argc, char **argv)
{
	char *old_file, *new_file;
	int ret;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <old_file> <new_file>\n", argv[0]);
		return 1;
	}

	old_file = argv[1];
	new_file = argv[2];

	ret = rename(old_file, new_file);
	if (ret != 0) {
		fprintf(stderr, "move file error\n");
	}

	return ret;
}

int change_directory(int argc, char **argv)
{
	char *dirname;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dir_name>\n", argv[0]);
		return 1;
	}

	dirname = argv[1];

	ret = chdir(dirname);
	if (ret != 0) {
		fprintf(stderr, "change directory error\n");
	}

	return ret;
}

int print_working_directory(void)
{
	char *cwd;

	cwd = getcwd(NULL, 0);
	if (cwd == 0) {
		fprintf(stderr, "get current working directory error\n");
		return 1;
	}

	fprintf(stderr, "%s\n", cwd);

	return 0;
}


int make_directory(int argc, char **argv)
{
	char *dirname;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dir_name>\n", argv[0]);
		return 1;
	}

	dirname = argv[1];

	ret = mkdir(dirname, DEFAULT_DIR_MODE);
	if (ret != 0) {
		fprintf(stderr, "make directory error\n");
	}

	return ret;
}

int remove_directory(int argc, char **argv)
{
	char *dirname;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dir_name>\n", argv[0]);
		return 1;
	}

	dirname = argv[1];

	ret = rmdir(dirname);
	if (ret != 0) {
		fprintf(stderr, "remove directory error\n");
	}

	return ret;
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