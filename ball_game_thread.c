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

// 砖块相关参数
#define BRICK_MAX_ROWS 7  // 最大砖块行数
#define BRICK_WIDTH 5  // 每个砖块的宽度（5个'@'）
#define BRICK_HEIGHT 1  // 每个砖块的高度
#define BRICK_SPACING 1  // 砖块之间的间距（上下左右各1个空白）
bool bricks[BRICK_MAX_ROWS][100];  // 砖块状态数组，true表示存在，false表示已销毁
int brick_rows;  // 当前砖块行数（3-7之间随机）
int brick_cols;  // 每行砖块数量

// 计分系统
int score;  // 当前分数
int high_score;  // 最高分
#define HIGH_SCORE_FILE ".highscore"  // 保存最高分的文件名

// 读取最高分函数
void read_high_score() {
    FILE *fp = fopen(HIGH_SCORE_FILE, "r");
    if (fp != NULL) {
        fscanf(fp, "%d", &high_score);
        fclose(fp);
    } else {
        high_score = 0;  // 文件不存在，初始化为0
    }
}

// 保存最高分函数
void write_high_score() {
    FILE *fp = fopen(HIGH_SCORE_FILE, "w");
    if (fp != NULL) {
        fprintf(fp, "%d", high_score);
        fclose(fp);
    }
}

// 检查场上是否还有砖块
bool check_bricks_remaining() {
    for (int i = 0; i < brick_rows; i++) {
        for (int j = 0; j < brick_cols; j++) {
            if (bricks[i][j]) {
                return true;  // 还有砖块存在
            }
        }
    }
    return false;  // 所有砖块都已销毁
}

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
    bary = LINES - 3;  // 挡板向上移动2行，从LINES-1改为LINES-3
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
    
    // 初始化分数
    score = 0;
    
    // 初始化砖块
    // 随机生成当前砖块行数（3-7之间）
    brick_rows = (rand() % 5) + 3;  // 3到7行（含）
    // 计算每行砖块数量：(游戏区域宽度 - 左侧间距) / (砖块宽度 + 间距)
    brick_cols = (game_width - BRICK_SPACING) / (BRICK_WIDTH + BRICK_SPACING);
    // 初始化所有砖块为存在状态
    for (int i = 0; i < brick_rows; i++) {
        for (int j = 0; j < brick_cols; j++) {
            bricks[i][j] = true;
        }
    }
    
    //pthread_mutex_unlock(&mutex);  // 解锁
}

// 动态计算延迟时间（根据分数调整难度）
long calculate_delay(int score) {
    // 基础延迟100ms，每3分速度提高3%（延迟减少约2.9%），最低30ms
    long delay = 100;
    int speed_increase_count = score / 3;  // 每3分提高一次速度
    
    // 每次速度提高3%，延迟 = 延迟 * 0.9709（因为速度与延迟成反比）
    for (int i = 0; i < speed_increase_count; i++) {
        delay = (long)(delay * 0.9709);  // 每次减少约2.9%的延迟
        if (delay < 30) {
            break;  // 达到最低延迟，停止减少
        }
    }
    
    if (delay < 30) {
        delay = 30;
    }
    return delay;
}

