// Provided under the GPL v2 license. See the included LICENSE.txt for details.

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "statements.h"
#include "keywords.h"
#include "lexer.h"

#include "lib_gfx.h"
#include "lib_dpcplus.h"

char includespath[500];
char user_includes[1000];

int includesfile_already_done = 0;
int decimal = 0;

char redefined_variables[500][100];
char constants[MAXCONSTANTS][100];

int bs;
int bank;
enum KernelType kernelType;
int kernel_options;
int smartbranching;
int superchip;
int last_bank;
int optimization;

int extra;
int extraactive;
int macroactive;

int ongosub;
int condpart;

int numfors;
char forvar[50][50];
char forlabel[50][50];
char forstep[50][50];
char forend[50][50];

char fixpoint44[2][50][50];
char fixpoint88[2][50][50];

int numfixpoint44;
int numfixpoint88;
int numredefvars;
int numconstants;
int line;
int numjsrs;
int doingfunction;
char Areg[50];
int branchtargetnumber;


void init_statement_processor() {
    condpart = 0;

    last_bank = 0;        // last bank when bs is enabled (0 for 2k/4k)
    bank = 1;            // current bank: 1 or 2 for 8k
    bs = 0;            // bankswtiching type; 0=none

    kernelType = STANDARD_KERNEL;

    ongosub = 0;
    superchip = 0;
    optimization = 0;
    smartbranching = 0;
    line = 0;
    numfixpoint44 = 0;
    numfixpoint88 = 0;

    ors = 0;
    numjsrs = 0;
    numfors = 0;
    numthens = 0;
    numelses = 0;
    numredefvars = 0;
    numconstants = 0;
    branchtargetnumber = 0;
    doingfunction = 0;

    extra = 0;
    extraactive = 0;
    macroactive = 0;
}

//-----------------------------------------------------
//--- Source File processing

static FILE *sourceFile = 0;
static FILE *outputFile = 0;

void use_source_file(FILE *sourceFileToUse) {
    sourceFile = sourceFileToUse;
}

void use_output_file(FILE *outputFileToUse) {
    outputFile = outputFileToUse;
}

FILE *getOutputFile() { return outputFile; }

/**
 * Read line from source code stream
 *
 * @param data - populated with new line
 * @return True if no more data
 */
bool read_source_line(char *data) {
    bool success = fgets(data, MAX_LINE_LENGTH, sourceFile);
    if (success) preprocess(data);
    return !success;
}

/**
 * Read unprocessed line from source code stream
 *
 * -- Used as ASM line pass-thru
 *
 * @param data - populated with new line
 * @return True if no more data
 */
bool read_original_line(char *data) {
    bool success = fgets(data, MAX_LINE_LENGTH, sourceFile);
    return !success;
}

#define IS_WHITESPACE(c) (((c)==' ')||((c)=='\n')||((c)=='\r')||((c)=='\t'))

void trim_string(char *data, bool addEol) {

    // trim whitespace off end of line
    int strEnd = strlen(data);
    while(strEnd>0 && (IS_WHITESPACE(data[strEnd]))) strEnd--;
    if (addEol) data[strEnd++] = '\n';
    data[strEnd++] = '\0';

    // trim whitespace from beginning of line
    if (data[0] == ' ') {
        int i = 0;
        int j = 0;
        while (j < strEnd && (data[j] == ' ')) j++;  // find first char after whitespace
        while (j < strEnd) data[i++] = data[j++];   //move data;
        if (addEol) data[i++] = '\n';
        data[i++] = '\0';
    }
}

void print_statement_breakdown(char **stmtList) {
    int idx;
    fprintf(stderr, "#%d:", bbgetlinenumber());
    for (idx=0; idx < 50; idx++) {
        char *stmt = stmtList[idx];
        if (stmt[0] == 0) break;
        fprintf(stderr, "token: '%s'\n", stmt);
    }
}


void write_footer() {
    fprintf(outputFile, " if ECHOFIRST\n");
    if (bs == 28) {
        fprintf(outputFile, "       echo \"    \",[(DPC_graphics_end - *)]d , \"bytes of ROM space left in graphics bank");
    } else {
        fprintf(outputFile, "       echo \"    \",[(scoretable - *)]d , \"bytes of ROM space left");
        if (bs == 8)
            fprintf(outputFile, " in bank 2");
        if (bs == 16)
            fprintf(outputFile, " in bank 4");
        if (bs == 32)
            fprintf(outputFile, " in bank 8");
        if (bs == 64)
            fprintf(outputFile, " in bank 16");
    }
    fprintf(outputFile, "\")\n");
    fprintf(outputFile, " endif \n");
    fprintf(outputFile, "ECHOFIRST = 1\n");
    fprintf(outputFile, " \n");
    fprintf(outputFile, " \n");
    fprintf(outputFile, " \n");
}

void prerror(char *myerror) {
    fprintf(stderr, "(%d): %s\n", line, myerror);
}


void currdir_foundmsg(char *foundfile) {
    fprintf(stderr, "User-defined %s found in the current directory\n", foundfile);
}


void incline() {
    line++;
}

int bbgetlinenumber() {     // returns current line number in source
    return line;
}

int number(unsigned char value) {
    return ((int) value) - '0';
}

void removeCR(char *linenumber)    // remove trailing CR from string
{
    while ((linenumber[strlen(linenumber) - 1] == (unsigned char) 0x0A) ||
           (linenumber[strlen(linenumber) - 1] == (unsigned char) 0x0D))
        linenumber[strlen(linenumber) - 1] = '\0';
}

void remove_trailing_commas(char *linenumber)    // remove trailing commas from string
{
    int i;
    for (i = strlen(linenumber) - 1; i > 0; i--) {
        if ((linenumber[i] != ',') &&
            (linenumber[i] != ' ') &&
            (linenumber[i] != (unsigned char) 0x0A) && (linenumber[i] != (unsigned char) 0x0D))
            break;
        if (linenumber[i] == ',') {
            linenumber[i] = ' ';
            break;
        }
    }
}

int printimmed(char *value) {
    int immed = isimmed(value);
    if (immed)
        fprintf(outputFile, "#");
    return immed;
}

int isimmed(char *value) {
    // search queue of constants
    int i;
    // removeCR(value);
    for (i = 0; i < numconstants; ++i) {
        if (!strcmp(value, constants[i])) {
            // a constant should be treated as an immediate
            return 1;
        }
    }
    if (!strcmp(value + (strlen(value) > 7 ? strlen(value) - 7 : 0), "_length")) {
        // Warning about use of data_length before data statement
        fprintf(stderr,
                "(%d): Warning: Possible use of data statement length before data statement is defined\n      Workaround: forward declaration may be done by const %s=%s at beginning of code\n",
                line, value, value);
    }
    if ((value[0] == '$') || (value[0] == '%') || (value[0] < (unsigned char) 0x3A)) {
        return 1;
    } else
        return 0;
}


void goto_last_bank() {// go to the last bank before barfing the graphics
    int i;
    if (!bank)
        bank = 1;
    for (i = bank; i < last_bank; ++i)
        newbank(i + 1);
}

void set_banking(int rom_kb, int lastBankNum) {
    bs = rom_kb;
    last_bank = lastBankNum;
}

bool hasSuperchip() {
    return superchip;
}

//------------------------------------------------------------------------------------------
//------------------------- batariBasic commands
//------------------------------------------------------------------------------------------

void doreboot() {
    fprintf(outputFile, "	JMP ($FFFC)\n");
}

void vblank() {
    // code that will be run in the vblank area
    // subject to limitations!
    // must exit with a return [thisbank if bankswitching used]
    fprintf(outputFile, "vblank_bB_code\n");
}




/**
 * Bank-switching aware JSR call used for calling bBasic library subroutines.
 *
 * One routine does a JSR to bank 1.
 * The other does a JSR to the last_bank.
 *
 * @param location
 * @param locBank
 */

void jsrbank(char *location, int locBank) {
    // determines whether to use the standard jsr (for 2k/4k or bankswitched stuff in current bank)
    // or to switch banks before calling the routine
    if ((!bs) || (bank == locBank)) {
        fprintf(outputFile, " jsr %s\n", location);
        return;
    }

    // we need to switch banks
    fprintf(outputFile, " sta temp7\n");

    // first create virtual return address
    if (bs == 64)
        fprintf(outputFile, " lda #(((>(ret_point%d-1)) & $0F) | $%1x0) \n", ++numjsrs, bank - 1);
    else
        fprintf(outputFile, " lda #>(ret_point%d-1)\n", ++numjsrs);
    fprintf(outputFile, " pha\n");
    fprintf(outputFile, " lda #<(ret_point%d-1)\n", numjsrs);
    fprintf(outputFile, " pha\n");

    // next we must push the place to jsr to
    fprintf(outputFile, " lda #>(%s-1)\n", location);
    fprintf(outputFile, " pha\n");
    fprintf(outputFile, " lda #<(%s-1)\n", location);
    fprintf(outputFile, " pha\n");

    // now store regs on stack
    fprintf(outputFile, " lda temp7\n");
    fprintf(outputFile, " pha\n");
    fprintf(outputFile, " txa\n");
    fprintf(outputFile, " pha\n");

    // select bank to switch to
    fprintf(outputFile, " ldx #%d\n", locBank);
    fprintf(outputFile, " jmp BS_jsr\n");
    fprintf(outputFile, "ret_point%d\n", numjsrs);

}

void jsrbank1(char *location) {     // specifically call code located in bank 1
    jsrbank(location, 1);
}

void jsr(char *location) {          // call code in another bank (last_bank)
    jsrbank(location, last_bank);
}



int switchjoy(char *input_source) {
    // place joystick/console switch reading code inline instead of as a subroutine
    // standard routines needed for pretty much all games
    // read switches, joysticks now compiler generated (more efficient)

    // returns 0 if we need beq/bne, 1 if bvc/bvs, and 2 if bpl/bmi

    //  invalidate_Areg()  // do we need this?

    if (!strncmp(input_source, "switchreset\0", 11)) {
        fprintf(outputFile, " lda #1\n");
        fprintf(outputFile, " bit SWCHB\n");
        return 0;
    }
    if (!strncmp(input_source, "switchselect\0", 12)) {
        fprintf(outputFile, " lda #2\n");
        fprintf(outputFile, " bit SWCHB\n");
        return 0;
    }
    if (!strncmp(input_source, "switchleftb\0", 11)) {
//     fprintf(outputFile, " lda #$40\n");
        fprintf(outputFile, " bit SWCHB\n");
        return 1;
    }
    if (!strncmp(input_source, "switchrightb\0", 12)) {
//     fprintf(outputFile, " lda #$80\n");
        fprintf(outputFile, " bit SWCHB\n");
        return 2;
    }
    if (!strncmp(input_source, "switchbw\0", 8)) {
        fprintf(outputFile, " lda #8\n");
        fprintf(outputFile, " bit SWCHB\n");
        return 0;
    }
    if (!strncmp(input_source, "joy0up\0", 6)) {
        fprintf(outputFile, " lda #$10\n");
        fprintf(outputFile, " bit SWCHA\n");
        return 0;
    }
    if (!strncmp(input_source, "joy0down\0", 8)) {
        fprintf(outputFile, " lda #$20\n");
        fprintf(outputFile, " bit SWCHA\n");
        return 0;
    }
    if (!strncmp(input_source, "joy0left\0", 8)) {
//     fprintf(outputFile, " lda #$40\n");
        fprintf(outputFile, " bit SWCHA\n");
        return 1;
    }
    if (!strncmp(input_source, "joy0right\0", 9)) {
//     fprintf(outputFile, " lda #$80\n");
        fprintf(outputFile, " bit SWCHA\n");
        return 2;
    }
    if (!strncmp(input_source, "joy1up\0", 6)) {
        fprintf(outputFile, " lda #1\n");
        fprintf(outputFile, " bit SWCHA\n");
        return 0;
    }
    if (!strncmp(input_source, "joy1down\0", 8)) {
        fprintf(outputFile, " lda #2\n");
        fprintf(outputFile, " bit SWCHA\n");
        return 0;
    }
    if (!strncmp(input_source, "joy1left\0", 8)) {
        fprintf(outputFile, " lda #4\n");
        fprintf(outputFile, " bit SWCHA\n");
        return 0;
    }
    if (!strncmp(input_source, "joy1right\0", 9)) {
        fprintf(outputFile, " lda #8\n");
        fprintf(outputFile, " bit SWCHA\n");
        return 0;
    }
    if (!strncmp(input_source, "joy0fire\0", 8)) {
//     fprintf(outputFile, " lda #$80\n");
        fprintf(outputFile, " bit INPT4\n");
        return 2;
    }
    if (!strncmp(input_source, "joy1fire\0", 8)) {
//     fprintf(outputFile, " lda #$80\n");
        fprintf(outputFile, " bit INPT5\n");
        return 2;
    }
    prerror("invalid console switch/controller reference\n");
    exit(1);
}

void newbank(int bankno) {
    FILE *bs_support;
    char line[500];
    char fullpath[500];
    int len;

    if (bankno == 1)
        return;            // "bank 1" is ignored

    fullpath[0] = '\0';
    if (includespath[0]) {
        strcpy(fullpath, includespath);
    }
    strcat(fullpath, "banksw.asm");

// from here on, code will compile in the next bank
// for 8k bankswitching, most of the libaries will go into bank 2
// and majority of bB program in bank 1

    bank = bankno;
    if (bank > last_bank)
        prerror("bank not supported\n");

    fprintf(outputFile, " if ECHO%d\n", bank - 1);
    fprintf(outputFile, " echo \"    \",[(start_bank%d - *)]d , \"bytes of ROM space left in bank %d\")\n", bank - 1, bank - 1);
    fprintf(outputFile, " endif\n");
    fprintf(outputFile, "ECHO%d = 1\n", bank - 1);


    // now display banksw.asm file

    if ((bs_support = fopen("banksw.asm", "r")) == NULL)    // open file in current dir
    {
        if ((bs_support = fopen(fullpath, "r")) == NULL)    // open file
        {
            fprintf(stderr, "Cannot open banksw.asm for reading\n");
            exit(1);
        }
    } else
        currdir_foundmsg("banksw.asm");
    while (fgets(line, 500, bs_support)) {
        if (!strncmp(line, ";size=", 6))
            break;
    }
    len = atoi(line + 6);

    if (bs == 64)
        len = len + 4;        //kludge

    if (bank == 2)
        sprintf(redefined_variables[numredefvars++], "bscode_length = %d", len);

    if (bs == 64)
        fprintf(outputFile, " ORG $%1XFE0-bscode_length\n", bank - 1);
    else
        fprintf(outputFile, " ORG $%dFF4-bscode_length\n", bank - 1);


    if (bs == 28)
        fprintf(outputFile, " RORG $%XF4-bscode_length\n", (2 * (bank - 1) - 1) * 16 + 15);
    else if (bs == 64)
        fprintf(outputFile, " RORG $%XE0-bscode_length\n", (31 - bs / 2 + 2 * (bank - 1)) * 16 + 15);
    else
        fprintf(outputFile, " RORG $%XF4-bscode_length\n", (15 - bs / 2 + 2 * (bank - 1)) * 16 + 15);


    fprintf(outputFile, "start_bank%d", bank - 1);


    while (fgets(line, 500, bs_support)) {
        if (line[0] == ' ')
            fprintf(outputFile, "%s", line);
    }

    fclose(bs_support);

    fprintf(outputFile, " ORG $%1XFFC\n", bank - 1);

    if (bs == 28)
        fprintf(outputFile, " RORG $%XFC\n", (2 * (bank - 1) - 1) * 16 + 15);
    else if (bs == 64)
        fprintf(outputFile, " RORG $%XFC\n", (31 - bs / 2 + 2 * (bank - 1)) * 16 + 15);
    else
        fprintf(outputFile, " RORG $%XFC\n", (15 - bs / 2 + 2 * (bank - 1)) * 16 + 15);

    fprintf(outputFile, " .word (start_bank%d & $ffff)\n", bank - 1);
    fprintf(outputFile, " .word (start_bank%d & $ffff)\n", bank - 1);

    // now end
    fprintf(outputFile, " ORG $%1X000\n", bank);
    if (bs == 28) {
        fprintf(outputFile, " RORG $%X00\n", (2 * bank - 1) * 16);
        switch (bank) {
            case 2:        // probably a better way to do this!!!
                fprintf(outputFile, "HMdiv\n");
                fprintf(outputFile, "  .byte 0, 0, 0, 0, 0, 0, 0\n");
                fprintf(outputFile, "  .byte 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2\n");
                fprintf(outputFile, "  .byte 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3\n");
                fprintf(outputFile, "  .byte 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4\n");
                fprintf(outputFile, "  .byte 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5\n");
                fprintf(outputFile, "  .byte 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6\n");
                fprintf(outputFile, "  .byte 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7\n");
                fprintf(outputFile, "  .byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8\n");
                fprintf(outputFile, "  .byte 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9\n");
                fprintf(outputFile, "  .byte 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10\n");
                fprintf(outputFile, "  .byte 10,10,10,10,10,10,0,0,0\n");
                break;
            default:
                fprintf(outputFile, " repeat 129\n .byte 0\n repend\n");
        }
    } else if (bs == 64)
        fprintf(outputFile, " RORG $%X00\n", (31 - bs / 2 + 2 * bank) * 16);
    else
        fprintf(outputFile, " RORG $%X00\n", (15 - bs / 2 + 2 * bank) * 16);
    if (superchip)
        fprintf(outputFile, " repeat 256\n .byte $ff\n repend\n");

    if (bank == last_bank)
        fprintf(outputFile, "; bB.asm file is split here\n");

    // not working yet - need to :
    // do something I forgot

}

