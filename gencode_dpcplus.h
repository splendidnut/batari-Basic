//
// Created by Admin on 6/13/2023.
//

#ifndef BATARI_BASIC_GENCODE_DPCPLUS_H
#define BATARI_BASIC_GENCODE_DPCPLUS_H

extern void do_push(char **statement);
extern void do_pull(char **statement);
extern void do_stack(char **statement);

extern void pfscroll_DPCPlus(char **statement, int lineNum);
extern void pfclear_DPCPlus(char **statement);

#endif //BATARI_BASIC_GENCODE_DPCPLUS_H
