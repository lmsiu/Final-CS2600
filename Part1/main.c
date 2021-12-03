//make a shell

//libs and stuff we will need
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//need to make some builtin shell commands
//builtin shell command declaration
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_num_bultins();

//func declarations
void lsh_loop();
char *lsh_read_line();
char **lsh_split_line(char *line);
int lsh_excecute(char **args);

int main(int argc, char **argv){
    //load any config files

    //runs command loop
    lsh_loop();

    //perform any shutdown/cleanup
    return EXIT_SUCCESS;

}

//lsh_loop reads from cmd line, seperates cmd string into a program and args, and excecutes the parsed command
void lsh_loop(){
    char *line;
    char **args;
    int status;

    do{
        printf("> ");
        //reads from the command line
        line = lsh_read_line();
        //parses line
        args = lsh_split_line(line);
        //excecutes the commands
        status = lsh_excecute(args);

        free(line);
        free(args);

    }while(status);
}

#define LSH_RL_BUFSIZE 1024
char *lsh_read_line(){
    //make a block and if user exceeds cmd space, reallocate space
    int bufsize = LSH_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    //if there are errors with the malloc
    if(!buffer){
        fprintf(stderr, "lsh: error with allocation\n");
        exit(EXIT_FAILURE);
    }

    //read from cmd line
    while(1){
        //read a char but it gets stored as an int
        c = getchar();

        //if EOF is hit, replace it with a null char and return
        if(c == EOF || c == '\n'){
            buffer[position] = '\0';
            return buffer;
        }else{
            buffer[position] = c;
        }

        position++;

        //if buffer is exceeded, reallocate
        if (position >= bufsize){
            bufsize += LSH_RL_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            //if there are errors with the malloc
             if(!buffer){
            fprintf(stderr, "lsh: error with allocation\n");
            exit(EXIT_FAILURE);
            }
        }

    }

}

/*
//lsh_read_line using getline()

char *lsh_read_line(){
    char *line = NULL;
    ssize_t bufsize = 0;

    if(getline(&line, &bufsize, stdin == -1)){
        if(feof(stdin)){
            exit(EXIT_SUCCESS);
        }else{
            perror("read line");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

*/

//parsing without using / or "" to seperate args
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char **lsh_split_line(char *line){
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if(!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    //using the strtok lib to tokenize the string using whitespace delimeters
    token = strtok(line, LSH_TOK_DELIM);
    while(token != NULL){
        tokens[position] = token;
        position++;

        if(position >= bufsize){
        bufsize += LSH_TOK_BUFSIZE;
        tokens = realloc(tokens, bufsize * sizeof(char*));
        if(!tokens) {
            fprintf(stderr, "lsh: alloc error\n");
            exit(EXIT_FAILURE);
            }
        }

    token = strtok(NULL, LSH_TOK_DELIM);
    }

    tokens[position] = NULL;
    return tokens;

}

//function to excecute but dif than lsh_excecute
int lsh_launch(char **args){
    pid_t pid, wpid;
    int status;

    //makes a copy of the current process running (a child)
    pid = fork();
    //after fork there are now 2 processes running, parnet and child
    if(pid == 0){
        //child process
        //uses execvp to replace the function
        if(execvp(args[0], args)){
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    //if fork() returns -1 there is an error
    }else if(pid < 0){
        //Error forking
        perror("lsh Forked it up!");
    } else{
        //parent process
        do{
            wpid = waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
    //returns 1 to prompt more user input
}

//List of built in commands followed by their funcs
char *builtin_str[] = { "cd", "help", "exit"};

int (*builtin_func[]) (char **) = {
    &lsh_cd, &lsh_help, &lsh_exit
};

//wrapper for lsh_launch and built ins
int lsh_excecute(char **args){
    int i;

    //if an empty command is entered
    if(args[0] == NULL){
        return 1;
    }

    for(i = 0; i < lsh_num_bultins(); i++){
        if(strcmp(args[0], builtin_str[i]) == 0){
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}

//builtin func implimentations
int lsh_cd(char **args){
    if(args[1] == NULL){
        fprintf(stderr, "lsh: Expected argument to \"cd\"\n");
    }else{
        if(chdir(args[1]) != 0){
            perror("lsh");
        }
    }

    return 1;
}

int lsh_num_bultins(){
    return sizeof(builtin_str) / sizeof(char *);
}

int lsh_help(char **args){
    int i;
    printf("Laura Siu's LSH\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for( i = 0; i < lsh_num_bultins(); i++){
        printf(" %s\n", builtin_str[i]);
    }

    printf("Please use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit(char **args){
    return 0; 
}