// 绘图线程函数（负责球的运动、碰撞检测和屏幕绘制）
void* paint_thread(void* arg)
{
    long delay = 100;  // 初始绘制周期（毫秒）
    
    while (true) {
        pthread_mutex_lock(&mutex);  // 加锁
        
        if (!game_over) {
            werase(bg_win);
            
            // 计算游戏区域边界（左右之间的距离为屏幕宽度的一半）
            int game_width = COLS / 2;
            int left_bound = (COLS - game_width) / 2;
            int right_bound = left_bound + game_width - 1;
            
            // 绘制游戏区域边界
            for (int i = 0; i < LINES; i++) {
                mvwaddch(bg_win, i, left_bound, '|');
                mvwaddch(bg_win, i, right_bound, '|');
            }
            
            // 绘制砖块
            for (int i = 0; i < brick_rows; i++) {
                for (int j = 0; j < brick_cols; j++) {
                    if (bricks[i][j]) {
                        // 计算砖块位置
                        int brick_x = left_bound + BRICK_SPACING + j * (BRICK_WIDTH + BRICK_SPACING);
                        int brick_y = BRICK_SPACING + i * (BRICK_HEIGHT + BRICK_SPACING);
                        // 绘制砖块（5个'@'）
                        for (int k = 0; k < BRICK_WIDTH; k++) {
                            mvwaddch(bg_win, brick_y, brick_x + k, '@');
                        }
                    }
                }
            }
            
            // 绘制球和挡板
            mvwaddch(bg_win, bally, ballx, ball);
            mvwaddstr(bg_win, bary, barx, bar);
            
            // 显示最高分（当前分数正上方3个字符位置）
            mvwprintw(bg_win, LINES - 8, 3, "High Score: %d", high_score);
            // 显示分数（左下角：距离底部向上5个字符，距离左侧边界向右3个字符）
            mvwprintw(bg_win, LINES - 5, 3, "Score: %d", score);
            
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
                if (ball_launched) {
                    beep();
                }
            }
            
            // 碰撞上边界：y方向反向
            if (bally < 0) {
                dy = -dy;
                if (ball_launched) {
                    beep();
                }
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
                
                if (ball_launched) {
                    beep();
                }
            }
            
            // 碰撞砖块检测
            for (int i = 0; i < brick_rows; i++) {
                for (int j = 0; j < brick_cols; j++) {
                    if (bricks[i][j]) {
                        // 计算砖块位置和边界
                        int brick_x = left_bound + BRICK_SPACING + j * (BRICK_WIDTH + BRICK_SPACING);
                        int brick_y = BRICK_SPACING + i * (BRICK_HEIGHT + BRICK_SPACING);
                        int brick_right = brick_x + BRICK_WIDTH - 1;
                        int brick_bottom = brick_y + BRICK_HEIGHT - 1;
                        
                        // 检测碰撞
                        if (ballx >= brick_x && ballx <= brick_right && 
                            bally >= brick_y && bally <= brick_bottom) {
                            // 销毁砖块
                            bricks[i][j] = false;
                            
                            // 分数加1
                            score++;
                            
                            // 检查是否更新最高分
                            if (score > high_score) {
                                high_score = score;
                                write_high_score();  // 保存新的最高分
                            }
                            
                            // 弹珠反弹，与挡板和边界碰撞逻辑一致
                            // 检测碰撞方向，决定反弹方向
                            if (ballx == brick_x || ballx == brick_right) {
                                dx = -dx;  // 左右碰撞，x方向反弹
                            }
                            if (bally == brick_y || bally == brick_bottom) {
                                dy = -dy;  // 上下碰撞，y方向反弹
                            }
                            
                            if (ball_launched) {
                                beep();
                            }
                            break;  // 跳出内层循环，只处理一个砖块碰撞
                        }
                    }
                }
            }
            
            // 检查是否需要刷新砖块（当所有砖块都被清除时）
            if (!check_bricks_remaining()) {
                // 随机生成新的砖块行数（3-7之间）
                brick_rows = (rand() % 5) + 3;  // 3到7行（含）
                // 初始化新的砖块
                for (int i = 0; i < brick_rows; i++) {
                    for (int j = 0; j < brick_cols; j++) {
                        bricks[i][j] = true;
                    }
                }
                if (ball_launched) {
                    beep();  // 发出声音提示
                }
            }
            
            // 球落地：游戏结束
            if (bally >= LINES - 1) {
                game_over = true;
                werase(bg_win);
                mvwaddstr(bg_win, LINES/2, COLS/2 - 5, "Game Over!");
                mvwaddstr(bg_win, LINES/2 + 1, COLS/2 - 10, "Press 'N' to restart");
                mvwaddstr(bg_win, LINES/2 + 2, COLS/2 - 8, "Press 'Q' to quit");
            }
        }
        
        // 双缓冲刷新：将后台窗口内容刷新到前台
        wrefresh(bg_win);
        
        pthread_mutex_unlock(&mutex);  // 解锁
        // 动态调整延迟时间（根据当前分数）
        delay = calculate_delay(score);
        // 无论游戏是否结束，都使用动态延迟，确保及时响应状态变化
        usleep(delay * 1000);
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
    // 【新增】告诉 ncurses 刷新后不要把物理光标移动回逻辑光标位置
    // 这可以微弱提升性能并减少光标乱跳的可能性
    leaveok(bg_win, TRUE);
    
    // 初始化互斥锁
    pthread_mutex_init(&mutex, NULL);
    
    // 读取最高分
    read_high_score();
    
    // 开始新游戏
    //pthread_mutex_lock(&mutex);
    new_game();
    //pthread_mutex_unlock(&mutex);
    
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
    
    // 保存最高分
    write_high_score();
    
    endwin();
    return 0;
}