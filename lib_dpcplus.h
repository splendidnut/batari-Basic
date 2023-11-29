//
// Created by Admin on 6/13/2023.
//

#ifndef BATARI_BASIC_LIB_DPCPLUS_H
#define BATARI_BASIC_LIB_DPCPLUS_H

extern void do_push(char **statement);
extern void do_pull(char **statement);
extern void do_stack(char **statement);

extern void pfscroll_DPCPlus(char **statement, int lineNum);
extern void pfclear_DPCPlus(char **statement);
extern void genCode_DPCPlusCollision(char firstPlayerParam, char secondPlayerParam);
extern void bkcolors_DPCPlus(char **statement);
extern void playfieldcolorandheight_DPCPlus(char **statement);
extern void pfread_DPCPlus(char **statement);
extern void pfpixel_DPCPlus(char *xpos, char *ypos, int on_off_flip);
extern void pfhline_DPCPlus(char *xpos, char *ypos, char *endXpos, int on_off_flip);
extern void pfvline_DPCPlus(char *xpos, char *ypos, char *endYpos, int on_off_flip);
//extern void scorecolors(char **statement);

#endif //BATARI_BASIC_LIB_DPCPLUS_H
