//
// Created by Admin on 8/4/2023.
//

#ifndef BATARI_BASIC_LIB_GFX_H
#define BATARI_BASIC_LIB_GFX_H

#include <stdbool.h>

enum KernelType {
    STANDARD_KERNEL,
    MULTI_SPRITE_KERNEL,
    MULTI_COLOR_SPRITE_KERNEL,
    DPC_PLUS_KERNEL,
};

// share sprite data collection buffer with other modules
enum { SPRITE_DATA_ENTRY_SIZE = 50,
    SPRITE_DATA_ENTRY_COUNT = 5000};
extern char sprite_data[SPRITE_DATA_ENTRY_COUNT][SPRITE_DATA_ENTRY_SIZE];

extern enum KernelType kernelType;
extern void set_kernel_type(const char *optionValue);
extern void set_kernel_options(int options);
extern void set_kernel_size_optimization(bool isSizeOptSet);

extern void output_sprite_data();
extern void output_playfield_data();

extern bool isPlayerGfxStatement(char *const *statement);
extern bool handleGraphicsCommand(char *command, const char *param, char **statement);

extern void pfclear(char **statement);
extern void bkcolors(char **statement);
extern void pfread(char **statement);
extern void pfpixel(char **);
extern void pfhline(char **);
extern void pfvline(char **);
extern void pfscroll(char **);
extern void player(char **);
extern void process_pfheight(char **);
extern void process_pfcolor(char **);
extern void playfield(char **);
extern void lives(char **);
extern void scorecolors(char **);
extern void drawscreen(void);


#endif //BATARI_BASIC_LIB_GFX_H
