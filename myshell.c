#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


/* 컴파일 옵션 매크로 정의 */
//#define HW_STAGE1


/* 상수 정의 */
#define MAXLINE		1024
#define MAXARGS		128
#define MAXPATH		1024
#define MAXTHREAD	10

#define DEFAULT_FILE_MODE	0664
#define DEFAULT_DIR_MODE	0775


/* 전역 변수 정의 */
char prompt[] = "myshell> ";
const char delim[] = " \t\n";
int bg_flag, pipe_flag, rd_flag;
char *pargv[MAXARGS];		// argv for pipe processing
char *rd_filename;			// filename for redirection
char src_path[MAXTHREAD][MAXPATH];	// pathname for directory copy
char dst_path[MAXTHREAD][MAXPATH];	// pathname for directory copy


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
int copy_directory(int argc, char **argv);
void *dcp_thr_fn(void *thr_num);



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
	int argc, status, ret;
	char *argv[MAXARGS];
#ifndef HW_STAGE1
	pid_t pid, pipe_pid;
	int pipefd[2], rd_fd, saved_stdout;
#endif

	// 명령 라인을 해석하여 인자 (argument) 배열로 변환한다.
	argc = parse_line(cmdline, argv);
	if (argc == 0) {
		// 종료된 background 프로세스를 wait하고 리턴한다.
		ret = waitpid(-1, &status, WNOHANG);
		if (ret > 0) {
			printf("PID %d is terminated.\n", ret);
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

	// pipe flag가 설정되어 있으면 파이프 생성
	if (pipe_flag) {
		if (pipe(pipefd) == -1) {
			fprintf(stderr,"pipe error\n");
			return;
		}
	}

	// redirection flag (stdout) 처리를 위한 파일 열기
	if (rd_flag) {
		if (!rd_filename || 
			((rd_fd = creat(rd_filename, DEFAULT_FILE_MODE)) < 0)) {
			fprintf(stderr,"redirection file creation error\n");
			return;
		}

		// stdout fd를 저장
		saved_stdout = dup(STDOUT_FILENO);

		// stdout 으로 복제 (duplication)
		if (dup2(rd_fd, STDOUT_FILENO) < 0) {
			fprintf(stderr, "redirection fd duplication error\n");
			return;
		}
		close(rd_fd);
	}

	/* 내장 명령 처리 함수를 수행한다. */
	if (builtin_cmd(argc, argv) == 0) {
		// redirection 되었던 stdout fd를 복구
		if (rd_flag) {
			dup2(saved_stdout, STDOUT_FILENO);
		}

		// 종료된 background 프로세스를 wait하고 리턴한다.
		ret = waitpid(-1, &status, WNOHANG);
		if (ret > 0) {
			printf("PID %d is terminated.\n", ret);
		}
		return;
	}


	/*
	 * 자식 프로세스를 생성하여 프로그램을 실행한다.
	 */

	// 자식 프로세스 생성
	if ((pid = fork()) < 0) {
		fprintf(stderr, "fork error\n");
		return;
	}	
	
	/*
	 * 자식 프로세스 (Child  process)
	 */
	if (pid == 0) {
		// 파이프의 첫번째 프로그램을 위한 파이프 설정
		if (pipe_flag) {
			// 파이프의 read를 닫는다.
			close(pipefd[0]);

			// 파이프 출력을 stdout으로 복제
			if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
				fprintf(stderr, "pipefd duplication error\n");
				exit(1);
			}
		}

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
			// 파이프의 write를 닫는다.
			close(pipefd[1]);

			// 파이프 입력을 stdin으로 복제
			if (dup2(pipefd[0], STDIN_FILENO) < 0) {
				fprintf(stderr, "pipefd duplication error\n");
				exit(1);
			}	

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

	// redirection 되었던 stdout fd를 복구
	if (rd_flag) {
		dup2(saved_stdout, STDOUT_FILENO);
	}

	// foreground 실행이면 자식 프로세스가 종료할 때까지 기다린다.
	if (!bg_flag) {
		ret = waitpid(pid, &status, 0);
		if (ret < 0) {
			fprintf(stderr, "wait error\n");
			return;
		}

		// 파이프 실행이면 두번째 자식 프로세스를 기다린다.
		if (pipe_flag) {
			ret = waitpid(pipe_pid, &status, 0);
			if (ret < 0) {
				fprintf(stderr, "wait error (pipe)\n");
				return;
			}
		}
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

	// background, pipe, redirection flag를 초기화
	bg_flag = pipe_flag = rd_flag = 0;

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
		// redirection flag 검사, 파일 이름 획득하고 종료
		else if (!strcmp(targv[targc], ">")) {
			rd_flag = 1;
			rd_filename = strtok(NULL, delim);
			if (!rd_filename) {
				return targc;
			}
			targv[targc--] = NULL;
		}
		// background flag 검사
		else if (!strcmp(targv[targc], "&")) {
			targv[targc] = NULL;
			bg_flag = 1;
			break;
		}

		tok = strtok(NULL, delim);
		targv[++targc] = tok;
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

	if (!strcmp(cmd, "dcp")) {
		copy_directory(argc, argv);
		return 0;
	}

	// 내장 명령어가 아님.
	return 1;
}


/*
 * 
 * 내장 명령 처리 함수들
 * argc, argv를 인자로 받는다.
 * 
 */
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


int copy_directory(int argc, char **argv)
{
	char *src_dirname, *dst_dirname;
	DIR *dp, *tmp_dp;
	struct dirent *d_entry;
	struct stat statbuf;
	pthread_t tid[MAXTHREAD];
	int i, ret;

	// 명령 인자 개수를 확인
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <src_dir> <dst_dir>\n", argv[0]);
		return 1;
	}
	src_dirname = argv[1];
	dst_dirname = argv[2];

	// source 디렉터리 열기
	dp = opendir(src_dirname);
	if (dp == NULL) {
		fprintf(stderr, "directory <%s> open error\n", src_dirname);
		return 1;
	}

	// destination 디렉터리 열기
	tmp_dp = opendir(dst_dirname);
	if (tmp_dp == NULL) {
		// destination 디렉터리 생성
		ret = mkdir(dst_dirname, DEFAULT_DIR_MODE);
		if (ret != 0) {
			fprintf(stderr, "directory <%s> creation error\n", dst_dirname);
			closedir(dp);
			return 1;
		}
	} else {
		closedir(tmp_dp);
	}

	// 스레드를 생성하여 모든 파일 복사
	i = 0;	// 스레드 번호 초기화
	d_entry = readdir(dp);
	while (d_entry != NULL) {
		// 소스 파일 확인
		sprintf(src_path[i], "%s/%s", src_dirname, d_entry->d_name);
		if (stat(src_path[i], &statbuf)) {
			fprintf(stderr, "file (%s) access error\n", src_path[i]);

			// 다음 파일 이름 읽기
			d_entry = readdir(dp);
			continue;
		}

		// 디렉터리는 무시
		if (S_ISDIR(statbuf.st_mode)) {
			// 다음 파일 이름 읽기
			d_entry = readdir(dp);
			continue;
		}

		// destination 경로 이름 완성
		sprintf(dst_path[i], "%s/%s", dst_dirname, d_entry->d_name);

		// 파일 복사 스레드 생성
		ret = pthread_create(&tid[i], NULL, dcp_thr_fn, (void *)i);
		if (ret != 0) {
			fprintf(stderr, "thread creation error\n");

			// 다음 파일 이름 읽기
			d_entry = readdir(dp);
			continue;
		}

		// MAXTHREAD 이면, 종료 기다림
		if (i == (MAXTHREAD - 1)) {
			for (i = 0; i < MAXTHREAD; i++) {
				ret = pthread_join(tid[i], (void *)NULL);
				if (ret != 0) {
					fprintf(stderr, "thread join error\n");
				}
			}
			// thread 번호 초기화
			i = 0;
		} else {
			// thread 번호 증가
			i++;
		}

		// 다음 파일 이름 읽기
		d_entry = readdir(dp);
	}

	// 마지막 스레드를 기다림
	if (i != 0) {
		for (i--; i >= 0; i--) {
			ret = pthread_join(tid[i], (void *)NULL);
			if (ret != 0) {
				fprintf(stderr, "thread join error\n");
			}
		}
	}

	closedir(dp);

	return 0;
}


void *dcp_thr_fn(void *thr_num)
{
	char *in_file, *out_file;
	FILE *in, *out;	
	int c;

	printf("Thread[%d]: copy \"%s\" into \"%s\"\n", (int)thr_num, 
			src_path[(int)thr_num], dst_path[(int)thr_num]);

	in_file = src_path[(int)thr_num];
	out_file = dst_path[(int)thr_num];

	if ( (in = fopen(in_file,"r")) == NULL) {
		fprintf(stderr,"Cannot open %s for reading\n",in_file);
		pthread_exit(0);
	}
	if ( (out = fopen(out_file,"w")) == NULL) {
		fprintf(stderr,"Cannot open %s for writing\n",out_file);
		pthread_exit(0);
	}

	while ( (c = getc(in)) != EOF)
		putc(c,out);

	fclose(in);
	fclose(out);

	pthread_exit(0);
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
