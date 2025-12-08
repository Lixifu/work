#include <curses.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

// 全局变量（主线程和子线程共享）
int ballx, bally, dx, dy;
int barx, bary, barlength;
char ball, *bar;
bool game_over;
bool ball_launched;  // 标记弹珠是否已发射
pthread_mutex_t mutex;  // 互斥锁（保护共享变量）
WINDOW *bg_win;  // 后台窗口（双缓冲用）

// 游戏初始化函数
void new_game()
{
    //pthread_mutex_lock(&mutex);  // 加锁保护共享变量
    
    // 计算游戏区域边界（左右之间的距离为屏幕宽度的一半）
    int game_width = COLS / 2;
    int left_bound = (COLS - game_width) / 2;
    int right_bound = left_bound + game_width - 1;
    
    // 初始化挡板参数
    barx = (left_bound + right_bound) / 2 - 5;
    bary = LINES - 1;
    barlength = 10;
    bar = "**********";
    
    // 初始化球的参数（固定在挡板正中央上方一格）
    ballx = barx + barlength / 2;
    bally = bary - 1;
    ball = 'O';
    
    // 弹珠初始状态：未发射
    ball_launched = false;
    dx = (rand() % 3) - 1;  // 初始x方向速度：-1、0或1（随机）
    dy = -1; // 初始y方向速度为向上（发射后使用）
    
    // 游戏状态
    game_over = false;
    
    //pthread_mutex_unlock(&mutex);  // 解锁
}

// 绘图线程函数（负责球的运动、碰撞检测和屏幕绘制）
void* paint_thread(void* arg)
{
    long delay = 100;  // 绘制周期（毫秒）
    
    while (true) {
        pthread_mutex_lock(&mutex);  // 加锁
        
        if (!game_over) {
            wclear(bg_win);
            
            // 计算游戏区域边界（左右之间的距离为屏幕宽度的一半）
            int game_width = COLS / 2;
            int left_bound = (COLS - game_width) / 2;
            int right_bound = left_bound + game_width - 1;
            
            // 绘制游戏区域边界
            for (int i = 0; i < LINES; i++) {
                mvwaddch(bg_win, i, left_bound, '|');
                mvwaddch(bg_win, i, right_bound, '|');
            }
            
            // 绘制球和挡板
            mvwaddch(bg_win, bally, ballx, ball);
            mvwaddstr(bg_win, bary, barx, bar);
            
            // 更新球的坐标
            if (ball_launched) {
                // 弹珠已发射，正常移动
                ballx += dx;
                bally += dy;
            } else {
                // 弹珠未发射，固定在挡板正中央上方一格
                ballx = barx + barlength / 2;
                bally = bary - 1;
            }
            
            // 碰撞检测与处理
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
            
            // 碰撞挡板：y方向反向，并根据碰撞位置调整x方向
            if (bally == bary - 1 && ballx >= barx && ballx < barx + barlength) {
                dy = -dy;
                
                // 根据碰撞位置调整x方向速度
                int bar_center = barx + barlength / 2;
                if (ballx < bar_center - 2) {
                    // 碰撞挡板左侧，x方向向左
                    dx = -1;
                } else if (ballx > bar_center + 2) {
                    // 碰撞挡板右侧，x方向向右
                    dx = 1;
                } else {
                    // 碰撞挡板中心附近，x方向轻微调整（-1、0或1）
                    dx = (rand() % 3) - 1;
                }
                
                beep();
            }
            
            // 球落地：游戏结束
            if (bally >= LINES - 1) {
                game_over = true;
                wclear(bg_win);
                mvwaddstr(bg_win, LINES/2, COLS/2 - 5, "Game Over!");
                mvwaddstr(bg_win, LINES/2 + 1, COLS/2 - 10, "Press 'N' to restart");
                mvwaddstr(bg_win, LINES/2 + 2, COLS/2 - 8, "Press 'Q' to quit");
            }
        }
        
        // 双缓冲刷新：将后台窗口内容刷新到前台
        wrefresh(bg_win);
        
        pthread_mutex_unlock(&mutex);  // 解锁
        // 无论游戏是否结束，都使用较短的延迟，确保及时响应状态变化
        usleep(delay * 1000);  // 正常游戏速度，100ms
    }
    
    return NULL;
}

int main()
{
    chtype input;
    
    // 初始化curses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);  // 启用非阻塞输入，提高响应速度
    srand(time(NULL));  // 随机数种子（用于球的初始方向）
    
    // 创建后台窗口（双缓冲）
    bg_win = newwin(LINES, COLS, 0, 0);
    if (bg_win == NULL) {
        endwin();
        fprintf(stderr, "Failed to create background window\n");
        return -1;
    }
    keypad(bg_win, TRUE);  // 为后台窗口启用键盘支持
    
    // 初始化互斥锁
    pthread_mutex_init(&mutex, NULL);
    
    // 开始新游戏
    pthread_mutex_lock(&mutex);
    new_game();
    pthread_mutex_unlock(&mutex);
    
    // 创建绘图线程
    pthread_t tidp;
    if (pthread_create(&tidp, NULL, paint_thread, NULL) != 0) {
        mvaddstr(0, 0, "Thread create error!");
        refresh();
        endwin();
        return -1;
    }
    
    // 主线程：处理用户输入
    while (true) {
        input = getch();
        
        // 检查是否退出
        if (input == 'q' || input == 'Q') {
            break;
        }
        
        // 只有当有有效输入时才处理
        if (input != ERR) {
            pthread_mutex_lock(&mutex);
            
            // 计算游戏区域边界（左右之间的距离为屏幕宽度的一半）
            int game_width = COLS / 2;
            int left_bound = (COLS - game_width) / 2;
            int right_bound = left_bound + game_width - 1;
            
            switch (input) {
                case KEY_LEFT:  // 挡板左移，每次移动2格
                    if (!game_over) {
                        barx -= 2;
                        if (barx < left_bound) {
                            barx = left_bound;
                        }
                    }
                    break;
                case KEY_RIGHT:  // 挡板右移，每次移动2格
                    if (!game_over) {
                        barx += 2;
                        if (barx + barlength > right_bound) {
                            barx = right_bound - barlength;
                        }
                    }
                    break;
                case ' ':// 空格键发射弹珠
                    if (!game_over && !ball_launched) {
                        // 发射弹珠，设置为已发射状态
                        ball_launched = true;
                        // 随机x方向速度：-1、0或1，避免直线运动
                        dx = (rand() % 3) - 1;
                        // y方向速度向上
                        dy = -1;
                    }
                    break;
                case 'n':  // 重新开始（小写n）
                case 'N':  // 重新开始（大写N）
                    new_game();
                    break;
            }
            
            pthread_mutex_unlock(&mutex);
        }
        
        // 短暂休眠，降低CPU占用
        usleep(50000);  // 50ms
    }
    
    // 清理资源
    pthread_cancel(tidp);  // 终止绘图线程
    pthread_join(tidp, NULL);  // 等待线程结束
    pthread_mutex_destroy(&mutex);  // 销毁互斥锁
    
    // 销毁后台窗口（双缓冲）
    if (bg_win != NULL) {
        delwin(bg_win);
        bg_win = NULL;
    }
    
    endwin();
    return 0;
}