//making a kilo text editor

//*** includes ***//
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>

//definitions
//welcome msg
#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f) //define ctrl key for ctrl-q to quits
#define KILO_TAB_STOP 8 //tab spaces

enum editorKey{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN, 
    HOME_KEY, 
    END_KEY,
    DEL_KEY,
    PAGE_UP, 
    PAGE_DOWN
};

//*** data ***//

//struct for rows in the text editor
typedef struct erow{
    int size;
    int rsize; //for nonprintable chars like tab
    char *chars;
    char *render; //holds value to print for nonprintable chars
} erow;

//struct to hold terminal state
struct editorConfig{
    int cx, cy; //x and y cordinates of cursor
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencolumns;
    int numrows;
    erow *row;
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
int editorReadKey(){
    int nread;
    char c;

    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) {
            die("read");
    }
    }

    if( c == '\x1b'){
    char seq[3];
    //read arrows
    if(read(STDIN_FILENO, &seq[0], 1) != 1){
        return '\x1b';
    }
    if(read(STDIN_FILENO, &seq[1], 1) != 1){
        return '\x1b';
    }

    if(seq[0] == '['){
        if(seq[1] >= '0' && seq[1] <='9'){
            if(read(STDIN_FILENO, &seq[2], 1) != 1){
                return '\x1b';
            }
            if(seq[2]=='~'){
                switch(seq[1]){
                    case '1': return HOME_KEY;
                    case '3': return DEL_KEY;
                    case '4': return END_KEY;
                    case '5': return PAGE_UP;
                    case '6': return PAGE_DOWN;
                    case '7': return HOME_KEY;
                    case '8': return END_KEY;
                }
            }
        }else{
        switch(seq[1]){
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
        }
    }
    }else if(seq[0] == 'O'){
        switch(seq[1]){
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
        }

    }

    return '\x1b';
    }else{
    return c;
    }
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

//*** row ops ***//

//calculate value of E.rx
int editorRowCxToRx(erow *row, int cx){
    int rx = 0;
    int j;

    for(j = 0; j < cx; j++){
        if(row->chars[j] == '\t'){
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        }
        rx++;
    }

    return rx;
}

//fill in render string
void editorUpdateRow(erow *row){
    int tabs = 0;
    int j;
    for(j = 0; j<row->size; j++){
        if(row->chars[j] == '\t'){
            tabs++;
        }
    }

    free(row->render);
    row->render = malloc(row->size + tabs*(KILO_TAB_STOP -1) + 1);

    int idx = 0;
    for(j = 0; j < row->size; j++){
        if(row->chars[j] == '\t'){
            row->render[idx++] = ' ';
            while(idx % KILO_TAB_STOP != 0){
                row->render[idx++] = ' ';
            }
        }else{
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char *s, size_t len){
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.numrows++;

    

}


//*** file i/o ***//

//allow users to open files
void editorOpen(char *filename){
    FILE *fp = fopen(filename, "r");
    if(!fp){
        die("fopen");
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    //read all of file into E.row
    while ((linelen = getline(&line, &linecap, fp)) != -1){
    //if(linelen != -1){
        while(linelen > 0 && (line[linelen-1] == '\n' || line[linelen] == '\r')){
            linelen--;
        }
        editorAppendRow(line, linelen);
    

    
    }

    free(line);
    fclose(fp);
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

void editorMoveCursor(int key){
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch(key){
        case ARROW_LEFT:
            if(E.cx != 0){
            E.cx--;
            }else if(E.cy > 0){
                //move to prev line if at end
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size){
            E.cx++;
            }else if(row && E.cx == row->size){
                //move to next line if at the end of a line
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if(E.cy != 0){
            E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy < E.numrows){
            E.cy++;
            }
            break;
    }

    //snap cursor to end of line
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if(E.cx > rowlen){
        E.cx = rowlen;
    }
}

void editorProcessKeyPress(){
    int c = editorReadKey();

    switch(c){
        case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

        case HOME_KEY:
            E.cx = 0;
            break;

        case END_KEY:
            E.cx = E.screencolumns - 1;
            break;

        case PAGE_UP:
        case PAGE_DOWN:
        {
            int times = E.screenrows;
            while(times--){
                editorMoveCursor(c==PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }

        //use w, s, d, a to move cursor
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

//*** output ***//

void editorScroll(){
    E.rx = 0;

    if(E.cy < E.numrows){
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    if(E.cy < E.rowoff){
        E.rowoff = E.cy;
    }
    if(E.cy >= E.rowoff + E.screenrows){
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if(E.cx < E.coloff){
        E.coloff = E.rx;
    }
    if(E.cx >= E.coloff + E.screencolumns){
        E.coloff = E.rx - E.screencolumns + 1;
    }
}

//make a column of ~
void editorDrawRows(struct abuf *ab){
    int y;
    for(y=0; y < E.screenrows; y++){
        int filerow = y + E.rowoff;

        if(filerow >= E.numrows){
        if(E.numrows == 0 && y==E.screenrows/3){
        //load welcome msg
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
        if(welcomelen > E.screencolumns){
            welcomelen = E.screencolumns;
        }

        int padding = (E.screencolumns - welcomelen) /2;
        if(padding){
            abAppend(ab, "~", 1);
            padding--;
        }

        while(padding--){
            abAppend(ab, " ", 1);
        }
        abAppend(ab, welcome, welcomelen);
        } else{
            abAppend(ab, "~", 1);
        }
        }else {
            int len = E.row[filerow].rsize - E.coloff;
            if(len < 0){
                len = 0;
            }
            if(len > E.screencolumns){
                len = E.screencolumns;
            }
            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }


        //abAppend(ab, "~", 1);
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
    editorScroll();

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

    editorDrawRows(&ab);

    //move cursor to E.cx and E.cy
    char buff[32];
    snprintf(buff, sizeof(buff), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buff, strlen(buff));

    //hide cursor when refreashing screen
    //abAppend(&ab, "\x1b[H", 3);
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
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    //set rows to 0 at first
    E.numrows = 0;
    E.row = NULL;
    
    if(getWindowsSize(&E.screenrows, &E.screencolumns) == -1){
        die("getWindowSize");
    }
}

int main(int argc, char *argv[]){
    enableRawMode();
    initEditor();
    if(argc >= 2){
        editorOpen(argv[1]);
    }

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