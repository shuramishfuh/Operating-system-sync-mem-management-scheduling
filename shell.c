#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <sys/types.h>

int args_count;

//built in commands foward declarations
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_print(char **args);
int lsh_ls(char **args);

//list of built in commands and functions
char *builtin_str[] = {
	"cd",
	"help",
	"exit",
	"print",
	"ls"
};

int (*builtin_func[]) (char **) = {
	&lsh_cd,
	&lsh_help,
	&lsh_exit,
	&lsh_print,
	&lsh_ls
};

//returns number of built in commands
int lsh_num_builtins()
{
	return (sizeof(builtin_str)) / (sizeof(char *));
}

//built in implementation for "print"
int lsh_print(char **args)
{
	pid_t pid; 
	pid = getpid();
	
	printf("PID: %lu\n", (unsigned long)pid);
		
	return 1;
}

//built in implentation for "cd"
int lsh_cd(char **args)
{
	//checks if the argument exists
	if (args[1] == NULL)
	{
		fprintf(stderr, "lsh: expected argument to \"cd\"\n");
	}
	
	else
	{	
		//checks for errors
		if (chdir(args[1]) != 0)
		{
			perror("lsh");
		}
	}
	
	return 1;
}

//built in implementation for "ls"
int lsh_ls(char **args)
{
	DIR* currdir;
	struct dirent* pdir;
	
	if ((currdir = opendir(args[1])) == NULL)
	{
		perror("dir error: ");
		exit(EXIT_FAILURE);
	}
	
	else
	{
		while ((pdir = readdir(currdir)) != NULL)
		{
			printf("%s\n", pdir->d_name);
		}
	}
	
	return 1;
}

//built in implementation for "help"
int lsh_help(char **args)
{
	int i;
	
	printf("Type command name the arguments then hit enter.\n");
	printf("Use the following commands: \n");
	
	//loops until all built in commands are printed
	for (i = 0; i < lsh_num_builtins(); i++)
	{
		printf(" %s\n", builtin_str[i]);
	}
	
	return 1;
}

//built in implementation for "exit"
int lsh_exit(char **args)
{
	//returns 0 to terminate lsh_loop
	return 0;
}

#define LSH_RL_BUFFSIZE 1024
char *lsh_read_line(void)
{
	int position = 0;
	int c;
	
	int buffsize = LSH_RL_BUFFSIZE;
	char *buffer = malloc(sizeof(char) * buffsize);
	
	if (!buffer)
	{
		fprintf(stderr, "lsh: allocation error\n");
		exit(EXIT_FAILURE);
	}
	
	//loops until line ends or an error is caught
	while (1)
	{
		//gets char from input stream and checks if its blank or EOF
		//if not enter the char c to buffer[]
		c = getchar();
		
		if (c == EOF || c == '\n')
		{
			buffer[position] = '\0';
			return buffer;
		}
		
		else
		{
			buffer[position] = c;
		}
		
		position++;
		
		//reallocate memory and increase the size of buffer if the line is too long
		if (position >= buffsize)
		{
			buffsize += LSH_RL_BUFFSIZE;
			buffer = realloc(buffer, buffsize);
			
			if (!buffer)
			{
				fprintf(stderr, "lsh: allocation error \n");
				exit(EXIT_FAILURE);
			}
		}
	}
}

#define LSH_TOK_BUFFSIZE 100
#define LSH_TOK_DELIM " \t\r\n\a&"
char **lsh_split_line(char *line)
{
	int position = 0;
	
	int buffsize = LSH_TOK_BUFFSIZE;
	char **tokens = malloc(buffsize * sizeof(char*));
	char *token;
	
	if (!tokens)
	{
		fprintf(stderr, "lsh: allocation error\n");
		exit(EXIT_FAILURE);
	}
	
	//returns a pointer to the first token
	token = strtok(line, LSH_TOK_DELIM);
	
	//store char pointers in buffer "tokens"
	while (token != NULL)
	{
		tokens[position] = token;
		position++;
		
		args_count++;
		
		//reallocate memory and increase the size of buffer if the line is too long
		if (position >= buffsize)
		{
			buffsize += LSH_TOK_BUFFSIZE;
			tokens = realloc(tokens, buffsize * sizeof(char));
			
			if (!tokens)
			{
				fprintf(stderr, "lsh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}

		token = strtok(NULL, LSH_TOK_DELIM);
	}
	//set the final position in tokens to NULL
	tokens[position] = NULL;
	
	return tokens;
}

void lsh_pipe_check(char **args)
{
	if (args_count > 2)
	{
		//look for instance of ' | ' in args, create fd[] if there is
		if (strcmp(args[1], "|") == 0)
		{
			int fd[2];
		
			//check for pipe error
			if ((pipe(fd)) < 0)
			{
				perror("pipe error: \n");
			}
		
			int pid = fork();
	
			//checks for fork error
			if (pid < 0)
			{
				perror("fork error: \n");
			}
		
			//child process 
			if (pid == 0)
			{
				dup2(fd[1], STDOUT_FILENO);
			
				//clear fd[]
				close(fd[0]);
				close(fd[1]);
		
				//execute command located at args[0]
				execlp(args[0], args[0], NULL);
			}
		
			//parent process
			else
			{
			pid = fork();
				
				if (pid == 0)
				{
					dup2(fd[0], STDIN_FILENO);
				
					//clear fd[]
					close(fd[1]);
					close(fd[0]);
				
					//execute command located at args[2]
					execlp(args[2], args[2], NULL);
				}
			
				else
				{
					int tmp;
					
					//clear fd[]
					close(fd[0]);
					close(fd[1]);
					
					//parent waits
					waitpid(pid, &tmp, 0);
				}
			}
		}		
		
		//look for instance of '&' in args, call args[0] and args[2] if there is
		else if (strcmp(args[1], "&") == 0)
		{
			system(args[0]);
			system(args[2]);
		}
	}
	
	//reset args_count
	args_count = 0;
}


int lsh_launch(char **args)
{
	pid_t pid;
	pid_t wpid;
	
	int status;
	
	//forks a new process
	pid = fork();
	
	if (pid == 0)
	{
		//print error if exec fails
		if (execvp(args[0], args) == -1)
		{
			perror("lsh");
		}
		exit(EXIT_FAILURE);
	}
	
	//if there's an error creating the fork print error
	else if (pid < 0)
	{
		perror("lsh");
	}
	
	//fork executed sucessfully
	else
	{
		//wait for the processes to complete
		do {
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
	
	//signals the processes are complete and input can be requested again
	return 1;
}

int lsh_execute(char **args)
{
	int i;
	
	//check for an empty argument
	if (args[0] == NULL)
	{
		return 1;
	}
	
	//checks if the argument is a built in command
	for (i = 0; i < lsh_num_builtins(); i++)
	{
		if (strcmp(args[0], builtin_str[i]) == 0)
		{
			return (*builtin_func[i])(args);
		}
	}
	
	//calls lsh_launch if argument is not a built in command
	return lsh_launch(args);
}
	
void lsh_loop(void)
{
	char *line;
	char **args;
	int status;
	
	//read, parse, and execute the command
	do {
		printf("$");
		line = lsh_read_line();
		args = lsh_split_line(line);
		
		if(args_count <= 2)
		{
			status = lsh_execute(args);
			args_count = 0;
		}
		
		else if(args[1] == "|" || "&")
		{
			lsh_pipe_check(args);
		}
		
		free(line);
		free(args);
	} while (status);
}

int main(int argc, char **argv)
{
    lsh_loop();

    return EXIT_SUCCESS;
}

