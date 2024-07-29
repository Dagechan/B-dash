#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ncurses.h>
#include <form.h>
#include <sys/sysinfo.h>
#include <locale.h>
#define ENTER 10
#define ESCAPE 27

int bdash_cd(char **args);
int bdash_help(char **args);
int bdash_exit(char **args);
int y, x;
int init = 0;
void scroll_result(int n);
void init_boot(bool init_flag);

WINDOW *prompt;
WINDOW *result;
WINDOW *directory;
WINDOW *syswin;
WINDOW *menuber;
WINDOW *messagebar;

char *builtin_str[] = {
    "cd",
    "help",
    "exit"};

int (*builtin_func[])(char **) = {
    &bdash_cd,
    &bdash_help,
    &bdash_exit};

int num_builtins(){
    return sizeof(builtin_str) / sizeof(char *);
}

int bdash_cd(char **args){
    if (args[1] == NULL){
        // 引数がない場合はホームディレクトリに移動
        if (chdir(getenv("HOME")) != 0){
            wmove(result, 1, 1);
            wprintw(result, "B-dash: cannot change directory\n");
        }else{
            // ディレクトリが正常に変更された場合、プロンプトにも反映させる
            wmove(directory, 1, 4);
            wprintw(directory, "%s", getcwd(NULL, 0));
            wclrtoeol(directory);
        }
    }else{
        if (chdir(args[1]) != 0){
            wmove(result, 1, 1);
            wprintw(result, "B-dash: cannot change directory\n");
        }else{
            // ディレクトリが正常に変更された場合、プロンプトにも反映させる
            wmove(directory, 1, 4);
            wprintw(directory, "%s", getcwd(NULL, 0));
            wclrtoeol(directory);
        }
    }
    return 1;
}

int bdash_help(char **args){
    int i;
    wmove(result, 1, 1);
    wprintw(result, "Takeaki's B-dash\n");
    wmove(result, 2, 1);
    wprintw(result, "Type program names and arguments, and hit enter.\n");
    wmove(result, 3, 1);
    wprintw(result, "The following are built in: \n");
    wrefresh(result);

    for (i = 0; i < num_builtins(); i++){
        wmove(result, 4 + i, 1);
        wprintw(result, " %s\n", builtin_str[i]);
    }
    wmove(result, 7, 1);
    wprintw(result, "Use the man command for information on other programs.\n");
    return 1;
}

int bdash_exit(char **args){
    return 0;
}

void get_system_info(char *hostname, unsigned long *total_memory){
    // get hostname
    gethostname(hostname, 256);

    // get information of system memory
    struct sysinfo info;
    sysinfo(&info);
    *total_memory = info.totalram / (1024 * 1024);
}

void show_system_info(WINDOW *window){
    char hostname[256];
    unsigned long total_memory;
    get_system_info(hostname, &total_memory);

    // show hostname and total memory in window
    mvwprintw(window, 1, 1, "Hostname: %s", hostname);
    mvwprintw(window, 2, 1, "Total Memory: %lu MB", total_memory);
    mvwprintw(window, 3, 1, "Shell: B-dash");

    wrefresh(window);
}

void menubar(WINDOW *menubar){
}