float immed_fixpoint(char *fixpointval) {
    int i = findpoint(fixpointval);
    if (i == 50)
        return 0;        // failsafe
    char decimalpart[50];
    fixpointval[i] = '\0';
    sprintf(decimalpart, "0.%s", fixpointval + i + 1);
    return atof(decimalpart);
}

int findpoint(char *item)    // determine if fixed point var
{
    int i;
    for (i = 0; i < 50; ++i) {
        if (item[i] == '\0')
            return 50;
        if (item[i] == '.')
            return i;
    }
    return i;
}

void freemem(char **statement) {
    int i;
    for (i = 0; i < 200; ++i)
        free(statement[i]);
    free(statement);
}

void printfrac(char *item) {                // prints the fractional part of a declared 8.8 fixpoint variable
    char getvar[50];
    int i;
    for (i = 0; i < numfixpoint88; ++i) {
        strcpy(getvar, fixpoint88[1][i]);
        if (!strcmp(fixpoint88[0][i], item)) {
            fprintf(outputFile, "%s\n", getvar);
            return;
        }
    }
    // must be immediate value
    if (findpoint(item) < 50)
        fprintf(outputFile, "#%d\n", (int) (immed_fixpoint(item) * 256.0));
    else
        fprintf(outputFile, "#0\n");
}

int isfixpoint(char *item) {
    // determines if a variable is fixed point, and if so, what kind
    int i;
    removeCR(item);
    for (i = 0; i < numfixpoint88; ++i)
        if (!strcmp(item, fixpoint88[0][i]))
            return 8;
    for (i = 0; i < numfixpoint44; ++i)
        if (!strcmp(item, fixpoint44[0][i]))
            return 4;
    if (findpoint(item) < 50)
        return 12;
    return 0;
}

void set_romsize(char *size) {
    if (!strncmp(size, "2k\0", 2)) {
        strcpy(redefined_variables[numredefvars++], "ROM2k = 1");
    } else if (!strncmp(size, "8k\0", 2)) {
        bs = 8;
        last_bank = 2;
        if (!strncmp(size, "8kEB\0", 4))
            strcpy(redefined_variables[numredefvars++], "bankswitch_hotspot = $083F");
        else
            strcpy(redefined_variables[numredefvars++], "bankswitch_hotspot = $1FF8");
        strcpy(redefined_variables[numredefvars++], "bankswitch = 8");
        strcpy(redefined_variables[numredefvars++], "bs_mask = 1");
        if (!strncmp(size, "8kSC\0", 4)) {
            strcpy(redefined_variables[numredefvars++], "superchip = 1");
            create_includes("superchip.inc");
            superchip = 1;
        } else
            create_includes("bankswitch.inc");
    } else if (!strncmp(size, "16k\0", 2)) {
        bs = 16;
        last_bank = 4;
        strcpy(redefined_variables[numredefvars++], "bankswitch_hotspot = $1FF6");
        strcpy(redefined_variables[numredefvars++], "bankswitch = 16");
        strcpy(redefined_variables[numredefvars++], "bs_mask = 3");
        if (!strncmp(size, "16kSC\0", 5)) {
            strcpy(redefined_variables[numredefvars++], "superchip = 1");
            create_includes("superchip.inc");
            superchip = 1;
        } else
            create_includes("bankswitch.inc");
    } else if (!strncmp(size, "32k\0", 2)) {
        bs = 32;
        last_bank = 8;
        strcpy(redefined_variables[numredefvars++], "bankswitch_hotspot = $1FF4");
        strcpy(redefined_variables[numredefvars++], "bankswitch = 32");
        strcpy(redefined_variables[numredefvars++], "bs_mask = 7");
//    if (multisprite == 1) create_includes("multisprite_bankswitch.inc");
        // else
        if (!strncmp(size, "32kSC\0", 5)) {
            strcpy(redefined_variables[numredefvars++], "superchip = 1");
            create_includes("superchip.inc");
            superchip = 1;
        } else
            create_includes("bankswitch.inc");
    } else if (!strncmp(size, "64k\0", 2)) {
        bs = 64;
        last_bank = 16;
        strcpy(redefined_variables[numredefvars++], "bankswitch_hotspot = $1FE0");
        strcpy(redefined_variables[numredefvars++], "bankswitch = 64");
        strcpy(redefined_variables[numredefvars++], "bs_mask = 15");
        if (!strncmp(size, "64kSC\0", 5)) {
            strcpy(redefined_variables[numredefvars++], "superchip = 1");
            create_includes("superchip.inc");
            superchip = 1;
        } else
            create_includes("bankswitch.inc");
    } else if (strncmp(size, "4k\0", 2)) {
        fprintf(stderr, "Warning: unsupported ROM size: %s Using 4k.\n", size);
    }
}

void add_includes(char *myinclude) {
    if (includesfile_already_done)
        fprintf(stderr, "%d: Warning: include ignored (includes should typically precede other commands)\n", line);
    strcat(user_includes, myinclude);
}

void add_inline(char *myinclude) {
    int eos = strlen(myinclude)-1;
    if (myinclude[eos] == '\n') myinclude[eos] = 0;
    fprintf(outputFile, " include \"%s\" \n", myinclude);
}

void init_includes(char *path) {
    if (path) {
        strcpy(includespath, path);
        if ((includespath[strlen(includespath) - 1] == '\\') || (includespath[strlen(includespath) - 1] == '/'))
            strcat(includespath, "includes/");
        else
            strcat(includespath, "/includes/");
    } else {
        includespath[0] = '\0';
    }
    user_includes[0] = '\0';
}


void create_includes(char *includesfile) {
    FILE *includesread, *includeswrite;
    char dline[500];
    char fullpath[500];
    int i;
    int writeline;
    removeCR(includesfile);
    if (includesfile_already_done)
        return;
    includesfile_already_done = 1;
    fullpath[0] = '\0';
    if (includespath[0]) {
        strcpy(fullpath, includespath);
    }
    strcat(fullpath, includesfile);
//  for (i=0;i<strlen(includesfile);++i) if (includesfile[i]=='\n') includesfile[i]='\0';
    if ((includesread = fopen(includesfile, "r")) == NULL)    // try again in current dir
    {
        if ((includesread = fopen(fullpath, "r")) == NULL)    // open file
        {
            fprintf(stderr, "Cannot open %s for reading\n", includesfile);
            exit(1);
        }
    } else
        currdir_foundmsg(includesfile);
    if ((includeswrite = fopen("includes.bB", "w")) == NULL)    // open file
    {
        fprintf(stderr, "Cannot open includes.bB for writing\n");
        exit(1);
    }

    while (fgets(dline, 500, includesread)) {
        for (i = 0; i < 500; ++i) {
            if (dline[i] == ';') {
                writeline = 0;
                break;
            }
            if (dline[i] == (unsigned char) 0x0A) {
                writeline = 0;
                break;
            }
            if (dline[i] == (unsigned char) 0x0D) {
                writeline = 0;
                break;
            }
            if (dline[i] > (unsigned char) 0x20) {
                writeline = 1;
                break;
            }
            writeline = 0;
        }
        if (writeline) {
            if (!strncasecmp(dline, "bb.asm\0", 6))
                if (user_includes[0] != '\0')
                    fprintf(includeswrite, "%s", user_includes);
            fprintf(includeswrite, "%s", dline);
        }
    }
    fclose(includesread);
    fclose(includeswrite);
}

void printindex(char *mystatement, int myindex) {
    if (!myindex) {
        printimmed(mystatement);
        fprintf(outputFile, "%s\n", mystatement);
    } else
        fprintf(outputFile, "%s,x\n", mystatement);    // indexed with x!
}

void loadindex(char *myindex) {
    if (strncmp(myindex, "TSX\0", 3)) {
        fprintf(outputFile, "	LDX ");    // get index
        printimmed(myindex);
        fprintf(outputFile, "%s\n", myindex);
    }
}

int getindex(char *mystatement, char *myindex) {
    int i, j, index = 0;
    for (i = 0; i < 200; ++i) {
        if (mystatement[i] == '\0') {
            i = 200;
            break;
        }
        if (mystatement[i] == '[') {
            index = 1;
            break;
        }
    }
    if (i < 200) {
        strcpy(myindex, mystatement + i + 1);
        myindex[strlen(myindex) - 1] = '\0';
        if (myindex[strlen(myindex) - 2] == ']')
            myindex[strlen(myindex) - 2] = '\0';
        if (myindex[strlen(myindex) - 1] == ']')
            myindex[strlen(myindex) - 1] = '\0';
        for (j = i; j < 200; ++j)
            mystatement[j] = '\0';
    }
    return index;
}

int checkmul(int value) {
// check to see if value can be optimized to save cycles

    if (!(value % 2))
        return 1;

    if (value < 11)
        return 1;        // always optimize these

    while (value != 1) {
        if (!(value % 9))
            value /= 9;
        else if (!(value % 7))
            value /= 7;
        else if (!(value % 5))
            value /= 5;
        else if (!(value % 3))
            value /= 3;
        else if (!(value % 2))
            value /= 2;
        else
            break;
        if (!(value % 2) && (optimization & 1) != 1)
            break;        // do not optimize multplications
    }
    if (value == 1)
        return 1;
    else
        return 0;
}

int checkdiv(int value) {
// check to see if value is a power of two - if not, run standard div routine
    while (value != 1) {
        if (value % 2)
            break;
        else
            value /= 2;
    }
    if (value == 1)
        return 1;
    else
        return 0;
}


void mul(char **statement, int bits) {
    // this will attempt to output optimized code depending on the multiplicand
    int multiplicand = atoi(statement[6]);
    int tempstorage = 0;
    // we will optimize specifically for 2,3,5,7,9
    if (bits == 16) {
        fprintf(outputFile, "	ldx #0\n");
        fprintf(outputFile, "	stx temp1\n");
    }
    while (multiplicand != 1) {
        if (!(multiplicand % 9)) {
            if (tempstorage) {
                strcpy(statement[4], "temp2");
                fprintf(outputFile, "	sta temp2\n");
            }
            multiplicand /= 9;
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	clc\n");
            fprintf(outputFile, "	adc ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            if (bits == 16) {
                fprintf(outputFile, "	tax\n");
                fprintf(outputFile, "	lda temp1\n");
                fprintf(outputFile, "	adc #0\n");
                fprintf(outputFile, "	sta temp1\n");
                fprintf(outputFile, "	txa\n");
            }
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	clc\n");
            fprintf(outputFile, "	adc ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            if (bits == 16) {
                fprintf(outputFile, "	tax\n");
                fprintf(outputFile, "	lda temp1\n");
                fprintf(outputFile, "	adc #0\n");
                fprintf(outputFile, "	sta temp1\n");
                fprintf(outputFile, "	txa\n");
            }
            tempstorage = 1;
        } else if (!(multiplicand % 9)) {
            if (tempstorage) {
                strcpy(statement[4], "temp2");
                fprintf(outputFile, "	sta temp2\n");
            }
            multiplicand /= 9;
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	clc\n");
            fprintf(outputFile, "	adc ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            if (bits == 16) {
                fprintf(outputFile, "	tax\n");
                fprintf(outputFile, "	lda temp1\n");
                fprintf(outputFile, "	adc #0\n");
                fprintf(outputFile, "	sta temp1\n");
                fprintf(outputFile, "	txa\n");
            }
            tempstorage = 1;
        } else if (!(multiplicand % 7)) {
            if (tempstorage) {
                strcpy(statement[4], "temp2");
                fprintf(outputFile, "	sta temp2\n");
            }
            multiplicand /= 7;
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	sec\n");
            fprintf(outputFile, "	sbc ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            if (bits == 16) {
                fprintf(outputFile, "	tax\n");
                fprintf(outputFile, "	lda temp1\n");
                fprintf(outputFile, "	sbc #0\n");
                fprintf(outputFile, "	sta temp1\n");
                fprintf(outputFile, "	txa\n");
            }
            tempstorage = 1;
        } else if (!(multiplicand % 5)) {
            if (tempstorage) {
                strcpy(statement[4], "temp2");
                fprintf(outputFile, "	sta temp2\n");
            }
            multiplicand /= 5;
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	clc\n");
            fprintf(outputFile, "	adc ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            if (bits == 16) {
                fprintf(outputFile, "	tax\n");
                fprintf(outputFile, "	lda temp1\n");
                fprintf(outputFile, "	adc #0\n");
                fprintf(outputFile, "	sta temp1\n");
                fprintf(outputFile, "	txa\n");
            }
            tempstorage = 1;
        } else if (!(multiplicand % 3)) {
            if (tempstorage) {
                strcpy(statement[4], "temp2");
                fprintf(outputFile, "	sta temp2\n");
            }
            multiplicand /= 3;
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
            fprintf(outputFile, "	clc\n");
            fprintf(outputFile, "	adc ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            if (bits == 16) {
                fprintf(outputFile, "	tax\n");
                fprintf(outputFile, "	lda temp1\n");
                fprintf(outputFile, "	adc #0\n");
                fprintf(outputFile, "	sta temp1\n");
                fprintf(outputFile, "	txa\n");
            }
            tempstorage = 1;
        } else if (!(multiplicand % 2)) {
            multiplicand /= 2;
            fprintf(outputFile, "	asl\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");
        } else {
            fprintf(outputFile, "	LDY #%d\n", multiplicand);
            fprintf(outputFile, "	jsr mul%d\n", bits);
            fprintf(stderr, "Warning - there seems to be a problem.  Your code may not run properly.\n");
            fprintf(stderr, "If you are seeing this message, please report it - it could be a bug.\n");
// WARNING: not fixed up for bankswitching

        }
    }
}

void divd(char **statement, int bits) {
    int divisor = atoi(statement[6]);
    if (bits == 16) {
        fprintf(outputFile, "	ldx #0\n");
        fprintf(outputFile, "	stx temp1\n");
    }
    while (divisor != 1) {
        if (!(divisor % 2))    // div by power of two is the only easy thing
        {
            divisor /= 2;
            fprintf(outputFile, "	lsr\n");
            if (bits == 16)
                fprintf(outputFile, "  rol temp1\n");    // I am not sure if this is actually correct
        } else {
            fprintf(outputFile, "	LDY #%d\n", divisor);
            fprintf(outputFile, "	jsr div%d\n", bits);
            fprintf(stderr, "Warning - there seems to be a problem.  Your code may not run properly.\n");
            fprintf(stderr, "If you are seeing this message, please report it - it could be a bug.\n");
// WARNING: Not fixed up for bankswitching

        }
    }

}

void endfunction() {
    if (!doingfunction)
        prerror("extraneous end keyword encountered\n");
    doingfunction = 0;
}

void function(char **statement) {
    // declares a function - only needed if function is in bB.  For asm functions, see
    // the help.html file.
    // determine number of args, then run until we get an end.
    doingfunction = 1;
    fprintf(outputFile, "%s\n", statement[2]);
    fprintf(outputFile, "	STA temp1\n");
    fprintf(outputFile, "	STY temp2\n");
}

void callfunction(char **statement) {
    // called by assignment to a variable
    // does not work as an argument in another function or an if-then... yet.
    int i, index = 0;
    char getindex0[200];
    int arguments = 0;
    int argnum[10];
    for (i = 6; statement[i][0] != ')'; ++i) {
        if (statement[i][0] != ',') {
            argnum[arguments++] = i;
        }
        if (i > 195)
            prerror("missing \")\" at end of function call\n");
    }
    if (!arguments)
        fprintf(stderr, "(%d) Warning: function call with no arguments\n", line);
    while (arguments) {
        // get [index]
        index = 0;
        index |= getindex(statement[argnum[--arguments]], &getindex0[0]);
        if (index)
            loadindex(&getindex0[0]);

        if (arguments == 1)
            fprintf(outputFile, "	LDY ");
        else
            fprintf(outputFile, "	LDA ");
        printindex(statement[argnum[arguments]], index);

        if (arguments > 1)
            fprintf(outputFile, "	STA temp%d\n", arguments + 1);
//    arguments--;
    }
//  jsr(statement[4]);
// need to fix above for proper function support
    fprintf(outputFile, " jsr %s\n", statement[4]);

    strcpy(Areg, "invalid");

}

void invalidate_Areg() {
    strcpy(Areg, "invalid");
}

