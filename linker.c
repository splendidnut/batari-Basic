//
// Created by Admin on 7/30/2023.
//

#include "linker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This reads the includes file created by bB and builds the
// final assembly that will be sent to DASM.
//
// This task used to be done with a batch file/shell script when
// the files in the final asm were static.
// Now, since the files in the final asm can be different, and it doesn't
// make sense to require the user to create a new batch file/shell script.

void link(char *path) {
    FILE *includesfile;
    FILE *asmfile;
    char ***readbBfile;
    char prepend[500];
    char line[500];
    char asmline[500];
    int bB = 2;            // part of bB file
    int writebBfile[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int i;
    int j;


    if ((includesfile = fopen("includes.bB", "r")) == NULL)    // open file
    {
        fprintf(stderr, "Cannot open includes.bB for reading\n");
        exit(1);
    }
    readbBfile = (char ***) malloc(sizeof(char **) * 17);

//  while (fgets(line,500,includesfile))
    while (1) {
        if (fscanf(includesfile, "%s", line) == EOF)
            break;
        if ((!strncmp(line, "bB", 2)) && (line[2] != '.')) {
            i = (int) (line[2]) - 48;
            for (j = 1; j < writebBfile[i]; ++j)
                printf("%s", readbBfile[i][j]);
            continue;
        } else {
            strcpy(prepend, path);
            strcat(prepend, line);
            if ((asmfile = fopen(line, "r")) == NULL)    // try file w/o includes path
            {
                if ((asmfile = fopen(prepend, "r")) == NULL)    // open file
                {
                    fprintf(stderr, "Cannot open %s for reading: %s\n", line, prepend);
                    exit(1);
                }
            } else if (strncmp(line, "bB\0", 2)!=0) {
                fprintf(stderr, "User-defined %s found in current directory\n", line);
            }
        }
        while (fgets(asmline, 500, asmfile)) {

            // check for last bank split point within the bB source file
            if (!strncmp(asmline, "; bB.asm file is split here", 20)) {

                writebBfile[bB]++;  // flag the split

                readbBfile[bB] = (char **) malloc(sizeof(char *) * 50000);
                readbBfile[bB][writebBfile[bB]] = (char *) malloc(strlen(line) + 3);

                sprintf(readbBfile[bB][writebBfile[bB]], ";%s\n", line);
            }
            if (!writebBfile[bB])
                printf("%s", asmline);
            else {
                readbBfile[bB][++writebBfile[bB]] = (char *) malloc(strlen(asmline) + 3);
                sprintf(readbBfile[bB][writebBfile[bB]], "%s", asmline);
            }
        }
        fclose(asmfile);

        // if file has been split at this point, move to next flag index
        if (writebBfile[bB])
            bB++;
        // if (writebBfile) fclose(bBfile);
    }
}