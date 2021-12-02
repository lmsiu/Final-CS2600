//make a shell
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
char **lsh_split_line(){
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if(!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    //using the strtok lib to tokenize the string using whitespace delimeters
    token = strtok(line, LSH_TOK_DELIM)
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

int lsh_excecute(char **args){
    
}