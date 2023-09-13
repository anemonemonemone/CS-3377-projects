//CS3377.505 Spring 2023
//Project 1 - Simple Shell
//Chelsea Chourp & Wei Liew


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

//Constants used for max arguements and history respectively
#define maxArg 20 
#define maxHistory 100

//Variables for history
char **archive;
int historySize = 0;

//Funct prototype
void sishell();
void addToHist(char*);
int tokenize(char**, char*[maxArg]);
int tokenizePipe(char**, char*[maxArg]);
void exec(char*[maxArg]);
void execPipe(char*[maxArg],int);
void cd(char*);
void history(char* arg);


//Program
//main clears screen + calls shell
int main(int argc, char **argv)
{
	system("clear");
	sishell();
	
	return EXIT_SUCCESS;
}

//Actual Shell
void sishell()
{
	//Initialization variables
	char *line = NULL;
	size_t length = 0;
	char *args[maxArg];
	int loop = 1;

	//create history
	archive = malloc(maxHistory*sizeof(char*));

	while(loop) {
		printf("sish> "); //prompt w/the space

		//read input
		getline(&line,&length,stdin);
		line[strlen(line)-1]='\0';

    //loops back if only input is enter
		if (strcmp(line, "\0") == 0){
      continue;
    }
	
		//archives input and increases history size if within history max
		addToHist(line);
		if(historySize != maxHistory-1)
			historySize++;

    //Exit command
		if(strcmp(line, "exit") == 0){
      loop = 0;
      return;
    }
		
		//If given input does not need piping to execute
		if(strchr(line, '|') == NULL){
		//tokenize input and execute it
			tokenize(&line,args);
			exec(args);
		}
    //If given input needs piping to execute
    else{
      int commandCount = tokenizePipe(&line, args);
  		execPipe(args,commandCount);
    }
	} 
}

//Archives input onto history
void addToHist(char *line)
{
	int size = historySize;
	
	//If history is already at max then the for-loop deletes
	//the oldest archived line and shifts all the archives over by one to make
	//room for the most recent command
  //Maybe a little inefficent? Can change
	if(size == maxHistory - 1){
		for(int i = 0; i < maxHistory -1; i++){
			archive[i] = archive[i+1];
		}
		size--;
	}
	
	//Otherwise space is allocated for the most recent entry
	archive[size] = malloc((strlen(line)+1) * sizeof(char));
	strcpy(archive[size],line);
	return;
}

//Break line up to tokens
int tokenize(char** line, char* args[maxArg])
{
    char* token;
    char *point;
    int count = 1;
    
    //First element
    args[0] = strtok_r(*line," ",&point);
    
    //White space seperates tokens
    token = strtok_r(NULL," ",&point);    
    while(token != NULL){
   	 args[count++] = token;
   	 token = strtok_r(NULL," ",&point);
    }

    //Last element NULL!!
    args[count] = NULL;

    return 1;
}

//break line up to tokens if theres pipes
int tokenizePipe(char** line, char* args[maxArg])
{
	char* saveptr1;
  int commandCount = 0;	//keeps track of how many commands within input line
  
  char* command = strtok_r(*line,"|", &saveptr1);	//tokenizes if piping is involved
  while(command != NULL)
  {
  	args[commandCount] = command;
  	commandCount++;
  	command = strtok_r(NULL, "|", &saveptr1);
  }

  //Last element NULL
  args[commandCount] = NULL;
  return commandCount;
}

//Execute commands
void exec(char*args[maxArg])
{
	//Built in? Check first element (0) then executes the command located in second element (1)
  if(strcmp(args[0], "cd") == 0){
    cd(args[1]);
    return;
  }
  if(strcmp(args[0], "history") == 0){
    history(args[1]);
    return;
  }
    
	//If not built in, built in fork to run process? pid parent id not process id
	else{
	 int pid = fork();
  
    //Error handling
	 if(pid < 0){
     perror("Error creating child process. \n");
     exit(EXIT_FAILURE);
   }
	 
   if(pid == 0){
       execvp(args[0], args);
       perror("Invalid input \n");
       exit(EXIT_FAILURE);
   }
	 else{
       //Included <sys/wait.h> for the command to process
       wait(NULL);
       return;
     }
  }
	return;
}

void execPipe(char* args[maxArg], int commandCount){
  int prev_fd[2] = {STDIN_FILENO, STDOUT_FILENO};
  int fd[2];

  for(int i = 0; i < commandCount; i++){
    //splits a given command according to " "
    char* line = args[i];
    char* command[strlen(line)];
    tokenize(&line, command);
    
    if (i < commandCount -1){
      if(pipe(fd) == -1){
        perror("Pipe failure.");
        exit(EXIT_FAILURE);
      }
      prev_fd[1] = fd[1];
    }
    else{
      prev_fd[1] = STDOUT_FILENO;
    }
    
      
    //fork a child process to execute command
    pid_t cpid = fork();
    //error handling
    if(cpid == -1){
      perror("Fork failure.");
      exit(EXIT_FAILURE);
    }

    //child process
    if(cpid == 0){
      if(prev_fd[0] != STDIN_FILENO){
        dup2(prev_fd[0],STDIN_FILENO);
        close(prev_fd[0]);
      }
      if(prev_fd[1] != STDOUT_FILENO){
        dup2(prev_fd[1], STDOUT_FILENO);
        close(prev_fd[1]);
      }
      int status = execvp(command[0],command);
      if (status == -1){
        perror("Execution failed.");
        exit(EXIT_FAILURE);
      }
    }

    //parent process
    else{
     //wait for child to complete
      wait(NULL);
      //update previous file decriptors
    }
    if (i < commandCount-1){
      close(fd[1]);
      prev_fd[0] = fd[0];
    }
  }
}


//Part 2: exit, history, cd
//History built-in command
void history(char* arg)
{
	int *size = &historySize;

	//-c, clear hist one by one, set archive size to 0
	if(arg != NULL && strcmp(arg,"-c") == 0){
		for(int i = 0; i<*size; i++){	
			archive[i] = NULL;
		}

		*size = 0;
    printf("History cleared. \n");
		return;
	}

	//Otherwise just print out last 100
	if(arg == NULL){
    printf("History: \n");
		for(int i = 0; i <*size; i++){
      printf(" %d %s \n", i, archive[i]);
		}
	}

  //For specific instance, executes line, otherwise prints error message
  else
	{
    //First, error message generated when out of used history bounds  
    //atoi -> arg to int
    int index = atoi(arg);

    if((strcmp(arg,"0") != 0 && index == 0) || (index >= *size-1))
			printf("Invalid index. \n");
    else{
      char* line;
      char* arg[maxArg];

      //If everythings good to go then it executes the command
      line = malloc((strlen(archive[index])+1));
      strcpy(line, archive[index]);

      if(strchr(line, '|') == NULL){
        tokenize(&line,arg);
        exec(arg);
       }
      
    }
	}
}

//cd built in command
void cd(char* directory)
{
  //cd is built in, if no such directory exists then error message.
  if(chdir(directory) < 0)
    perror("Directory not found. \n");    
  return;
}