int islabel(char **statement) {            // this is for determining if the item after a "then" is a label or a statement
    // return of 0=label, 1=statement
    int i;
    // find the "then" or a "goto"
    for (i = 0; i < 198;)
        if (!strncmp(statement[i++], "then\0", 4))
            break;
    return findlabel(statement, i);
}

int islabelelse(
        char **statement) {                // this is for determining if the item after an "else" is a label or a statement
    // return of 0=label, 1=statement
    int i;
    // find the "else"
    for (i = 0; i < 198;)
        if (!strncmp(statement[i++], "else\0", 4))
            break;
    return findlabel(statement, i);
}

int findlabel(char **statement, int i) {
    char statementcache[100];
    // 0 if label, 1 if not
    if ((statement[i][0] > (unsigned char) 0x2F) && (statement[i][0] < (unsigned char) 0x3B))
        return 0;
    if ((statement[i + 1][0] == ':') && (strncmp(statement[i + 2], "rem\0", 3)))
        return 1;
    if ((statement[i + 1][0] == ':') && (!strncmp(statement[i + 2], "rem\0", 3)))
        return 0;

    if (!strncmp(statement[i + 1], "else\0", 4))
        return 0;
//  if (!strncmp(statement[i+1],"bank\0",4)) return 0;
// uncomment to allow then label bankx (needs work)
    if (statement[i + 1][0] != '\0')
        return 1;
    // only possibilities left are: drawscreen, player0:, player1:, asm, next, return, maybe others added later?

    strcpy(statementcache, statement[i]);
    removeCR(statementcache);
    if (!strncmp(statementcache, "drawscreen\0", 10))
        return 1;
    if (!strncmp(statementcache, "lives:\0", 6))
        return 1;
    if (!strncmp(statementcache, "player0color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player1color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player2color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player3color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player4color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player5color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player6color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player7color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player8color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player9color:\0", 13))
        return 1;
    if (!strncmp(statementcache, "player0:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player1:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player2:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player3:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player4:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player5:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player6:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player7:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player8:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player9:\0", 8))
        return 1;
    if (!strncmp(statementcache, "player1-\0", 8))
        return 1;
    if (!strncmp(statementcache, "player2-\0", 8))
        return 1;
    if (!strncmp(statementcache, "player3-\0", 8))
        return 1;
    if (!strncmp(statementcache, "player4-\0", 8))
        return 1;
    if (!strncmp(statementcache, "player5-\0", 8))
        return 1;
    if (!strncmp(statementcache, "player6-\0", 8))
        return 1;
    if (!strncmp(statementcache, "player7-\0", 8))
        return 1;
    if (!strncmp(statementcache, "player8-\0", 8))
        return 1;
    if (!strncmp(statementcache, "playfield:\0", 10))
        return 1;
    if (!strncmp(statementcache, "pfcolors:\0", 9))
        return 1;
    if (!strncmp(statementcache, "scorecolors:\0", 12))
        return 1;
    if (!strncmp(statementcache, "pfheights:\0", 10))
        return 1;
    if (!strncmp(statementcache, "asm\0", 4))
        return 1;
    if (!strncmp(statementcache, "pop\0", 4))
        return 1;
    if (!strncmp(statementcache, "stack\0", 6))
        return 1;
    if (!strncmp(statementcache, "push\0", 5))
        return 1;
    if (!strncmp(statementcache, "pull\0", 5))
        return 1;
    if (!strncmp(statementcache, "rem\0", 4))
        return 1;
    if (!strncmp(statementcache, "next\0", 5))
        return 1;
    if (!strncmp(statementcache, "reboot\0", 7))
        return 1;
    if (!strncmp(statementcache, "return\0", 7))
        return 1;
    if (!strncmp(statementcache, "callmacro\0", 9))
        return 1;
    if (statement[i + 1][0] == '=')
        return 1;        // it's a variable assignment


    return 0;            // I really hope we've got a label !!!!
}

void sread(char **statement) {
    // read sequential data
    fprintf(outputFile, "	ldx #%s\n", statement[6]);
    fprintf(outputFile, "	lda (0,x)\n");
    fprintf(outputFile, "	inc 0,x\n");
    fprintf(outputFile, "	bne *+4\n");
    fprintf(outputFile, "	inc 1,x\n");
    strcpy(Areg, "invalid");
}

void sdata(char **statement) {
    // sequential data, works like regular basics and doesn't have the 256 byte restriction
    char data[200];
    int i;
    removeCR(statement[4]);
    sprintf(redefined_variables[numredefvars++], "%s = %s", statement[2], statement[4]);
    fprintf(outputFile, "	lda #<%s_begin\n", statement[2]);
    fprintf(outputFile, "	sta %s\n", statement[4]);
    fprintf(outputFile, "	lda #>%s_begin\n", statement[2]);
    fprintf(outputFile, "	sta %s+1\n", statement[4]);

    fprintf(outputFile, "	JMP .skip%s\n", statement[0]);
    // not affected by noinlinedata

    fprintf(outputFile, "%s_begin\n", statement[2]);
    while (1) {
        if ((read_source_line(data)
             || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {
            prerror("Error: Missing \"end\" keyword at end of data\n");
            exit(1);
        }
        line++;
        if (!strncmp(data, "end\0", 3))
            break;
        remove_trailing_commas(data);
        for (i = 0; i < 200; ++i) {
            if ((int) data[i] > 32)
                break;
            if (((int) data[i] < 14) && ((int) data[i] != 9))
                i = 200;
        }
        if (i < 200)
            fprintf(outputFile, "	.byte %s\n", data);
    }
    fprintf(outputFile, ".skip%s\n", statement[0]);

}

void data(char **statement) {
    char data[200];
    char **data_length;
    char **deallocdata_length;
    int i, j;
    data_length = (char **) malloc(sizeof(char *) * 200);
    for (i = 0; i < 200; ++i) {
        data_length[i] = (char *) malloc(sizeof(char) * 200);
        for (j = 0; j < 200; ++j)
            data_length[i][j] = '\0';
    }
    deallocdata_length = data_length;
    removeCR(statement[2]);

    if (!(optimization & 4))
        fprintf(outputFile, "	JMP .skip%s\n", statement[0]);
    // if optimization level >=4 then data cannot be placed inline with code!

    fprintf(outputFile, "%s\n", statement[2]);
    while (1) {
        if ((read_source_line(data)
             || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {
            prerror("Error: Missing \"end\" keyword at end of data\n");
            exit(1);
        }
        line++;
        if (!strncmp(data, "end\0", 3))
            break;
        remove_trailing_commas(data);
        for (i = 0; i < 200; ++i) {
            if ((int) data[i] > 32)
                break;
            if (((int) data[i] < 14) && ((int) data[i] != 9))
                i = 200;
        }
        if (i < 200)
            fprintf(outputFile, "	.byte %s\n", data);
    }
    fprintf(outputFile, ".skip%s\n", statement[0]);
    strcpy(data_length[0], " ");
    strcpy(data_length[1], "const");
    sprintf(data_length[2], "%s_length", statement[2]);
    strcpy(data_length[3], "=");
    sprintf(data_length[4], ".skip%s-%s", statement[0], statement[2]);
    strcpy(data_length[5], "\n");
    data_length[6][0] = '\0';
    keywords(data_length);
    freemem(deallocdata_length);
}

void shiftdata(char **statement, int num) {
    int i, j;
    for (i = 199; i > num; i--)
        for (j = 0; j < 200; ++j)
            statement[i][j] = statement[i - 1][j];
}

void compressdata(char **statement, int num1, int num2) {
    int i, j;
    for (i = num1; i < 200 - num2; i++)
        for (j = 0; j < 200; ++j)
            statement[i][j] = statement[i + num2][j];
}


void dim(char **statement) {
    // just take the statement and pass it right to a header file
    int i;
    char *fixpointvar1;
    char fixpointvar2[50];
    // check for fixedpoint variables
    i = findpoint(statement[4]);
    if (i < 50) {
        removeCR(statement[4]);
        strcpy(fixpointvar2, statement[4]);
        fixpointvar1 = fixpointvar2 + i + 1;
        fixpointvar2[i] = '\0';
        if (!strcmp(fixpointvar1, fixpointvar2))    // we have 4.4
        {
            strcpy(fixpoint44[1][numfixpoint44], fixpointvar1);
            strcpy(fixpoint44[0][numfixpoint44++], statement[2]);
        } else            // we have 8.8
        {
            strcpy(fixpoint88[1][numfixpoint88], fixpointvar1);
            strcpy(fixpoint88[0][numfixpoint88++], statement[2]);
        }
        statement[4][i] = '\0';    // terminate string at '.'
    }
    i = 2;
    redefined_variables[numredefvars][0] = '\0';
    while ((statement[i][0] != '\0') && (statement[i][0] != ':')) {
        strcat(redefined_variables[numredefvars], statement[i++]);
        strcat(redefined_variables[numredefvars], " ");
    }
    numredefvars++;
}

void doconst(char **statement) {
    // basically the same as dim, except we keep a queue of variable names that are constant
    int i = 2;
    redefined_variables[numredefvars][0] = '\0';
    while ((statement[i][0] != '\0') && (statement[i][0] != ':')) {
        strcat(redefined_variables[numredefvars], statement[i++]);
        strcat(redefined_variables[numredefvars], " ");
    }
    numredefvars++;
    strcpy(constants[numconstants++], statement[2]);    // record to queue
}


void doreturn(char **statement) {
    int index = 0;
    char getindex0[200];
    int bankedreturn = 0;
    // 0=no special action
    // 1=return thisbank
    // 2=return otherbank

    if (!strncmp(statement[2], "thisbank\0", 8) || !strncmp(statement[3], "thisbank\0", 8))
        bankedreturn = 1;
    else if (!strncmp(statement[2], "otherbank\0", 9) || !strncmp(statement[3], "otherbank\0", 9))
        bankedreturn = 2;

    // several types of returns:
    // return by itself (or with a value) can return to any calling bank
    // this one has the most overhead in terms of cycles and ROM space
    // use sparingly if cycles or space are an issue

    // return [value] thisbank will only return within the same bank
    // this one is the fastest

    // return [value] otherbank will slow down returns to the same bank
    // but speed up returns to other banks - use if you are primarily returning
    // across banks

    if (statement[2][0] && (statement[2][0] != ' ') &&
        (strncmp(statement[2], "thisbank\0", 8)) && (strncmp(statement[2], "otherbank\0", 9))) {

        index |= getindex(statement[2], &getindex0[0]);

        if (index & 1)
            loadindex(&getindex0[0]);

        if (bs == 64)
            fprintf(outputFile, "	LDA ");
        else if (!bankedreturn)
            fprintf(outputFile, "	LDY ");
        else
            fprintf(outputFile, "	LDA ");
        printindex(statement[2], index & 1);

    }

    if (bankedreturn == 1) {
        fprintf(outputFile, "	RTS\n");
        return;
    }
    if (bankedreturn == 2) {
        fprintf(outputFile, "	JMP BS_return\n");
        return;
    }

    if (bs)            // check if sub was called from the same bank
    {
        if (bs == 64) {
            // for 64kb carts, the onus is on the user to use "return otherbank" from bankswitch gosubs.
            // if we're here, we assume that it was from a non-bankswitched gosub.
            fprintf(outputFile, "	RTS\n");
            return;
        } else {
            fprintf(outputFile, "	tsx\n");
            fprintf(outputFile, "	lda 2,x ; check return address\n");
            fprintf(outputFile, "	eor #(>*) ; vs. current PCH\n");
            fprintf(outputFile, "	and #$E0 ;  mask off all but top 3 bits\n");
        }

        // if zero, then banks are the same
        if (statement[2][0] && (statement[2][0] != ' ')) {
            fprintf(outputFile, "	beq *+6 ; if equal, do normal return\n");
            fprintf(outputFile, "	tya\n");
        } else
            fprintf(outputFile, "	beq *+5 ; if equal, do normal return\n");
        fprintf(outputFile, "	JMP BS_return\n");
    }

    if (statement[2][0] && (statement[2][0] != ' '))
        fprintf(outputFile, "	tya\n");
    fprintf(outputFile, "	RTS\n");
}



void doasm() {
    char data[200];
    while (1) {
        if ((read_original_line(data)
             || ((data[0] < (unsigned char) 0x3A) && (data[0] > (unsigned char) 0x2F))) && (data[0] != 'e')) {
            prerror("Error: Missing \"end\" keyword at end of inline asm\n");
            exit(1);
        }
        line++;
        if (!strncmp(data, "end\0", 3))
            break;
        fprintf(outputFile, "%s\n", data);

    }
}

void domacro(char **statement) {
    int k, j = 1, i = 3;
    macroactive = 1;
    fprintf(outputFile, " MAC %s\n", statement[2]);

    while ((statement[i][0] != ':') && (statement[i][0] != '\0')) {
        for (k = 0; k < 200; ++k)
            if ((statement[i][k] == (unsigned char) 0x0A) || (statement[i][k] == (unsigned char) 0x0D))
                statement[i][k] = '\0';
        if (!strncmp(statement[i], "const\0", 5))
            strcpy(constants[numconstants++], statement[i + 1]);    // record to const queue
        else
            fprintf(outputFile, "%s SET {%d}\n", statement[i], j++);
        i++;
    }
}

void callmacro(char **statement) {
    int k, i = 3;
    macroactive = 1;
    fprintf(outputFile, " %s", statement[2]);

    while ((statement[i][0] != ':') && (statement[i][0] != '\0')) {
        for (k = 0; k < 200; ++k)
            if ((statement[i][k] == (unsigned char) 0x0A) || (statement[i][k] == (unsigned char) 0x0D))
                statement[i][k] = '\0';
        if (isimmed(statement[i]))
            fprintf(outputFile, " #%s,", statement[i]);    // we're assuming the assembler doesn't mind extra commas!
        else
            fprintf(outputFile, " %s,", statement[i]);    // we're assuming the assembler doesn't mind extra commas!
        i++;
    }
    fprintf(outputFile, "\n");
}

void doextra(char *extrano) {
    extraactive = 1;
    fprintf(outputFile, "extra set %d\n", ++extra);
//  fprintf(outputFile, ".extra%c",extrano[5]);
//   if (extrano[6]!=':') fprintf(outputFile, "%c",extrano[6]);
//  fprintf(outputFile, "\n");
    fprintf(outputFile, " MAC extra%c", extrano[5]);
    if (extrano[6] != ':')
        fprintf(outputFile, "%c", extrano[6]);
    fprintf(outputFile, "\n");
}

void doend() {
    if (extraactive) {
        fprintf(outputFile, " ENDM\n");
        extraactive = 0;
    } else if (macroactive) {
        fprintf(outputFile, " ENDM\n");
        macroactive = 0;
    } else
        prerror("extraneous end statement found");
}


enum { COLLISION_OBJ_CNT = 6 };
const char* COLLISION_OBJ_NAMES[COLLISION_OBJ_CNT] = {
        "player0",
        "player1",
        "missile0",
        "missile1",
        "ball",
        "playfield"
        };
enum {
    CLD_PLAYER0,
    CLD_PLAYER1,
    CLD_MISSILE0,
    CLD_MISSILE1,
    CLD_BALL,
    CLD_PLAYFIELD
};

int get_collision_obj(char *param) {
    int ci;
    for (ci = 0; ci < COLLISION_OBJ_CNT; ci++) {
        if (!strncmp(param, COLLISION_OBJ_NAMES[ci], 10)) return ci;
    }
    return -1;
}

/**
 * Process for collision() command
 *
 * @param statement
 * @return bit to change,  0 = error
 */
int check_collisions(char **statement) {
    int bit;
    if ((statement[1][0] != '(') ||
        (statement[3][0] != ',') ||
        (statement[5][0] != ')')) {
        fprintf(stderr, "Error in collision test: '%s %s %s %s %s'\n",
                statement[1], statement[2], statement[3], statement[4], statement[5]);
        return 0;
    }
    char *param1 = statement[2];
    int collObj1 = get_collision_obj(param1);
    if (collObj1 < 0) {
        fprintf(stderr, "Error in collision test: invalid object name '%s'\n", param1);
        return 0;
    }

    char *param2 = statement[4];
    int collObj2 = get_collision_obj(param2);
    if (collObj2 < 0) {
        fprintf(stderr, "Error in collision test: invalid object name '%s'\n", param2);
        return 0;
    }

    char *collisionReg = 0;
    bit = 0;
    if (collObj1 == CLD_MISSILE0) {
        switch (collObj2) {
            case CLD_PLAYER0:   collisionReg = "CXM0P";  bit = 6; break;
            case CLD_PLAYER1:   collisionReg = "CXM0P";  bit = 7; break;
            case CLD_PLAYFIELD: collisionReg = "CXM0FB"; bit = 7; break;
            case CLD_BALL:      collisionReg = "CXM0FB"; bit = 6; break;
            case CLD_MISSILE1:  collisionReg = "CXPPMM"; bit = 6; break;
        }
    } else if (collObj1 == CLD_MISSILE1) {
        switch (collObj2) {
            case CLD_PLAYER0:   collisionReg = "CXM1P";  bit = 7; break;
            case CLD_PLAYER1:   collisionReg = "CXM1P";  bit = 6; break;
            case CLD_PLAYFIELD: collisionReg = "CXM1FB"; bit = 7; break;
            case CLD_BALL:      collisionReg = "CXM1FB"; bit = 6; break;
            case CLD_MISSILE0:  collisionReg = "CXPPMM"; bit = 6; break;
        }
    } else if (collObj1 == CLD_PLAYER0) {
        switch (collObj2) {
            case CLD_PLAYER1:   collisionReg = "CXPPMM"; bit = 7; break;
            case CLD_MISSILE0:  collisionReg = "CXM0P";  bit = 6; break;
            case CLD_MISSILE1:  collisionReg = "CXM1P";  bit = 7; break;
            case CLD_PLAYFIELD: collisionReg = "CXP0FB"; bit = 7; break;
            case CLD_BALL:      collisionReg = "CXP0FB"; bit = 6; break;
        }
    } else if (collObj1 == CLD_PLAYER1) {
        switch (collObj2) {
            case CLD_PLAYER0:   collisionReg = "CXPPMM"; bit = 7; break;
            case CLD_MISSILE0:  collisionReg = "CXM0P";  bit = 7; break;
            case CLD_MISSILE1:  collisionReg = "CXM1P";  bit = 6; break;
            case CLD_PLAYFIELD: collisionReg = "CXP1FB"; bit = 7; break;
            case CLD_BALL:      collisionReg = "CXP1FB"; bit = 6; break;
        }
    } else if (collObj1 == CLD_BALL) {
        switch (collObj2) {
            case CLD_PLAYER0:   collisionReg = "CXP0FB"; bit = 6; break;
            case CLD_PLAYER1:   collisionReg = "CXP1FB"; bit = 6; break;
            case CLD_MISSILE0:  collisionReg = "CXM0FB"; bit = 6; break;
            case CLD_MISSILE1:  collisionReg = "CXM1FB"; bit = 6; break;
            case CLD_PLAYFIELD: collisionReg = "CXBLPF"; bit = 7; break;
        }
    } else if (collObj1 == CLD_PLAYFIELD) {
        switch (collObj2) {
            case CLD_PLAYER0:   collisionReg = "CXP0FB"; bit = 7; break;
            case CLD_PLAYER1:   collisionReg = "CXP1FB"; bit = 7; break;
            case CLD_MISSILE0:  collisionReg = "CXM0FB"; bit = 7; break;
            case CLD_MISSILE1:  collisionReg = "CXM1FB"; bit = 7; break;
            case CLD_BALL:      collisionReg = "CXBLPF"; bit = 7; break;
        }
    }
    if (bit) fprintf(outputFile, "    %s", collisionReg);    //--- output collision register
    return bit;
}

void hotspotcheck(char *linenumber) {
    if (bs)            //if bankswitching, check for reverse branches from $1fXX that trigger hotspots...
    {
        fprintf(outputFile,
                " if ( (((((#>*)&$1f)*256)|(#<.%s))>=bankswitch_hotspot) && (((((#>*)&$1f)*256)|(#<.%s))<=(bankswitch_hotspot+bs_mask)) )\n",
                 linenumber, linenumber);
        fprintf(outputFile,
                "   echo \"WARNING: branch near the end of bank %d may accidentally trigger a bankswitch. Reposition code there if bad things happen.\"\n",
                 bank);
        fprintf(outputFile, " endif\n");
    }
}


void bmi(char *linenumber) {
    removeCR(linenumber);
    if (smartbranching) {
        fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -128)\n	bmi .%s\n", linenumber, linenumber, linenumber);
        // branches might be allowed as below - check carefully to make sure!
        // fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -129)\n    bmi .%s\n",linenumber,linenumber,linenumber);
        fprintf(outputFile, " else\n	bpl .%dskip%s\n	jmp .%s\n", branchtargetnumber, linenumber, linenumber);
        fprintf(outputFile, ".%dskip%s\n", branchtargetnumber++, linenumber);
        fprintf(outputFile, " endif\n");
    } else {
        fprintf(outputFile, "	bmi .%s\n", linenumber);
        hotspotcheck(linenumber);
    }
}

void bpl(char *linenumber) {
    removeCR(linenumber);
    if (smartbranching) {
        fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -128)\n	bpl .%s\n", linenumber, linenumber, linenumber);
        fprintf(outputFile, " else\n	bmi .%dskip%s\n	jmp .%s\n", branchtargetnumber, linenumber, linenumber);
        fprintf(outputFile, ".%dskip%s\n", branchtargetnumber++, linenumber);
        fprintf(outputFile, " endif\n");
    } else {
        fprintf(outputFile, "	bpl .%s\n", linenumber);
        hotspotcheck(linenumber);
    }
}

void bne(char *linenumber) {
    removeCR(linenumber);
    if (smartbranching) {
        fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -128)\n	BNE .%s\n", linenumber, linenumber, linenumber);
        fprintf(outputFile, " else\n	beq .%dskip%s\n	jmp .%s\n", branchtargetnumber, linenumber, linenumber);
        fprintf(outputFile, ".%dskip%s\n", branchtargetnumber++, linenumber);
        fprintf(outputFile, " endif\n");
    } else {
        fprintf(outputFile, "	bne .%s\n", linenumber);
        hotspotcheck(linenumber);
    }
}

void beq(char *linenumber) {
    removeCR(linenumber);
    if (smartbranching) {
        fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -128)\n	BEQ .%s\n", linenumber, linenumber, linenumber);
        fprintf(outputFile, " else\n	bne .%dskip%s\n	jmp .%s\n", branchtargetnumber, linenumber, linenumber);
        fprintf(outputFile, ".%dskip%s\n", branchtargetnumber++, linenumber);
        fprintf(outputFile, " endif\n");
    } else {
        fprintf(outputFile, "	beq .%s\n", linenumber);
        hotspotcheck(linenumber);
    }
}

