//
// Created by Admin on 7/30/2023.
//

#include "linker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// This reads the includes file created by bB and builds the
// final assembly that will be sent to DASM.
//
// This task used to be done with a batch file/shell script when
// the files in the final asm were static.
// Now, since the files in the final asm can be different, and it doesn't
// make sense to require the user to create a new batch file/shell script.

void link_files(char *path) {
    FILE *includesfile;
    FILE *asmfile;
    FILE *bbfile;

    char prepend[500];
    char line[500];
    char asmline[500];

    bool readNextFile;

    if ((includesfile = fopen("includes.bB", "r")) == NULL)    // open file
    {
        fprintf(stderr, "Cannot open includes.bB for reading\n");
        exit(1);
    }

    if ((bbfile = fopen("bB.asm", "r")) == NULL) {
        fprintf(stderr, "Error reading bB file!");
        exit(1);
    }

    // read line of include file (essentially the build / make file)
    while (fgets(line, 500, includesfile)) {

        // trim off eol character.
        int line_len = strlen(line);
        if (line[line_len-1] == '\n') line[line_len-1] = 0;

        // check if including a portion of the bB.asm code.
        bool isBbFile = (!strncmp(line, "bB", 2)) && (line[2] == '.' || line[3] == '.');

        if (isBbFile) {
            readNextFile = false;

            // read bb assembly source file until EOF or split point
            while ((fgets(asmline, 500, bbfile) != NULL) && !readNextFile) {

                // check for last bank split point within the bB source file
                if (!strncmp(asmline, "; bB.asm file is split here", 20)) {
                    readNextFile = true;
                } else {
                    printf("%s", asmline);
                }
            }

        } else {
            // reading from another file
            if ((asmfile = fopen(line, "r")) == NULL)    // try file w/o includes path
            {
                strcpy(prepend, path);
                strcat(prepend, line);
                if ((asmfile = fopen(prepend, "r")) == NULL)    // open file
                {
                    fprintf(stderr, "Cannot open %s for reading: %s\n", line, prepend);
                    exit(1);
                }
            } else if (strncmp(line, "bB\0", 2) != 0) {
                fprintf(stderr, "User-defined %s found in current directory\n", line);
            }

            // read from an included file
            while (fgets(asmline, 500, asmfile) != NULL) {
                printf("%s", asmline);
            }
            fclose(asmfile);
        }

    }
    if (bbfile) fclose(bbfile);
}