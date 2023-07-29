//
// Created by Admin on 6/13/2023.
//

#include <stdlib.h>
#include <string.h>

#include "lib_dpcplus.h"
#include "statements.h"


void do_push(char **statement) {
    FILE *outputFile = getOutputFile();
    int k, i = 2;
    // syntax: push [vars]
    // eg: vars can be a b c d e or a-e
    removeCR(statement[4]);
    if (statement[3][0] == '-')    // range
    {
        fprintf(outputFile, "	ldx #255-%s+%s\n", statement[4], statement[2]);
        fprintf(outputFile, "pushlabel%s\n", statement[0]);
        fprintf(outputFile, "	lda %s+1,x\n", statement[4]);
        fprintf(outputFile, "	sta DF7PUSH\n");
        fprintf(outputFile, "	inx\n");
        fprintf(outputFile, "	bmi pushlabel%s\n", statement[0]);
    } else {
        while ((statement[i][0] != ':') && (statement[i][0] != '\0')) {
            for (k = 0; k < 200; ++k)
                if ((statement[i][k] == (unsigned char) 0x0A) || (statement[i][k] == (unsigned char) 0x0D))
                    statement[i][k] = '\0';
            fprintf(outputFile, "	lda %s\n", statement[i++]);
            fprintf(outputFile, "	sta DF7PUSH\n");
        }
    }

}


void do_pull(char **statement) {
    FILE *outputFile = getOutputFile();
    int k, i = 2;
    int savei;
    // syntax: pull [vars]
    // eg: vars can be a b c d e or a-e
    removeCR(statement[4]);
    if (statement[3][0] == '-')    // range
    {
        fprintf(outputFile, "	ldx #%s-%s\n", statement[4], statement[2]);
        fprintf(outputFile, "pulllabel%s\n", statement[0]);
        fprintf(outputFile, "	lda DF7DATA\n");
        fprintf(outputFile, "	sta %s,x\n", statement[2]);
        fprintf(outputFile, "	dex\n");
        fprintf(outputFile, "	bpl pulllabel%s\n", statement[0]);
    } else {

        savei = i;
        while ((statement[i][0] != ':') && (statement[i][0] != '\0'))
            i++;
        i--;
        while (i >= savei) {
            for (k = 0; k < 200; ++k)
                if ((statement[i][k] == (unsigned char) 0x0A) || (statement[i][k] == (unsigned char) 0x0D))
                    statement[i][k] = '\0';
            fprintf(outputFile, "	lda DF7DATA\n");
            fprintf(outputFile, "	sta %s\n", statement[i--]);
        }
    }

}

/**
 * Do Stack operation (DPC+ only)
 * @param statement
 */
void do_stack(char **statement) {
    FILE *outputFile = getOutputFile();
    removeCR(statement[2]);
    if (isimmed(statement[2])) {
        fprintf(outputFile, "	lda #<(STACKbegin+%s)\n", statement[2]);
        fprintf(outputFile, "	STA DF7LOW\n");
        fprintf(outputFile, "	lda #(>(STACKbegin+%s)) & $0F\n", statement[2]);
        fprintf(outputFile, "	STA DF7HI\n");
    } else {
        fprintf(outputFile, "LDA #<STACKbegin\n");
        fprintf(outputFile, "clc\n");
        fprintf(outputFile, "adc %s\n", statement[2]);
        fprintf(outputFile, "STA DF7LOW\n");
        fprintf(outputFile, "LDA #>STACKbegin\n");
        fprintf(outputFile, "adc #0\n");
        fprintf(outputFile, "AND #$0F\n");
        fprintf(outputFile, "STA DF7HI\n");
    }
}


