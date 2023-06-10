#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ncurses.h>

int bdash_cd(char **args);
int bdash_help(char **args);
int bdash_exit(char **args);


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
        fprintf(stderr, "B-dash: expected argument to \"cd\"\n");
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
    printw("Dagechan's B-dash\n");
    printw("Type program names and arguments, and hit enter.\n");
    printw("The following are built in: \n");

    for (i = 0; i < num_builtins(); i++){
        printf(" %s\n", builtin_str[i]);
    }

    printw("Use the man command for information on other programs.\n");

    return 1;
}

int bdash_exit(char **args){
    return 0;
}

int launch(char **args){
    pid_t pid, wpid;
    int status;

    pid = fork(); //子プロセス生成
    if (pid == 0){
        if (execvp(args[0], args) == -1){
            perror("B-dash");
        }
        exit(EXIT_FAILURE);
    }else if (pid < 0){
        perror("B-dash");
    }else{
        do{
            wpid = waitpid(pid, &status, WUNTRACED);
        }while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
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
        fprintf(stderr, "B-dash: allocation error\n");
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
                fprintf(stderr, "B-dash: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, TOKEN_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}


// char *read_line(void){
//     char *line = NULL; //文字列のポインタ
//     ssize_t bufsize = 0; //ssize_tはobjサイズをバイト数＆負数エラー対応
//     getline(&line, &bufsize, stdin); //標準入力(stdin)から行を読み
//     return line;
// }
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
                printw("\b \b"); // BackSpace文字を出力したカーソル移動
                refresh();
            }
        }else{
            input[index++] = ch;
        }

    }

    char *line = malloc((strlen(input) + 1) * sizeof(char));
    strcpy(line, input);
    printw("\n");
    refresh();
    return line;
}



void bdash_loop(void){

    char *line;
    char **args;
    int status;
    
    WINDOW *result = newwin(15, 10, 2, 10);
    WINDOW *prompt = newwin(2, 10, 2, 100);
    initscr();

    box()


    //doはwhile条件不満足でも必ず1回は実行
    do{
        // printw("(`・ω・´)☞ >>"); //顔文字prompt
        mvwprintw(prompt, 0, 0, ">>");
        wrefresh(prompt); //refreshでprompt表示

        line = read_line();
        args = split_line(line);
        status = exe(args);

        free(line);
        free(args);

        printw("\n");


    } while (status);
    
    refresh();
    endwin(); //ncursesの終了
}

// void window(){
//     initscr(); //ncursesの初期化
//     start_color();
//     init_pair(1, COLOR_WHITE, COLOR_BLUE);

//     int height, width;
//     getmaxyx(stdscr, height, width);

//     int box1_h = height - 7;
//     int box1_w = width - 6;

//     int box2_h = height - 100;
//     int box2_w = width -6;

//     // ボックスの枠を描画
//     box(stderr, '|', '-')

//     bkgd(' ' |COLOR_PAIR(1));
//     refresh();


// }

// #define box_height 5
// #define box_width 8








int main(int argc, char *argv[]){


    //自作shell "B-dash" を終了まで起動するためのループ
    bdash_loop();
    

    return EXIT_SUCCESS;
}