void bcc(char *linenumber) {
    removeCR(linenumber);
    if (smartbranching) {
        fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -128)\n	bcc .%s\n", linenumber, linenumber, linenumber);
        fprintf(outputFile, " else\n	bcs .%dskip%s\n	jmp .%s\n", branchtargetnumber, linenumber, linenumber);
        fprintf(outputFile, ".%dskip%s\n", branchtargetnumber++, linenumber);
        fprintf(outputFile, " endif\n");
    } else {
        fprintf(outputFile, "	bcc .%s\n", linenumber);
        hotspotcheck(linenumber);
    }

}

void bcs(char *linenumber) {
    removeCR(linenumber);
    if (smartbranching) {
        fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -128)\n	bcs .%s\n", linenumber, linenumber, linenumber);
        fprintf(outputFile, " else\n	bcc .%dskip%s\n	jmp .%s\n", branchtargetnumber, linenumber, linenumber);
        fprintf(outputFile, ".%dskip%s\n", branchtargetnumber++, linenumber);
        fprintf(outputFile, " endif\n");
    } else {
        fprintf(outputFile, "	bcs .%s\n", linenumber);
        hotspotcheck(linenumber);
    }
}

void bvc(char *linenumber) {
    removeCR(linenumber);
    if (smartbranching) {
        fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -128)\n	bvc .%s\n", linenumber, linenumber, linenumber);
        fprintf(outputFile, " else\n	bvs .%dskip%s\n	jmp .%s\n", branchtargetnumber, linenumber, linenumber);
        fprintf(outputFile, ".%dskip%s\n", branchtargetnumber++, linenumber);
        fprintf(outputFile, " endif\n");
    } else {
        fprintf(outputFile, "	bvc .%s\n", linenumber);
        hotspotcheck(linenumber);
    }
}

void bvs(char *linenumber) {
    removeCR(linenumber);
    if (smartbranching) {
        fprintf(outputFile, " if ((* - .%s) < 127) && ((* - .%s) > -128)\n	bvs .%s\n", linenumber, linenumber, linenumber);
        fprintf(outputFile, " else\n	bvc .%dskip%s\n	jmp .%s\n", branchtargetnumber, linenumber, linenumber);
        fprintf(outputFile, ".%dskip%s\n", branchtargetnumber++, linenumber);
        fprintf(outputFile, " endif\n");
    } else {
        fprintf(outputFile, "	bvs .%s\n", linenumber);
        hotspotcheck(linenumber);
    }
}


void do_if_collision(char **statement, char **cstatement, int not) {
    int bit = 0;
    int i, k;

    if (bs == 28) {
        if ((!strncmp(statement[2], "collision(player\0", 16))
            && ((!strncmp(statement[2] + 17, ",player\0", 7)) || (!strncmp(statement[2] + 17, ",_player\0", 7)))) {

            // DPC+ custom collision
            char firstPlayerParam = statement[2][16];
            char secondPlayerParam = statement[2][24];
            if (secondPlayerParam == 'r') {
                secondPlayerParam = statement[2][25];
            }

            if (firstPlayerParam + secondPlayerParam != '0' + '1') {
                genCode_DPCPlusCollision(firstPlayerParam, secondPlayerParam);
                bit = 7;
            }
        }
    }

    if (!bit) {
        fprintf(outputFile, "	bit ");
        //bit = check_colls(statement[2]);
        bit = check_collisions(&statement[2]);
        fprintf(outputFile, "\n");
    }
    if (!bit)        //error
    {
        fprintf(stderr, "(%d) Error: Unknown collision type: %s\n", line, statement[2] + 9);
        exit(1);
    }


    if (!islabel(statement)) {
        // stmt[3] now [6] = 'then'
        // stmt[4] now [7] = (linenum)
        // old = 4 + 5-> new= 9
        char *thenGotoLabel = statement[9];
        if (!not) {
            if (bit == 7)
                bmi(thenGotoLabel);
            else
                bvs(thenGotoLabel);
        } else {
            if (bit == 7)
                bpl(thenGotoLabel);
            else
                bvc(thenGotoLabel);
        }

    } else            // then statement
    {
        if (not) {
            if (bit == 7)
                fprintf(outputFile, "	BMI ");
            else
                fprintf(outputFile, "	BVS ");
        } else {
            if (bit == 7)
                fprintf(outputFile, "	BPL ");
            else
                fprintf(outputFile, "	BVC ");
        }

        fprintf(outputFile, ".skip%s\n", statement[0]);
        // separate statement
        int stmtStart = 8; // old: 3 with collision() = 2
        for (i = stmtStart; i < 200; ++i) {
            for (k = 0; k < 200; ++k) {
                cstatement[i - stmtStart][k] = statement[i][k];
            }
        }
        fprintf(outputFile, ".condpart%d\n", condpart++);
        keywords(cstatement);
        fprintf(outputFile, ".skip%s\n", statement[0]);
    }
}

void do_if_joyswitch(char **statement, char **cstatement, int not) {
    int k;
    int i = switchjoy(statement[2]);
    if (!islabel(statement)) {
        if (!i) {
            if (not)
                bne(statement[4]);
            else
                beq(statement[4]);
        } else if (i == 1)    // bvc/bvs
        {
            if (not)
                bvs(statement[4]);
            else
                bvc(statement[4]);
        } else if (i == 2)    // bpl/bmi
        {
            if (not)
                bmi(statement[4]);
            else
                bpl(statement[4]);
        }
    } else            // then statement
    {
        if (!i) {
            if (not)
                fprintf(outputFile, "	BEQ ");
            else
                fprintf(outputFile, "	BNE ");
        }
        if (i == 1) {
            if (not)
                fprintf(outputFile, "	BVC ");
            else
                fprintf(outputFile, "	BVS ");
        }
        if (i == 2) {
            if (not)
                fprintf(outputFile, "	BPL ");
            else
                fprintf(outputFile, "	BMI ");
        }

        fprintf(outputFile, ".skip%s\n", statement[0]);
        // separate statement
        for (i = 3; i < 200; ++i) {
            for (k = 0; k < 200; ++k) {
                cstatement[i - 3][k] = statement[i][k];
            }
        }
        fprintf(outputFile, ".condpart%d\n", condpart++);
        keywords(cstatement);
        fprintf(outputFile, ".skip%s\n", statement[0]);
    }
}

void do_if_pfread(char **statement, char **cstatement, int not) {
    int i,k;
    pfread(statement);
    if (!islabel(statement)) {
        if (not)
            bne(statement[9]);
        else
            beq(statement[9]);

    } else            // then statement
    {
        if (not)
            fprintf(outputFile, "	BEQ ");
        else
            fprintf(outputFile, "	BNE ");

        fprintf(outputFile, ".skip%s\n", statement[0]);
        // separate statement
        for (i = 8; i < 200; ++i) {
            for (k = 0; k < 200; ++k) {
                cstatement[i - 8][k] = statement[i][k];
            }
        }
        fprintf(outputFile, ".condpart%d\n", condpart++);
        keywords(cstatement);
        fprintf(outputFile, ".skip%s\n", statement[0]);
    }
}