int launch(char **args){
    pid_t pid, wpid;
    int status;
    int wx, wy;

    // 一時的なファイルを作成
    char tmp_filename[] = "/tmp/shell_outputXXXXXX";
    int tmp_fd = mkstemp(tmp_filename);
    if (tmp_fd == -1){
        perror("B-dash");
        return 1;
    }

    pid = fork(); // 子プロセス生成
    if (pid == 0){
        // 子プロセス内で標準出力を一時的なファイルにリダイレクト
        dup2(tmp_fd, STDOUT_FILENO);
        close(tmp_fd);

        if (execvp(args[0], args) == -1){
            mvwprintw(result, 1, 1, "B-dash: command not found: %s\n", args[0]);
            // perror("B-dash");
            wrefresh(result);
        }
        exit(EXIT_FAILURE);
    }else if (pid < 0){
        mvwprintw(result, 1, 1, "B-dash: command not found: %s\n", args[0]);
    }else{
        do{
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    // 一時的なファイルの内容を読み取り、resultウィンドウに表示
    FILE *tmp_file = fopen(tmp_filename, "r");
    if (tmp_file == NULL){
        perror("B-dash");
        return 1;
    }
    char line[100];
    wmove(result, 1, 0);
    while (fgets(line, sizeof(line), tmp_file) != NULL){
        getyx(result, wy, wx);
        wmove(result, wy, wx + 1);
        wprintw(result, "%s", line);
    }
    fclose(tmp_file);
    unlink(tmp_filename); // 一時的なファイルを削除

    return 1;
}

int exe(char **args){
    int i;

    if (args[0] == NULL){
        return 1;
    }

    // clear used result
    wclear(result);
    wrefresh(result);

    for (i = 0; i < num_builtins(); i++){
        if (strcmp(args[0], builtin_str[i]) == 0){
            return (*builtin_func[i])(args);
        }
    }
    return launch(args);
}

#define TOKEN_BUFSIZE 64
#define TOKEN_DELIM " \t\r\n\a"

char **split_line(char *line){
    // 受け取った文字列をTOKEN_DELIMで指定した形で分割
    int bufsize = TOKEN_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    wmove(result, 1, 1);
    // mallocでのメモリ割り当てが失敗した場合の処理
    if (!tokens){
        wprintw(result, "B-dash: allocation error\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, TOKEN_DELIM);
    while (token != NULL){
        tokens[position] = token;
        position++;

        if (position >= bufsize){
            bufsize += TOKEN_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens){
                wprintw(result, "B-dash: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, TOKEN_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

#define MAX_INPUT_LENGTH 100

char *read_line(void){
    // cbreak(); //行バッファリングの無効化
    char input[MAX_INPUT_LENGTH];
    memset(input, 0, sizeof(input)); // バッファの初期化

    wmove(result, 1, 1);
    int ch;
    int index = 0;
    int prompt_y = getcury(prompt);
    int wx, wy;

    while ((ch = getch()) != '\n' && index < MAX_INPUT_LENGTH - 1){
        // switch (ch) {
        //     case KEY_UP:
        //         scroll_result(-1);
        //         break;
        //     case KEY_DOWN:
        //         scroll_result(1);
        //         break;
        // }
        if (ch == KEY_BACKSPACE || ch == KEY_DC || ch == 127){
            getyx(stdscr, wy, wx);
            if (index > 0){
                input[--index] = '\0'; // 消す文字分，バッファを終端文字で埋める
                wprintw(stdscr, "\b"); // BackSpace文字を出力したカーソル移動
                wrefresh(stdscr);

            }else{
                move(32, x / 4 + 4);
                wmove(prompt, 32, x / 4 + 4);
            }
        }else{
            input[index++] = ch;
        }
        wmove(prompt, prompt_y + 1, 4);
        mvwprintw(stdscr, wy, wx, " "); //文字を空で上書き
        move(wy, wx);
        //	wrefresh(prompt);
    }

    char *line = malloc((strlen(input) + 1) * sizeof(char));
    strcpy(line, input);
    wprintw(prompt, "\n");
    wrefresh(prompt);
    return line;
}

void scroll_result(int n){
    int current_line = getcury(result);
    int max_line = getmaxy(result);

    if (n > 0){
	// scroll down
        if (current_line < max_line - 1){
            wscrl(result, n);
        }
    } else if (n < 0) {
	// scroll up
        if (current_line > 0){
            wscrl(result, n);
        }
    }

    wrefresh(result);
}

void draw_menubar(WINDOW *menubar){
    wbkgd(menubar, COLOR_PAIR(1));
    waddstr(menubar, "Menu1");
    wattron(menubar, COLOR_PAIR(3));
    waddstr(menubar,"(F1)");
    wattroff(menubar, COLOR_PAIR(3));
    wmove(menubar,0,20);
    waddstr(menubar, "Menu2");
    wattron(menubar, COLOR_PAIR(3));
    waddstr(menubar, "(F2)");
    wattroff(menubar, COLOR_PAIR(3));
}

WINDOW **draw_menu(int start_col){
    int i;
    WINDOW **items;
    items=(WINDOW **)malloc(9*sizeof(WINDOW *));
    
    items[0]=newwin(10, 19, 1, start_col);
    wbkgd(items[0],COLOR_PAIR(2));
    box(items[0],ACS_VLINE,ACS_HLINE);
    items[1]=subwin(items[0],1,17,2,start_col+1);
    items[2]=subwin(items[0],1,17,3,start_col+1);
    items[3]=subwin(items[0],1,17,4,start_col+1);
    for(i = 1; i < 9; i++){
        wprintw(items[i],"Item%d", i);
    }
    wbkgd(items[1],COLOR_PAIR(1));
    wrefresh(items[0]);
    return items;
}

void delete_menu(WINDOW **items,int count){
    int i;
    for(i = 0; i < count; i++){
        delwin(items[i]);
    }
}

int scroll_menu(WINDOW **items, int count, int menu_start_col){
    int key;
    int selected = 0;
    while (1){
        key = getch();
        if (key == KEY_DOWN || key == KEY_UP){
        wbkgd(items[selected + 1], COLOR_PAIR(2));
        wnoutrefresh(items[selected + 1]);
        if (key == KEY_DOWN){
            selected = (selected + 1) % count;
        }else{
            selected = (selected + count - 1) % count;
        }
        wbkgd(items[selected + 1], COLOR_PAIR(1));
        wnoutrefresh(items[selected + 1]);
        doupdate();
        }else if (key == KEY_LEFT || key == KEY_RIGHT){
            delete_menu(items, count + 1);
            touchwin(stdscr);
            refresh();
            items = draw_menu(20 - menu_start_col);
            return scroll_menu(items, 8, 20 - menu_start_col);
        }else if (key == ESCAPE){
            return -1;
        }else if (key == ENTER){
            return selected;
        }
    }
}

void init_boot(bool init_flag){
    if(init_flag == true){
        mvwprintw(result, 1, 1, "*****************************************");
		mvwprintw(result, 2, 1, "*******B-dash ver 0.0.1 by Dagechan*******");
		mvwprintw(result, 3, 1, "*****************************************");
        wmove(directory, 1, 4);
        wprintw(directory, "%s", getcwd(NULL, 0));
        wclrtoeol(directory);
    }
}

void make_win(void){
    use_default_colors();
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_BLACK, COLOR_CYAN);
    init_pair(3, COLOR_WHITE, COLOR_MAGENTA);
    init_pair(4, COLOR_BLACK, COLOR_YELLOW + 8);


    bkgd(COLOR_PAIR(4));
    refresh();
    wbkgd(result, COLOR_PAIR(1));
    wbkgd(prompt, COLOR_PAIR(1));
    wbkgd(directory, COLOR_PAIR(1));
    wrefresh(result);

    //scrollok(result, TRUE); // permit scrolling but this function has errors.
    box(result, 0, 0);
    box(prompt, 0, 0);
    box(directory, 0, 0);
    box(syswin, 0, 0);
    // overlay(result, result_frame);
    show_system_info(syswin);
    mvwprintw(prompt, 1, 1, ">> ");
    mvwprintw(prompt, 0, 1, "PROMPT");
    mvwprintw(result, 0, 1, "RESULT");
    mvwprintw(directory, 0, 1, "CURRENT DIRECTORY");
    wmove(result, 1, 1);
	if(init == 0){
		init_boot(true);
	}
    // wprintw(result, "hello");
    wrefresh(result);
    wrefresh(prompt);
    wrefresh(directory);
    refresh();
    move(32, x / 4 + 4);
}



void bdash_loop(void){
    char *line;
    char **args;
    int status;

    // height, width, Y, X
    prompt = newwin(3, 100, 31, x / 4);
    result = newwin(25, 100, 2, x / 4);
    directory = newwin(3, 50, 28, x / 4);
    syswin = newwin(5, 30, 2, 2);

    do{
        make_win();

        line = read_line();
        args = split_line(line);

        clear();
        wclear(result);
        make_win();
        refresh();
        wrefresh(result);

        // Execute command and display result
        status = exe(args);

        free(line);
        free(args);

        wmove(result, 0, 0); // Move cursor to the beginning of result window
        wrefresh(result);
		init++;

    } while (status);

    refresh();
}

int main(int argc, char *argv[]){
    setlocale(LC_ALL, "");
    initscr();
    crmode();
    getmaxyx(stdscr, y, x);
    keypad(stdscr, TRUE);
    define_key("^?", KEY_BACKSPACE);

    refresh();
    // 自作shell "B-dash" を終了まで起動するためのループ
    bdash_loop();
    endwin(); // ncursesの終了

    return EXIT_SUCCESS;
}