void pfscroll_DPCPlus(char **statement, int lineNum) {
    FILE *outputFile = getOutputFile();
    //DPC+ version of function uses the syntax: pfscroll #LINES [start queue#] [end queue#]
    if ((!strncmp(statement[2], "up\0", 2)) ||
        (!strncmp(statement[2], "down\0", 4)) ||
        (!strncmp(statement[2], "right\0", 5)) || (!strncmp(statement[2], "left\0", 4))) {
        fprintf(stderr, "(%d) pfscroll for DPC+ doesn't use up/down/left/right. try a value or variable instead.\n",
                lineNum);
        exit(1);
    }
    fprintf(outputFile, " lda #<C_function\n");
    fprintf(outputFile, " sta DF0LOW\n");
    fprintf(outputFile, " lda #(>C_function) & $0F\n");
    fprintf(outputFile, " sta DF0HI\n");

    fprintf(outputFile, " lda #32\n");
    fprintf(outputFile, " sta DF0WRITE\n");

    if ((statement[2][0] >= '0') && (statement[2][0] <= '9'))
        fprintf(outputFile, " LDA #%s\n", statement[2]);
    else
        fprintf(outputFile, " LDA %s\n", statement[2]);
    fprintf(outputFile, " sta DF0WRITE\n");

    if ((statement[3][0] >= '0') && (statement[3][0] <= '6')) {
        if ((statement[4][0] >= '0') && (statement[4][0] <= '6')) {
            fprintf(outputFile, " LDA #%d\n", statement[3][0] - '0');
            fprintf(outputFile, " sta DF0WRITE\n");
            fprintf(outputFile, " LDA #%d\n", statement[4][0] - '0' + 1);
            fprintf(outputFile, " sta DF0WRITE\n");
        } else {
            fprintf(stderr, "(%d) initial queue provided for DPC+ pfscroll, but invalid end queue was provided.\n",
                    lineNum);
            exit(1);
        }
    } else {
        //the default is to scroll all playfield columns, without color scrolling
        fprintf(outputFile, " LDA #0\n");
        fprintf(outputFile, " sta DF0WRITE\n");
        fprintf(outputFile, " LDA #4\n");
        fprintf(outputFile, " sta DF0WRITE\n");
    }

    fprintf(outputFile, " lda #255\n");
    fprintf(outputFile, " sta CALLFUNCTION\n");
}

void pfclear_DPCPlus(char **statement) {
    FILE *outputFile = getOutputFile();
    char getindex0[200];
    int index = 0;

    fprintf(outputFile, "	lda #<C_function\n");
    fprintf(outputFile, "	sta DF0LOW\n");
    fprintf(outputFile, "	lda #(>C_function) & $0F\n");
    fprintf(outputFile, "	sta DF0HI\n");
    fprintf(outputFile, "	ldx #28\n");
    fprintf(outputFile, "	stx DF0WRITE\n");

    if ((!statement[2][0]) || (statement[2][0] == ':'))
        fprintf(outputFile, "	LDA #0\n");
    else {
        index = getindex(statement[2], &getindex0[0]);
        if (index)
            loadindex(&getindex0[0]);
        fprintf(outputFile, "	LDA ");
        printindex(statement[2], index);
    }
    fprintf(outputFile, "	sta DF0WRITE\n");
    fprintf(outputFile, "	lda #255\n	sta CALLFUNCTION\n");
}


void genCode_DPCPlusCollision(char firstPlayerParam, char secondPlayerParam) {
    FILE *outputFile = getOutputFile();
    fprintf(outputFile, "	lda #<C_function\n");
    fprintf(outputFile, "	sta DF0LOW\n");
    fprintf(outputFile, "	lda #(>C_function) & $0F\n");
    fprintf(outputFile, "	sta DF0HI\n");
    fprintf(outputFile, "  lda #20\n");
    fprintf(outputFile, "  sta DF0WRITE\n");
    fprintf(outputFile, "  lda #%c\n", firstPlayerParam);
    fprintf(outputFile, "  sta DF0WRITE\n");
    fprintf(outputFile, "  lda #%c\n", secondPlayerParam);
    fprintf(outputFile, "  sta DF0WRITE\n");
    fprintf(outputFile, "  lda #255\n");
    fprintf(outputFile, "  sta CALLFUNCTION\n");
    fprintf(outputFile, "  BIT DF0DATA\n");
}

