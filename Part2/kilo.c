//making a kilo text editor

//*** includes ***//
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>


//*** data ***//
struct termios orig_termios;

//*** terminal ***//
void die(const char *s){
    perror(s);
    exit(1);
}

void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1){
        die("tcsetattr");
    }
}

void enableRawMode(){

    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1){
        die("tcgetattr");
    }
    //tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    //turn off the echo from canonical mode
    struct termios raw = orig_termios;

    //tcgetattr(STDIN_FILENO, &raw);

    //disable ctrl-s and ctrl-q and fix ctrl-m to show 13 instead of 10 and misc. other ones
    raw.c_iflag &= ~(BRKINT| ICRNL | INPCK| ISTRIP | IXON);
    //turn off output processing
    raw.c_oflag &= ~(OPOST);
    //turn off some misc. flags
    raw.c_cflag |= (CS8);
    //ISIG turns off Ctrl-C and Ctrl-Z
    //IEXTEN disable Ctrl-V
    //ICANON turns off canonical mode so we can read input byte by byte instead of line by line
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    //help read() return after a set amount of time
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){
        die("tcsetattr");
    }
    //tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


//*** init ***//
int main(){
    enableRawMode();

    //read 1 character from the standaed input into c until there are no more bytes to read
    //return 0 at the end of the file
    //press q to quit when in canonical mode
    //canonical mode needs to have enter pressed to read the line, we want raw mode
    while(1){
        char c = '\0';
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN){
            die("read");
        }
        //read(STDIN_FILENO, &c, 1);
        //iscntrl tests if a char is a control char
        if(iscntrl(c)){
            printf("%d\r\n", c);
        }else{
            printf("%d ('%c')\r\n", c, c);
        }
        if(c == 'q'){
            break;
        }
        
    }
    
    return 0;
    
}