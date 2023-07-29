// Provided under the GPL v2 license. See the included LICENSE.txt for details.

#ifndef STATEMENTS_H
#define STATEMENTS_H

#define _readpaddle 1
#define _player1colors 2
#define _playercolors 4
#define _no_blank_lines 8
#define _pfcolors 16
#define _pfheights 32
#define _background 64
#define MAX_EXTRAS 5

#include <stdbool.h>
#include <stdio.h>

// share sprite data collection buffer with other modules
enum { SPRITE_DATA_ENTRY_SIZE = 50,
        SPRITE_DATA_ENTRY_COUNT = 5000};
extern char sprite_data[SPRITE_DATA_ENTRY_COUNT][SPRITE_DATA_ENTRY_SIZE];

// initialization function
extern void init_statement_processor();

// functions to process source file and output file
extern void use_source_file(FILE *sourceFileToUse);
extern void use_output_file(FILE *outputFileToUse);
extern FILE *getOutputFile();
extern bool read_source_line(char *data);
extern void trim_string(char *data, bool addEol);

extern void removeCR(char *);
extern int printimmed(char *);
extern int isimmed(char *);


// functions for debugging
extern void print_statement_breakdown(char **stmtList);

// functions for compiler output
extern void write_footer();
extern void prerror(char *);

// shared processing functions
extern int process_gfx_data(const char *label, const char *dataTypeName);

// statement processing functions
extern void doextra(char *);
extern void callmacro(char **);
extern void do_stack(char **);
extern void do_pull(char **);
extern void do_push(char **);
extern void domacro(char **);
extern void lives(char **);
extern void scorecolors(char **);
extern void playfield(char **);
extern void bkcolors(char **);
extern void playfieldcolorandheight(char **);
extern void vblank();
extern void doreboot();
extern void dopop();
extern void doasm();
extern void pfclear(char **);
extern void data(char **);
extern void sdata(char **);
extern void newbank(int);
extern void dogoto(char **);
extern void doif(char **);
extern void function(char **);
extern void add_inline(char *);
extern void add_includes(char *);
extern void create_includes(char *);
extern void endfunction();
extern void invalidate_Areg();
extern int getcondpart();
extern int linenum();
extern int islabel(char **);
extern int islabelelse(char **);
extern void compressdata(char **, int, int);
extern void shiftdata(char **, int);
extern int findpoint(char *);
extern int getindex(char *, char *);
extern int bbgetlinenumber();
extern void doend();
extern void output_sprite_data();
extern void output_playfield_data();
extern void printindex(char *, int);
extern void loadindex(char *);
extern void jsr(char *);
extern int findlabel(char **, int i);
extern void incline();
extern void init_includes(char *path);
extern void callfunction(char **);
extern void ongoto(char **);
extern void doreturn(char **);
extern void doconst(char **);
extern void dim(char **);
extern void dofor(char **);
extern void mul(char **, int);
extern void divd(char **, int);
extern void next(char **);
extern void gosub(char **);
extern void dolet(char **);
extern void dec(char **);
extern void rem(char **);
extern void set(char **);
extern void pfpixel(char **);
extern void pfhline(char **);
extern void pfvline(char **);
extern void pfscroll(char **);
extern void player(char **);
extern void drawscreen(void);
extern void output_redefvars_file(char *);

#endif