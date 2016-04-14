#include <stdio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>

#define SPACE      32
#define NEWLINE    10
#define TAB        9

#define KNRM       "\x1B[0m"
#define KMAG       "\x1B[35m"
#define KCYN       "\x1B[36m"

struct winpos {
	char source;
	char mask;
	int row;
	int col;
	int s1_time;
	int s2_time;
	struct winpos *next;
};

char nmsprintBuffer[10000];
char *display = NULL;

void nmsprintf(const char *, ...);
void nmsexec(void);
char getMaskChar(void);

void nmsprintf(const char *format, ...) {
	va_list argp;
	va_start(argp, format);
	vsprintf(nmsprintBuffer, format, argp);
	va_end(argp);

	if (display == NULL) {
		display = malloc(strlen(nmsprintBuffer) + 1);
		strcpy(display, nmsprintBuffer);
	} else {
		display = realloc(display, strlen(display) + strlen(nmsprintBuffer) + 1);
		strcat(display, nmsprintBuffer);
	}
}


void nmsexec(void) {
	struct winpos *list_pointer = NULL;
	struct winpos *start;                   // Always points to start of list
	struct winpos *temp;                    // Used for free()ing the list
	int termSizeRows, termSizeCols;
	int c, n, x = 0, y = 0;
	int r_time, r_time_l, r_time_s;
	int ms, ls;
	bool first = true;

	// Start and initialize curses mode
	initscr();
	cbreak();
	noecho();

	// Setting up and starting colors if terminal supports them
	if (has_colors()) {
		start_color();
		init_pair(1, COLOR_BLUE, COLOR_BLACK);
	}

	// Get terminal window size
	getmaxyx(stdscr, termSizeRows, termSizeCols);

	// Seed my random number generator with the current time
	srand(time(NULL));

	// Geting input
	n = 0;
	while ((c = display[n++]) != '\0') {
		if (c == NEWLINE) {
			++y;
			x = 0;
		} else if (c == TAB && x + 4 <= termSizeCols) {
			x += 4;
		} else if (isspace(c)) {
			if (++x > termSizeCols) {
				++y;
				x = 0;
			}
		} else {
			if (first) {
				list_pointer = malloc(sizeof(struct winpos));
				start = list_pointer;
				first = false;
			} else {
				list_pointer->next = malloc(sizeof(struct winpos));
				list_pointer = list_pointer->next;
			}

			r_time = rand() % 50;
			r_time_s = r_time * .25;
			r_time_l = r_time * .75;
			r_time *= 100;
			r_time_s *= 100;
			r_time_l *= 100;

			list_pointer->source = c;
			list_pointer->mask = getMaskChar();
			list_pointer->row = y;
			list_pointer->col = x;
			list_pointer->s1_time = r_time > 1000 ? r_time_l : r_time;
			list_pointer->s2_time = r_time > 1000 ? r_time_s : 0;
			list_pointer->next = NULL;

			if (++x > termSizeCols) {
				++y;
				x = 0;
			}
		}
	}

	// Freeing the display character array
	free(display);

	// Initially display the characters in the terminal with a 'type effect'.
	ms = 5;             // miliseconds, used for usleep()
	list_pointer = start;
	while (list_pointer != NULL && list_pointer->row <= termSizeRows) {
		mvaddch(list_pointer->row, list_pointer->col, list_pointer->mask);
		refresh();
		list_pointer->mask = getMaskChar();
		list_pointer = list_pointer->next;
		usleep(ms * 1000);
	}

	// TODO: pause with getchar() - something about the input stream being redirected
	// to a file is causing getchar() to immediately return here.
	sleep(1);

	// Jumble loop
	ms = 35;           // miliseconds, used for usleep()
	ls = 2;            // loop seconds, number of seconds to loop
	x = 0;
	while (x < (ls * 1000) / ms) {
		list_pointer = start;
		while (list_pointer != NULL && list_pointer->row <= termSizeRows) {
			mvaddch(list_pointer->row, list_pointer->col, list_pointer->mask);
			refresh();
			list_pointer->mask = getMaskChar();
			list_pointer = list_pointer->next;
		}
		usleep(ms * 1000);
		++x;
	}


	// Reveal loop
	ms = 100;                     // miliseconds to sleep (must always evenly divisible by 5)
	int s1_remask_time = 500;     // time, in milliseconds, we change the mask for stage 1
	bool loop = true;
	while (loop) {
		loop = false;
		list_pointer = start;
		while (list_pointer != NULL && list_pointer->row <= termSizeRows) {
			if (list_pointer->s1_time > 0) {
				loop = true;
				list_pointer->s1_time -= ms;
				if (list_pointer->s1_time % s1_remask_time == 0) {
					list_pointer->mask = getMaskChar();
				}
			} else if (list_pointer->s2_time > 0) {
				loop = true;
				list_pointer->s2_time -= ms;
				list_pointer->mask = getMaskChar();
			} else {
				list_pointer->mask = list_pointer->source;
				//printf(KCYN);
			}
			mvaddch(list_pointer->row, list_pointer->col, list_pointer->mask);
			refresh();
			list_pointer = list_pointer->next;
		}
		usleep(ms * 1000);
	}

	// Printing remaining characters from list if we stopped short due to 
	// a terminal row limitation. i.e. the number of textual rows in the input
	// stream were greater than the number of rows in the terminal.
	/* TODO - Handle overflow data via ncurses in some way
	int prevRow;
	if (list_pointer != NULL) {
		prevRow = list_pointer->row;
		//printf(KCYN);
		while (list_pointer != NULL) {
			while (list_pointer->row > prevRow) {
				//printf("\n");
				++prevRow;
			}
			mvaddch(list_pointer->row, list_pointer->col, list_pointer->source);
			list_pointer = list_pointer->next;
		}
		//printf(KNRM);
		//printf("\n");
	}
	*/

	// End curses mode
	endwin();

	// Freeing the list. 
	list_pointer = start;
	while (list_pointer != NULL) {
		temp = list_pointer;
		list_pointer = list_pointer->next;
		free(temp);
	}
}

char getMaskChar(void) {
	char *maskChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	                  "abcdefghijklmnopqrstuvwxyz"
	                  "1234567890"
	                  "!@#$%^&*()-_=+{}[]:;|\\\"'<>,.?/";

	return maskChars[rand() % strlen(maskChars)];
}