void bkcolors_DPCPlus(char **statement) {
    FILE *outputFile = getOutputFile();
    char label[200];
    int l = 0;

    sprintf(label, "backgroundcolor%s\n", statement[0]);
    removeCR(label);
    fprintf(outputFile, "	LDA #<BKCOLS\n");
    fprintf(outputFile, "	STA DF0LOW\n");
    fprintf(outputFile, "	LDA #(>BKCOLS) & $0F\n");
    fprintf(outputFile, "	STA DF0HI\n");
    fprintf(outputFile, "	LDA #<%s\n", label);
    fprintf(outputFile, "	STA PARAMETER\n");
    fprintf(outputFile, "	LDA #((>%s) & $0f) | (((>%s) / 2) & $70)\n", label, label);    // DPC+
    fprintf(outputFile, "	STA PARAMETER\n");
    fprintf(outputFile, "	LDA #0\n");
    fprintf(outputFile, "	STA PARAMETER\n");

    process_gfx_data(label, "bkcolors");
    fprintf(outputFile, "	LDA #%d\n", l);
    fprintf(outputFile, "	STA PARAMETER\n");
    fprintf(outputFile, "	LDA #1\n");
    fprintf(outputFile, "	STA CALLFUNCTION\n");
}

void playfieldcolorandheight_DPCPlus(char **statement) {
    FILE *outputFile = getOutputFile();
    int l = 0;
    char label[200];

    sprintf(label, "playfieldcolor%s\n", statement[0]);
    removeCR(label);
    fprintf(outputFile, "	LDA #<PFCOLS\n");
    fprintf(outputFile, "	STA DF0LOW\n");
    fprintf(outputFile, "	LDA #(>PFCOLS) & $0F\n");
    fprintf(outputFile, "	STA DF0HI\n");
    fprintf(outputFile, "	LDA #<%s\n", label);
    fprintf(outputFile, "	STA PARAMETER\n");
    fprintf(outputFile, "	LDA #((>%s) & $0f) | (((>%s) / 2) & $70)\n", label, label);    // DPC+
    fprintf(outputFile, "	STA PARAMETER\n");
    fprintf(outputFile, "	LDA #0\n");
    fprintf(outputFile, "	STA PARAMETER\n");

    process_gfx_data(label, "pfcolor");
    fprintf(outputFile, "	LDA #%d\n", l);
    fprintf(outputFile, "	STA PARAMETER\n");
    fprintf(outputFile, "	LDA #1\n");
    fprintf(outputFile, "	STA CALLFUNCTION\n");
}

void pfread_DPCPlus(char **statement) {
    FILE *outputFile = getOutputFile();
    char getindex0[200];
    char getindex1[200];
    int index = 0;
    index |= getindex(statement[3], &getindex0[0]);
    index |= getindex(statement[4], &getindex1[0]) << 1;

    fprintf(outputFile, "	lda #<C_function\n");
    fprintf(outputFile, "	sta DF0LOW\n");
    fprintf(outputFile, "	lda #(>C_function) & $0F\n");
    fprintf(outputFile, "	sta DF0HI\n");
    fprintf(outputFile, "    lda #24\n");
    fprintf(outputFile, "    sta DF0WRITE\n");

    if (index & 1)
        loadindex(&getindex0[0]);

    fprintf(outputFile, "	LDA ");
    printindex(statement[4], index & 1);
    fprintf(outputFile, "	STA DF0WRITE\n");
    if (index & 2)
        loadindex(&getindex1[0]);

    fprintf(outputFile, "	LDY ");
    printindex(statement[6], index & 2);

    fprintf(outputFile, "	STY DF0WRITE\n");
    fprintf(outputFile, "	lda #255\n	sta CALLFUNCTION\n");
    fprintf(outputFile, "    LDA DF0DATA\n");
}

