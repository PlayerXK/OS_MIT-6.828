#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

// Simplifed xv6 shell.

#define MAXARGS 10

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
  int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd {
  int type;              // ' '
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {
  int type;          // < or > 
  struct cmd *cmd;   // the command to be run (e.g., an execcmd)
  char *file;        // the input/output file
  int flags;         // flags for open() indicating read or write
  int fd;            // the file descriptor number to use for the file
};

struct pipecmd {
  int type;          // |
  struct cmd *left;  // left side of pipe
  struct cmd *right; // right side of pipe
};

int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2], r;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    _exit(0);
  
  switch(cmd->type){
  default:
    fprintf(stderr, "unknown runcmd\n");
    _exit(-1);

  case ' ':
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      _exit(0);
    //fprintf(stderr, "exec not implemented\n");
    // 执行命令,当前目录下找不到对应命令时,去Bin目录下找
	if(-1 == execv(ecmd->argv[0],ecmd->argv))
	{
		char path[20] = "/bin/";
		strcat(path,ecmd->argv[0]);
		if(-1 == execv(path,ecmd->argv))
		{	
			char path1[20] = "/usr";
			strcat(path1,path);
			if(-1 == execv(path1,ecmd->argv))
			{
				fprintf(stderr,"%s : command can not found\n", ecmd->argv[0]);
			}
		}
	}
	
    break;

  case '>':
  case '<':
    rcmd = (struct redircmd*)cmd;
    ///重定向时,先关闭默认文件描述符,再打开目标文件即可,因为会为打开的目标文件分配未被使用的最小文件描述符
	close(rcmd->fd);
	if(open(rcmd->file,rcmd->flags,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0)
	{
		fprintf(stderr, "file %s can not open\n", rcmd->file);
		 _exit(0);
	}
    runcmd(rcmd->cmd);
    break;

  case '|':
    pcmd = (struct pipecmd*)cmd;
	///fprintf(stderr, "pipe not implemented\n");
    // Your code here ...
	pipe(p); ///构建无名管道pipe
	///创建两个子进程执行命令
	int cid = fork1();
	if(cid == 0)
	{
		close(1); ///重定向标准输出到p[1]
		dup(p[1]); //dup将p[1]复制到当前未打开的最小文件描述符即1,此时对文件描述符1操作即对管道写端进行操作
		close(p[0]); 
		close(p[1]);
		//fprintf(stderr, "1.run cmd\n");
		runcmd(pcmd->left); 
	}
	else
	{
		//fprintf(stderr,"1.child process  id == %d\n",cid);
	}
	wait(&r);
	cid = fork1();
	if(cid == 0) ///创建子进程读取上一条命令的输出作为输入
	{
		close(0);
		dup(p[0]);//dup将p[0]复制到当前未打开的最小文件描述符即0,此时对文件描述符0操作即对管道读端进行操作
		close(p[0]);
		close(p[1]);
		//fprintf(stderr,"2.run cmd\n");
		runcmd(pcmd->right); 
	}
	else
	{
		//fprintf(stderr,"2.cid == %d\n", cid);
	}
	close(p[0]);
	close(p[1]);
	wait(&r);
	//fprintf(stderr,"child process end\n");
	///管道使用完毕后关闭
	
    break;
  }    
  _exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  if (isatty(fileno(stdin)))
    fprintf(stdout, "6.828$ ");
  memset(buf, 0, nbuf);
  if(fgets(buf, nbuf, stdin) == 0)
    return -1; // EOF
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd, r;

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Clumsy but will have to do for now.
      // Chdir has no effect on the parent if run in the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(stderr, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait(&r);
  }
  exit(0);
}

int
fork1(void)
{
  int pid;
  
  pid = fork();
  if(pid == -1)
    perror("fork");
  return pid;
}

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = ' ';
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, int type)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->flags = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC; ///重定位符号为'<'时,打开标志为只读,为'>'时，打开标志为只写|创建|截短    echo "6.828 is cool" > x.txt cat < x.txt
  cmd->fd = (type == '<') ? 0 : 1; 	/// 源文件默认为标准输入和标准输出，重定向时,首先应关闭默认文件描述符,再打开目标文件
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = '|';
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s)) ///去除前面各种空字符
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0: ///第一个非空字符为0时,直接跳出
    break;
  case '|':///为'|','<','>'时，指针后移一个位置,然后跳出
  case '<':
    s++;
    break;
  case '>':
    s++;
    break;
  default: ///非上述情况时,指针后移到第一个空格字符或者符号字符处,返回值为'a'
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)) ///当前字符不是空格也不是"<|>"符号时,指针往后走
      s++;
    break;
  }
  if(eq)
    *eq = s;
  
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s; ///重定位ps到符号出现的地方,找到要重定向的文件
  return ret;
}

int
peek(char **ps, char *es, char *toks)///在*ps-es范围内查找是否含有tokes中的字符,没有返回0,有则返回值大于0
{
  char *s;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s)) 
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char 
*mkcopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s); ///定位到命令末尾
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsepipe(ps, es); ///ps是命令字符串指针存储的地址,es指向命令的末尾
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){ ///构建redircmd型命令
    case '<':
      cmd = redircmd(cmd, mkcopy(q, eq), '<');
      break;
    case '>':
      cmd = redircmd(cmd, mkcopy(q, eq), '>');
      break;
    }
  }
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  
  ret = execcmd(); ///动态构建一个execcmd类型,为其分配内存空间并初始化相关数据,并返回分配后的内存空间起始地址
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    cmd->argv[argc] = mkcopy(q, eq);
    argc++;
    if(argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  return ret;
}
