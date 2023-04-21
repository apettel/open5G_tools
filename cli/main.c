
#include <ncurses.h>
#include <unistd.h> // For usleep()

#define NUM_FIELDS 2
#define NUM_OUT_FIELDS 6

int main() {
    int i;
    const int out_col = 40;
    int selected_field = 0; // Initially selected field is the first one
    char data_fields[NUM_FIELDS][50] = { // Example data fields
        "PSS_detector CFO_mode    :",
        "PSS_detector noise_limit :"
    };
    char in_data[NUM_FIELDS][50] = {0};
    char out_fields[NUM_OUT_FIELDS][50] = {
        "PSS_detector id  :",
        "FIFO_detector id :",
        "RX_ id           :",
        "PSS_detector peak_count_0 :",
        "PSS_detector peak_count_1 :",
        "PSS_detector peak_count_2 :",
    };

    initscr(); // Initialize ncurses screen
    cbreak(); // Disable line buffering
    // noecho(); // Disable echoing of input
    keypad(stdscr, TRUE); // Enable arrow keys

    start_color(); // Enable color mode
    init_pair(1, COLOR_BLACK, COLOR_WHITE); // Define color pair for highlighted field

    while (1) {
        clear(); // Clear the screen

        // Display data fields
        for (i = 0; i < NUM_FIELDS; i++) {
            if (i == selected_field) {
                attron(COLOR_PAIR(1)); // Highlight selected field in a different color
            }
            mvprintw(i, 0, "%s", data_fields[i]);
            attroff(COLOR_PAIR(1));
        }

        // Display output data
        for (i = 0; i < NUM_OUT_FIELDS; i++) {
            mvprintw(i, 40, "%s", out_fields[i]);
        }

        refresh(); // Update the screen

        // Handle user input
        int key = getch();
        switch (key) {
            case KEY_UP:
                selected_field--;
                if (selected_field < 0) {
                    selected_field = NUM_FIELDS - 1; // Wrap around to the last field
                }
                break;
            case KEY_DOWN:
                selected_field++;
                if (selected_field >= NUM_FIELDS) {
                    selected_field = 0; // Wrap around to the first field
                }
                break;
            case '\n': // Enter key
                mvprintw(selected_field, 26, ""); // Redraw prompt to overwrite previous input
                getnstr(in_data[selected_field], 50); // Read new input
                break;
            default:
                break;
        }

        usleep(100000); // Wait for 100ms
    }

    endwin(); // Restore terminal settings and exit
    return 0;
}