void pfpixel_DPCPlus(char *xpos, char *ypos, int on_off_flip) {
    FILE *outputFile = getOutputFile();
    char getindex0[200];
    char getindex1[200];
    int index = 0;

    index |= getindex(xpos, &getindex0[0]);
    index |= getindex(ypos, &getindex1[0]) << 1;

    fprintf(outputFile, "	lda #<C_function\n");
    fprintf(outputFile, "	sta DF0LOW\n");
    fprintf(outputFile, "	lda #(>C_function) & $0F\n");
    fprintf(outputFile, "	sta DF0HI\n");
    fprintf(outputFile, "	LDX #");
    fprintf(outputFile, "%d\n	STX DF0WRITE\n	STX DF0WRITE\n", on_off_flip | 12);

    if (index & 2)
        loadindex(&getindex1[0]);
    fprintf(outputFile, "	LDY ");
    printindex(ypos, index & 2);

    fprintf(outputFile, "	STY DF0WRITE\n");
    if (index & 1)
        loadindex(&getindex0[0]);
    fprintf(outputFile, "	LDA ");
    printindex(xpos, index & 1);
    fprintf(outputFile, "	STA DF0WRITE\n");
    fprintf(outputFile, "	lda #255\n	sta CALLFUNCTION\n");
}

void pfhline_DPCPlus(char *xpos, char *ypos, char *endXpos, int on_off_flip) {
    FILE *outputFile = getOutputFile();
    int index = 0;
    char getindex0[200];
    char getindex1[200];
    char getindex2[200];
    index |= getindex(xpos, &getindex0[0]);
    index |= getindex(ypos, &getindex1[0]) << 1;
    index |= getindex(endXpos, &getindex2[0]) << 2;

    fprintf(outputFile, "	lda #<C_function\n");
    fprintf(outputFile, "	sta DF0LOW\n");
    fprintf(outputFile, "	lda #(>C_function) & $0F\n");
    fprintf(outputFile, "	sta DF0HI\n");

    fprintf(outputFile, "	LDX #");
    fprintf(outputFile, "%d\n	STX DF0WRITE\n", on_off_flip | 8);

    if (index & 4)
        loadindex(&getindex2[0]);
    fprintf(outputFile, "	LDA ");
    printindex(endXpos, index & 4);

    fprintf(outputFile, "	STA DF0WRITE\n");

    if (index & 2)
        loadindex(&getindex1[0]);
    fprintf(outputFile, "	LDY ");
    printindex(ypos, index & 2);

    fprintf(outputFile, "	STY DF0WRITE\n");

    if (index & 1)
        loadindex(&getindex0[0]);
    fprintf(outputFile, "	LDA ");
    printindex(xpos, index & 1);

    fprintf(outputFile, "	STA DF0WRITE\n");
    fprintf(outputFile, "	lda #255\n	sta CALLFUNCTION\n");
}

void pfvline_DPCPlus(char *xpos, char *ypos, char *endYpos, int on_off_flip) {
    FILE *outputFile = getOutputFile();
    char getindex0[200];
    char getindex1[200];
    char getindex2[200];
    int index = 0;
    index |= getindex(xpos, &getindex0[0]);
    index |= getindex(ypos, &getindex1[0]) << 1;
    index |= getindex(endYpos, &getindex2[0]) << 2;

    fprintf(outputFile, "	lda #<C_function\n");
    fprintf(outputFile, "	sta DF0LOW\n");
    fprintf(outputFile, "	lda #(>C_function) & $0F\n");
    fprintf(outputFile, "	sta DF0HI\n");

    fprintf(outputFile, "	LDX #");
    fprintf(outputFile, "%d\n	STX DF0WRITE\n", on_off_flip | 4);

    if (index & 4)
        loadindex(&getindex2[0]);
    fprintf(outputFile, "	LDA ");
    printindex(endYpos, index & 4);
    fprintf(outputFile, "	STA DF0WRITE\n");

    if (index & 2)
        loadindex(&getindex1[0]);
    fprintf(outputFile, "	LDY ");
    printindex(ypos, index & 2);
    fprintf(outputFile, "	STY DF0WRITE\n");

    if (index & 1)
        loadindex(&getindex0[0]);
    fprintf(outputFile, "	LDA ");
    printindex(xpos, index & 1);

    fprintf(outputFile, "	STA DF0WRITE\n");
    fprintf(outputFile, "	lda #255\n	sta CALLFUNCTION\n");
}

