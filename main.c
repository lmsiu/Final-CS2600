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

char *lsh_read_line(){

}

char **lsh_split_line(){

}

int lsh_excecute(char **args){
    
}