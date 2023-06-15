//
// Created by Admin on 6/13/2023.
//

#include <stdlib.h>
#include <string.h>

#include "gencode_dpcplus.h"
#include "statements.h"


void do_push(char **statement) {
    int k, i = 2;
    // syntax: push [vars]
    // eg: vars can be a b c d e or a-e
    removeCR(statement[4]);
    if (statement[3][0] == '-')    // range
    {
        printf("	ldx #255-%s+%s\n", statement[4], statement[2]);
        printf("pushlabel%s\n", statement[0]);
        printf("	lda %s+1,x\n", statement[4]);
        printf("	sta DF7PUSH\n");
        printf("	inx\n");
        printf("	bmi pushlabel%s\n", statement[0]);
    } else {
        while ((statement[i][0] != ':') && (statement[i][0] != '\0')) {
            for (k = 0; k < 200; ++k)
                if ((statement[i][k] == (unsigned char) 0x0A) || (statement[i][k] == (unsigned char) 0x0D))
                    statement[i][k] = '\0';
            printf("	lda %s\n", statement[i++]);
            printf("	sta DF7PUSH\n");
        }
    }

}


void do_pull(char **statement) {
    int k, i = 2;
    int savei;
    // syntax: pull [vars]
    // eg: vars can be a b c d e or a-e
    removeCR(statement[4]);
    if (statement[3][0] == '-')    // range
    {
        printf("	ldx #%s-%s\n", statement[4], statement[2]);
        printf("pulllabel%s\n", statement[0]);
        printf("	lda DF7DATA\n");
        printf("	sta %s,x\n", statement[2]);
        printf("	dex\n");
        printf("	bpl pulllabel%s\n", statement[0]);
    } else {

        savei = i;
        while ((statement[i][0] != ':') && (statement[i][0] != '\0'))
            i++;
        i--;
        while (i >= savei) {
            for (k = 0; k < 200; ++k)
                if ((statement[i][k] == (unsigned char) 0x0A) || (statement[i][k] == (unsigned char) 0x0D))
                    statement[i][k] = '\0';
            printf("	lda DF7DATA\n");
            printf("	sta %s\n", statement[i--]);
        }
    }

}

/**
 * Do Stack operation (DPC+ only)
 * @param statement
 */
void do_stack(char **statement) {
    removeCR(statement[2]);
    if (isimmed(statement[2])) {
        printf("	lda #<(STACKbegin+%s)\n", statement[2]);
        printf("	STA DF7LOW\n");
        printf("	lda #(>(STACKbegin+%s)) & $0F\n", statement[2]);
        printf("	STA DF7HI\n");
    } else {
        printf("LDA #<STACKbegin\n");
        printf("clc\n");
        printf("adc %s\n", statement[2]);
        printf("STA DF7LOW\n");
        printf("LDA #>STACKbegin\n");
        printf("adc #0\n");
        printf("AND #$0F\n");
        printf("STA DF7HI\n");
    }
}


void pfscroll_DPCPlus(char **statement, int lineNum) {
    //DPC+ version of function uses the syntax: pfscroll #LINES [start queue#] [end queue#]
    if ((!strncmp(statement[2], "up\0", 2)) ||
        (!strncmp(statement[2], "down\0", 4)) ||
        (!strncmp(statement[2], "right\0", 5)) || (!strncmp(statement[2], "left\0", 4))) {
        fprintf(stderr, "(%d) pfscroll for DPC+ doesn't use up/down/left/right. try a value or variable instead.\n",
                lineNum);
        exit(1);
    }
    printf(" lda #<C_function\n");
    printf(" sta DF0LOW\n");
    printf(" lda #(>C_function) & $0F\n");
    printf(" sta DF0HI\n");

    printf(" lda #32\n");
    printf(" sta DF0WRITE\n");

    if ((statement[2][0] >= '0') && (statement[2][0] <= '9'))
        printf(" LDA #%s\n", statement[2]);
    else
        printf(" LDA %s\n", statement[2]);
    printf(" sta DF0WRITE\n");

    if ((statement[3][0] >= '0') && (statement[3][0] <= '6')) {
        if ((statement[4][0] >= '0') && (statement[4][0] <= '6')) {
            printf(" LDA #%d\n", statement[3][0] - '0');
            printf(" sta DF0WRITE\n");
            printf(" LDA #%d\n", statement[4][0] - '0' + 1);
            printf(" sta DF0WRITE\n");
        } else {
            fprintf(stderr, "(%d) initial queue provided for DPC+ pfscroll, but invalid end queue was provided.\n",
                    lineNum);
            exit(1);
        }
    } else {
        //the default is to scroll all playfield columns, without color scrolling
        printf(" LDA #0\n");
        printf(" sta DF0WRITE\n");
        printf(" LDA #4\n");
        printf(" sta DF0WRITE\n");
    }

    printf(" lda #255\n");
    printf(" sta CALLFUNCTION\n");
}

void pfclear_DPCPlus(char **statement) {
    char getindex0[200];
    int index = 0;

    printf("	lda #<C_function\n");
    printf("	sta DF0LOW\n");
    printf("	lda #(>C_function) & $0F\n");
    printf("	sta DF0HI\n");
    printf("	ldx #28\n");
    printf("	stx DF0WRITE\n");

    if ((!statement[2][0]) || (statement[2][0] == ':'))
        printf("	LDA #0\n");
    else {
        index = getindex(statement[2], &getindex0[0]);
        if (index)
            loadindex(&getindex0[0]);
        printf("	LDA ");
        printindex(statement[2], index);
    }
    printf("	sta DF0WRITE\n");
    printf("	lda #255\n	sta CALLFUNCTION\n");
}


void genCode_DPCPlusCollision(char firstPlayerParam, char secondPlayerParam) {
    printf("	lda #<C_function\n");
    printf("	sta DF0LOW\n");
    printf("	lda #(>C_function) & $0F\n");
    printf("	sta DF0HI\n");
    printf("  lda #20\n");
    printf("  sta DF0WRITE\n");
    printf("  lda #%c\n", firstPlayerParam);
    printf("  sta DF0WRITE\n");
    printf("  lda #%c\n", secondPlayerParam);
    printf("  sta DF0WRITE\n");
    printf("  lda #255\n");
    printf("  sta CALLFUNCTION\n");
    printf("  BIT DF0DATA\n");
}