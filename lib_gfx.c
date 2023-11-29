//
// Created by Admin on 8/4/2023.
//

#include <stdlib.h>
#include <string.h>
#include "lib_gfx.h"
#include "statements.h"
#include "lib_dpcplus.h"

int ROMpf;
int lifekernel;

int sprite_index;
int extralabel;

int playfield_index[50];
int playfield_number;
int pfdata[50][256];
int pfcolorindexsave;
int pfcolornumber;

bool usePFHeights = false;
bool usePFColors  = false;
bool sizeOptimization = false;

//--------------------------------------------


void init_gfx_library() {
    sprite_index = 0;
    extralabel = 0;

    ROMpf = 0;
    lifekernel = 0;
    playfield_number = 0;
    playfield_index[0] = 0;
}


void set_kernel_type(const char *optionValue) {
    init_gfx_library();
    if (!strncmp(optionValue, "multisprite\0", 11)) {

        kernelType = MULTI_SPRITE_KERNEL;
        add_redefined_variable("multisprite", "1");
        create_includes("multisprite.inc");
        ROMpf = 1;

    } else if (!strncmp(optionValue, "DPC\0", 3)) {

        kernelType = DPC_PLUS_KERNEL;
        add_redefined_variable("multisprite", "2");
        create_includes("DPCplus.inc");
        set_banking(28, 7);

        add_redefined_variable("bankswitch_hotspot", "$1FF6");
        add_redefined_variable("bankswitch", "28");
        add_redefined_variable("bs_mask", "7");

    } else if (!strncmp(optionValue, "multisprite_no_include\0", 11)) {

        kernelType = MULTI_SPRITE_KERNEL;
        add_redefined_variable("multisprite", "1");
        ROMpf = 1;

    } else {
        prerror("set kernel: kernel name unknown or unspecified\n");
    }
    fprintf(stderr, "Using kernel: %s\n", optionValue);
}


void set_kernel_options(int options) {
    if (options & _pfheights) usePFHeights = true;
    if (options & _pfcolors)  usePFColors  = true;
}


void set_kernel_size_optimization(bool isSizeOptSet) {
    sizeOptimization = isSizeOptSet;
}

/**
 * Process a DATA chunk that's specifically for graphics:  sprites, playfield, etc.
 *
 * @return length of data
 */
