#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <sixel.h>

#define IMAGE_WIN_WIDTH  60
#define IMAGE_WIN_HEIGHT 20

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s image.png\n", argv[0]);
        return 1;
    }

    const char *image_path = argv[1];

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    int start_y = 3, start_x = 10;

    // Create a window with a border
    WINDOW *border_win = newwin(IMAGE_WIN_HEIGHT + 2, IMAGE_WIN_WIDTH + 2, start_y - 1, start_x - 1);
    box(border_win, 0, 0);
    mvwprintw(border_win, 0, 2, " SIXEL Image ");
    wrefresh(border_win);

    // Move the cursor to inside the box (leave space for border)
    move(start_y, start_x);
    refresh();

    // Create the encoder
    sixel_encoder_t *encoder;
    if (sixel_encoder_new(&encoder, NULL) != 0) {
        endwin();
        fprintf(stderr, "Failed to create sixel encoder\n");
        return 1;
    }

    // Estimate terminal cell size in pixels (e.g. ~6x12 px per cell)
    int cell_width_px = 6;
    int cell_height_px = 12;

    int max_width_px = IMAGE_WIN_WIDTH * cell_width_px;
    int max_height_px = IMAGE_WIN_HEIGHT * cell_height_px;

    // Build resize options string
    char opt_string[64];
    snprintf(opt_string, sizeof(opt_string), "-m %dx%d", max_width_px, max_height_px);

    // Apply the size limit
    // sixel_encoder_set_encoder_option(encoder, opt_string);
    sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_HEIGHT, "64px");
    sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_WIDTH, "64px");

    // Output image
    if (sixel_encoder_encode(encoder, image_path) != 0) {
        endwin();
        fprintf(stderr, "Failed to encode image: %s\n", image_path);
        sixel_encoder_unref(encoder);
        return 1;
    }

    sixel_encoder_unref(encoder);

    // Wait for user
    mvprintw(start_y + IMAGE_WIN_HEIGHT + 2, 0, "Press any key to exit...");
    refresh();
    getch();

    endwin();
    return 0;
}

// #include <ncurses.h>

// int main() {
//     initscr();
//     cbreak();
//     noecho();
//     curs_set(0);

//     int rows, cols;
//     getmaxyx(stdscr, rows, cols);

//     // Create a status bar window at the top
//     WINDOW *status_win = newwin(1, cols, 0, 0);
//     mvwprintw(status_win, 0, 0, "STATUS: Do not update this area!");
//     wrefresh(status_win); // Show it once

//     // Create a main window below the status bar
//     WINDOW *main_win = newwin(rows - 1, cols, 1, 0);

//     // Example: update main window repeatedly
//     for (int i = 0; i < 5; i++) {
//         werase(main_win);
//         mvwprintw(main_win, 1, 1, "Updating main screen... %d", i);
//         box(main_win, 0, 0);
//         wrefresh(main_win);
//         napms(500);
//     }

//     getch();
//     endwin();
//     return 0;
// }