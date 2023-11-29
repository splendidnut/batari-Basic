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

extern enum KernelType kernelType;
extern void set_kernel_type(const char *optionValue);
extern void set_kernel_options(int options);
extern void set_kernel_size_optimization(bool isSizeOptSet);

extern void output_sprite_data();
extern void output_playfield_data();

extern void pfclear(char **statement);
extern void bkcolors(char **statement);
extern void pfread(char **statement);
extern void pfpixel(char **);
extern void pfhline(char **);
extern void pfvline(char **);
extern void pfscroll(char **);
extern void player(char **);
extern void playfieldcolorandheight(char **);
extern void playfield(char **);
extern void lives(char **);
extern void scorecolors(char **);
extern void drawscreen(void);


#endif //BATARI_BASIC_LIB_GFX_H