int process_gfx_data(const char *label, const char *dataTypeName) {
    char data[200];
    int len = 0;

    sprintf(sprite_data[sprite_index++], "%s\n", label);
    while (len < 256) {
        if ((read_source_line(data)
             || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {

            // error message
            char errMissingEnd[200];
            sprintf(errMissingEnd, "Error: Missing \"end\" keyword at end of %s declaration\n", dataTypeName);
            prerror(errMissingEnd);
            exit(1);
        }
        incline();
        if (!strncmp(data, "end\0", 3))
            break;
        sprintf(sprite_data[sprite_index++], "	.byte %s\n", data);
        len++;
    }
    if (len > 255) {
        char errTooMuchData[200];
        sprintf(errTooMuchData, "Error: too much data in %s declaration\n", dataTypeName);
        prerror(errTooMuchData);
    }
    return len;
}




void output_sprite_data() {
    int i;
    goto_last_bank();

    FILE *outputFile = getOutputFile();
    for (i = 0; i < sprite_index; ++i) {
        fprintf(outputFile, "%s", sprite_data[i]);
    }
}

void output_playfield_data() {
    FILE *outputFile = getOutputFile();
    int i,j,k;
    // now we must regurgitate the PF data (if ROM-based playfield)

    if (ROMpf) {
        for (i = 0; i < playfield_number; ++i) {
            fprintf(outputFile, " if ((>(*+%d)) > (>*))\n ALIGN 256\n endif\n", playfield_index[i]);
            fprintf(outputFile, "PF1_data%d\n", i);
            for (j = playfield_index[i] - 1; j >= 0; j--) {
                fprintf(outputFile, " .byte %%");

                for (k = 15; k > 7; k--) {
                    if (pfdata[i][j] & (1 << k))
                        fprintf(outputFile, "1");
                    else
                        fprintf(outputFile, "0");
                }
                fprintf(outputFile, "\n");
            }

            fprintf(outputFile, " if ((>(*+%d)) > (>*))\n ALIGN 256\n endif\n", playfield_index[i]);
            fprintf(outputFile, "PF2_data%d\n", i);
            for (j = playfield_index[i] - 1; j >= 0; j--) {
                fprintf(outputFile, " .byte %%");
                for (k = 0; k < 8; ++k)    // reversed bit order!
                {
                    if (pfdata[i][j] & (1 << k))
                        fprintf(outputFile, "1");
                    else
                        fprintf(outputFile, "0");
                }
                fprintf(outputFile, "\n");
            }
        }
    }
}



//=============================================================
//----- Main commands for Graphics
//----------------------------------


void pfclear(char **statement) {
    FILE *outputFile = getOutputFile();
    char getindex0[200];

    invalidate_Areg();

    if (kernelType == DPC_PLUS_KERNEL) {
        pfclear_DPCPlus(statement);
        return;
    }

    if ((!statement[2][0]) || (statement[2][0] == ':'))
        fprintf(outputFile, "	LDA #0\n");
    else {
        int index = getindex(statement[2], &getindex0[0]);
        if (index)
            loadindex(&getindex0[0]);
        fprintf(outputFile, "	LDA ");
        printindex(statement[2], index);
    }
    removeCR(statement[1]);
    jsr(statement[1]);
}

void bkcolors(char **statement) {
    if (kernelType == DPC_PLUS_KERNEL) {
        bkcolors_DPCPlus(statement);
        return;            // get out
    } else {
        prerror("Error: bkcolors only works in DPC+ kernels\n");
        exit(1);
    }
}

/*
 * if pfread(xpos,ypos) then
 */
void pfread(char **statement) {
    invalidate_Areg();

    if (kernelType == DPC_PLUS_KERNEL) {
        pfread_DPCPlus(statement);

    } else {
        FILE *outputFile = getOutputFile();
        char getindex0[200];
        char getindex1[200];
        int index = 0;
        index |= getindex(statement[3], &getindex0[0]);
        index |= getindex(statement[4], &getindex1[0]) << 1;

        if (index & 1)
            loadindex(&getindex0[0]);
        fprintf(outputFile, "	LDA ");
        printindex(statement[4], index & 1);

        if (index & 2)
            loadindex(&getindex1[0]);
        fprintf(outputFile, "	LDY ");
        printindex(statement[6], index & 2);

        jsr("pfread");
    }
}



/**
 * pfpixel xpos ypos function
 *
 * @param statement
 */
void pfpixel(char **statement) {

    int on_off_flip = 0;
    if (kernelType == MULTI_SPRITE_KERNEL) {
        prerror("Error: pfpixel not supported in multisprite kernel\n");
        exit(1);
    }

    char *xpos = statement[2];
    char *ypos = statement[3];
    if (!strncmp(statement[4], "flip", 2))
        on_off_flip = 2;
    else if (!strncmp(statement[4], "off", 2))
        on_off_flip = 1;

    invalidate_Areg();


    if (kernelType == DPC_PLUS_KERNEL)        // DPC+
    {
        pfpixel_DPCPlus(xpos, ypos, on_off_flip);

    } else {                // Standard and MultiSprite kernels
        FILE *outputFile = getOutputFile();
        char getindex0[200];
        char getindex1[200];
        int index = 0;
        index |= getindex(xpos, &getindex0[0]);
        index |= getindex(ypos, &getindex1[0]) << 1;

        fprintf(outputFile, "	LDX #");
        fprintf(outputFile, "%d\n", on_off_flip);

        if (index & 2)
            loadindex(&getindex1[0]);
        fprintf(outputFile, "	LDY ");
        printindex(statement[3], index & 2);
        if (index & 1)
            loadindex(&getindex0[0]);
        fprintf(outputFile, "	LDA ");
        printindex(statement[2], index & 1);
        jsr("pfpixel");
    }
}






/**
 * Playfield command to draw horizontal line
 *
 * pfhline xpos ypos endxpos function
 *
 * @param statement
 */
void pfhline(char **statement) {

    int on_off_flip = 0;
    if (kernelType == MULTI_SPRITE_KERNEL) {
        prerror("Error: pfhline not supported in multisprite kernel\n");
        exit(1);
    }

    invalidate_Areg();

    char *xpos = statement[2];
    char *ypos = statement[3];
    char *endXpos = statement[4];
    if (!strncmp(statement[5], "flip", 2))
        on_off_flip = 2;
    else if (!strncmp(statement[5], "off", 2))
        on_off_flip = 1;


    if (kernelType == DPC_PLUS_KERNEL)        // DPC+
    {
        pfhline_DPCPlus(xpos, ypos, endXpos, on_off_flip);

    } else {
        FILE *outputFile = getOutputFile();
        int index = 0;
        char getindex0[200];
        char getindex1[200];
        char getindex2[200];
        index |= getindex(xpos, &getindex0[0]);
        index |= getindex(ypos, &getindex1[0]) << 1;
        index |= getindex(endXpos, &getindex2[0]) << 2;

        fprintf(outputFile, "	LDX #");
        fprintf(outputFile, "%d\n", on_off_flip);

        if (index & 4)
            loadindex(&getindex2[0]);
        fprintf(outputFile, "	LDA ");
        printindex(endXpos, index & 4);

        fprintf(outputFile, "	STA temp3\n");

        if (index & 2)
            loadindex(&getindex1[0]);
        fprintf(outputFile, "	LDY ");
        printindex(ypos, index & 2);

        if (index & 1)
            loadindex(&getindex0[0]);
        fprintf(outputFile, "	LDA ");
        printindex(xpos, index & 1);

        jsr("pfhline");

    }
}



/**
 * Playfield command to draw vertical line
 *
 * pfvline xpos ypos endypos function
 *
 * @param statement
 */
void pfvline(char **statement) {

    int on_off_flip = 0;
    if (kernelType == MULTI_SPRITE_KERNEL) {
        prerror("Error: pfvline not supported in multisprite kernel\n");
        exit(1);
    }

    invalidate_Areg();

    char *xpos = statement[2];
    char *ypos = statement[3];
    char *endYpos = statement[4];
    if (!strncmp(statement[5], "flip", 2))
        on_off_flip = 2;
    else if (!strncmp(statement[5], "off", 2))
        on_off_flip = 1;


    if (kernelType == DPC_PLUS_KERNEL)        // DPC+
    {
        pfvline_DPCPlus(xpos, ypos, endYpos, on_off_flip);

    } else {
        FILE *outputFile = getOutputFile();
        char getindex0[200];
        char getindex1[200];
        char getindex2[200];
        int index = 0;
        index |= getindex(xpos, &getindex0[0]);
        index |= getindex(ypos, &getindex1[0]) << 1;
        index |= getindex(endYpos, &getindex2[0]) << 2;

        fprintf(outputFile, "	LDX #");
        fprintf(outputFile, "%d\n", on_off_flip);

        if (index & 4)
            loadindex(&getindex2[0]);
        fprintf(outputFile, "	LDA ");
        printindex(endYpos, index & 4);
        fprintf(outputFile, "	STA temp3\n");

        if (index & 2)
            loadindex(&getindex1[0]);
        fprintf(outputFile, "	LDY ");
        printindex(ypos, index & 2);

        if (index & 1)
            loadindex(&getindex0[0]);
        fprintf(outputFile, "	LDA ");
        printindex(xpos, index & 1);
        jsr("pfvline");
    }
}



void pfscroll(char **statement) {
    FILE *outputFile = getOutputFile();
    char *scrollDir = statement[2];

    invalidate_Areg();
    switch (kernelType) {
        case DPC_PLUS_KERNEL:
            pfscroll_DPCPlus(statement, bbgetlinenumber());
            return;

        case MULTI_SPRITE_KERNEL: {
            fprintf(outputFile, "	LDA #");
            if (!strncmp(scrollDir, "up\0", 2))
                fprintf(outputFile, "0\n");
            else if (!strncmp(scrollDir, "down", 2))
                fprintf(outputFile, "1\n");
            else {
                prerror("pfscroll direction unknown in multisprite kernel\n");
                exit(1);
            }
            jsr("pfscroll");
        } break;

        //---------------- standard kernel / default
        default: {
            fprintf(outputFile, "	LDA #");
            if (!strncmp(scrollDir, "left", 2))
                fprintf(outputFile, "0\n");
            else if (!strncmp(scrollDir, "right", 2))
                fprintf(outputFile, "1\n");
            else if (!strncmp(scrollDir, "upup\0", 4))
                fprintf(outputFile, "6\n");
            else if (!strncmp(scrollDir, "downdown", 6))
                fprintf(outputFile, "8\n");
            else if (!strncmp(scrollDir, "up\0", 2))
                fprintf(outputFile, "2\n");
            else if (!strncmp(scrollDir, "down", 2))
                fprintf(outputFile, "4\n");
            else {
                prerror("pfscroll direction unknown\n");
                exit(1);
            }
            jsr("pfscroll");
        }
    }
}


/**
 * Process player graphics / player color data blocks
 *
 * Processes the following types of data statements:
 *           player0:
 *           player1color:
 *           player1-9:
 *           player1-9color:
 *
 * @param statement
 */
void player(char **statement) {
    FILE *outputFile = getOutputFile();

    int height = 0, i = 0;    //calculating sprite height
    int doingcolor = 0;        //doing player colors?
    char label[200];
    char j;
    char data[200];
    int heightrecord;

    //--- process parameters and figure out which type of statement this is
    char playerNum = statement[1][6];      // player number
    char *param = statement[2];     //-- will be ':' or '-'
    char *param2 = statement[3];    //-- will be secondary value for range
    bool isSimpleLabel = (param[0] == ':');
    bool isRangeLabel = ((param[0] == '-') && (statement[4][0] == ':'));
    bool isMultiplePlayers = (isRangeLabel) && (playerNum != '0');
    char rangeEnd   = param2[0];

    if (statement[1][7] == 'c')
        doingcolor = 1;
    if (isRangeLabel && (param2[1] == 'c')) //-- if secondary parameter has 'color' after it
        doingcolor = 1;

    // create labels for asm code
    if (!doingcolor)
        sprintf(label, "player%s_%c\n", statement[0], playerNum);
    else
        sprintf(label, "playercolor%s_%c\n", statement[0], playerNum);
    removeCR(label);

    if (kernelType == DPC_PLUS_KERNEL) {
        // handle DPC+ version
        if (playerNum != '0') {
            fprintf(outputFile, "	lda #<(playerpointers+%d)\n", (playerNum - 49) * 2 + 18 * doingcolor);
            fprintf(outputFile, "	sta DF0LOW\n");
            fprintf(outputFile, "	lda #(>(playerpointers+%d)) & $0F\n", (playerNum - 49) * 2 + 18 * doingcolor);
            fprintf(outputFile, "	sta DF0HI\n");
        }
        fprintf(outputFile, "	LDX #<%s\n", label);
        if (playerNum != '0') {
            fprintf(outputFile, "	STX DF0WRITE\n");
        } else {
            if (!doingcolor) {
                fprintf(outputFile, "	STX player%cpointerlo\n", playerNum);
            } else {
                fprintf(outputFile, "	STX player%ccolor\n", playerNum);
            }
        }
        fprintf(outputFile, "	LDA #((>%s) & $0f) | (((>%s) / 2) & $70)\n", label, label);    // DPC+
        if (playerNum != '0') {
            fprintf(outputFile, "	STA DF0WRITE\n");
        } else {
            if (!doingcolor) {
                fprintf(outputFile, "	STA player%cpointerhi\n", playerNum);
            } else {
                fprintf(outputFile, "	STA player%ccolor+1\n", playerNum);
            }
        }

        if (isMultiplePlayers)    // multiple players
        {
            for (j = playerNum; j < rangeEnd; j++) {
                fprintf(outputFile, "	STX DF0WRITE\n");
                fprintf(outputFile, "	STA DF0WRITE\n");    // creates multiple "copies" of single sprite
            }
        }

    } else {
        if (doingcolor) {
            fprintf(outputFile, "	LDX #<%s\n", label);
            fprintf(outputFile, "	STX player%ccolor\n", playerNum);
            fprintf(outputFile, "	LDA #>%s\n", label);
            fprintf(outputFile, "	STA player%ccolor+1\n", playerNum);
        } else {
            fprintf(outputFile, "	LDX #<%s\n", label);
            fprintf(outputFile, "	STX player%cpointerlo\n", playerNum);
            fprintf(outputFile, "	LDA #>%s\n", label);
            fprintf(outputFile, "	STA player%cpointerhi\n", playerNum);
        }
    }

    //fprintf(outputFile, "    JMP .%sjump%c\n",statement[0],playerNum);

    // insert DASM stuff to prevent page-wrapping of player data
    // stick this in a data file instead of displaying

    if (kernelType != DPC_PLUS_KERNEL)    // DPC+ has no pages to wrap
    {
        heightrecord = sprite_index++;
        sprite_index += 2;
        // record index for creation of the line below
    }
    if (kernelType == MULTI_SPRITE_KERNEL) {
        sprintf(sprite_data[sprite_index++],
                " if (<*) < 90\n");    // is 90 enough? could this be the cause of page wrapping issues at the bottom of the screen?
        // This is potentially wasteful, therefore the user now has an option to use this space for data or code
        // (Not an ideal way, but a way nonetheless)
        if (sizeOptimization) {
            sprintf(sprite_data[sprite_index++], "extralabel%d\n", extralabel);
            sprintf(sprite_data[sprite_index++], " ifconst extra\n");
            for (i = 4; i >= 0; i--) {
                if (i == 4)
                    sprintf(sprite_data[sprite_index++], " if (extra > %d)\n", i);
                else
                    sprintf(sprite_data[sprite_index++], " else\n if (extra > %d)\n", i);
                sprintf(sprite_data[sprite_index++], "extra set extra-1\n");
                sprintf(sprite_data[sprite_index++], " extra%d\n", i);
            }
            sprintf(sprite_data[sprite_index++], " endif\n endif\n endif\n endif\n endif\n endif\n");

            sprintf(sprite_data[sprite_index++], " echo [90-(<*)]d,\"bytes found in extra%d", extralabel);
            sprintf(sprite_data[sprite_index++], " (\",[(*-extralabel%d)]d,\"used)\"\n", extralabel);
            sprintf(sprite_data[sprite_index++], " if (<*) < 90\n");    // do it again
            extralabel++;
        }
        sprintf(sprite_data[sprite_index++], "	repeat (90-<*)\n	.byte 0\n");
        sprintf(sprite_data[sprite_index++], "	repend\n	endif\n");
        if (sizeOptimization)
            sprintf(sprite_data[sprite_index++], "	endif\n");
    }                // potential bug: should this go after the below page wrapping stuff to prevent possible issues?

    sprintf(sprite_data[sprite_index++], "%s\n", label);
    if (kernelType == MULTI_SPRITE_KERNEL && playerNum == '0') {
        sprintf(sprite_data[sprite_index++], "	.byte 0\n");
    }

    while (1) {
        if ((read_source_line(data)
             || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {

            prerror("Error: Missing \"end\" keyword at end of player declaration\n");
            exit(1);
        }
        incline();
        if (!strncmp(data, "end\0", 3))
            break;
        height++;
        sprintf(sprite_data[sprite_index++], "	.byte %s\n", data);

    }


    if (kernelType == MULTI_SPRITE_KERNEL && playerNum == '0')
        height++;

// record height and add page-wrap prevention
    if (kernelType != DPC_PLUS_KERNEL)    // DPC+ has no pages to wrap
    {
        sprintf(sprite_data[heightrecord], " if (<*) > (<(*+%d))\n", height - 1);    //+1);
        sprintf(sprite_data[heightrecord + 1], "	repeat ($100-<*)\n	.byte 0\n");
        sprintf(sprite_data[heightrecord + 2], "	repend\n	endif\n");
    }
    if (kernelType == MULTI_SPRITE_KERNEL && playerNum == '0')
        height--;

//  fprintf(outputFile, ".%sjump%c\n",statement[0],playerNum);
    if (kernelType == MULTI_SPRITE_KERNEL)
        fprintf(outputFile, "	LDA #%d\n", height + 1);    //2);
    else if (kernelType == DPC_PLUS_KERNEL && (!doingcolor))
        fprintf(outputFile, "	LDA #%d\n", height);
    else if (!doingcolor)
        fprintf(outputFile, "	LDA #%d\n", height - 1);    // added -1);
    if (!doingcolor)
        fprintf(outputFile, "	STA player%cheight\n", playerNum);

    if ((statement[1][7] == '-') && (kernelType == DPC_PLUS_KERNEL) && (playerNum != '0'))    // multiple players
    {
        for (j = statement[1][6] + 1; j <= statement[1][8]; j++) {
            if (!doingcolor)
                fprintf(outputFile, "	STA player%cheight\n", j);
        }
    }
}



void process_pfheight() {
    FILE *outputFile = getOutputFile();
    char data[200];
    int pfpos = 0, indexsave;
    bool bothColorAndHeight = (usePFColors && usePFHeights);

    if (bothColorAndHeight) {

        sprintf(sprite_data[sprite_index++], " ifconst pfres\n");
        sprintf(sprite_data[sprite_index++], " if (<*) > (254-pfres)\n");
        sprintf(sprite_data[sprite_index++], "	align 256\n	endif\n");
        sprintf(sprite_data[sprite_index++], " if (<*) < (136-pfres*pfwidth)\n");
        sprintf(sprite_data[sprite_index++], "	repeat ((136-pfres*pfwidth)-(<*))\n	.byte 0\n");
        sprintf(sprite_data[sprite_index++], "	repend\n	endif\n");
        sprintf(sprite_data[sprite_index++], " else\n");
        sprintf(sprite_data[sprite_index++], " if (<*) > 206\n");
        sprintf(sprite_data[sprite_index++], "	align 256\n	endif\n");
        sprintf(sprite_data[sprite_index++], " if (<*) < 88\n");
        sprintf(sprite_data[sprite_index++], "	repeat (88-(<*))\n	.byte 0\n");
        sprintf(sprite_data[sprite_index++], "	repend\n	endif\n");
        sprintf(sprite_data[sprite_index++], " endif\n");
        sprintf(sprite_data[sprite_index++], "playfieldcolorandheight\n");

        pfcolorindexsave = sprite_index;
        while (1) {
            if ((read_source_line(data)
                 || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {

                prerror("Error: Missing \"end\" keyword at end of pfheight declaration\n");
                exit(1);
            }
            incline();
            if (!strncmp(data, "end\0", 3))
                break;
            removeCR(data);
            if (!pfpos)
                fprintf(outputFile, " lda #%s\n sta playfieldpos\n", data);
            else
                sprintf(sprite_data[sprite_index++], " .byte %s,0,0,0\n", data);
            pfpos++;
        }

    } else if (usePFHeights) {

        sprintf(sprite_data[sprite_index++], " ifconst pfres\n");
        sprintf(sprite_data[sprite_index++], " if (<*) > 235-pfres\n");
        sprintf(sprite_data[sprite_index++], "	repeat (265+pfres-<*)\n	.byte 0\n");
        sprintf(sprite_data[sprite_index++], "	repend\n	endif\n");
        sprintf(sprite_data[sprite_index++], "   if (<*) < (pfres+9)\n");
        sprintf(sprite_data[sprite_index++], "	repeat ((pfres+9)-(<*))\n	.byte 0\n");
        sprintf(sprite_data[sprite_index++], "	repend\n");
        sprintf(sprite_data[sprite_index++], "   endif\n");
        sprintf(sprite_data[sprite_index++], " else\n");
        sprintf(sprite_data[sprite_index++], "   if (<*) > 223\n");
        sprintf(sprite_data[sprite_index++], "	repeat (277-<*)\n	.byte 0\n");
        sprintf(sprite_data[sprite_index++], "	repend\n");
        sprintf(sprite_data[sprite_index++], "   endif\n");
        sprintf(sprite_data[sprite_index++], "   if (<*) < 21\n");
        sprintf(sprite_data[sprite_index++], "	repeat (21-(<*))\n	.byte 0\n");
        sprintf(sprite_data[sprite_index++], "	repend\n");
        sprintf(sprite_data[sprite_index++], "   endif\n");
        sprintf(sprite_data[sprite_index++], " endif\n");
        sprintf(sprite_data[sprite_index], "pfcolorlabel%d\n", sprite_index);
        indexsave = sprite_index;
        sprite_index++;
        while (1) {
            if ((read_source_line(data)
                 || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {

                prerror("Error: Missing \"end\" keyword at end of pfheight declaration\n");
                exit(1);
            }
            incline();
            if (!strncmp(data, "end\0", 3))
                break;
            removeCR(data);
            if (!pfpos)
                fprintf(outputFile, " lda #%s\n sta playfieldpos\n", data);
            else
                sprintf(sprite_data[sprite_index++], " .byte %s\n", data);
            pfpos++;
        }
        fprintf(outputFile, " ifconst pfres\n");
        fprintf(outputFile, " lda #>(pfcolorlabel%d-pfres-9)\n", indexsave);
        fprintf(outputFile, " else\n");
        fprintf(outputFile, " lda #>(pfcolorlabel%d-21)\n", indexsave);
        fprintf(outputFile, " endif\n");
        fprintf(outputFile, " sta pfcolortable+1\n");
        fprintf(outputFile, " ifconst pfres\n");
        fprintf(outputFile, " lda #<(pfcolorlabel%d-pfres-9)\n", indexsave);
        fprintf(outputFile, " else\n");
        fprintf(outputFile, " lda #<(pfcolorlabel%d-21)\n", indexsave);
        fprintf(outputFile, " endif\n");
        fprintf(outputFile, " sta pfcolortable\n");
    } else {
        prerror("PFheights kernel option not set");
        exit(1);
    }
}

void process_pfcolor(char **statement) {
    FILE *outputFile = getOutputFile();

    char data[200];
    char rewritedata[200];
    int i = 0, j = 0;
    int pfpos = 0, pfoffset = 0;

    if (kernelType == DPC_PLUS_KERNEL) {
        playfieldcolorandheight_DPCPlus(statement);

        return;        // get out
    }

    bool bothColorAndHeight = (usePFColors && usePFHeights);

    if (bothColorAndHeight) {
        // pf color fixed table
        while (1) {
            if ((read_source_line(data)
                 || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {
                prerror("Error: Missing \"end\" keyword at end of pfcolor declaration\n");
                exit(1);
            }
            incline();
            if (!strncmp(data, "end\0", 3))
                break;
            removeCR(data);

            if (!pfpos)
                fprintf(outputFile, " lda #%s\n sta COLUPF\n", data);
            else {
                j = 0;
                i = 0;
                while (!j) {
                    if (sprite_data[pfcolorindexsave + pfoffset][i++] == ',')
                        j++;
                    if (i > 199) {
                        prerror("Warning: size of subsequent pfcolor tables should match\n");
                        break;
                    }
                }
                //fprintf(stderr,"%s\n",sprite_data[pfcolorindexsave+pfoffset]);
                strcpy(rewritedata, sprite_data[pfcolorindexsave + pfoffset]);
                rewritedata[i - 1] = '\0';
                if (i < 200)
                    sprintf(sprite_data[pfcolorindexsave + pfoffset++], "%s,%s%s", rewritedata, data,
                            rewritedata + i + 1);
            }
            pfpos++;
        }

    } else if (usePFColors) {
        if (!pfcolornumber) {
            sprintf(sprite_data[sprite_index++], " ifconst pfres\n");
            sprintf(sprite_data[sprite_index++], " if (<*) > (254-pfres*pfwidth)\n");
            sprintf(sprite_data[sprite_index++], "	align 256\n	endif\n");
            sprintf(sprite_data[sprite_index++], " if (<*) < (136-pfres*pfwidth)\n");
            sprintf(sprite_data[sprite_index++], "	repeat ((136-pfres*pfwidth)-(<*))\n	.byte 0\n");
            sprintf(sprite_data[sprite_index++], "	repend\n	endif\n");
            sprintf(sprite_data[sprite_index++], " else\n");

            sprintf(sprite_data[sprite_index++], " if (<*) > 206\n");
            sprintf(sprite_data[sprite_index++], "	align 256\n	endif\n");
            sprintf(sprite_data[sprite_index++], " if (<*) < 88\n");
            sprintf(sprite_data[sprite_index++], "	repeat (88-(<*))\n	.byte 0\n");
            sprintf(sprite_data[sprite_index++], "	repend\n	endif\n");
            sprintf(sprite_data[sprite_index++], " endif\n");
            sprintf(sprite_data[sprite_index], "pfcolorlabel%d\n", sprite_index);
            sprite_index++;
        }
        //indexsave=sprite_index;
        pfoffset = 1;
        while (1) {
            if ((read_source_line(data)
                 || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {
                prerror("Error: Missing \"end\" keyword at end of pfcolor declaration\n");
                exit(1);
            }
            incline();
            if (!strncmp(data, "end\0", 3))
                break;
            removeCR(data);
            if (!pfpos) {
                fprintf(outputFile, " lda #%s\n sta COLUPF\n", data);
                if (!pfcolornumber)
                    pfcolorindexsave = sprite_index - 1;
                pfcolornumber = (pfcolornumber + 1) % 4;
                pfpos++;
            } else {
                if (pfcolornumber != 1)    // add to existing table
//        if ((pfcolornumber%3)!=1) // add to existing table (possible correction?)
                {
                    j = 0;
                    i = 0;
                    while (j != (pfcolornumber + 3) % 4) {
                        if (sprite_data[pfcolorindexsave + pfoffset][i++] == ',')
                            j++;
                        if (i > 199) {
                            prerror("Warning: size of subsequent pfcolor tables should match\n");
                            break;
                        }
                    }
//fprintf(stderr,"%s\n",sprite_data[pfcolorindexsave+pfoffset]);
                    strcpy(rewritedata, sprite_data[pfcolorindexsave + pfoffset]);
                    rewritedata[i - 1] = '\0';
                    if (i < 200)
                        sprintf(sprite_data[pfcolorindexsave + pfoffset++], "%s,%s%s", rewritedata, data,
                                rewritedata + i + 1);
                } else    // new table
                {
                    sprintf(sprite_data[sprite_index++], " .byte %s,0,0,0\n", data);
                    // pad with zeros - later we can fill this with additional color data if defined
                }
            }
        }
        fprintf(outputFile, " ifconst pfres\n");
        fprintf(outputFile, " lda #>(pfcolorlabel%d-%d+pfres*pfwidth)\n", pfcolorindexsave,
                85 + 48 - pfcolornumber - ((((pfcolornumber << 1) | (pfcolornumber << 2)) ^ 4) & 4));
        fprintf(outputFile, " else\n");
        fprintf(outputFile, " lda #>(pfcolorlabel%d-%d)\n", pfcolorindexsave,
                85 - pfcolornumber - ((((pfcolornumber << 1) | (pfcolornumber << 2)) ^ 4) & 4));
        fprintf(outputFile, " endif\n");
        fprintf(outputFile, " sta pfcolortable+1\n");
        fprintf(outputFile, " ifconst pfres\n");
        fprintf(outputFile, " lda #<(pfcolorlabel%d-%d+pfres*pfwidth)\n", pfcolorindexsave,
                85 + 48 - pfcolornumber - ((((pfcolornumber << 1) | (pfcolornumber << 2)) ^ 4) & 4));
        fprintf(outputFile, " else\n");
        fprintf(outputFile, " lda #<(pfcolorlabel%d-%d)\n", pfcolorindexsave,
                85 - pfcolornumber - ((((pfcolornumber << 1) | (pfcolornumber << 2)) ^ 4) & 4));
        fprintf(outputFile, " endif\n");
        fprintf(outputFile, " sta pfcolortable\n");
    } else {
        prerror("PFcolors kernel option not set");
        exit(1);
    }
}

void playfieldcolorandheight(char **statement) {

    // PF colors and/or heights
    // PFheights use offset of 21-31
    // PFcolors use offset of 84-124
    // if used together: playfieldblocksize-88, playfieldcolor-87
    if (!strncasecmp(statement[1], "pfheights:\0", 9)) {
        process_pfheight();
    } else {           // has to be pfcolors
        process_pfcolor(statement);
    }
}


void playfield(char **statement) {
    FILE *outputFile = getOutputFile();

    // for specifying a ROM playfield
    char zero = '.';
    char one = 'X';
    int i, j, k, height = 0;
    char data[200];
    char pframdata[255][40];
    if ((unsigned char) statement[3][0] > (unsigned char) 0x20)
        zero = statement[3][0];
    if ((unsigned char) statement[4][0] > (unsigned char) 0x20)
        one = statement[4][0];

    // read data until we get an end
    // stored in global var and output at end of code

    while (1) {
        bool failed_to_read = read_source_line(data);
        if (!failed_to_read) trim_string(data, true);

        if (failed_to_read || ((data[0] != zero) && (data[0] != one) && (data[0] != 'e'))) {
            prerror("Error: Missing \"end\" keyword at end of playfield declaration\n");
            //fprintf(stderr, "  Found: '%c' '%c'\n", data[0], zero);
            //fprintf(stderr, " failed_to_read: %s\n", failed_to_read ? "true" : "false");
            exit(1);
        }
        incline();
        if (!strncmp(data, "end\0", 3))
            break;
        if (ROMpf)        // if playfield is in ROM:
        {
            int curPfIdx = playfield_index[playfield_number];
            pfdata[playfield_number][curPfIdx] = 0;
            for (i = 0; i < 32; ++i) {
                if ((data[i] != zero) && (data[i] != one))
                    break;
                pfdata[playfield_number][curPfIdx] <<= 1;
                if (data[i] == one)
                    pfdata[playfield_number][curPfIdx] |= 1;
            }
            playfield_index[playfield_number]++;
        } else {
            strncpy(pframdata[height], data, 40);

            // translate 0s and 1s here
            // the below should be changed to check for zero instead of defaulting to it
            for (k = 0; k < 32; ++k)
                pframdata[height][k] = (pframdata[height][k] == one) ? '1' : '0';
            height++;
        }

    }


    if (ROMpf)            // if playfield is in ROM:
    {
        fprintf(outputFile, "	LDA #<PF1_data%d\n", playfield_number);
        fprintf(outputFile, "	STA PF1pointer\n");
        fprintf(outputFile, "	LDA #>PF1_data%d\n", playfield_number);
        fprintf(outputFile, "	STA PF1pointer+1\n");

        fprintf(outputFile, "	LDA #<PF2_data%d\n", playfield_number);
        fprintf(outputFile, "	STA PF2pointer\n");
        fprintf(outputFile, "	LDA #>PF2_data%d\n", playfield_number);
        fprintf(outputFile, "	STA PF2pointer+1\n");
        playfield_number++;
    } else if (kernelType != DPC_PLUS_KERNEL)        // RAM pf, as in std_kernel, not DPC+
    {
        fprintf(outputFile, "  ifconst pfres\n");
        fprintf(outputFile, "	  ldx #(%d>pfres)*(pfres*pfwidth-1)+(%d<=pfres)*%d\n", height, height, height * 4 - 1);
        fprintf(outputFile, "  else\n");
        fprintf(outputFile, "	  ldx #((%d*pfwidth-1)*((%d*pfwidth-1)<47))+(47*((%d*pfwidth-1)>=47))\n", height, height, height);
        fprintf(outputFile, "  endif\n");
        fprintf(outputFile, "	jmp pflabel%d\n", playfield_number);

        // no need to align to page boundaries

        char pfBinData0[9] = "00000000";
        char pfBinData1[9] = "00000000";
        char pfBinData2[9] = "00000000";
        char pfBinData3[9] = "00000000";

        fprintf(outputFile, "PF_data%d\n", playfield_number);
        for (j = 0; j < height; ++j)    // stored right side up
        {
            // split apart playfield data
            for (k = 0; k < 8; ++k)    pfBinData0[k]    = pframdata[j][k];
            for (k = 15; k >= 8; k--)  pfBinData1[15-k] = pframdata[j][k];
            for (k = 16; k < 24; ++k)  pfBinData2[k-16] = pframdata[j][k];
            for (k = 31; k >= 24; k--) pfBinData3[31-k] = pframdata[j][k];

            fprintf(outputFile, "	.byte %%%s, %%%s", pfBinData0, pfBinData1);
            fprintf(outputFile, "\n	if (pfwidth>2)\n	.byte %%%s, %%%s", pfBinData2, pfBinData3);
            fprintf(outputFile, "\n endif\n");
        }

        fprintf(outputFile, "pflabel%d\n", playfield_number);
        fprintf(outputFile, "	lda PF_data%d,x\n", playfield_number);
        if (hasSuperchip()) {
            //        fprintf(outputFile, "  ifconst pfres\n");
            //      fprintf(outputFile, "      sta playfield+48-pfres*pfwidth-128,x\n");
            //    fprintf(outputFile, "  else\n");
            fprintf(outputFile, "	sta playfield-128,x\n");
            //  fprintf(outputFile, "  endif\n");
        } else {
            //        fprintf(outputFile, "  ifconst pfres\n");
            //      fprintf(outputFile, "      sta playfield+48-pfres*pfwidth,x\n");
            //    fprintf(outputFile, "  else\n");
            fprintf(outputFile, "	sta playfield,x\n");
            //  fprintf(outputFile, "  endif\n");
        }
        fprintf(outputFile, "	dex\n");
        fprintf(outputFile, "	bpl pflabel%d\n", playfield_number);
        playfield_number++;

    } else            // RAM pf in DPC+
    {
        playfield_number++;

        // height is pf data height
        fprintf(outputFile, " ldy #%d\n", height);
        fprintf(outputFile, "	LDA #<PF_data%d\n", playfield_number);
        fprintf(outputFile, "	LDX #((>PF_data%d) & $0f) | (((>PF_data%d) / 2) & $70)\n", playfield_number, playfield_number);
        jsrbank1("pfsetup");

        // use sprite data recorder for pf data
        sprintf(sprite_data[sprite_index++], "PF_data%d\n", playfield_number);

        char binData[9] = "00000000";
        for (j = 0; j < height; ++j)    // stored right side up
        {
            i = 0;
            for (k = 31; k >= 24; k--) binData[i++] = pframdata[j][k];
            sprintf(data, "	.byte %%%s\n", binData);
            strcpy(sprite_data[sprite_index++], data);
        }

        for (j = 0; j < height; ++j)    // stored right side up
        {
            i = 0;
            for (k = 16; k < 24; ++k) binData[i++] = pframdata[j][k];
            sprintf(data, "	.byte %%%s\n", binData);
            strcpy(sprite_data[sprite_index++], data);
        }

        for (j = 0; j < height; ++j)    // stored right side up
        {
            i = 0;
            for (k = 15; k >= 8; k--) binData[i++] = pframdata[j][k];
            sprintf(data, "	.byte %%%s\n", binData);
            strcpy(sprite_data[sprite_index++], data);
        }

        for (j = 0; j < height; ++j)    // stored right side up
        {
            i = 0;
            for (k = 0; k < 8; ++k) binData[i++] = pframdata[j][k];
            sprintf(data, "	.byte %%%s\n", binData);
            strcpy(sprite_data[sprite_index++], data);
        }

    }

}




void lives(char **statement) {
    FILE *outputFile = getOutputFile();

    int i = 0;
    char label[200];
    char data[200];
    if (!lifekernel) {
        lifekernel = 1;
        //strcpy(redefined_variables[numredefvars++],"lifekernel = 1");
    }

    sprintf(label, "lives__%s\n", statement[0]);
    removeCR(label);

    fprintf(outputFile, "	LDA #<%s\n", label);
    fprintf(outputFile, "	STA lifepointer\n");

    fprintf(outputFile, "	LDA lifepointer+1\n");
    fprintf(outputFile, "	AND #$E0\n");
    fprintf(outputFile, "	ORA #(>%s)&($1F)\n", label);
    fprintf(outputFile, "	STA lifepointer+1\n");

    sprintf(sprite_data[sprite_index++], " if (<*) > (<(*+8))\n");
    sprintf(sprite_data[sprite_index++], "	repeat ($100-<*)\n	.byte 0\n");
    sprintf(sprite_data[sprite_index++], "	repend\n	endif\n");

    sprintf(sprite_data[sprite_index++], "%s\n", label);

    for (i = 0; i < 9; ++i) {
        bool failed_to_read = read_source_line(data);
        if (!failed_to_read) trim_string(data, true);

        if (failed_to_read
            || (((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {

            prerror("Error: Not enough data or missing \"end\" keyword at end of lives declaration\n");
            exit(1);
        }
        incline();
        if (!strncmp(data, "end\0", 3))
            break;
        sprintf(sprite_data[sprite_index++], "	.byte %s", data);
    }
}


void scorecolors(char **statement) {
    if (kernelType != DPC_PLUS_KERNEL) {
        prerror("Error: scorecolors is only supported in the DPC+ kernel\n");
        exit(1);
    }

    FILE *outputFile = getOutputFile();
    int i = 0;    //height can change
    char data[200];
    fprintf(outputFile, "	lda #<scoredata\n");
    fprintf(outputFile, "	STA DF0LOW\n");

    fprintf(outputFile, "	lda #((>scoredata) & $0f)\n");
    fprintf(outputFile, "	STA DF0HI\n");
    for (i = 0; i < 9; ++i) {
        if (read_source_line(data)) {
            prerror("Error: Not enough data for scorecolor declaration\n");
            exit(1);
        }
        incline();
        if (!strncmp(data, "end\0", 3))
            break;
        if (i == 8) {
            prerror("Error: Missing \"end\" keyword at end of scorecolor declaration\n");
            exit(1);
        }
        fprintf(outputFile, "	lda ");
        printimmed(data);
        fprintf(outputFile, "%s\n", data);
        fprintf(outputFile, "	sta DF0WRITE\n");
    }
}

void drawscreen() {
    invalidate_Areg();
    if (kernelType == DPC_PLUS_KERNEL)
        jsrbank1("drawscreen");
    else
        jsr("drawscreen");
}
