//some new text here from the program running!
//making a kilo text editor for fun! 

//this is a line that was added

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
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

//definitions
//welcome msg
#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f) //define ctrl key for ctrl-q to quits
#define KILO_TAB_STOP 8 //tab spaces
#define KILO_QUIT_TIMES 3 //user must press ctrl-q 3 times to quit w/o saving

enum editorKey{
    BACKSPACE = 127, //define specail char so it doesn't get "typed" when text editing
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

/* this is a comment */


enum editorHighlight{
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT, //multiline comments
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER, 
    HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

//*** data ***//

struct editorSyntax{
    //for adding color to file types
    char *filetype;
    char **filematch;
    char **keywords;
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    int flags;
};

//struct for rows in the text editor
typedef struct erow{
    int idx;
    int size;
    int rsize; //for nonprintable chars like tab
    char *chars;
    char *render; //holds value to print for nonprintable chars
    unsigned char *hl; //portion of text to highlight
    int hl_open_comment;
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
    int dirty; //if the user has modified file w/o saving
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;
    struct termios orig_termios;
};


struct editorConfig E;

//*** file types ***//

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {"switch", "if", "while", "for", "break", "continue", "return", "else", "struct", "union", "typedef", "static", "enum", "class", "case", "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL};

struct editorSyntax HLDB[] = {
    {
    "c", 
    C_HL_extensions,
    C_HL_keywords,
    "//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

//*** prototypes ***//
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

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
        } else {
        switch(seq[1]){
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
        }
    }
    } else if(seq[0] == 'O'){
        switch(seq[1]){
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
        }

    }

    return '\x1b';
    } else {
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
        } else {
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

//*** syntax highlighting ***//

//return true if the char is considered a seperator
int is_seperator(int c){
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

//get a row and highlight each char by setting each value to hl in the array
void editorUpdateSyntx(erow *row){
    row->hl = realloc(row->hl, row->size);
    memset(row->hl, HL_NORMAL, row->size); //set all chars to normal by default

    if(E.syntax == NULL){
        return;
    }

    char **keywords = E.syntax->keywords;

    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while(i<row->size){
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i-1] : HL_NORMAL;

        if(scs_len && !in_string && !in_comment){
            if(!strncmp(&row->render[i], scs, scs_len)){
                memset(&row->hl[i], HL_COMMENT, row->size - i);
                break;
            }
        }

        if(mcs_len && mce_len && !in_string){
            if(in_comment){
                row->hl[i] = HL_MLCOMMENT;
                if(!strncmp(&row->render[i], mce, mce_len)){
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i+=mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if(!strncmp(&row->render[i], mcs, mcs_len)){
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if(E.syntax->flags & HL_HIGHLIGHT_STRINGS){
            if(in_string){
                row->hl[i] = HL_STRING;
                if(c == '\\' && i + 1 < row->size){
                    row->hl[i+1] = HL_STRING;
                    i +=2;
                    continue;
                }
                if(c == in_string){
                    in_string  = 0;
                }
                i++;
                prev_sep = 1;
                continue;
            
        } else {
            if(c == '"' || c == '\''){
                in_string = c;
                row->hl[i] = HL_STRING;
                i++;
                continue;
            }
        }
        }
        
        if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
        if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)){
            row->hl[i] = HL_NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }
        }

        //highlight keywords
        if(prev_sep){
            int j;
            for(j = 0; keywords[j]; j++){
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen-1] == '|';
                if(kw2){
                    klen--;
                }

                if(!strncmp(&row->render[i], keywords[j], klen) && is_seperator(row->render[i+klen])){
                    //is_seperator(row->render[i+klen])){
                        memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                        i += klen;
                        break;
                    //}
                }
            }

            if(keywords[j] != NULL){
                    prev_sep = 0;
                    continue;
            }
        }

        prev_sep = is_seperator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if(changed &&row->idx + 1 <E.numrows){
        editorUpdateSyntx(&E.row[row->idx + 1]);
    }
}

//set colors
int editorSyntaxToColor(int hl){
    switch(hl){
        case HL_COMMENT:
        case HL_MLCOMMENT:
            return 36; //cyan
        case HL_KEYWORD1:
            return 33; //yellow
        case HL_KEYWORD2:
            return 38; //blue
        case HL_STRING:
            return 35; //magenta
        case HL_NUMBER:
            return 31; //red
        case HL_MATCH:
            return 32; //green
        default:
            return 37;
    }
}

//check if filetype matches one in HLDB and sets E.syntax to it as so
void editorSelectSyntaxHighlight(){
    E.syntax = NULL;
    if(E.filename == NULL){
        return;
    }

    char *ext = strrchr(E.filename, '.');

    for(unsigned int j = 0; j < HLDB_ENTRIES; j++){
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while(s->filematch[i]){
            int is_ext = (s->filematch[i][0] == '.');
            if((is_ext && ext && !strcmp(ext, s->filematch[i])) || (!is_ext && strstr(E.filename, s->filematch[i]))){
                E.syntax = s;

                int filerow;
                for(filerow = 0; filerow < E.numrows; filerow++){
                    editorUpdateSyntx(&E.row[filerow]);
                }



                return;
            }
            i++;
        }
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

//handles tabs in search
int editorRowRxToCx(erow *row, int rx){
    int cur_rx = 0;
    int cx;
    for(cx = 0; cx < row->size; cx++){
        if(row->chars[cx] == '\t'){
            cur_rx += (KILO_TAB_STOP-1) - (cur_rx % KILO_TAB_STOP);
        }
        cur_rx++;

        if(cur_rx > rx){
            return cx;
        }
    }

    return cx;
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
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntx(row);
}

//formerly editorAppendRow
void editorInsertRow(int at, char *s, size_t len){
    if(at < 0 || at > E.numrows){
        return;
    }
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    for(int j = at + 1; j <= E.numrows; j++){
        E.row[j].idx++;
    }

    E.row[at].idx = at;

    //int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    editorUpdateRow(&E.row[at]);

    E.numrows++;
    E.dirty++;
}

void editorFreeRow(erow *row){
    free(row->render);
    free(row->chars);
    free(row->hl);
}

//delete a row if backspacing at the begining of a row
void editorDeleteRow(int at){
    if(at < 0 || at >= E.numrows){
        return;
    }
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at+1], sizeof(erow) * (E.numrows - at -1));
    for(int j = at; j<E.numrows -1; j++){
        E.row[j].idx--;
    }
    E.numrows--;
    E.dirty++;
}

//allow for cahrs to be insterted into an erow
void editorRowInsertChar(erow *row, int at, int c){
    if(at < 0 || at > row->size) {
        at = row->size;
    }

    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at +1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);

    E.dirty++;
}

//appends a string to the end of another string
void editorRowAppendString(erow *row, char *s, size_t len){
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

//backspacing capabilities
void editorRowDeleteChar(erow *row, int at){
    if(at < 0 || at >= row->size){
        return;
    }
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

//*** editor ops **//

void editorInsertChar(int c){
    if(E.cy == E.numrows){
        editorInsertRow(E.numrows, "", 0);
    }
    //update cursor after 
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertNewLine(){
    if(E.cx == 0){
        editorInsertRow(E.cy, "", 0);
    } else {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy+1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

void editorDelChar(){
    if(E.cy == E.numrows){
        return;
    }

    if(E.cx == 0 && E.cy == 0){
        return;
    }

    erow *row = &E.row[E.cy];
    if(E.cx > 0){
        editorRowDeleteChar(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy-1], row->chars, row->size);
        editorDeleteRow(E.cy);
        E.cy--;
    }
}


//*** file i/o ***//

//move array of erow struct to a single string to be written to a file
char *editorRowsToString(int *buflen){
    int totlen = 0;
    int j;
    for(j = 0; j < E.numrows; j++){
        totlen += E.row[j].size + 1;
    }

    *buflen = totlen;

    char *buff = malloc(totlen);
    char *p = buff;
    for(j = 0; j < E.numrows; j++){
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buff;
}

//allow users to open files
void editorOpen(char *filename){
    free(E.filename);
    E.filename = strdup(filename); //save the file name to filename

    editorSelectSyntaxHighlight();


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
        editorInsertRow(E.numrows, line, linelen);
    

    
    }

    free(line);
    fclose(fp);
    E.dirty = 0;
}

//write strings from editorRowsToString() to disk
void editorSave(){
    if(E.filename == NULL){
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    

        if(E.filename == NULL){
            editorSetStatusMessage("Save aborted");
            return;
        }

        editorSelectSyntaxHighlight();
    }

    int len;
    char *buff = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if(fd != -1){
        if(ftruncate(fd, len) != -1){
            if(write(fd, buff, len)== len){
                close(fd);
                free(buff);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
        editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
    }


    //ftruncate(fd, len);
    //write(fd, buff, len);
    //close(fd);
    free(buff);
}

//*** find/search ***//

//incrimental searching
void editorFindCallback(char *query, int key){
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if(saved_hl){
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    //allows for scrolling through results
    if(key == '\r' || key == '\x1b'){
        last_match = -1;
        direction = 1;
        return;   
    } else if(key == ARROW_RIGHT || key == ARROW_DOWN){
        direction = 1;
    } else if(key == ARROW_LEFT || key == ARROW_UP){
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }

    if(last_match == -1){
        direction =1;
    }

    int current = last_match;
    //actual searching
    int i;
    for(i = 0; i<E.numrows; i++){
        current += direction;

        if(current == -1){
            current = E.numrows -1;
        } else if(current ==E.numrows){
            current = 0;
        }

        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if(match){
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;

            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);

            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}



//search
void editorFind(){
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;


    char *query = editorPrompt("Search: %s (ESC to cancel, Arrows and Enter to move through results)", editorFindCallback);
    //if(query == NULL){
      //  return;
    //}

    if(query){
        free(query);
    } else {
        //restore cursor if search is cancelled
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
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

//*** input ***//

char *editorPrompt(char *prompt, void (*callback)(char *, int)){
    size_t bufsize = 128;
    char *buff = malloc(bufsize);

    size_t buflen = 0;
    buff[0] = '\0';

    while(1){
        editorSetStatusMessage(prompt, buff);
        editorRefreshScreen();

        int c = editorReadKey();
        if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE){
            if(buflen != 0){
                buff[--buflen] = '\0';
            }
        } else if(c == '\x1b'){
            //allows users to hit ESC to cancel input prompt
            editorSetStatusMessage("");
            if(callback){
                callback(buff,c);
            }
            free(buff);
            return NULL;
        } else if(c == '\r'){
            if(buflen !=0){
                editorSetStatusMessage("");
                if(callback){
                callback(buff,c);
                }
                return buff;
            }
        } else if(!iscntrl(c) && c < 128){
            if(buflen == bufsize -1){
                bufsize *= 2;
                buff = realloc(buff, bufsize);
            }
            buff[buflen++]=c;
            buff[buflen] = '\0';
        }

        if(callback){
                callback(buff,c);
            }
    }
}

void editorMoveCursor(int key){
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch(key){
        case ARROW_LEFT:
            if(E.cx != 0){
            E.cx--;
            } else if(E.cy > 0){
                //move to prev line if at end
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size){
            E.cx++;
            } else if(row && E.cx == row->size){
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
    static int quit_times = KILO_QUIT_TIMES;

    int c = editorReadKey();

    switch(c){
        case '\r':
            //enter key
            editorInsertNewLine();
            break;


        case CTRL_KEY('q'):
            if(E.dirty && quit_times > 0){
                //print a warning if the user is trying to quit w/o saving
                editorSetStatusMessage("WARNING File has unsaved changes. " "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        
        case CTRL_KEY('s'):
            editorSave();
            break;


        case HOME_KEY:
            E.cx = 0;
            break;

        case END_KEY:
            if(E.cy < E.numrows){
                //move to the end of the line
                E.cx = E.row[E.cy].size;
            }
            break;

        case CTRL_KEY('f'):
            //click ctrl-f for search
            editorFind();
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(c == DEL_KEY){
                editorMoveCursor(ARROW_RIGHT);
            }
            editorDelChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN:
        {
            //make PAGE_DOWN go over a whole page
            if(c == PAGE_UP){
                E.cy = E.rowoff;
            } else if(c == PAGE_DOWN){
                E.cy = E.rowoff + E.screenrows - 1;
                if(E.cy > E.numrows){
                    E.cy = E.numrows;
                }
            }

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

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        default:
            editorInsertChar(c);
            break;
    }

    quit_times = KILO_QUIT_TIMES;
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

//draw rows and print out file content
//if no file, print a column of ~
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
        } else {
            abAppend(ab, "~", 1);
        }
        } else {
            int len = E.row[filerow].rsize - E.coloff;
            if(len < 0){
                len = 0;
            }
            if(len > E.screencolumns){
                len = E.screencolumns;
            }

            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int current_color = -1;
            int j;
            for(j = 0; j < len; j++){
                if(iscntrl(c[j])){
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if(current_color != -1){
                        char buff[16];
                        int clen = snprintf(buff, sizeof(buff), "\x1b[%dm", current_color);
                        abAppend(ab, buff, clen);
                    }
                }else if(hl[j] == HL_NORMAL){
                    //add color to syntax but only if there is a color change
                    if(current_color != -1){
                        abAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                abAppend(ab, &c[j], 1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if(color != current_color){
                        current_color = color;
                         char buff[16];
                         int clen = snprintf(buff, sizeof(buff), "\x1b[%dm", color);
                         abAppend(ab, buff, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            //abAppend(ab, &E.row[filerow].render[E.coloff], len);
            abAppend(ab, "\x1b[39m", 5);
        }


        //abAppend(ab, "~", 1);
        //write(STDOUT_FILENO, "~", 1);
        //make a column of ~
        //clear each line before redrawing them
        abAppend(ab, "\x1b[K", 3);
        //if(y<E.screenrows - 1){
        abAppend(ab, "\r\n", 2);
        //write(STDOUT_FILENO, "\r\n", 2);
        //}
    }
}

//draw the status bar with inverted colors
void editorDrawStatusBar(struct abuf *ab){
    abAppend(ab, "\x1b[7m", 4);
    //get file name and print up to 20 chars of it to the line, if no file name print "No Name"
    char status[80], rstatus[80];

    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no file type", E.cy + 1, E.numrows); //E.cy is the current line number but add 1 since E.cy starts at index 0

    if(len > E.screencolumns){
        len = E.screencolumns;
    }

    abAppend(ab, status, len);

    while(len < E.screencolumns){
        if(E.screencolumns - len == rlen){
            //print current line number
            abAppend(ab, rstatus, rlen);
            break;
        } else {
        abAppend(ab, " ", 1);
        len++;
        }
    }

    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);


}

void editorDrawMessageBar(struct abuf *ab){
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);

    if(msglen > E.screencolumns){
        msglen = E.screencolumns;
    }

    if(msglen && time(NULL) - E.statusmsg_time < 5){
        abAppend(ab, E.statusmsg, msglen);
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
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

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

//... allows for any number of args to be taken in
void editorSetStatusMessage(const char *fmt, ...){
    //used to store the args
    va_list ap;
    va_start(ap, fmt);
    //kinda like a printf with multiple vars
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
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
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.syntax = NULL;
    
    if(getWindowsSize(&E.screenrows, &E.screencolumns) == -1){
        die("getWindowSize");
    }

    //make space for a status bar
    E.screenrows -= 2;
    
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

    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
    
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
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
        if(c == CTRL_KEY('q')){
            break;
        }
        
    }*/
    
    return 0;
    
}
