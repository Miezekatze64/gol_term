#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

typedef unsigned char bool;
#define true 1
#define false 0

#if defined(_win32) || defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__CYGWIN__)
#error This program currently does NOT support windows (and very likely never wikk)! \
    if you are thinking otherwise, please delete this line or add `undef(_WIN32)`..
int main() {}
#else
#ifndef __linux__
#warning this program is not tested for your operating systems, some bugs might occur...
#endif //__linux__

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define die(str)                                                               \
  do {                                                                         \
    printf("An unexpected error occured at %s:%d (%s): %s [ERRNO: %d]\n",      \
           __FILE__, __LINE__, str, strerror(errno), errno);                   \
    exit(1);                                                                   \
  } while (0);                                                                 \

void print(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}

typedef struct termios termios_t;
termios_t orig_term;

void restore_term() {
    if (tcsetattr(0, TCSAFLUSH, &orig_term) < 0) {
        die("restore_term / tcsetattr");
    }
}

void raw_mode() {
    if (tcgetattr(1, &orig_term) < 0) die("raw_mode / tcgetattr");
    atexit(restore_term);

    termios_t raw = orig_term;
    raw.c_lflag &= ~(IXON | ICRNL);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    
    if (tcsetattr(0, TCSAFLUSH, &raw) < 0) die("raw_mode / tcsetattr");
}

int termsize(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

typedef struct {
    bool *cells;
    size_t size;
    size_t cols;
} grid;

#define ADD(buf, index, str)                                                   \
  do {                                                                         \
    memcpy(buf + index, str, strlen(str));                                     \
    index += strlen(str);                                                      \
  } while (0);

void render(grid grid, int x, int y) {
    char buf[grid.size * 8];
    size_t buf_pos = 0;
    ADD(buf, buf_pos, "\033[H\033[2J\033[3J");
    for (size_t i = 0; i < grid.size; i++) {
        ADD(buf, buf_pos, (grid.cells[i])?"#":" ");
        if (grid.cols == 0) {
            errno = EINVAL;
            die("render: cols should not be 0");
        }
        if (i % grid.cols == 0) {
            ADD(buf, buf_pos, "\n");
        }
    }

    // reset cursor
    ADD(buf, buf_pos, "\033[H");

    int xlen = snprintf(NULL, 0, "%d", x);
    int ylen = snprintf(NULL, 0, "%d", y);

    char *xstr = malloc((xlen+1) * sizeof(char));
    char *ystr = malloc((ylen+1) * sizeof(char));

    snprintf(xstr, xlen+1, "%d", x);
    snprintf(ystr, ylen+1, "%d", y);

    fprintf(stderr, "X: %s (%d), Y: %s (%d)\n", xstr, xlen, ystr, ylen);
    
    ADD(buf, buf_pos, "\033[");
    ADD(buf, buf_pos, ystr);
    ADD(buf, buf_pos, ";");
    ADD(buf, buf_pos, xstr);
    ADD(buf, buf_pos, "H");

    free(xstr);
    free(ystr);

    write(STDOUT_FILENO, buf, buf_pos);
}

bool *clone(bool *other, size_t len) {
    bool *new = malloc(len * sizeof(bool));
    for (size_t i = 0; i < len; i++) {
        new[i] = other[i];
    }
    return new;
}

void step(grid *grid) {
    int h = grid->size / grid->cols;
    int w = grid->cols;

    bool *next = clone(grid->cells, grid->size);
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t neighbours = 0;
            if (x > 0 && y > 0 &&     grid->cells[(x-1)+w*(y-1)]) neighbours++;
            if (x > 0 &&              grid->cells[(x-1)+w*(y)])   neighbours++;
            if (x > 0 && y < h-1 &&   grid->cells[(x-1)+w*(y+1)]) neighbours++;
            if (y > 0 &&              grid->cells[(x)+w*(y-1)])   neighbours++;
            if (y < h-1 &&            grid->cells[(x)+w*(y+1)])   neighbours++;
            if (x < w-1 && y > 0 &&   grid->cells[(x+1)+w*(y-1)]) neighbours++;
            if (x < w-1 &&            grid->cells[(x+1)+w*(y)])   neighbours++;
            if (x < w-1 && y < h-1 && grid->cells[(x+1)+w*(y+1)]) neighbours++;

            if (neighbours == 3 && !grid->cells[x+w*y]) next[x+w*y] = true;
            if ((neighbours < 2 || neighbours > 3) && grid->cells[x+w*y]) next[x+w*y] = false;
        }
    }

    grid->cells = next;
}

#define DELAY (20 * 1000)

int main() {
    raw_mode();

    bool esc_pre = false, esc = false;
    int x = 1, y = 1;

    int w, h;
    termsize(&w, &h);
    
    grid grid = {
        .cells = malloc((w*h)*sizeof(bool)),
        .size = w*h,
        .cols = w,
    };

    bool rerender = true, active = false;
    for (;;) {
        if (active) step(&grid);
        if (rerender || active) render(grid, x, y);
        rerender = false;
        char c = '\0';
        if (read(1, &c, 1) < 0) die("main / read");
        if (c == 'q') {
            printf("\033[H\033[2J");
            exit(0);
            break;
        }

        switch (c) {
        case 27:
            esc_pre = true;
            break;
        case '[':
            esc = esc_pre;
            break;
        case 'A':
            if (esc) y --;
            esc = false;
            rerender = true;
            break;
        case 'B':
            if (esc) y ++;
            esc = false;
            rerender = true;
            break;
        case 'C':
            if (esc) x ++;
            esc = false;
            rerender = true;
            break;
        case 'D':
            if (esc) x --;
            esc = false;
            rerender = true;
            break;
        case 'r':
        case '\n':
        case '\r':
            active = !active;
            break;
        case ' ':
            grid.cells[(y-1)*w+x] = !grid.cells[(y-1)*w+x];
            rerender = true;
            break;
        default:
            esc = esc_pre = false;
            break;
        }
        usleep(DELAY);
    }
    return 0;
}

#endif //!defined(_WIN32)
