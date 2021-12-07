//making a kilo text editor

//*** includes ***//
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

//definitions
//welcome msg
#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f) //define ctrl key for ctrl-q to quits

//*** data ***//
//struct to hold terminal state
struct editorConfig{
    int cx, cy; //x and y cordinates of cursor
    int screenrows;
    int screencolumns;
    struct termios orig_termios;
};


struct editorConfig E;

//*** terminal ***//
void die(const char *s){
    //clear screen on exit
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){
        die("tcsetattr");
    }
}

void enableRawMode(){

    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1){
        die("tcgetattr");
    }
    //tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    //turn off the echo from canonical mode
    struct termios raw = E.orig_termios;

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

//read basic keypress mapping and stop printing key presses
char editorReadKey(){
    int nread;
    char c;

    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) {
            die("read");
    }
    }
    return c;
}

//get cursor position
int getCursorPosition(int *rows, int *cols){
    char buff[32];
    unsigned int i = 0;
    
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4){
        return -1;
    }

    while(i < sizeof(buff) - 1){
        if(read(STDIN_FILENO, &buff[i], 1) != 1){
            break;
        }

        if(buff[i] == 'R'){
            break;
        }
        i++;
    }

    buff[i] = '\0';

    if(buff[0] != '\x1b' || buff[1] != '['){
        return -1;
    }

    if(sscanf(&buff[2], "%d;%d", rows, cols) != 2){
        return -1;
    }

    return 0;
    //printf("\r\n&buff[1]: '%s'\r\n", &buff[1]);
    /*
    printf("\r\n");
    char c;
    while(read(STDIN_FILENO, &c, 1) == 1){
        if(iscntrl(c)){
            printf("%d\r\n", c);
        }else{
            printf("%d ('c')\r\n", c, c);
        }
    }
    */

    //editorReadKey();

    //return -1;
}

//use ioctl to get size of terminal, returns -1 if fail
int getWindowsSize(int *rows, int *columns){
    struct winsize ws;

    //getting window size if unable to use ioctl on device
    //"\x1b[999C\x1b[999B" moves curser to bottom of the screen
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col==0){
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){
            return -1;
        }
        return getCursorPosition(rows, columns);
        //editorReadKey();
        //return -1;
    } else {
        *columns = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

//*** append buffer ***//
//buffers for rewriting and refreashing things
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len){
    char *new = realloc(ab->b, ab->len + len);

    if(new == NULL){
        return;
    }

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;

}

//destructor for append buffer
void abFree(struct abuf *ab){
    free(ab->b);
}

//input

void editorMoveCursor(char key){
    switch(key){
        case 'a':
        E.cx--;
        break;
        case 'd'
        E.cx++;
        break;
        case 'w':
        E.cy--;
        break;
        case 's':
        E.cy++;
        break;
    }
}

void editorProcessKeyPress(){
    char c = editorReadKey();

    switch(c){
        case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

        //use w, s, d, a to move cursor
        case 'w':
        case 's':
        case 'a':
        case 'd':
        editorMoveCursor(c);
        break;
    }
}

//*** output ***//

//make a column of ~
void editorDrawSquigleRows(struct abuf *ab){
    int y;
    for(y=0; y < E.screenrows; y++){
        if(y==E,screenrows/3){
        //load welcome msg
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
        if(welcomelen > E.screencols){
            welcomelen = E.screencols;
        }
        abAppend(ab, welcome, welcomelen);
        } else{
            abAppend(ab, "~", 1)
        }


        abAppend(ab, "~", 1);
        //write(STDOUT_FILENO, "~", 1);
        //make a column of ~
        //clear each line before redrawing them
        abAppend(ab, "\x1b[K", 3);
        if(y<E.screenrows - 1){
            abAppend(ab, "\r\n", 2);
        //write(STDOUT_FILENO, "\r\n", 2);
    }
    }
}

void editorRefreshScreen(){
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    //abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);

    // \x1b is the escape key(27 in decimal)
    //[ is part of the escape sequence
    //J clears screen
    //write(STDOUT_FILENO, "\x1b[2J", 4);
    //reposition curser to bottom left
    //can use <esc>[12;40H for a bigger size screen 80x24
    //write(STDOUT_FILENO, "\x1b[H", 3);

    editorDrawSquigleRows(&ab);

    //move cursor to E.cx and E.cy
    char buff[32];
    snprintf(buff, sizeof(buff), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buff, strlen(buff));

    //hide cursor when refreashing screen
    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}



//*** init ***//

//initialize all fields in E struct
void initEditor(){
    //set cursor to 0,0
    E.cx = 0;
    E.cy = 0;
    
    if(getWindowsSize(&E.screenrows, &E.screencolumns) == -1){
        die("getWindowSize");
    }
}

int main(){
    enableRawMode();
    initEditor();

    //read 1 character from the standaed input into c until there are no more bytes to read
    //return 0 at the end of the file
    //press q to quit when in canonical mode
    //canonical mode needs to have enter pressed to read the line, we want raw mode
    


    while(1){
        editorRefreshScreen();
        editorProcessKeyPress();
    }
    
    /*while(1){
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
        if(c == CTRL_KEY('q')){
            break;
        }
        
    }*/
    
    return 0;
    
}