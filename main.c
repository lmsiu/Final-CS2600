//make a shell
int main(int argc, char **argv){
    //load any config files

    //runs command loop
    lsh_loop();

    //perform any shutdown/cleanup
    return EXIT_SUCCESS;

}