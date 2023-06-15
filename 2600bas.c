// Provided under the GPL v2 license. See the included LICENSE.txt for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "statements.h"
#include "keywords.h"
#include "lexer.h"
#include <math.h>
#include <stdbool.h>

#define BB_VERSION_INFO "batari Basic v1.8 (c)2023\n"



int main(int argc, char *argv[]) {
    char **statement;
    int i, j;
    int cntUnnamedLabel = 0;

    bool failedToRead;
    char code[500];
    char displaycode[500];
    char *includes_file = "default.inc";
    char *redefVars_filename = "2600basic_variable_redefs.h";
    char *path = 0;
    char def[500][100];
    char defr[500][100];
    char finalcode[500];
    char *codeadd;
    char mycode[500];
    int defi = 0;

    // get command line arguments
    while ((i = getopt(argc, argv, "i:r:v")) != -1) {
        switch (i) {
            case 'i':
                path = strdup(optarg);
                break;
            case 'r':
                //redefVars_filename = (char *) malloc(100);
                //strcpy(redefVars_filename, optarg);
                redefVars_filename = strdup(optarg);
                break;
            case 'v':
                printf("%s", BB_VERSION_INFO);
                exit(0);
            case '?':
                fprintf(stderr, "usage: %s -r <variable redefs file> -i <includes path>\n", argv[0]);
                exit(1);
        }
    }
    init_statement_processor();

    fprintf(stderr, BB_VERSION_INFO);

    // provide statement processor with input stream
    char *mySourceFileName = "1942_HSC.bas";
    FILE *mySourceFile = fopen(mySourceFileName, "r");
    use_source_file(mySourceFile);

    FILE *outFile = fopen("bB.asm", "w");
    use_output_file(outFile);
    printf("game\n");        // label for start of game

    init_includes(path);

    //----  allocated memory for statement token list
    statement = (char **) malloc(sizeof(char *) * 200);
    for (i = 0; i < 200; ++i) {
        statement[i] = (char *) malloc(sizeof(char) * 200);
    }

    while (1) {
        // clear out statement cache
        for (i = 0; i < 200; ++i) {
            for (j = 0; j < 200; ++j) {
                statement[i][j] = '\0';
            }
        }

        // get next line from input
        failedToRead = read_source_line(code);
        incline();
        strcpy(displaycode, code);

        // look for defines and remember them
        strcpy(mycode, code);
        for (i = 0; i < 500; ++i)
            if (code[i] == ' ')
                break;
        if (code[i + 1] == 'd' && code[i + 2] == 'e' && code[i + 3] == 'f' &&
            code[i + 4] == ' ') {            // found a define
            i += 5;
            for (j = 0; code[i] != ' '; i++) {
                def[defi][j++] = code[i];    // get the define
            }
            def[defi][j] = '\0';

            i += 3;

            for (j = 0; code[i] != '\0'; i++) {
                defr[defi][j++] = code[i];    // get the definition
            }
            defr[defi][j] = '\0';
            removeCR(defr[defi]);
            printf(";.%s.%s.\n", def[defi], defr[defi]);
            defi++;
        } else if (defi) {
            for (i = 0; i < defi; ++i) {
                codeadd = NULL;
                finalcode[0] = '\0';
                int defcount = 0;
                while (1) {
                    if (defcount++ > 500) {
                        fprintf(stderr, "(%d) Infinitely repeating definition or too many instances of a definition\n",
                                bbgetlinenumber());
                        exit(1);
                    }
                    codeadd = strstr(mycode, def[i]);
                    if (codeadd == NULL)
                        break;
                    for (j = 0; j < 500; ++j)
                        finalcode[j] = '\0';
                    strncpy(finalcode, mycode, strlen(mycode) - strlen(codeadd));
                    strcat(finalcode, defr[i]);
                    strcat(finalcode, codeadd + strlen(def[i]));
                    strcpy(mycode, finalcode);
                }
            }
        }
        if (strcmp(mycode, code)!=0)
            strcpy(code, mycode);

        // check for end of file
        if (failedToRead)
            break;        //end of file

        // tokenize the statement
        tokenize(statement, code, bbgetlinenumber());

        // check if label is necessary
        if (statement[0][0] == '\0') {
            sprintf(statement[0], "L0%d", cntUnnamedLabel++);
        } else {
            if (strchr(statement[0], '.') != NULL) {
                fprintf(stderr, "(%d) Invalid character in label.\n", bbgetlinenumber());
                exit(1);
            }

        }

        if (strncmp(statement[0], "end\0", 3)!=0)
            printf(".%s ; %s\n", statement[0], displaycode);    //    printf(".%s ; %s\n",statement[0],code);
        else
            doend();

        // process the statement
        keywords(statement);

        if (numconstants == (MAXCONSTANTS - 1)) {
            fprintf(stderr, "(%d) Maximum number of constants exceeded.\n", bbgetlinenumber());
            exit(1);
        }
    }
    barf_sprite_data();

    write_footer();
    output_redefvars_file(redefVars_filename);
    create_includes(includes_file);
    fprintf(stderr, "2600 Basic compilation complete.\n");
    return 0;
}
