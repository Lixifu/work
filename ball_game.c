#include <curses.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>

// 全局变量定义（球、挡板、速度、游戏状态等）
int ballx, bally, dx, dy;
int barx, bary, barlength;
char ball, *bar;
bool game_over;

// 定时器设置函数
int set_ticker(long n_msecs)
{
    struct itimerval new_timeset;
    long n_sec = n_msecs / 1000;
    long n_usecs = (n_msecs % 1000) * 1000L;

    new_timeset.it_interval.tv_sec = n_sec;
    new_timeset.it_interval.tv_usec = n_usecs;
    new_timeset.it_value.tv_sec = n_sec;
    new_timeset.it_value.tv_usec = n_usecs;

    return setitimer(ITIMER_REAL, &new_timeset, NULL);
}

// 绘制图像函数（SIGALRM信号触发）
void paint()
{
    if (game_over) {
        // 绘制游戏结束界面
        mvaddstr(LINES/2, COLS/2 - 5, "Game Over!");
        mvaddstr(LINES/2 + 1, COLS/2 - 10, "Press 'N' to restart");
        refresh();
        return;
    }

    clear();

    // 计算新的左右边界（屏幕宽度的1/4和3/4）
    int left_bound = COLS / 4;
    int right_bound = COLS * 3 / 4;
    
    // 绘制左右边界线
    for (int i = 0; i < LINES; i++) {
        mvaddch(i, left_bound, '|');
        mvaddch(i, right_bound, '|');
    }

    // 绘制球、挡板等
    mvaddch(bally, ballx, ball);
    mvaddstr(bary, barx, bar);

    refresh();

    // 更新球的坐标
    ballx += dx;
    bally += dy;

    // 碰撞检测与处理（边界、挡板、游戏结束）
    // 碰撞左右边界：x方向反向
    if (ballx >= right_bound || ballx <= left_bound) {
        dx = -dx;
        beep();
    }

    // 碰撞上边界：y方向反向
    if (bally < 0) {
        dy = -dy;
        beep();
    }

    // 碰撞挡板：y方向反向
    if (bally == bary - 1 && ballx >= barx && ballx < barx + barlength) {
        dy = -dy;
        beep();
    }

    // 球落地：游戏结束
    if (bally >= LINES - 1) {
        game_over = true;
    }
}

// 游戏初始化函数
void init_game()
{
    // 初始化球的位置和速度（斜线运动）
    int left_bound = COLS / 4;
    int right_bound = COLS * 3 / 4;
    ballx = (left_bound + right_bound) / 2;
    bally = LINES / 2;
    dx = 1;
    dy = 1;
    ball = 'O';

    // 初始化挡板位置和参数
    barx = (left_bound + right_bound) / 2 - 5;
    bary = LINES - 1;
    barlength = 10;
    bar = "**********";

    // 游戏状态
    game_over = false;
}

int main(int argc, char *argv[])
{
    chtype input;
    long delay = 100;

    // 初始化curses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  // 隐藏光标

    // 初始化游戏
    init_game();

    // 绘制初始画面
    clear();
    
    // 绘制左右边界线
    int left_bound = COLS / 4;
    int right_bound = COLS * 3 / 4;
    for (int i = 0; i < LINES; i++) {
        mvaddch(i, left_bound, '|');
        mvaddch(i, right_bound, '|');
    }
    
    mvaddch(bally, ballx, ball);
    mvaddstr(bary, barx, bar);
    refresh();

    // 设置定时器和信号处理
    signal(SIGALRM, paint);
    if (set_ticker(delay) < 0) {
        perror("set_ticker failed");
        endwin();
        return 1;
    }

    // 处理用户输入
    while ((input = getch()) && input != ERR && input != 'q') {
        switch (input) {
            case 'f':  // 加速
                delay = delay > 10 ? delay / 2 : 10;  // 最小延迟10毫秒
                set_ticker(delay);
                break;
            case 's':  // 减速
                delay *= 2;
                set_ticker(delay);
                break;
            case KEY_LEFT:  // 挡板左移
                {
                    int left_bound = COLS / 4;
                    int new_barx = barx - 2;
                    barx = new_barx > left_bound ? new_barx : left_bound;
                    break;
                }
            case KEY_RIGHT:  // 挡板右移
                {
                    int right_bound = COLS * 3 / 4;
                    int new_barx = barx + 2;
                    barx = new_barx < right_bound - barlength ? new_barx : right_bound - barlength;
                    break;
                }
            case 'n':  // 重新开始游戏
                init_game();
                break;
        }
    }

    endwin();
    return 0;
}
