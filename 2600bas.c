// Provided under the GPL v2 license. See the included LICENSE.txt for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "statements.h"
#include "keywords.h"
#include "lexer.h"
#include "linker.h"
#include "lib_gfx.h"
#include <stdbool.h>

#define BB_VERSION_INFO "batari Basic v1.8 (c)2023\n"

// variables used to process "def" statements
char def[500][100];
char defr[500][100];
char finalcode[500];
char *codeadd;
char mycode[500];
int defi = 0;

// look for defines and remember them
void processDefs(char *code) {
    int i,j;
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
        fprintf(getOutputFile(), ";.%s.%s.\n", def[defi], defr[defi]);
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
}

void compile(FILE *outFile) {
    char **statement;
    int i, j;

    bool failedToRead;
    char code[500];
    char displaycode[500];

    use_output_file(outFile);

    // label for start of game
    fprintf(outFile, "game\n");

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

        // check for end of file
        if (failedToRead)
            break;        //end of file

        // process any "def" statements
        processDefs(code);

        // tokenize the statement
        tokenize(statement, code, bbgetlinenumber());

        // check if label is necessary and needs to be generated
        if (statement[0][0] == '\0' && statement[1][0] != '\0') {

            // Auto-generate a label based on the current line number-base label for any 'unlabeled' lines
            sprintf(statement[0], "L0%d", bbgetlinenumber());

        } else {

            // Batari-Basic does not allow '.' in labels
            if (strchr(statement[0], '.') != NULL) {
                fprintf(stderr, "(%d) Invalid character in label.\n", bbgetlinenumber());
                exit(1);
            }

        }

        if (strncmp(statement[0], "end\0", 3)!=0) {
            if (statement[0][0] != 0)
                fprintf(outFile, "\n.%s ; %s\n", statement[0], displaycode);
        } else {
            doend();
        }

        // process the statement
        keywords(statement);

        if (numconstants == (MAXCONSTANTS - 1)) {
            fprintf(stderr, "(%d) Maximum number of constants exceeded.\n", bbgetlinenumber());
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    int i;
    char *includes_file = "default.inc";
    char *redefVars_filename = "2600basic_variable_redefs.h";
    char *path = 0;
    char *outputFilename = 0;
    char *mySourceFileName = 0;

    // get command line arguments
    while ((i = getopt(argc, argv, "i:o:s:r:v")) != -1) {
        switch (i) {
            case 'i':
                path = strdup(optarg);
                break;
            case 'o':
                outputFilename = strdup(optarg);
                break;
            case 's':
                mySourceFileName = strdup(optarg);
                break;
            case 'r':
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

    char includesPath[500];
    strcpy(includesPath, path);
    char slashChar = includesPath[strlen(includesPath) - 1];
    if ((slashChar == '\\') || (slashChar == '/')) {
        strcat(includesPath, "includes/");
    } else {
        strcat(includesPath, "/includes/");
    }

    init_statement_processor();

    fprintf(stderr, BB_VERSION_INFO);

    // provide statement processor with input stream
    FILE *mySourceFile;
    if (mySourceFileName) {
        mySourceFile = fopen(mySourceFileName, "r");
    } else {
        mySourceFile = stdin;
    }
    use_source_file(mySourceFile);

    // create + open output file
    if (!outputFilename) outputFilename = "bB.asm";
    FILE *outFile = fopen(outputFilename, "w");

    //-------------------------
    // compile

    init_includes(path);
    compile(outFile);
    output_sprite_data();
    output_playfield_data();
    write_footer();
    fclose(outFile);

    output_redefvars_file(redefVars_filename);
    create_includes(includes_file);
    fprintf(stderr, "2600 Basic compilation complete.\n");

    link_files(includesPath);
    fprintf(stderr, "2600 Basic linking complete.\n");
    return 0;
}