void doif(char **statement) {
    int index = 0;
    int situation = 0;
    char getindex0[200];
    char getindex1[200];
    int not = 0;
    int complex_boolean = 0;
    int i, j, k, h;
    int push1 = 0;
    int push2 = 0;
    int bit = 0;
    int Aregmatch = 0;
    char Aregcopy[200];
    char **cstatement;        //conditional statement
    char **dealloccstatement;    //for deallocation

    strcpy(Aregcopy, "index-invalid");

    cstatement = (char **) malloc(sizeof(char *) * 200);
    for (k = 0; k < 200; ++k)
        cstatement[k] = (char *) malloc(sizeof(char) * 200);
    dealloccstatement = cstatement;
    for (k = 0; k < 200; ++k)
        for (j = 0; j < 200; ++j)
            cstatement[j][k] = '\0';
    if ((statement[2][0] == '!') && (statement[2][1] != '\0')) {
        not = 1;
        for (i = 0; i < 199; ++i) {
            statement[2][i] = statement[2][i + 1];
        }
    } else if (!strncmp(statement[2], "!\0", 2)) {
        not = 1;
        compressdata(statement, 2, 1);
    }

    if (statement[2][0] == '(') {
        j = 0;
        k = 0;
        for (i = 2; i < 199; ++i) {
            if (statement[i][0] == '(')
                j++;
            if (statement[i][0] == ')')
                j--;
            if (statement[i][0] == '<')
                break;
            if (statement[i][0] == '>')
                break;
            if (statement[i][0] == '=')
                break;
            if (statement[i][0] == '&' && statement[i][1] == '\0')
                k = j;
            if (!strncmp(statement[i], "then\0", 4)) {
                complex_boolean = 1;
                break;
            }            //prerror("Complex boolean not yet supported\n");exit(1);}
        }
        if (i == 199 && k)
            j = k;
        if (j) {
            compressdata(statement, 2, 1);    //remove first parenthesis
            for (i = 2; i < 199; ++i)
                if ((!strncmp(statement[i], "then\0", 4)) ||
                    (!strncmp(statement[i], "&&\0", 2)) || (!strncmp(statement[i], "||\0", 2)))
                    break;
            if (i != 199) {
                if (statement[i - 1][0] != ')') {
                    prerror("Unbalanced parentheses in if-then\n");
                    exit(1);
                }
                compressdata(statement, i - 1, 1);
            }
        }
    }

    //------------------------------------------------------
    //--- handle special conditional checks

    if ((!strncmp(statement[2], "joy\0", 3)) || (!strncmp(statement[2], "switch\0", 6))) {
        do_if_joyswitch(statement, cstatement, not);
        freemem(dealloccstatement);
        return;
    }

    if (!strncmp(statement[2], "pfread\0", 6)) {
        do_if_pfread(statement, cstatement, not);
        freemem(dealloccstatement);
        return;
    }


    if (!strncmp(statement[2], "collision", 9)) {
        do_if_collision(statement, cstatement, not);
        freemem(dealloccstatement);
        return;
    }

    //--------------------------------------------------------
    //--- handle 'IF' cases utilizing arrays

    // check for array, e.g. x{1} to get bit 1 of x
    for (i = 3; i < 200; ++i) {
        if (statement[2][i] == '\0') {
            i = 200;
            break;
        }
        if (statement[2][i] == '}')
            break;
    }
    if (i < 200)        // found array
    {
        // extract expression in parantheses - for now just whole numbers allowed
        bit = (int) statement[2][i - 1] - '0';
        if ((bit > 9) || (bit < 0)) {
            fprintf(stderr, "(%d) Error: variables in bit access not supported\n", line);
            exit(1);
        }
        if ((bit == 7) || (bit == 6))    // if bit 6 or 7, we can use BIT and save 2 bytes
        {
            fprintf(outputFile, "	BIT ");
            for (i = 0; i < 200; ++i) {
                if (statement[2][i] == '{')
                    break;
                fprintf(outputFile, "%c", statement[2][i]);
            }
            fprintf(outputFile, "\n");
            if (!islabel(statement)) {
                if (!not) {
                    if (bit == 7)
                        bmi(statement[4]);
                    else
                        bvs(statement[4]);
                } else {
                    if (bit == 7)
                        bpl(statement[4]);
                    else
                        bvc(statement[4]);
                }
            } else        // then statement
            {
                if (not) {
                    if (bit == 7)
                        fprintf(outputFile, "	BMI ");
                    else
                        fprintf(outputFile, "	BVS ");
                } else {
                    if (bit == 7)
                        fprintf(outputFile, "	BPL ");
                    else
                        fprintf(outputFile, "	BVC ");
                }

                fprintf(outputFile, ".skip%s\n", statement[0]);
                // separate statement
                for (i = 3; i < 200; ++i) {
                    for (k = 0; k < 200; ++k) {
                        cstatement[i - 3][k] = statement[i][k];
                    }
                }
                fprintf(outputFile, ".condpart%d\n", condpart++);
                keywords(cstatement);
                fprintf(outputFile, ".skip%s\n", statement[0]);

            }
        } else {
            Aregmatch = 0;
            fprintf(outputFile, "	LDA ");
            for (i = 0; i < 200; ++i) {
                if (statement[2][i] == '{')
                    break;
                fprintf(outputFile, "%c", statement[2][i]);
            }
            fprintf(outputFile, "\n");
            if (!bit)        // if bit 0, we can use LSR and save a byte
                fprintf(outputFile, "	LSR\n");
            else
                fprintf(outputFile, "	AND #%d\n", 1 << bit);    //(int)pow(2,bit));
            if (!islabel(statement)) {
                if (not) {
                    if (!bit)
                        bcc(statement[4]);
                    else
                        beq(statement[4]);
                } else {
                    if (!bit)
                        bcs(statement[4]);
                    else
                        bne(statement[4]);
                }

            } else        // then statement
            {
                if (not) {
                    if (!bit)
                        fprintf(outputFile, "	BCS ");
                    else
                        fprintf(outputFile, "	BNE ");
                } else {
                    if (!bit)
                        fprintf(outputFile, "	BCC ");
                    else
                        fprintf(outputFile, "	BEQ ");
                }

                fprintf(outputFile, ".skip%s\n", statement[0]);
                // separate statement
                for (i = 3; i < 200; ++i) {
                    for (k = 0; k < 200; ++k) {
                        cstatement[i - 3][k] = statement[i][k];
                    }
                }
                fprintf(outputFile, ".condpart%d\n", condpart++);
                keywords(cstatement);
                fprintf(outputFile, ".skip%s\n", statement[0]);

            }
        }
        freemem(dealloccstatement);
        return;
    }

// generic if-then (no special considerations)
    //check for [indexing]
    strcpy(Aregcopy, statement[2]);
    if (!strcmp(statement[2], Areg))
        Aregmatch = 1;        // do we already have the correct value in A?

    for (i = 3; i < 200; ++i)
        if ((!strncmp(statement[i], "then\0", 4)) ||
            (!strncmp(statement[i], "&&\0", 2)) || (!strncmp(statement[i], "||\0", 2)))
            break;

    j = 0;
    for (k = 3; k < i; ++k) {
        if (statement[k][0] == '&' && statement[k][1] == '\0')
            j = k;
        if ((statement[k][0] == '<') || (statement[k][0] == '>') || (statement[k][0] == '='))
            break;
    }
    if ((k == i) && j)
        k = j;            // special case of & for efficient code

    if ((complex_boolean) || (k == i && i > 4)) {
        // complex boolean found
        // assign value to contents, reissue statement as boolean
        strcpy(cstatement[2], "Areg\0");
        strcpy(cstatement[3], "=\0");
        for (j = 2; j < i; ++j)
            strcpy(cstatement[j + 2], statement[j]);

        dolet(cstatement);

        if (!islabel(statement))    // then linenumber
        {
            if (not)
                beq(statement[i + 1]);
            else
                bne(statement[i + 1]);
        } else            // then statement
        {            // first, take negative of condition and branch around statement
            j = i;
            if (not)
                fprintf(outputFile, "	BNE ");
            else
                fprintf(outputFile, "	BEQ ");
        }
        fprintf(outputFile, ".skip%s\n", statement[0]);
        // separate statement
        for (i = j; i < 200; ++i) {
            for (k = 0; k < 200; ++k) {
                cstatement[i - j][k] = statement[i][k];
            }
        }
        fprintf(outputFile, ".condpart%d\n", condpart++);
        keywords(cstatement);
        fprintf(outputFile, ".skip%s\n", statement[0]);


        Aregmatch = 0;
        freemem(dealloccstatement);
        return;
    } else if (((k < i) && (i - k != 2)) || ((k < i) && (k > 3))) {
        fprintf(outputFile, "; complex condition detected\n");
        // complex statements will be changed to assignments and reissued as assignments followed by a simple compare
        // i=location of then
        // k=location of conditional operator
        // if is at 2
        if (not) {            // handles =, <, <=, >, >=, <>
            // & handled later
            if (!strncmp(statement[k], "=\0", 2)) {
                statement[3][0] = '<';    // force beq/bne below
                statement[3][1] = '>';
                statement[3][2] = '\0';
            } else if (!strncmp(statement[k], "<>", 2)) {
                statement[3][0] = '=';    // force beq/bne below
                statement[3][1] = '\0';
            } else if (!strncmp(statement[k], "<=", 2)) {
                statement[3][0] = '>';    // force beq/bne below
                statement[3][1] = '\0';
            } else if (!strncmp(statement[k], ">=", 2)) {
                statement[3][0] = '<';    // force beq/bne below
                statement[3][1] = '\0';
            } else if (!strncmp(statement[k], "<\0", 2)) {
                statement[3][0] = '>';    // force beq/bne below
                statement[3][1] = '=';
                statement[3][2] = '\0';
            } else if (!strncmp(statement[k], "<\0", 2)) {
                statement[3][0] = '>';    // force beq/bne below
                statement[3][1] = '=';
                statement[3][2] = '\0';
            }
        }
        if (k > 4)
            push1 = 1;        // first statement is complex
        if (i - k != 2)
            push2 = 1;        // second statement is complex

        // <, >=, &, = do not swap
        // > or <= swap

        if (push1 == 1 && push2 == 1 && (strncmp(statement[k], ">\0", 2)) && (strncmp(statement[k], "<=\0", 2))) {
            // Assign to Areg and push
            strcpy(cstatement[2], "Areg\0");
            strcpy(cstatement[3], "=\0");
            for (j = 2; j < k; ++j) {
                for (h = 0; h < 200; ++h) {
                    cstatement[j + 2][h] = statement[j][h];
                }
            }
            dolet(cstatement);
            fprintf(outputFile, "  PHA\n");
            // second statement:
            strcpy(cstatement[2], "Areg\0");
            strcpy(cstatement[3], "=\0");
            for (j = k + 1; j < i; ++j) {
                for (h = 0; h < 200; ++h) {
                    cstatement[j - k + 3][h] = statement[j][h];
                }
            }
            dolet(cstatement);
            fprintf(outputFile, "  PHA\n");
            situation = 1;
        } else if (push1 == 1 && push2 == 1)    // two pushes plus swaps
        {
            // second statement first:
            strcpy(cstatement[2], "Areg\0");
            strcpy(cstatement[3], "=\0");
            for (j = k + 1; j < i; ++j) {
                for (h = 0; h < 200; ++h) {
                    cstatement[j - k + 3][h] = statement[j][h];
                }
            }
            dolet(cstatement);
            fprintf(outputFile, "  PHA\n");

            // first statement second
            strcpy(cstatement[2], "Areg\0");
            strcpy(cstatement[3], "=\0");
            for (j = 2; j < k; ++j) {
                for (h = 0; h < 200; ++h) {
                    cstatement[j + 2][h] = statement[j][h];
                }
            }
            dolet(cstatement);
            fprintf(outputFile, "  PHA\n");

            // now change operator
            // > or <= swap
            if (!strncmp(statement[k], ">\0", 2))
                strcpy(statement[k], "<\0");
            if (!strncmp(statement[k], "<=\0", 2))
                strcpy(statement[k], ">=\0");
            situation = 2;
        } else if (push1 == 1 && (strncmp(statement[k], ">\0", 2)) && (strncmp(statement[k], "<=\0", 2))) {
            // first statement only, no swap
            strcpy(cstatement[2], "Areg\0");
            strcpy(cstatement[3], "=\0");
            for (j = 2; j < k; ++j) {
                for (h = 0; h < 200; ++h) {
                    cstatement[j + 2][h] = statement[j][h];
                }
            }
            dolet(cstatement);
            //fprintf(outputFile, "  PHA\n");
            situation = 3;

        } else if (push1 == 1) {
            // first statement only, swap
            strcpy(cstatement[2], "Areg\0");
            strcpy(cstatement[3], "=\0");
            for (j = 2; j < k; ++j) {
                for (h = 0; h < 200; ++h) {
                    cstatement[j + 2][h] = statement[j][h];
                }
            }
            dolet(cstatement);
            fprintf(outputFile, "  PHA\n");

            // now change operator
            // > or <= swap
            if (!strncmp(statement[k], ">\0", 2))
                strcpy(statement[k], "<\0");
            if (!strncmp(statement[k], "<=\0", 2))
                strcpy(statement[k], ">=\0");

            // swap pushes and vars:
            push1 = 0;
            push2 = 1;
            strcpy(statement[2], statement[k + 1]);
            situation = 4;

        } else if (push2 == 1 && (strncmp(statement[k], ">\0", 2)) && (strncmp(statement[k], "<=\0", 2))) {
            // second statement only, no swap:
            strcpy(cstatement[2], "Areg\0");
            strcpy(cstatement[3], "=\0");
            for (j = k + 1; j < i; ++j) {
                for (h = 0; h < 200; ++h) {
                    cstatement[j - k + 3][h] = statement[j][h];
                }
            }
            dolet(cstatement);
            fprintf(outputFile, "  PHA\n");
            situation = 5;
        } else if (push2 == 1) {
            // second statement only, swap:
            strcpy(cstatement[2], "Areg\0");
            strcpy(cstatement[3], "=\0");
            for (j = k + 1; j < i; ++j) {
                for (h = 0; h < 200; ++h) {
                    cstatement[j - k + 3][h] = statement[j][h];
                }
            }
            dolet(cstatement);
            //fprintf(outputFile, "  PHA\n");
            // now change operator
            // > or <= swap
            if (!strncmp(statement[k], ">\0", 2))
                strcpy(statement[k], "<\0");
            if (!strncmp(statement[k], "<=\0", 2))
                strcpy(statement[k], ">=\0");

            // swap pushes and vars:
            push1 = 1;
            push2 = 0;
            strcpy(statement[k + 1], statement[2]);
            situation = 6;
        } else            // should never get here
        {
            prerror("Parse error in complex if-then statement\n");
            exit(1);
        }
        if (situation != 6 && situation != 3) {
            fprintf(outputFile, "  TSX\n");    //index to stack
            if (push1)
                fprintf(outputFile, "  PLA\n");
            if (push2)
                fprintf(outputFile, "  PLA\n");
        }
        if (push1 && push2)
            strcpy(cstatement[2], " 2[TSX]\0");
        else if (push1)
            strcpy(cstatement[2], " 1[TSX]\0");
        else
            strcpy(cstatement[2], statement[2]);
        strcpy(cstatement[3], statement[k]);
        if (push2)
            strcpy(cstatement[4], " 1[TSX]\0");
        else
            strcpy(cstatement[4], statement[k + 1]);
        for (j = 5; j < 40; ++j)
            strcpy(cstatement[j], statement[j - 5 + i]);
        strcpy(cstatement[0], statement[0]);    // copy label
        if (situation != 4 && situation != 5)
            strcpy(Areg, cstatement[2]);    // attempt to suppress superfluous LDA

        if (not && statement[k][0] == '&') {
            shiftdata(cstatement, 5);
            cstatement[5][0] = ')';
            cstatement[5][1] = '\0';
            shiftdata(cstatement, 2);
            shiftdata(cstatement, 2);
            cstatement[2][0] = '!';
            cstatement[2][1] = '\0';
            cstatement[3][0] = '(';
            cstatement[3][1] = '\0';
        }
        strcpy(cstatement[1], "if\0");
        if (statement[i][0] == 't')
            doif(cstatement);    // okay to recurse
        else if (statement[i][0] == '&') {
            if (situation != 4 && situation != 5)
                fprintf(outputFile, "; todo: this LDA is spurious and should be prevented ->");
            keywords(cstatement);    // statement still has booleans - attempt to reanalyze
        } else {
            prerror("if-then too complex for logical OR\n");
            exit(1);
        }
        Aregmatch = 0;
        freemem(dealloccstatement);
        return;
    }
    index |= getindex(statement[2], &getindex0[0]);
    if (strncmp(statement[3], "then\0", 4))
        index |= getindex(statement[4], &getindex1[0]) << 1;

    if (!Aregmatch)        // do we already have the correct value in A?
    {
        if (index & 1)
            loadindex(&getindex0[0]);
        fprintf(outputFile, "	LDA ");
        printindex(statement[2], index & 1);
        strcpy(Areg, Aregcopy);
    }
    if (index & 2)
        loadindex(&getindex1[0]);
//todo:check for cmp #0--useless except for <, > to clear carry
    if (strncmp(statement[3], "then\0", 4)) {
        if (statement[3][0] == '&') {
            fprintf(outputFile, "	AND ");
            if (not) {
                statement[3][0] = '=';    // force beq/bne below
                statement[3][1] = '\0';
            } else {
                statement[3][0] = '<';    // force beq/bne below
                statement[3][1] = '>';
                statement[3][2] = '\0';
            }
        } else
            fprintf(outputFile, "	CMP ");
        printindex(statement[4], index & 2);
    }

    if (!islabel(statement)) {                // then linenumber
        if (statement[3][0] == '=')
            beq(statement[6]);
        if (!strncmp(statement[3], "<>\0", 2))
            bne(statement[6]);
        else if (statement[3][0] == '<')
            bcc(statement[6]);
        if (statement[3][0] == '>')
            bcs(statement[6]);
        if (!strncmp(statement[3], "then\0", 4)) {
            if (not)
                beq(statement[4]);
            else
                bne(statement[4]);
        }
    } else            // then statement
    {                // first, take negative of condition and branch around statement
        if (statement[3][0] == '=')
            fprintf(outputFile, "     BNE ");
        if (!strcmp(statement[3], "<>"))
            fprintf(outputFile, "     BEQ ");
        else if (statement[3][0] == '<')
            fprintf(outputFile, "     BCS ");
        if (statement[3][0] == '>')
            fprintf(outputFile, "     BCC ");
        j = 5;

        if (!strncmp(statement[3], "then\0", 4)) {
            j = 3;
            if (not)
                fprintf(outputFile, "	BNE ");
            else
                fprintf(outputFile, "	BEQ ");
        }
        fprintf(outputFile, ".skip%s\n", statement[0]);
        // separate statement

        // separate statement
        for (i = j; i < 200; ++i) {
            for (k = 0; k < 200; ++k) {
                cstatement[i - j][k] = statement[i][k];
            }
        }
        fprintf(outputFile, ".condpart%d\n", condpart++);
        keywords(cstatement);
        fprintf(outputFile, ".skip%s\n", statement[0]);

        freemem(dealloccstatement);
        return;


//    i=4;
//    while (statement[i][0]!='\0')
//    {
//      cstatement[i-4]=statement[i++];
//    }
//    keywords(cstatement);
//    fprintf(outputFile, ".skip%s\n",statement[0]);
    }
    freemem(dealloccstatement);
}


void ongoto(char **statement) {
// warning!!! bankswitching not yet supported
    int k, i = 4;

    if (!strncmp(statement[3], "gosub\0", 5)) {
        fprintf(outputFile, "	lda #>(ongosub%d-1)\n", ongosub);
        fprintf(outputFile, "	PHA\n");
        fprintf(outputFile, "	lda #<(ongosub%d-1)\n", ongosub);
        fprintf(outputFile, "	PHA\n");
    }
    if (strcmp(statement[2], Areg))
        fprintf(outputFile, "	LDX %s\n", statement[2]);
    //fprintf(outputFile, "    ASL\n");
    //fprintf(outputFile, "    TAX\n");
    fprintf(outputFile, "	LDA .%sjumptablehi,x\n", statement[0]);
    fprintf(outputFile, "	PHA\n");
    //fprintf(outputFile, "    INX\n");
    fprintf(outputFile, "	LDA .%sjumptablelo,x\n", statement[0]);
    fprintf(outputFile, "	PHA\n");
    fprintf(outputFile, "	RTS\n");
    fprintf(outputFile, ".%sjumptablehi\n", statement[0]);
    while ((statement[i][0] != ':') && (statement[i][0] != '\0')) {
        for (k = 0; k < 200; ++k)
            if ((statement[i][k] == (unsigned char) 0x0A) || (statement[i][k] == (unsigned char) 0x0D))
                statement[i][k] = '\0';
        fprintf(outputFile, "	.byte >(.%s-1)\n", statement[i++]);
    }
    fprintf(outputFile, ".%sjumptablelo\n", statement[0]);
    i = 4;
    while ((statement[i][0] != ':') && (statement[i][0] != '\0')) {
        for (k = 0; k < 200; ++k)
            if ((statement[i][k] == (unsigned char) 0x0A) || (statement[i][k] == (unsigned char) 0x0D))
                statement[i][k] = '\0';
        fprintf(outputFile, "	.byte <(.%s-1)\n", statement[i++]);
    }
    if (!strncmp(statement[3], "gosub\0", 5))
        fprintf(outputFile, "ongosub%d\n", ongosub++);
}

