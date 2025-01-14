// Provided under the GPL v2 license. See the included LICENSE.txt for details.

#include "keywords.h"
#include "statements.h"
#include "lib_gfx.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int numelses;
int numthens;
int ors;


int swaptest(char *value)    // check for then, && or ||
{
    if (!strncmp(value, "then\0", 4) || !strncmp(value, "&&\0", 2) || !strncmp(value, "||\0", 2))
        return 1;
    return 0;
}

void keywords(char **cstatement) {
    char errorcode[100];
    char **statement;
    int i, j, k;
    int colons = 0;
    int currentcolon = 0;

    char **elstatement;
    char **orstatement;
    char **swapstatement;
    int door;
    int foundelse = 0;
    statement = (char **) malloc(sizeof(char *) * 200);
    orstatement = (char **) malloc(sizeof(char *) * 200);
    for (i = 0; i < 200; ++i) {
        orstatement[i] = (char *) malloc(sizeof(char) * 200);
    }
    elstatement = (char **) malloc(sizeof(char *) * 200);
    for (i = 0; i < 200; ++i) {
        elstatement[i] = (char *) malloc(sizeof(char) * 200);
    }

    // check if there are boolean && or || in an if-then.
    // change && to "then if"
    // change || to two if thens
    // also change operands around to allow <= and >, since 
    // currently all we can do is < and >=
    // if we encounter an else, break into two lines, and the first line jumps ahead.
    door = 0;
    for (k = 0; k <= 190; ++k)    // reversed loop since last build, need to check for rems first!
    {
        if (!strncmp(cstatement[k + 1], "rem\0", 3))
            break;        // if statement has a rem, do not process it
        if (!strncmp(cstatement[k + 1], "if\0", 2)) {
            for (i = k + 2; i < 200; ++i) {
                if (!strncmp(cstatement[i], "if\0", 2))
                    break;
                if (!strncmp(cstatement[i], "else\0", 4))
                    foundelse = i;
            }

            // Switch items compared with ('<=', '>') around so that they
            //    use operators ('>=','<') which are supported directly by the CPU
            if (!strncmp(cstatement[k + 3], ">\0", 2) && (!strncmp(cstatement[k + 1], "if\0", 2))
                && (swaptest(cstatement[k + 5]))) {
                // swap operands and switch compare
                strcpy(cstatement[k + 3], cstatement[k + 2]);    // stick 1st operand here temporarily
                strcpy(cstatement[k + 2], cstatement[k + 4]);
                strcpy(cstatement[k + 4], cstatement[k + 3]);    // get it back
                strcpy(cstatement[k + 3], "<");    // replace compare
            } else if (!strncmp(cstatement[k + 3], "<=\0", 2) && (!strncmp(cstatement[k + 1], "if\0", 2))
                       && (swaptest(cstatement[k + 5]))) {
                // swap operands and switch compare
                strcpy(cstatement[k + 3], cstatement[k + 2]);
                strcpy(cstatement[k + 2], cstatement[k + 4]);
                strcpy(cstatement[k + 4], cstatement[k + 3]);
                strcpy(cstatement[k + 3], ">=");
            }

            // Handle AND between two comparisons
            if (!strncmp(cstatement[k + 3], "&&\0", 2)) {
                shiftdata(cstatement, k + 3);
                sprintf(cstatement[k + 3], "then%d", ors++);
                strcpy(cstatement[k + 4], "if");
            } else if (!strncmp(cstatement[k + 5], "&&\0", 2)) {
                shiftdata(cstatement, k + 5);
                sprintf(cstatement[k + 5], "then%d", ors++);
                strcpy(cstatement[k + 6], "if");

            } else if (!strncmp(cstatement[k + 7], "&&\0", 2)) {        // Handle && after collision check
                shiftdata(cstatement, k + 7);
                sprintf(cstatement[k + 7], "then%d", ors++);
                strcpy(cstatement[k + 8], "if");
            } else if (!strncmp(cstatement[k + 8], "&&\0", 2)) {        // Handle && after collision check
                shiftdata(cstatement, k + 8);
                sprintf(cstatement[k + 8], "then%d", ors++);
                strcpy(cstatement[k + 9], "if");

            } else if (!strncmp(cstatement[k + 3], "||\0", 2)) {
            // Handle OR between two comparisons

                if (!strncmp(cstatement[k + 5], ">\0", 2) && (!strncmp(cstatement[k + 1], "if\0", 2))
                    && (swaptest(cstatement[k + 7]))) {
                    // swap operands and switch compare
                    strcpy(cstatement[k + 5], cstatement[k + 4]);    // stick 1st operand here temporarily
                    strcpy(cstatement[k + 4], cstatement[k + 6]);
                    strcpy(cstatement[k + 6], cstatement[k + 5]);    // get it back
                    strcpy(cstatement[k + 5], "<");    // replace compare
                } else if (!strncmp(cstatement[k + 5], "<=\0", 2) && (!strncmp(cstatement[k + 1], "if\0", 2))
                           && (swaptest(cstatement[k + 7]))) {
                    // swap operands and switch compare
                    strcpy(cstatement[k + 5], cstatement[k + 4]);
                    strcpy(cstatement[k + 4], cstatement[k + 6]);
                    strcpy(cstatement[k + 6], cstatement[k + 5]);
                    strcpy(cstatement[k + 5], ">=");
                }

                for (i = 2; i < 198 - k; ++i)
                    strcpy(orstatement[i], cstatement[k + i + 2]);
                if (!strncmp(cstatement[k + 5], "then\0", 4))
                    compressdata(cstatement, k + 3, k + 2);
                else if (!strncmp(cstatement[k + 7], "then\0", 4))
                    compressdata(cstatement, k + 3, k + 4);
                strcpy(cstatement[k + 3], "then");
                sprintf(orstatement[0], "%dOR", ors++);
                strcpy(orstatement[1], "if");
                door = 1;
// todo: need to skip over the next statement!

            } else if (!strncmp(cstatement[k + 5], "||\0", 2)) {
                if (!strncmp(cstatement[k + 7], ">\0", 2) && (!strncmp(cstatement[k + 1], "if\0", 2))
                    && (swaptest(cstatement[k + 9]))) {
                    // swap operands and switch compare
                    strcpy(cstatement[k + 7], cstatement[k + 6]);    // stick 1st operand here temporarily
                    strcpy(cstatement[k + 6], cstatement[k + 8]);
                    strcpy(cstatement[k + 8], cstatement[k + 7]);    // get it back
                    strcpy(cstatement[k + 7], "<");    // replace compare
                } else if (!strncmp(cstatement[k + 7], "<=\0", 2) && (!strncmp(cstatement[k + 1], "if\0", 2))
                           && (swaptest(cstatement[k + 9]))) {
                    // swap operands and switch compare
                    strcpy(cstatement[k + 7], cstatement[k + 6]);
                    strcpy(cstatement[k + 6], cstatement[k + 8]);
                    strcpy(cstatement[k + 8], cstatement[k + 7]);
                    strcpy(cstatement[k + 7], ">=");
                }
                for (i = 2; i < 196 - k; ++i)
                    strcpy(orstatement[i], cstatement[k + i + 4]);
                if (!strncmp(cstatement[k + 7], "then\0", 4))
                    compressdata(cstatement, k + 5, k + 2);
                else if (!strncmp(cstatement[k + 9], "then\0", 4))
                    compressdata(cstatement, k + 5, k + 4);
                strcpy(cstatement[k + 5], "then");
                sprintf(orstatement[0], "%dOR", ors++);
                strcpy(orstatement[1], "if");
                door = 1;
            }
        }
        if (door)
            break;
    }
    if (foundelse) {
        char **pass2elstatement;
        if (door)
            pass2elstatement = orstatement;
        else
            pass2elstatement = cstatement;

        for (i = 1; i < 200; ++i)
            if (!strncmp(pass2elstatement[i], "else\0", 4)) {
                foundelse = i;
                break;
            }

        for (i = foundelse; i < 200; ++i)
            strcpy(elstatement[i - foundelse], pass2elstatement[i]);
        if (islabelelse(pass2elstatement)) {
            strcpy(pass2elstatement[foundelse++], ":");
            strcpy(pass2elstatement[foundelse++], "goto");
            sprintf(pass2elstatement[foundelse++], "skipelse%d", numelses);
        }
        for (i = foundelse; i < 200; ++i)
            pass2elstatement[i][0] = '\0';
        if (!islabelelse(elstatement)) {
            strcpy(elstatement[2], elstatement[1]);
            strcpy(elstatement[1], "goto");
        }
        if (door) {
            for (i = 1; i < 200; ++i)
                if (!strncmp(cstatement[i], "else\0", 4))
                    break;
            for (k = i; k < 200; ++k)
                cstatement[k][0] = '\0';
        }

    }


    if (door) {
        swapstatement = orstatement;    // swap statements because of recursion
        orstatement = cstatement;
        cstatement = swapstatement;
        // this hacks off the conditional statement from the copy of the statement we just created
        // and replaces it with a goto.  This can be improved (i.e., there is no need to copy...)



        if (islabel(orstatement)) {
// make sure islabel function works right!


            // find end of statement
            i = 3;
//      while (orstatement[i++][0]){} // not sure if this will work...
            while (strncmp(orstatement[i++], "then\0", 4)) {
            }            // not sure if this will work...
            // add goto to it
            if (i > 190) {
                i = 190;
                fprintf(stderr, "%d: Cannot find end of line - statement may have been truncated\n", bbgetlinenumber());
            }
            //strcpy(orstatement[i++],":");
            strcpy(orstatement[i++], "goto");
            sprintf(orstatement[i++], "condpart%d",
                    getcondpart() + 1);    // goto unnamed line number for then statemtent
            for (; i < 200; ++i)
                orstatement[i][0] = '\0';    // clear out rest of statement

        }


        keywords(orstatement);    // recurse
    }
    if (foundelse) {
        swapstatement = elstatement;    // swap statements because of recursion
        elstatement = cstatement;
        cstatement = swapstatement;
        keywords(elstatement);    // recurse
    }

    //----------------------------------------------
    //--- load statements array

    for (i = 0; i < 200; i++) {
        statement[i] = cstatement[i];
        trim_string(statement[i], false);
    }


    for (i = 0; i < 200; ++i) {
        if (statement[i][0] == '\0')
            break;
        else if (statement[i][0] == ':')
            colons++;
    }


    if (!strncmp(statement[0], "then\0", 4))
        sprintf(statement[0], "%dthen", numthens++);

    invalidate_Areg();

    while (1) {
        char *command = statement[1];       // TODO: command might contain '\n' at the end... need to figure out why.

        if (command[0] == '\0') {
            return;
        } else if (command[0] == ' ') {
            return;
        } else if (!strncmp(command, "def\0", 4)) {
            return;
        } else if (!strncmp(command, "rem\0", 4)) {
            rem(statement);
            return;
        } else if (!strncmp(command, "if\0", 3)) {
            doif(statement);
            break;
        } else if (!strncmp(statement[0], "end\0", 4))
            endfunction();
        else {
            char *param = statement[2];
            if (!strncmp(command, "includesfile\0", 13))
                create_includes(param);
            else if (!strncmp(command, "include\0", 7))
                add_includes(param);
            else if (!strncmp(command, "inline\0", 7))
                add_inline(param);
            else if (!strncmp(command, "function\0", 9))
                function(statement);
            else if (!strncmp(command, "goto\0", 5))
                dogoto(statement);
            else if (!strncmp(command, "bank\0", 5))
                newbank(atoi(param));
            else if (!strncmp(command, "sdata\0", 6))
                sdata(statement);
            else if (!strncmp(command, "data\0", 5))
                data(statement);
            else if ((!strncmp(command, "on\0", 3)) && (!strncmp(statement[3], "go\0", 2)))
                ongoto(statement);    // on ... goto or on ... gosub
            else if (!strncmp(command, "const\0", 6))
                doconst(statement);
            else if (!strncmp(command, "dim\0", 4))
                dim(statement);
            else if (!strncmp(command, "for\0", 4))
                dofor(statement);
            else if (!strncmp(command, "next\0", 5))
                next(statement);
            else if (!strncmp(command, "gosub\0", 6))
                gosub(statement);
            else if (!strncmp(command, "asm\0", 4))
                doasm();
            else if (!strncmp(command, "pop\0", 4))
                dopop();
            else if (!strncmp(command, "set\0", 4))
                set(statement);
            else if ((!strncmp(command, "return\0", 7)) || (!strncmp(command, "return\n", 7)))
                doreturn(statement);
            else if (!strncmp(command, "reboot\0", 7))
                doreboot();
            else if (!strncmp(command, "vblank\0", 7))
                vblank();
            else if (!strncmp(param, "=\0", 1))
                dolet(statement);
            else if (!strncmp(command, "let\0", 4))
                dolet(statement);
            else if (!strncmp(command, "dec\0", 4))
                dec(statement);
            else if (!strncmp(command, "macro\0", 6))
                domacro(statement);
            else if (!strncmp(command, "push\0", 5))
                do_push(statement);
            else if (!strncmp(command, "pull\0", 5))
                do_pull(statement);
            else if (!strncmp(command, "stack\0", 6))
                do_stack(statement);
            else if (!strncmp(command, "callmacro\0", 10))
                callmacro(statement);
            else if (!strncmp(command, "extra\0", 5))
                doextra(command);

            // check for graphics command (and handle it), otherwise...
            else if (!handleGraphicsCommand(command, param, statement)) {

                //-----------------------------------------------------------------------
                // Not a graphics command

                //  sadly, a kludge for complex statements followed by "then label"
                int lastc = strlen(statement[0]) - 1;
                if ((lastc > 3) && (((statement[0][lastc - 4] >= '0') && (statement[0][lastc - 4] <= '9')) &&
                                    (statement[0][lastc - 3] == 't') &&
                                    (statement[0][lastc - 2] == 'h') &&
                                    (statement[0][lastc - 1] == 'e') && (statement[0][lastc - 0] == 'n')))
                    return;
                sprintf(errorcode, "Error: Unknown keyword: %s\n", command);
                prerror(&errorcode[0]);
                print_statement_breakdown(statement);
                exit(1);
            }

        }
        // see if there is a colon
        if ((!colons) || (currentcolon == colons))
            break;
        currentcolon++;

        i = 0;
        k = 0;
        while (i != currentcolon) {
            if (cstatement[k++][0] == ':')
                i++;
        }

        for (j = k; j < 200; ++j)
            statement[j - k + 1] = cstatement[j];
        for (; (j - k + 1) < 200; ++j)
            statement[j - k + 1][0] = '\0';

    }
    if (foundelse)
        fprintf(getOutputFile(), ".skipelse%d\n", numelses++);
}
