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

WINDOW *prompt;
WINDOW *result;
WINDOW *result_frame;


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

int bdash_cd(char **args){
    if (args[1] == NULL){
        wprintw(result, "B-dash: expected argument to \"cd\"\n");
    }else{
        if (chdir(args[1]) != 0){
            perror("B-dash");
        } else {
            return 0; //成功したら0返す
        }
    }

    return 1; //失敗したら1返す
}

int bdash_help(char **args){
    int i;
    wprintw(result, "Dagechan's B-dash\n");
    wprintw(result, "Type program names and arguments, and hit enter.\n");
    wprintw(result, "The following are built in: \n");
    wrefresh(result);

    for (i = 0; i < num_builtins(); i++){
        wprintw(result, " %s\n", builtin_str[i]);
    }

    wprintw(result, "Use the man command for information on other programs.\n");
    return 1;
}

int bdash_exit(char **args){
    return 0;
}

int launch(char **args) {
    pid_t pid, wpid;
    int status;


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
            perror("B-dash");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("B-dash");
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

    int ch;
    int index = 0;
    while ((ch = getch()) != '\n' && index < MAX_INPUT_LENGTH - 1){
        if(ch == KEY_BACKSPACE || ch == 127){
            if(index > 0){
                input[--index] = '\0';
                wprintw(prompt, "\b \b"); // BackSpace文字を出力したカーソル移動
                wrefresh(prompt);
                
            }
        }else{
            input[index++] = ch;
        }

    }

    char *line = malloc((strlen(input) + 1) * sizeof(char));
    strcpy(line, input);
    wprintw(prompt, "\n");
    wrefresh(prompt);
    return line;
}



void bdash_loop(void){

    char *line;
    char **args;
    int status;
    
    // newwin(height, width, Y, X)
    prompt = newwin(3, 70, 27, 10);
    result_frame = newwin(25, 70, 2, 10);
    result = newwin(23, 68, 3, 11);

    //doはwhile条件不満足でも必ず1回は実行
    do{
        // printw("(`・ω・´)☞ >>"); //顔文字prompt
        box(result_frame, 0, 0);
        box(prompt, 0, 0);
        overlay(result, result_frame);
        mvwprintw(prompt, 1, 1, ">> ");
        mvwprintw(prompt, 0, 0, "PROMPT");
        mvwprintw(result_frame, 0, 1, "RESULT");
        // wprintw(result, "hello");
        wrefresh(result_frame);
        wrefresh(result);
        wrefresh(prompt); //refreshでprompt表示
        refresh();

        move(28, 14); //move(Y, X)

        line = read_line();
        args = split_line(line);
        wclear(result);

        FILE *old_stdout = stdout;
        stdout = fopen("/dev/tty", "w");
        status = exe(args);
        fclose(stdout);
        stdout = old_stdout;

        free(line);
        free(args);

        wprintw(result, "\n");
        wrefresh(result);


    } while (status);
    
    refresh();
    endwin(); //ncursesの終了
}



int main(int argc, char *argv[]){
    initscr();
    crmode();

    refresh();
    //自作shell "B-dash" を終了まで起動するためのループ
    bdash_loop();
    

    return EXIT_SUCCESS;
}