void dofor(char **statement) {
    if (strcmp(statement[4], Areg)) {
        fprintf(outputFile, "	LDA ");
        printimmed(statement[4]);
        fprintf(outputFile, "%s\n", statement[4]);
    }

    fprintf(outputFile, "	STA %s\n", statement[2]);

    forlabel[numfors][0] = '\0';
    sprintf(forlabel[numfors], "%sfor%s", statement[0], statement[2]);
    fprintf(outputFile, ".%s\n", forlabel[numfors]);

    forend[numfors][0] = '\0';
    strcpy(forend[numfors], statement[6]);

    forvar[numfors][0] = '\0';
    strcpy(forvar[numfors], statement[2]);

    forstep[numfors][0] = '\0';

    if (!strncasecmp(statement[7], "step\0", 4))
        strcpy(forstep[numfors], statement[8]);
    else
        strcpy(forstep[numfors], "1");

    numfors++;
}

void next(char **statement) {
    int immed = 0;
    int immedend = 0;
    int failsafe = 0;
    char failsafelabel[200];

    invalidate_Areg();

    if (!numfors) {
        fprintf(stderr, "(%d) next without for\n", line);
        exit(1);
    }
    numfors--;
    if (!strncmp(forstep[numfors], "1\0", 2))    // step 1
    {
        fprintf(outputFile, "	LDA %s\n", forvar[numfors]);
        fprintf(outputFile, "	CMP ");
        printimmed(forend[numfors]);
        fprintf(outputFile, "%s\n", forend[numfors]);
        fprintf(outputFile, "	INC %s\n", forvar[numfors]);
        bcc(forlabel[numfors]);
    } else if ((!strncmp(forstep[numfors], "-1\0", 3)) ||
               (!strncmp(forstep[numfors], "255\0", 4))) {                // step -1
        fprintf(outputFile, "	DEC %s\n", forvar[numfors]);
        if (strncmp(forend[numfors], "1\0", 2)) {
            fprintf(outputFile, "	LDA %s\n", forvar[numfors]);
            fprintf(outputFile, "	CMP ");
            if (!strncmp(forend[numfors], "0\0", 2)) {
                // the special case of 0 as end, since we can't check to see if it was smaller than 0
                fprintf(outputFile, "#255\n");
                bne(forlabel[numfors]);
            } else // general case
            {
                printimmed(forend[numfors]);
                fprintf(outputFile, "%s\n", forend[numfors]);
                bcs(forlabel[numfors]);
            }
        } else
            bne(forlabel[numfors]);
    } else            // step other than 1 or -1
    {
        // normally, the generated code adds to or subtracts from the for variable, and checks
        // to see if it's less than the ending value.
        // however, if the step would make the variable less than zero or more than 255
        // then this check will not work.  The compiler will attempt to detect this situation
        // if the step and end are known.  If the step and end are not known (that is,
        // either is a variable) then much more complex code must be generated.

        fprintf(outputFile, "	LDA %s\n", forvar[numfors]);
        fprintf(outputFile, "	CLC\n");
        fprintf(outputFile, "	ADC ");
        immed = printimmed(forstep[numfors]);
        fprintf(outputFile, "%s\n", forstep[numfors]);

        if (immed && isimmed(forend[numfors]))    // the step and end are immediate
        {
            if (atoi(forstep[numfors]) & 128)    // step is negative
            {
                if ((256 - (atoi(forstep[numfors]) & 255)) >=
                    atoi(forend[numfors])) {        // if we are in danger of going < 0...we will have carry clear after ADC
                    failsafe = 1;
                    sprintf(failsafelabel, "%s_failsafe", forlabel[numfors]);
                    bcc(failsafelabel);
                }
            } else {            // step is positive
                if ((atoi(forstep[numfors]) + atoi(forend[numfors])) >
                    255) {        // if we are in danger of going > 255...we will have carry set after ADC
                    failsafe = 1;
                    sprintf(failsafelabel, "%s_failsafe", forlabel[numfors]);
                    bcs(failsafelabel);
                }
            }

        }
        fprintf(outputFile, "	STA %s\n", forvar[numfors]);

        fprintf(outputFile, "	CMP ");
        immedend = printimmed(forend[numfors]);
        // add 1 to immediate compare for increasing loops
        if (immedend && !(atoi(forstep[numfors]) & 128))
            strcat(forend[numfors], "+1");
        fprintf(outputFile, "%s\n", forend[numfors]);
// if step number is 1 to 127 then add 1 and use bcc, otherwise bcs
// if step is a variable, we'll need to check for every loop iteration
//
// Warning! no failsafe checks with variables as step or end - it's the
// programmer's job to make sure the end value doesn't overflow
        if (!immed) {
            fprintf(outputFile, "	LDX %s\n", forstep[numfors]);
            fprintf(outputFile, "	BMI .%sbcs\n", statement[0]);
            bcc(forlabel[numfors]);
            fprintf(outputFile, "	CLC\n");
            fprintf(outputFile, ".%sbcs\n", statement[0]);
            bcs(forlabel[numfors]);
        } else if (atoi(forstep[numfors]) & 128)
            bcs(forlabel[numfors]);
        else {
            bcc(forlabel[numfors]);
            if (!immedend)
                beq(forlabel[numfors]);
        }
    }
    if (failsafe)
        fprintf(outputFile, ".%s\n", failsafelabel);
}




int getcondpart() {
    return condpart;
}

int orderofoperations(char op1, char op2) {
// specify order of operations for complex equations
// i.e.: parens, divmul (*/), +-, logical (^&|)
    if (op1 == '(')
        return 0;
    else if (op2 == '(')
        return 0;
    else if (op2 == ')')
        return 1;
    else if (((op1 == '^') || (op1 == '|') || (op1 == '&')) && ((op2 == '^') || (op2 == '|') || (op2 == '&')))
        return 0;
// else if (op1 == '^') return 1;
// else if (op1 == '&') return 1;
// else if (op1 == '|') return 1;
// else if (op2 == '^') return 0;
// else if (op2 == '&') return 0;
// else if (op2 == '|') return 0;
    else if ((op1 == '*') || (op1 == '/'))
        return 1;
    else if ((op2 == '*') || (op2 == '/'))
        return 0;
    else if ((op1 == '+') || (op1 == '-'))
        return 1;
    else if ((op2 == '+') || (op2 == '-'))
        return 0;
    else
        return 1;
}

int isoperator(char op) {
    if (!((op == '+') || (op == '-') || (op == '/') ||
          (op == '*') || (op == '&') || (op == '|') || (op == '^') || (op == ')') || (op == '(')))
        return 0;
    else
        return 1;
}

void displayoperation(char *opcode, char *operand, int index) {
    if (!strncmp(operand, "stackpull\0", 9)) {
        if (opcode[0] == '-') {
            // operands swapped
            fprintf(outputFile, "	TAY\n");
            fprintf(outputFile, "	PLA\n");
            fprintf(outputFile, "	TSX\n");
            fprintf(outputFile, "	STY $00,x\n");
            fprintf(outputFile, "	SEC\n");
            fprintf(outputFile, "	SBC $00,x\n");
        } else if (opcode[0] == '/') {
            // operands swapped
            fprintf(outputFile, "	TAY\n");
            fprintf(outputFile, "	PLA\n");
        } else {
            fprintf(outputFile, "	TSX\n");
            fprintf(outputFile, "	INX\n");
            fprintf(outputFile, "	TXS\n");
            fprintf(outputFile, "	%s $00,x\n", opcode + 1);
        }
    } else {
        fprintf(outputFile, "	%s ", opcode + 1);
        printindex(operand, index);
    }
}

void dec(char **cstatement) {
    decimal = 1;
    fprintf(outputFile, "	SED\n");
    dolet(cstatement);
    fprintf(outputFile, "	CLD\n");
    decimal = 0;
}


