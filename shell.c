#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ncurses.h>
#include <form.h>

int bdash_cd(char **args);
int bdash_help(char **args);
int bdash_exit(char **args);
int y, x;

WINDOW *prompt;
WINDOW *result;
WINDOW *directory;


char *builtin_str[] = {
    "cd",
    "help",
    "exit"
};

int (*builtin_func[])(char **) = {
    &bdash_cd,
    &bdash_help,
    &bdash_exit
};

int num_builtins(){
    return sizeof(builtin_str) / sizeof(char *);
}

// int bdash_cd(char **args){
//     if (args[1] == NULL){
// 	wmove(result, 1, 1);
//         wprintw(result, "B-dash: expected argument to \"cd\"\n");
//     }else{
//         if (chdir(args[1]) != 0){
// 	    wmove(result, 1, 1);
//             wprintw(result, "B-dash: cannot change drectory\n");
//         } else {
// 	    // ディレクトリが正常に変更された場合，プロンプトにも反映
// 	    wmove(prompt, 1, 4);
// 	    wprintw(prompt, "%s", getcwd(NULL, 0));
// 	    wclrtoeol(prompt);
//             //return 0; //成功したら0返す
//         }
//     }

//     return 1; //失敗したら1返す
// }

int bdash_cd(char **args){
    if (args[1] == NULL){
        // 引数がない場合はホームディレクトリに移動
        if (chdir(getenv("HOME")) != 0){
            wmove(result, 1, 1);
            wprintw(result, "B-dash: cannot change directory\n");
        }else{
            // ディレクトリが正常に変更された場合、プロンプトにも反映させる
            wmove(prompt, 1, 4);
            wprintw(prompt, "%s", getcwd(NULL, 0));
            wclrtoeol(prompt);
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
    wprintw(result, "Dagechan's B-dash\n");
    wmove(result, 2, 1);
    wprintw(result, "Type program names and arguments, and hit enter.\n");
    wmove(result, 3, 1);
    wprintw(result, "The following are built in: \n");
    wrefresh(result);

    for (i = 0; i < num_builtins(); i++){
	wmove(result, 4+i, 1);
        wprintw(result, " %s\n", builtin_str[i]);
    }
    wmove(result, 7, 1);
    wprintw(result, "Use the man command for information on other programs.\n");
    return 1;
}

int bdash_exit(char **args){
    return 0;
}


int launch(char **args) {
    pid_t pid, wpid;
    int status;
    int wx, wy;


    // 一時的なファイルを作成
    char tmp_filename[] = "/tmp/shell_outputXXXXXX";
    int tmp_fd = mkstemp(tmp_filename);
    if (tmp_fd == -1) {
        perror("B-dash");
        return 1;
    }

    pid = fork(); //子プロセス生成
    if (pid == 0) {
        // 子プロセス内で標準出力を一時的なファイルにリダイレクト
        dup2(tmp_fd, STDOUT_FILENO);
        close(tmp_fd);

        if (execvp(args[0], args) == -1) {
            mvwprintw(result, 1, 1, "B-dash: command not found: %s\n",args[0]);
            //perror("B-dash");
            wrefresh(result);
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        mvwprintw(result, 1, 1, "B-dash: command not found: %s\n",args[0]);
    } else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    // 一時的なファイルの内容を読み取り、resultウィンドウに表示
    FILE *tmp_file = fopen(tmp_filename, "r");
    if (tmp_file == NULL) {
        perror("B-dash");
        return 1;
    }
    char line[100];
    while (fgets(line, sizeof(line), tmp_file) != NULL) {
	getyx(result, wy, wx);
	wmove(result, wy, wx+1);
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

char **split_line(char* line){
    //受け取った文字列をTOKEN_DELIMで指定した形で分割
    int bufsize = TOKEN_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    wmove(result, 1, 1);
    //mallocでのメモリ割り当てが失敗した場合の処理
    if(!tokens){
        wprintw(result, "B-dash: allocation error\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, TOKEN_DELIM);
    while(token != NULL){
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
    //cbreak(); //行バッファリングの無効化
    char input[MAX_INPUT_LENGTH]; 
    memset(input, 0, sizeof(input));//バッファの初期化
    
    wmove(result, 1, 1);
    int ch;
    int index = 0;
    int prompt_y = getcury(prompt);
    while ((ch = getch()) != '\n' && index < MAX_INPUT_LENGTH - 1){
        if(ch == KEY_BACKSPACE || ch == KEY_DC || ch == 127){
            if(index > 0){
                input[--index] = '\0';
                wprintw(prompt, "\b \b"); // BackSpace文字を出力したカーソル移動
                wrefresh(prompt);
                
            }
        }else{
            input[index++] = ch;
        }
        wmove(prompt, prompt_y, 4);

    }

    char *line = malloc((strlen(input) + 1) * sizeof(char));
    strcpy(line, input);
    wprintw(prompt, "\n");
    wrefresh(prompt);
    return line;
}

void make_win(void){
    use_default_colors();
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_BLACK, COLOR_CYAN);
    init_pair(3, COLOR_WHITE, COLOR_MAGENTA);
    init_pair(4, COLOR_BLACK, COLOR_YELLOW+8);

    bkgd(COLOR_PAIR(4));
    refresh();
    wbkgd(result, COLOR_PAIR(1));
    wbkgd(prompt, COLOR_PAIR(1));
    wbkgd(directory, COLOR_PAIR(1)); 
    wrefresh(result);

    scrollok(result, TRUE); //permit scrolling
    box(result, 0, 0);
    box(prompt, 0, 0);
    box(directory, 0, 0);
    //overlay(result, result_frame);
    mvwprintw(prompt, 1, 1, ">> ");
    mvwprintw(prompt, 0, 1, "PROMPT");
    mvwprintw(result, 0, 1, "RESULT");
    mvwprintw(directory, 0, 1, "CURRENT DIRECTORY");
    wmove(result, 1, 1);
    //wprintw(result, "hello");
    wrefresh(result);
    wrefresh(prompt);
    wrefresh(directory);
    refresh();
    move(32, x/6 + 4);
}



void bdash_loop(void) {
    char *line;
    char **args;
    int status;
   
    //height, width, Y, X
    prompt = newwin(3, 100, 31, x/6);
    result = newwin(25, 100, 2, x/6);
    directory = newwin(3, 50, 28, x/6);

    do {
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

        wmove(result, 0, 0);  // Move cursor to the beginning of result window
        wrefresh(result);

    } while (status);
    
    refresh();
}

int main(int argc, char *argv[]){
    initscr();
    crmode();
    getmaxyx(stdscr, y, x);
    keypad(stdscr, TRUE); 

    refresh();
    //自作shell "B-dash" を終了まで起動するためのループ
    bdash_loop();
    endwin(); //ncursesの終了
    

    return EXIT_SUCCESS;
}