void dolet(char **cstatement) {
    int i, j = 0, bit = 0, k;
    int index = 0;
    char getindex0[200];
    char getindex1[200];
    char getindex2[200];
    int score[6] = {0, 0, 0, 0, 0, 0};
    char **statement;
    char *getbitvar;
    int Aregmatch = 0;
    char Aregcopy[200];
    char operandcopy[200];
    int fixpoint1;
    int fixpoint2;
    int fixpoint3 = 0;
    char **deallocstatement;
    char **rpn_statement;    // expression in rpn
    char rpn_stack[200][200];    // prolly doesn't need to be this big
    int sp = 0;            // stack pointer for converting to rpn
    int numrpn = 0;
    char **atomic_statement;    // singular statements to recurse back to here
    char tempstatement1[200], tempstatement2[200];

    strcpy(Aregcopy, "index-invalid");

    statement = (char **) malloc(sizeof(char *) * 200);
    deallocstatement = statement;
    if (!strncmp(cstatement[2], "=\0", 1)) {
        for (i = 198; i > 0; --i) {
            statement[i + 1] = cstatement[i];
        }
    } else
        statement = cstatement;

    // check for unary minus (e.g. a=-a) and insert zero before it
    if ((statement[4][0] == '-') && (statement[5][0]) > (unsigned char) 0x3F) {
        shiftdata(statement, 4);
        statement[4][0] = '0';
    }


    fixpoint1 = isfixpoint(statement[2]);
    fixpoint2 = isfixpoint(statement[4]);
    removeCR(statement[4]);

    // check for complex statement
    if ((!((statement[4][0] == '-') && (statement[6][0] == ':'))) &&
        (statement[5][0] != ':') && (statement[7][0] != ':')
        && (!((statement[5][0] == '(') && (statement[4][0] != '(')))
        && ((unsigned char) statement[5][0] > (unsigned char) 0x20)
        && ((unsigned char) statement[7][0] > (unsigned char) 0x20)) {
        fprintf(outputFile, "; complex statement detected\n");
        // complex statement here, hopefully.
        // convert equation to reverse-polish notation so we can express it in terms of
        // atomic equations and stack pushes/pulls
        rpn_statement = (char **) malloc(sizeof(char *) * 200);
        for (i = 0; i < 200; ++i) {
            rpn_statement[i] = (char *) malloc(sizeof(char) * 200);
            for (k = 0; k < 200; ++k)
                rpn_statement[i][k] = '\0';
        }

        atomic_statement = (char **) malloc(sizeof(char *) * 10);
        for (i = 0; i < 10; ++i) {
            atomic_statement[i] = (char *) malloc(sizeof(char) * 200);
            for (k = 0; k < 200; ++k)
                atomic_statement[i][k] = '\0';
        }

        // this converts expression to rpn
        for (k = 4; (statement[k][0] != '\0') && (statement[k][0] != ':'); k++) {
            // ignore CR/LF
            if (statement[k][0] == (unsigned char) 0x0A)
                continue;
            if (statement[k][0] == (unsigned char) 0x0D)
                continue;

            strcpy(tempstatement1, statement[k]);
            if (!isoperator(tempstatement1[0])) {
                strcpy(rpn_statement[numrpn++], tempstatement1);
            } else {
                while ((sp) && (orderofoperations(rpn_stack[sp - 1][0], tempstatement1[0]))) {
                    strcpy(tempstatement2, rpn_stack[sp - 1]);
                    sp--;
                    strcpy(rpn_statement[numrpn++], tempstatement2);
                }
                if ((sp) && (tempstatement1[0] == ')'))
                    sp--;
                else
                    strcpy(rpn_stack[sp++], tempstatement1);
            }
        }



        // get stuff off of our rpn stack
        while (sp) {
            strcpy(tempstatement2, rpn_stack[sp - 1]);
            sp--;
            strcpy(rpn_statement[numrpn++], tempstatement2);
        }

//for(i=0;i<20;++i)fprintf(stderr,"%s ",rpn_statement[i]);i=0;

        // now parse rpn statement

//    strcpy(atomic_statement[2],"Areg");
//    strcpy(atomic_statement[3],"=");
//    strcpy(atomic_statement[4],rpn_statement[0]);
//    strcpy(atomic_statement[5],rpn_statement[2]);
//    strcpy(atomic_statement[6],rpn_statement[1]);
//    dolet(atomic_statement);

        sp = 0;            // now use as pointer into rpn_statement
        while (sp < numrpn) {
            // clear atomic statement cache
            for (i = 0; i < 10; ++i)
                for (k = 0; k < 200; ++k)
                    atomic_statement[i][k] = '\0';
            if (isoperator(rpn_statement[sp][0])) {
                // operator: need stack pull as 2nd arg
                // Areg=Areg (op) stackpull
                strcpy(atomic_statement[2], "Areg");
                strcpy(atomic_statement[3], "=");
                strcpy(atomic_statement[4], "Areg");
                strcpy(atomic_statement[5], rpn_statement[sp++]);
                strcpy(atomic_statement[6], "stackpull");
                dolet(atomic_statement);
            } else if (isoperator(rpn_statement[sp + 1][0])) {
                // val,operator: Areg=Areg (op) val
                strcpy(atomic_statement[2], "Areg");
                strcpy(atomic_statement[3], "=");
                strcpy(atomic_statement[4], "Areg");
                strcpy(atomic_statement[6], rpn_statement[sp++]);
                strcpy(atomic_statement[5], rpn_statement[sp++]);
                dolet(atomic_statement);
            } else if (isoperator(rpn_statement[sp + 2][0])) {
                // val,val,operator: stackpush, then Areg=val1 (op) val2
                if (sp)
                    fprintf(outputFile, "	PHA\n");
                strcpy(atomic_statement[2], "Areg");
                strcpy(atomic_statement[3], "=");
                strcpy(atomic_statement[4], rpn_statement[sp++]);
                strcpy(atomic_statement[6], rpn_statement[sp++]);
                strcpy(atomic_statement[5], rpn_statement[sp++]);
                dolet(atomic_statement);
            } else {
                if ((rpn_statement[sp] == 0) || (rpn_statement[sp + 1] == 0) || (rpn_statement[sp + 2] == 0)) {
                    // incomplete or unrecognized expression
                    prerror("Cannot evaluate expression\n");
                    exit(1);
                }
                // val,val,val: stackpush, then load of next value
                if (sp)
                    fprintf(outputFile, "	PHA\n");
                strcpy(atomic_statement[2], "Areg");
                strcpy(atomic_statement[3], "=");
                strcpy(atomic_statement[4], rpn_statement[sp++]);
                dolet(atomic_statement);
            }
        }
        // done, now assign A-reg to original value
        for (i = 0; i < 10; ++i)
            for (k = 0; k < 200; ++k)
                atomic_statement[i][k] = '\0';
        strcpy(atomic_statement[2], statement[2]);
        strcpy(atomic_statement[3], "=");
        strcpy(atomic_statement[4], "Areg");
        dolet(atomic_statement);
        return;            // bye-bye!
    }


    //check for [indexing]
    strcpy(Aregcopy, statement[4]);
    if (!strcmp(statement[4], Areg))
        Aregmatch = 1;        // do we already have the correct value in A?

    index |= getindex(statement[2], &getindex0[0]);
    index |= getindex(statement[4], &getindex1[0]) << 1;
    if (statement[5][0] != ':')
        index |= getindex(statement[6], &getindex2[0]) << 2;


    // check for array, e.g. x(1) to access bit 1 of x
    for (i = 3; i < 200; ++i) {
        if (statement[2][i] == '\0') {
            i = 200;
            break;
        }
        if (statement[2][i] == '}')
            break;
    }
    if (i < 200)        // found bit
    {
        strcpy(Areg, "invalid");
        // extract expression in parantheses - for now just whole numbers allowed
        bit = (int) statement[2][i - 1] - '0';
        if ((bit > 9) || (bit < 0)) {
            fprintf(stderr, "(%d) Error: variables in bit access not supported\n", line);
            exit(1);
        }
        if (bit > 7) {
            fprintf(stderr, "(%d) Error: invalid bit access\n", line);
            exit(1);
        }

        if (statement[4][0] == '0') {
            fprintf(outputFile, "	LDA ");
            for (i = 0; i < 200; ++i) {
                if (statement[2][i] == '{')
                    break;
                fprintf(outputFile, "%c", statement[2][i]);
            }
            fprintf(outputFile, "\n");
            fprintf(outputFile, "	AND #%d\n", 255 ^ (1 << bit));    //(int)pow(2,bit));
        } else if (statement[4][0] == '1') {
            fprintf(outputFile, "	LDA ");
            for (i = 0; i < 200; ++i) {
                if (statement[2][i] == '{')
                    break;
                fprintf(outputFile, "%c", statement[2][i]);
            }
            fprintf(outputFile, "\n");
            fprintf(outputFile, "	ORA #%d\n", 1 << bit);    //(int)pow(2,bit));
        } else if ((getbitvar = strtok(statement[4], "{"))) {            // assign one bit to another
            // removed NOP Abs to eventully support 0840 bankswitching
            if (getbitvar[0] == '!')
                fprintf(outputFile, "	LDA %s\n", getbitvar + 1);
            else
                fprintf(outputFile, "	LDA %s\n", getbitvar);
            fprintf(outputFile, "	AND #%d\n", (1 << ((int) statement[4][strlen(getbitvar) + 1] - '0')));
            fprintf(outputFile, "  PHP\n");
            fprintf(outputFile, "	LDA ");
            for (i = 0; i < 200; ++i) {
                if (statement[2][i] == '{')
                    break;
                fprintf(outputFile, "%c", statement[2][i]);
            }
            fprintf(outputFile, "\n	AND #%d\n", 255 ^ (1 << bit));    //(int)pow(2,bit));
            fprintf(outputFile, "  PLP\n");
            if (getbitvar[0] == '!')
                fprintf(outputFile, "	.byte $D0, $02\n");    //bad, bad way to do BEQ addr+5
            else
                fprintf(outputFile, "	.byte $F0, $02\n");    //bad, bad way to do BNE addr+5

            fprintf(outputFile, "	ORA #%d\n", 1 << bit);    //(int)pow(2,bit));

        } else {
            fprintf(stderr, "(%d) Error: can only assign 0, 1 or another bit to a bit\n", line);
            exit(1);
        }
        fprintf(outputFile, "	STA ");
        for (i = 0; i < 200; ++i) {
            if (statement[2][i] == '{')
                break;
            fprintf(outputFile, "%c", statement[2][i]);
        }
        fprintf(outputFile, "\n");
        free(deallocstatement);
        return;
    }

    if (statement[4][0] == '-')    // assignment to negative
    {
        strcpy(Areg, "invalid");
        if ((!fixpoint1) && (isfixpoint(statement[5]) != 12)) {
            if (statement[5][0] > (unsigned char) 0x39)    // perhaps include constants too?
            {
                fprintf(outputFile, "	LDA #0\n");
                fprintf(outputFile, "  SEC\n");
                fprintf(outputFile, "	SBC %s\n", statement[5]);
            } else
                fprintf(outputFile, "	LDA #%d\n", 256 - atoi(statement[5]));
        } else {
            if (fixpoint1 == 4) {
                if (statement[5][0] > (unsigned char) 0x39)    // perhaps include constants too?
                {
                    fprintf(outputFile, "	LDA #0\n");
                    fprintf(outputFile, "  SEC\n");
                    fprintf(outputFile, "	SBC %s\n", statement[5]);
                } else
                    fprintf(outputFile, "	LDA #%d\n", (int) ((16 - atof(statement[5])) * 16));
                fprintf(outputFile, "	STA %s\n", statement[2]);
                free(deallocstatement);
                return;
            }
            if (fixpoint1 == 8) {
                fprintf(outputFile, "	LDX ");
                sprintf(statement[4], "%f", 256.0 - atof(statement[5]));
                printfrac(statement[4]);
                fprintf(outputFile, "	STX ");
                printfrac(statement[2]);
                fprintf(outputFile, "	LDA #%s\n", statement[4]);
                fprintf(outputFile, "	STA %s\n", statement[2]);
                free(deallocstatement);
                return;
            }
        }
    } else if (!strncmp(statement[4], "rand\0", 4)) {
        strcpy(Areg, "invalid");
        if (optimization & 8) {
            fprintf(outputFile, "        lda rand\n");
            fprintf(outputFile, "        lsr\n");
            fprintf(outputFile, " ifconst rand16\n");
            fprintf(outputFile, "        rol rand16\n");
            fprintf(outputFile, " endif\n");
            fprintf(outputFile, "        bcc *+4\n");
            fprintf(outputFile, "        eor #$B4\n");
            fprintf(outputFile, "        sta rand\n");
            fprintf(outputFile, " ifconst rand16\n");
            fprintf(outputFile, "        eor rand16\n");
            fprintf(outputFile, " endif\n");
        } else if (bs == 28)
            fprintf(outputFile, "        lda rand\n");
        else
            jsr("randomize");
    } else if ((!strncmp(statement[2], "score\0", 6)) && (strncmp(statement[2], "scorec\0", 6))) {
// break up into three parts
        strcpy(Areg, "invalid");
        if (statement[5][0] == '+') {
            fprintf(outputFile, "	SED\n");
            fprintf(outputFile, "	CLC\n");
            for (i = 5; i >= 0; i--) {
                if (statement[6][i] != '\0')
                    score[j] = number(statement[6][i]);
                score[j] = number(statement[6][i]);
                if ((score[j] < 10) && (score[j] >= 0))
                    j++;
            }
            if (score[0] | score[1]) {
                fprintf(outputFile, "	LDA score+2\n");
                if (statement[6][0] > (unsigned char) 0x3F)
                    fprintf(outputFile, "	ADC %s\n", statement[6]);
                else
                    fprintf(outputFile, "	ADC #$%d%d\n", score[1], score[0]);
                fprintf(outputFile, "	STA score+2\n");
            }
            if (score[0] | score[1] | score[2] | score[3]) {
                fprintf(outputFile, "	LDA score+1\n");
                if (score[0] > 9)
                    fprintf(outputFile, "	ADC #0\n");
                else
                    fprintf(outputFile, "	ADC #$%d%d\n", score[3], score[2]);
                fprintf(outputFile, "	STA score+1\n");
            }
            fprintf(outputFile, "	LDA score\n");
            if (score[0] > 9)
                fprintf(outputFile, "	ADC #0\n");
            else
                fprintf(outputFile, "	ADC #$%d%d\n", score[5], score[4]);
            fprintf(outputFile, "	STA score\n");
            fprintf(outputFile, "	CLD\n");
        } else if (statement[5][0] == '-') {
            fprintf(outputFile, "	SED\n");
            fprintf(outputFile, "	SEC\n");
            for (i = 5; i >= 0; i--) {
                if (statement[6][i] != '\0')
                    score[j] = number(statement[6][i]);
                score[j] = number(statement[6][i]);
                if ((score[j] < 10) && (score[j] >= 0))
                    j++;
            }
            fprintf(outputFile, "	LDA score+2\n");
            if (score[0] > 9)
                fprintf(outputFile, "	SBC %s\n", statement[6]);
            else
                fprintf(outputFile, "	SBC #$%d%d\n", score[1], score[0]);
            fprintf(outputFile, "	STA score+2\n");
            fprintf(outputFile, "	LDA score+1\n");
            if (score[0] > 9)
                fprintf(outputFile, "	SBC #0\n");
            else
                fprintf(outputFile, "	SBC #$%d%d\n", score[3], score[2]);
            fprintf(outputFile, "	STA score+1\n");
            fprintf(outputFile, "	LDA score\n");
            if (score[0] > 9)
                fprintf(outputFile, "	SBC #0\n");
            else
                fprintf(outputFile, "	SBC #$%d%d\n", score[5], score[4]);
            fprintf(outputFile, "	STA score\n");
            fprintf(outputFile, "	CLD\n");
        } else {
            for (i = 5; i >= 0; i--) {
                if (statement[4][i] != '\0')
                    score[j] = number(statement[4][i]);
                score[j] = number(statement[4][i]);
                if ((score[j] < 10) && (score[j] >= 0))
                    j++;
            }
            fprintf(outputFile, "	LDA #$%d%d\n", score[1], score[0]);
            fprintf(outputFile, "	STA score+2\n");
            fprintf(outputFile, "	LDA #$%d%d\n", score[3], score[2]);
            fprintf(outputFile, "	STA score+1\n");
            fprintf(outputFile, "	LDA #$%d%d\n", score[5], score[4]);
            fprintf(outputFile, "	STA score\n");
        }
        free(deallocstatement);
        return;

    } else if ((statement[6][0] == '1') && ((statement[6][1] > (unsigned char) 0x39)
                                            || (statement[6][1] < (unsigned char) 0x30)) &&
               ((statement[5][0] == '+') || (statement[5][0] == '-')) &&
               (!strncmp(statement[2], statement[4], 200)) &&
               (strncmp(statement[2], "Areg\0", 4)) &&
               (statement[6][1] == '\0' || statement[6][1] == ' ' || statement[6][1] == '\n') &&
               (decimal == 0)) {                // var=var +/- something
        strcpy(Areg, "invalid");
        if ((fixpoint1 == 4) && (fixpoint2 == 4)) {
            if (statement[5][0] == '+') {
                fprintf(outputFile, "	LDA %s\n", statement[2]);
                fprintf(outputFile, "	CLC\n");
                fprintf(outputFile, "	ADC #16\n");
                fprintf(outputFile, "	STA %s\n", statement[2]);
                free(deallocstatement);
                return;
            }
            if (statement[5][0] == '-') {
                fprintf(outputFile, "	LDA %s\n", statement[2]);
                fprintf(outputFile, "	SEC\n");
                fprintf(outputFile, "	SBC #16\n");
                fprintf(outputFile, "	STA %s\n", statement[2]);
                free(deallocstatement);
                return;
            }
        }

        if (index & 1)
            loadindex(&getindex0[0]);
        if (statement[5][0] == '+')
            fprintf(outputFile, "	INC ");
        else
            fprintf(outputFile, "	DEC ");
        if (!(index & 1))
            fprintf(outputFile, "%s\n", statement[2]);
        else
            fprintf(outputFile, "%s,x\n", statement[4]);    // indexed with x!
        free(deallocstatement);

        return;
    } else {                // This is generic x=num or var

        if (!Aregmatch)        // do we already have the correct value in A?
        {
            if (index & 2)
                loadindex(&getindex1[0]);

// if 8.8=8.8+8.8: this LDA will be superfluous - fix this at some point

//      if (!fixpoint1 && !fixpoint2 && statement[5][0]!='(')
            if (((!fixpoint1 && !fixpoint2) || (!fixpoint1 && fixpoint2 == 8)) && statement[5][0] != '(')

//      printfrac(statement[4]);
//      else
            {
                if (strncmp(statement[4], "Areg\n", 4)) {
                    fprintf(outputFile, "	LDA ");
                    printindex(statement[4], index & 2);
                }
            }
            strcpy(Areg, Aregcopy);
        }
    }
    if ((statement[5][0] != '\0') && (statement[5][0] != ':')) {                // An operator was found
        fixpoint3 = isfixpoint(statement[6]);
        strcpy(Areg, "invalid");
        if (index & 4)
            loadindex(&getindex2[0]);
        if (statement[5][0] == '+') {
//      if ((fixpoint1 == 4) && (fixpoint2 == 4))
//      {

//      }
//      else
//      {
            if ((fixpoint1 == 8) && ((fixpoint2 & 8) == 8) && ((fixpoint3 & 8) == 8)) {            //8.8=8.8+8.8
                fprintf(outputFile, "	LDA ");
                printfrac(statement[4]);
                fprintf(outputFile, "	CLC \n");
                fprintf(outputFile, "	ADC ");
                printfrac(statement[6]);
                fprintf(outputFile, "	STA ");
                printfrac(statement[2]);
                fprintf(outputFile, "	LDA ");
                printimmed(statement[4]);
                fprintf(outputFile, "%s\n", statement[4]);
                fprintf(outputFile, "	ADC ");
                printimmed(statement[6]);
                fprintf(outputFile, "%s\n", statement[6]);
            } else if ((fixpoint1 == 8) && ((fixpoint2 & 8) == 8) && (fixpoint3 == 4)) {
                fprintf(outputFile, "	LDY %s\n", statement[6]);
                fprintf(outputFile, "	LDX ");
                printfrac(statement[4]);
                fprintf(outputFile, "	LDA ");
                printimmed(statement[4]);
                fprintf(outputFile, "%s\n", statement[4]);
                jsrbank1("Add44to88");
                fprintf(outputFile, "	STX ");
                printfrac(statement[2]);
            } else if ((fixpoint1 == 8) && ((fixpoint3 & 8) == 8) && (fixpoint2 == 4)) {
                fprintf(outputFile, "	LDY %s\n", statement[4]);
                fprintf(outputFile, "	LDX ");
                printfrac(statement[6]);
                fprintf(outputFile, "	LDA ");
                printimmed(statement[6]);
                fprintf(outputFile, "%s\n", statement[6]);
                jsrbank1("Add44to88");
                fprintf(outputFile, "	STX ");
                printfrac(statement[2]);
            } else if ((fixpoint1 == 4) && (fixpoint2 == 8) && ((fixpoint3 & 4) == 4)) {
                if (fixpoint3 == 4)
                    fprintf(outputFile, "	LDY %s\n", statement[6]);
                else
                    fprintf(outputFile, "	LDY #%d\n", (int) (atof(statement[6]) * 16.0));
                fprintf(outputFile, "	LDA %s\n", statement[4]);
                fprintf(outputFile, "	LDX ");
                printfrac(statement[4]);
                jsrbank1("Add88to44");
            } else if ((fixpoint1 == 4) && (fixpoint2 == 4) && (fixpoint3 == 12)) {
                fprintf(outputFile, "	CLC\n");
                fprintf(outputFile, "	LDA %s\n", statement[4]);
                fprintf(outputFile, "	ADC #%d\n", (int) (atof(statement[6]) * 16.0));
            } else if ((fixpoint1 == 4) && (fixpoint2 == 12) && (fixpoint3 == 4)) {
                fprintf(outputFile, "	CLC\n");
                fprintf(outputFile, "	LDA %s\n", statement[6]);
                fprintf(outputFile, "	ADC #%d\n", (int) (atof(statement[4]) * 16.0));
            } else        // this needs work - 44+8+44 and probably others are screwy
            {
                if (fixpoint2 == 4)
                    fprintf(outputFile, "	LDA %s\n", statement[4]);
                if ((fixpoint3 == 4) && (fixpoint2 == 0)) {
                    fprintf(outputFile, "	LDA ");    // this LDA might be superfluous
                    printimmed(statement[4]);
                    fprintf(outputFile, "%s\n", statement[4]);
                }
                displayoperation("+CLC\n	ADC", statement[6], index & 4);
            }
//    }
        } else if (statement[5][0] == '-') {
            if ((fixpoint1 == 8) && ((fixpoint2 & 8) == 8) && ((fixpoint3 & 8) == 8)) {            //8.8=8.8-8.8
                fprintf(outputFile, "	LDA ");
                printfrac(statement[4]);
                fprintf(outputFile, "	SEC \n");
                fprintf(outputFile, "	SBC ");
                printfrac(statement[6]);
                fprintf(outputFile, "	STA ");
                printfrac(statement[2]);
                fprintf(outputFile, "	LDA ");
                printimmed(statement[4]);
                fprintf(outputFile, "%s\n", statement[4]);
                fprintf(outputFile, "	SBC ");
                printimmed(statement[6]);
                fprintf(outputFile, "%s\n", statement[6]);
            } else if ((fixpoint1 == 8) && ((fixpoint2 & 8) == 8) && (fixpoint3 == 4)) {
                fprintf(outputFile, "	LDY %s\n", statement[6]);
                fprintf(outputFile, "	LDX ");
                printfrac(statement[4]);
                fprintf(outputFile, "	LDA ");
                printimmed(statement[4]);
                fprintf(outputFile, "%s\n", statement[4]);
                jsrbank1("Sub44from88");
                fprintf(outputFile, "	STX ");
                printfrac(statement[2]);
            } else if ((fixpoint1 == 4) && (fixpoint2 == 8) && ((fixpoint3 & 4) == 4)) {
                if (fixpoint3 == 4)
                    fprintf(outputFile, "	LDY %s\n", statement[6]);
                else
                    fprintf(outputFile, "	LDY #%d\n", (int) (atof(statement[6]) * 16.0));
                fprintf(outputFile, "	LDA %s\n", statement[4]);
                fprintf(outputFile, "	LDX ");
                printfrac(statement[4]);
                jsrbank1("Sub88from44");
            } else if ((fixpoint1 == 4) && (fixpoint2 == 4) && (fixpoint3 == 12)) {
                fprintf(outputFile, "	SEC\n");
                fprintf(outputFile, "	LDA %s\n", statement[4]);
                fprintf(outputFile, "	SBC #%d\n", (int) (atof(statement[6]) * 16.0));
            } else if ((fixpoint1 == 4) && (fixpoint2 == 12) && (fixpoint3 == 4)) {
                fprintf(outputFile, "	SEC\n");
                fprintf(outputFile, "	LDA #%d\n", (int) (atof(statement[4]) * 16.0));
                fprintf(outputFile, "	SBC %s\n", statement[6]);
            } else {
                if (fixpoint2 == 4)
                    fprintf(outputFile, "	LDA %s\n", statement[4]);
                if ((fixpoint3 == 4) && (fixpoint2 == 0))
                    fprintf(outputFile, "	LDA #%s\n", statement[4]);
                displayoperation("-SEC\n	SBC", statement[6], index & 4);
            }
        } else if (statement[5][0] == '&') {
            if (fixpoint2 == 4)
                fprintf(outputFile, "	LDA %s\n", statement[4]);
            displayoperation("&AND", statement[6], index & 4);
        } else if (statement[5][0] == '^') {
            if (fixpoint2 == 4)
                fprintf(outputFile, "	LDA %s\n", statement[4]);
            displayoperation("^EOR", statement[6], index & 4);
        } else if (statement[5][0] == '|') {
            if (fixpoint2 == 4)
                fprintf(outputFile, "	LDA %s\n", statement[4]);
            displayoperation("|ORA", statement[6], index & 4);
        } else if (statement[5][0] == '*') {
            if (isimmed(statement[4]) && !isimmed(statement[6]) && checkmul(atoi(statement[4]))) {
                // swap operands to avoid mul routine
                strcpy(operandcopy, statement[4]);    // place here temporarily
                strcpy(statement[4], statement[6]);
                strcpy(statement[6], operandcopy);
            }
            if (fixpoint2 == 4)
                fprintf(outputFile, "	LDA %s\n", statement[4]);
            if ((!isimmed(statement[6])) || (!checkmul(atoi(statement[6])))) {
                displayoperation("*LDY", statement[6], index & 4);
                if (statement[5][1] == '*')
                    jsrbank1("mul16");    // general mul routine
                else
                    jsrbank1("mul8");
            } else if (statement[5][1] == '*')
                mul(statement, 16);
            else
                mul(statement, 8);    // attempt to optimize - may need to call mul anyway

        } else if (statement[5][0] == '/') {
            if (fixpoint2 == 4)
                fprintf(outputFile, "	LDA %s\n", statement[4]);
            if ((!isimmed(statement[6])) || (!checkdiv(atoi(statement[6])))) {
                displayoperation("/LDY", statement[6], index & 4);
                if (statement[5][1] == '/')
                    jsrbank1("div16");    // general div routine
                else
                    jsrbank1("div8");
            } else if (statement[5][1] == '/')
                divd(statement, 16);
            else
                divd(statement, 8);    // attempt to optimize - may need to call divd anyway

        } else if (statement[5][0] == ':') {
            strcpy(Areg, Aregcopy);    // A reg is not invalid
        } else if (statement[5][0] == '(') {
            // we've called a function, hopefully
            strcpy(Areg, "invalid");
            if (!strncmp(statement[4], "sread\0", 5))
                sread(statement);
            else
                callfunction(statement);
        } else if (statement[4][0] != '-')    // if not unary -
        {
            fprintf(stderr, "(%d) Error: invalid operator: %s\n", line, statement[5]);
            exit(1);
        }

    } else            // simple assignment
    {
        // check for fixed point stuff here
        // bugfix: forgot the LDA (?) did I do this correctly???
        if ((fixpoint1 == 4) && (fixpoint2 == 0)) {
            fprintf(outputFile, "	LDA ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            fprintf(outputFile, "  ASL\n");
            fprintf(outputFile, "  ASL\n");
            fprintf(outputFile, "  ASL\n");
            fprintf(outputFile, "  ASL\n");
        } else if ((fixpoint1 == 0) && (fixpoint2 == 4)) {
            fprintf(outputFile, "	LDA ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            fprintf(outputFile, "  LSR\n");
            fprintf(outputFile, "  LSR\n");
            fprintf(outputFile, "  LSR\n");
            fprintf(outputFile, "  LSR\n");
        } else if ((fixpoint1 == 4) && (fixpoint2 == 8)) {
            fprintf(outputFile, "	LDA ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            fprintf(outputFile, "  LDX ");
            printfrac(statement[4]);
// note: this shouldn't be changed to jsr(); (why???)
            fprintf(outputFile, " jsr Assign88to44");
            if (bs)
                fprintf(outputFile, "bs");
            fprintf(outputFile, "\n");
        } else if ((fixpoint1 == 8) && (fixpoint2 == 4)) {
// note: this shouldn't be changed to jsr();
            fprintf(outputFile, "	LDA ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
            fprintf(outputFile, "  JSR Assign44to88");
            if (bs)
                fprintf(outputFile, "bs");
            fprintf(outputFile, "\n");
            fprintf(outputFile, "  STX ");
            printfrac(statement[2]);
        } else if ((fixpoint1 == 8) && ((fixpoint2 & 8) == 8)) {
            fprintf(outputFile, "	LDX ");
            printfrac(statement[4]);
            fprintf(outputFile, "	STX ");
            printfrac(statement[2]);
            fprintf(outputFile, "	LDA ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
        } else if ((fixpoint1 == 4) && ((fixpoint2 & 4) == 4)) {
            if (fixpoint2 == 4)
                fprintf(outputFile, "	LDA %s\n", statement[4]);
            else
                fprintf(outputFile, "	LDA #%d\n", (int) (atof(statement[4]) * 16));
        } else if ((fixpoint1 == 8) && (fixpoint2 == 0)) {            // should handle 8.8=number w/o point or int var
            fprintf(outputFile, "	LDA #0\n");
            fprintf(outputFile, "	STA ");
            printfrac(statement[2]);
            fprintf(outputFile, "	LDA ");
            printimmed(statement[4]);
            fprintf(outputFile, "%s\n", statement[4]);
        }
    }
    if (index & 1)
        loadindex(&getindex0[0]);
    if (strncmp(statement[2], "Areg\0", 4)) {
        fprintf(outputFile, "	STA ");
        printindex(statement[2], index & 1);
    }
    free(deallocstatement);
}

void dogoto(char **statement) {
    int anotherbank = 0;
    //fprintf(stderr, "goto: %s %s %s\n", statement[1], statement[2], statement[3]);
    if (!strncmp(statement[3], "bank", 4)) {
        anotherbank = (int) (statement[3][4]) - '0';
        if ((statement[3][5] >= '0') && (statement[3][5] <= '9'))
            anotherbank = (int) (statement[3][5]) - 38;
    } else {
        fprintf(outputFile, " jmp .%s\n", statement[2]);
        return;
    }

// if here, we're jmp'ing to another bank
// we need to switch banks
    fprintf(outputFile, " sta temp7\n");
// next we must push the place to jmp to
    fprintf(outputFile, " lda #>(.%s-1)\n", statement[2]);
    fprintf(outputFile, " pha\n");
    fprintf(outputFile, " lda #<(.%s-1)\n", statement[2]);
    fprintf(outputFile, " pha\n");
// now store regs on stack
    fprintf(outputFile, " lda temp7\n");
    fprintf(outputFile, " pha\n");
    fprintf(outputFile, " txa\n");
    fprintf(outputFile, " pha\n");
// select bank to switch to
    fprintf(outputFile, " ldx #%d\n", anotherbank);
    fprintf(outputFile, " jmp BS_jsr\n");    // also works for jmps
}

void gosub(char **statement) {
    int anotherbank = 0;
    invalidate_Areg();
    //if (numgosubs++>3) {
    // fprintf(stderr,"Max. nested gosubs exceeded in line %s\n",statement[0]);
    // exit(1);
    //}
    if (!strncmp(statement[3], "bank", 4)) {
        anotherbank = (int) (statement[3][4]) - '0';
        if ((statement[3][5] >= '0') && (statement[3][5] <= '9'))
            anotherbank = (int) (statement[3][5]) - 38;
    } else {
        fprintf(outputFile, " jsr .%s\n", statement[2]);
        return;
    }

// if here, we're jsr'ing to another bank
// we need to switch banks
    fprintf(outputFile, " sta temp7\n");
// first create virtual return address

    // if it's 64k banks, store the bank directly in the high nibble
    if (bs == 64)
        fprintf(outputFile, " lda #(((>(ret_point%d-1)) & $0F) | $%1x0) \n", ++numjsrs, bank - 1);
    else
        fprintf(outputFile, " lda #>(ret_point%d-1)\n", ++numjsrs);

    fprintf(outputFile, " pha\n");

    fprintf(outputFile, " lda #<(ret_point%d-1)\n", numjsrs);
    fprintf(outputFile, " pha\n");

// next we must push the place to jsr to
    fprintf(outputFile, " lda #>(.%s-1)\n", statement[2]);
    fprintf(outputFile, " pha\n");

    fprintf(outputFile, " lda #<(.%s-1)\n", statement[2]);
    fprintf(outputFile, " pha\n");

// now store regs on stack
    fprintf(outputFile, " lda temp7\n");
    fprintf(outputFile, " pha\n");
    fprintf(outputFile, " txa\n");
    fprintf(outputFile, " pha\n");

// select bank to switch to
    fprintf(outputFile, " ldx #%d\n", anotherbank);
    fprintf(outputFile, " jmp BS_jsr\n");
    fprintf(outputFile, "ret_point%d\n", numjsrs);
}


void set(char **statement) {
    int i;
    int v;
    int valid_kernel_combos[] = {    // C preprocessor should turn these into numbers!
            _player1colors,
            _no_blank_lines,
            _pfcolors,
            _pfheights,
            _pfcolors | _pfheights,
            _pfcolors | _pfheights | _background,
            _pfcolors | _no_blank_lines,
            _pfcolors | _no_blank_lines | _background,
            _player1colors | _no_blank_lines,
            _player1colors | _pfcolors,
            _player1colors | _pfheights,
            _player1colors | _pfcolors | _pfheights,
            _player1colors | _pfcolors | _background,
            _player1colors | _pfheights | _background,
            _player1colors | _pfcolors | _pfheights | _background,
            _player1colors | _no_blank_lines | _readpaddle,
            _player1colors | _no_blank_lines | _pfcolors,
            _player1colors | _no_blank_lines | _pfcolors | _background,
            _playercolors | _player1colors | _pfcolors,
            _playercolors | _player1colors | _pfheights,
            _playercolors | _player1colors | _pfcolors | _pfheights,
            _playercolors | _player1colors | _pfcolors | _background,
            _playercolors | _player1colors | _pfheights | _background,
            _playercolors | _player1colors | _pfcolors | _pfheights | _background,
            _no_blank_lines | _readpaddle,
            255
    };

    char *optionName = statement[2];
    char *optionValue = statement[3];

    if (!strncasecmp(optionName, "tv\0", 2)) {
        if (!strncasecmp(optionValue, "ntsc\0", 4)) {
            // pick constant timer values for now, later maybe add more lines
            strcpy(redefined_variables[numredefvars++], "overscan_time = 37");
            strcpy(redefined_variables[numredefvars++], "vblank_time = 43");
        } else if (!strncasecmp(optionValue, "pal\0", 3)) {
            // 36 and 48 scanlines, respectively
            strcpy(redefined_variables[numredefvars++], "overscan_time = 82");
            strcpy(redefined_variables[numredefvars++], "vblank_time = 58");
        } else
            prerror("set TV: invalid TV type\n");
    } else if (!strncmp(optionName, "smartbranching\0", 14)) {
        if (!strncmp(optionValue, "on\0", 2))
            smartbranching = 1;
        else
            smartbranching = 0;
    } else if (!strncmp(optionName, "dpcspritemax\0", 12)) {
        v = atoi(optionValue);
        if ((v == 0) || (v > 9)) {
            prerror("set dpcspritemax: invalid value\n");
            exit(1);
        }
        sprintf(redefined_variables[numredefvars++], "dpcspritemax = %d", v);
    } else if (!strncmp(optionName, "romsize\0", 7)) {
        set_romsize(optionValue);
    } else if (!strncmp(optionName, "optimization\0", 5)) {
        if (!strncmp(optionValue, "speed\0", 5)) {
            optimization |= 1;
        }
        if (!strncmp(optionValue, "size\0", 4)) {
            optimization |= 2;
            set_kernel_size_optimization(true);
        }
        if (!strncmp(optionValue, "noinlinedata\0", 4)) {
            optimization |= 4;
        }
        if (!strncmp(optionValue, "inlinerand\0", 4)) {
            optimization |= 8;
        }
        if (!strncmp(optionValue, "none\0", 4)) {
            optimization = 0;
        }
    } else if (!strncmp(optionName, "kernal\0", 6)) {
        prerror
                ("The proper spelling is \"kernel.\"  With an e.  Please make a note of this to save yourself from further embarrassment.\n");
    } else if (!strncmp(optionName, "kernel_options\0", 10)) {
        i = 3;
        kernel_options = 0;
        while (((unsigned char) statement[i][0] > (unsigned char) 64)
               && ((unsigned char) statement[i][0] < (unsigned char) 123)) {
            if (!strncmp(statement[i], "readpaddle\0", 10)) {
                strcpy(redefined_variables[numredefvars++], "readpaddle = 1");
                if (bs == 28) {
                    fprintf(outputFile, "DPC_kernel_options = INPT0+$40\n");
                    return;
                } else
                    kernel_options |= 1;
            } else if (!strncmp(statement[i], "collision\0", 9)) {
                if (bs == 28) {
                    fprintf(outputFile, "DPC_kernel_options = ");
                    if (check_collisions(&statement[i]) == 7)
                        fprintf(outputFile, "+$40\n");
                    else
                        fprintf(outputFile, "\n");
                    return;
                }
            } else if (!strncmp(statement[i], "player1colors\0", 13)) {
                strcpy(redefined_variables[numredefvars++], "player1colors = 1");
                kernel_options |= 2;
            } else if (!strncmp(statement[i], "playercolors\0", 12)) {
                strcpy(redefined_variables[numredefvars++], "playercolors = 1");
                strcpy(redefined_variables[numredefvars++], "player1colors = 1");
                kernel_options |= 6;
            } else if (!strncmp(statement[i], "no_blank_lines\0", 13)) {
                strcpy(redefined_variables[numredefvars++], "no_blank_lines = 1");
                kernel_options |= 8;
            } else if (!strncasecmp(statement[i], "pfcolors\0", 8)) {
                kernel_options |= 16;
            } else if (!strncasecmp(statement[i], "pfheights\0", 9)) {
                kernel_options |= 32;
            } else if (!strncasecmp(statement[i], "backgroundchange\0", 10)) {
                strcpy(redefined_variables[numredefvars++], "backgroundchange = 1");
                kernel_options |= 64;
            } else {
                prerror("set kernel_options: Options unknown or invalid\n");
                exit(1);
            }
            i++;
        }
        if ((kernel_options & 48) == 32)
            strcpy(redefined_variables[numredefvars++], "PFheights = 1");
        else if ((kernel_options & 48) == 16)
            strcpy(redefined_variables[numredefvars++], "PFcolors = 1");
        else if ((kernel_options & 48) == 48)
            strcpy(redefined_variables[numredefvars++], "PFcolorandheight = 1");
//fprintf(stderr,"%d\n",kernel_options);
        // check for valid combinations
        if (kernel_options == 1) {
            prerror("set kernel_options: readpaddle must be used with no_blank_lines\n");
            exit(1);
        }
        i = 0;
        while (1) {
            if (valid_kernel_combos[i] == 255) {
                prerror("set kernel_options: Invalid combination of options\n");
                exit(1);
            }
            if (kernel_options == valid_kernel_combos[i++])
                break;
        }

        set_kernel_options(kernel_options);

    } else if (!strncmp(optionName, "kernel\0", 6)) {

        set_kernel_type(optionValue);
    } else if (!strncmp(optionName, "debug\0", 5)) {
        if (!strncmp(optionValue, "cyclescore\0", 10)) {
            strcpy(redefined_variables[numredefvars++], "debugscore = 1");
        } else if (!strncmp(optionValue, "cycles\0", 6)) {
            strcpy(redefined_variables[numredefvars++], "debugcycles = 1");
        } else
            prerror("set debug: debugging mode unknown\n");
    } else if (!strncmp(optionName, "legacy\0", 6)) {
        sprintf(redefined_variables[numredefvars++], "legacy = %d", (int) (100 * (atof(optionValue))));
    } else
        prerror("set: unknown parameter\n");

}

void rem(char **statement) {
    if (!strncmp(statement[2], "smartbranching\0", 14)) {
        if (!strncmp(statement[3], "on\0", 2))
            smartbranching = 1;
        else
            smartbranching = 0;
    }
}

void dopop() {
    fprintf(outputFile, "	pla\n");
    fprintf(outputFile, "	pla\n");
}

void add_redefined_variable(char *varName, char *varValue) {
    char var_def[200];
    sprintf(var_def, "%s = %s", varName, varValue);
    strcpy(redefined_variables[numredefvars++], var_def);
}


void output_redefvars_file(char *filename) {
    FILE *header;
    int i;
    if ((header = fopen(filename, "w")) == NULL)    // open file
    {
        fprintf(stderr, "Cannot open %s for writing\n", filename);
        exit(1);
    }

    strcpy(redefined_variables[numredefvars],
           "; This file contains variable mapping and other information for the current project.\n");

    for (i = numredefvars; i >= 0; i--) {
        fprintf(header, "%s\n", redefined_variables[i]);
    }
    fclose(header);

}
