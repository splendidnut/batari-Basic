/***************************************************************************
 * Copyright (c) 2020-2022 by Philip Blackman
 * -------------------------------------------------------------------------
 *
 * Licensed under the GNU General Public License v2.0
 *
 * See the "LICENSE.TXT" file for more information regarding usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * -------------------------------------------------------------------------
 */

//
//   Identifier Module
//
// Functions and storage for parsed identifiers
//
// Created by User on 8/27/2021.
//

#include <ctype.h>      // needed for isalpha / isalnum
#include <stdio.h>      // needed for printf
#include <stdlib.h>     // needed for malloc and free
#include <string.h>     // needed for strcmp
#include <stdbool.h>

#include "identifiers.h"
#include "statements.h"
#include "lexer.h"

enum IdentiferType {
    ID_NONE,
    ID_VARIABLE,
    ID_CONST,
    ID_LABEL,
    ID_RESERVED
};

//-------------------------------------------------------
static int hashItemCount = 0;
static int hashMemoryUsed = 0;

void *HASH_allocMem(int size) {
    hashMemoryUsed += size;
    return malloc(size);
}

void HASH_freeMem(void *mem) {
    free(mem);
}


//-------------------------------------------------------

typedef struct hashNode {
    struct hashNode *next;
    char *name;
    int value;
} hashNode;

#define HASH_TABLE_SIZE 11
static struct hashNode *identHT[32][HASH_TABLE_SIZE];
bool bucketHasValues[32];

/**
 * Generate hash value for string using djb2 method
 *
 * LINK:  http://www.cse.yorku.ca/~oz/hash.html
 */
unsigned int hash(char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; // hash * 33 + c

    return hash % HASH_TABLE_SIZE;
}

struct hashNode * lookup(char *s) {
    struct hashNode *np;
    char firstChar = s[0] & 31;
    for (np = identHT[firstChar][hash(s)]; np != NULL; np = np->next)
        if (strcmp(s, np->name) == 0)
            return np;
    return NULL;
}

char *copyString(const char *src) {
    char *dst = HASH_allocMem(strlen (src) + 1);
    if (dst == NULL) return NULL;
    strcpy(dst, src);
    return dst;
}

struct hashNode * install(char *name, int value) {
    struct hashNode *np;
    unsigned int hashVal;
    char firstChar = name[0] & 31;

    if ((np = lookup(name)) == NULL) {
        np = (struct hashNode *) HASH_allocMem(sizeof(*np));
        np->name = copyString(name);
        np->value = value;
        hashVal = hash(name);
        np->next = identHT[firstChar][hashVal];
        identHT[firstChar][hashVal] = np;
        bucketHasValues[firstChar] = true;
        hashItemCount++;
    }
    return np;
}

//---------------------------------------------

char * Ident_lookup(char *lookupName) {
    struct hashNode *np = lookup(lookupName);
    return np ? np->name : 0;
}


char * Ident_add(char *name, int value) {
    struct hashNode *np = install(name, value);
    return np ? np->name : 0;
}

void initHashTable() {
    int firstChar;
    for (firstChar = 0; firstChar < 32; firstChar++)
        bucketHasValues[firstChar] = false;
}

void printHashTable() {
    struct hashNode *np;
    int maxDepth = 0;
    int index, firstChar;
    for (firstChar = 0; firstChar < 32; firstChar++) if (bucketHasValues[firstChar])  {
        fprintf(stderr, "%d\n", firstChar);
        for (index = 0; index < HASH_TABLE_SIZE; index++) {
            int depth = 0;
            for (np = identHT[firstChar][index]; np != NULL; np = np->next) {
                depth++;
                if (depth > maxDepth) maxDepth = depth;
                fprintf(stderr, "\t%-20s  %d\n", np->name, np->value);
            }
            //printf("\n");
        }
    }
    fprintf(stderr, "Number of items in hash table: %d\n", hashItemCount);
    fprintf(stderr, "Max depth of hash search: %d\n", maxDepth);
    fprintf(stderr, "Hash table memory usage: %d\n", hashMemoryUsed);
}


//---------------------------------------------

char *bbasicCommands[] = {
        "asm",        "bank",       "callmacro",    "collision",
        "const",      "data",       "dec",          "def",
        "dim",        "drawscreen", "else",         "end",
        "for",        "function",   "gosub",        "goto",
        "if",         "include",    "includesfile", "inline",
        "joy0down",   "joy0fire",   "joy0left",     "joy0right",    "joy0up",
        "joy1down",   "joy1fire",   "joy1left",     "joy1right",    "joy1up",
        "let",
        "macro", "next", "on", "otherbank",
        "pfclear", "pfhline", "pfpixel", "pfread", "pfscroll", "pfvline",
        "pop", "push", "pull",
        "rand", "reboot",
        "rem", "return",
        "sdata", "set", "sread", "stack",
        "switchbw", "switchleftb", "switchreset","switchrightb","switchselect",
        "then", "thisbank",
        "vblank",
        0};

void init_identifiers() {
    // initialize identifier list with reserved words
    int i=0;
    while (bbasicCommands[i] != 0) {
        Ident_add(bbasicCommands[i], ID_RESERVED);
        i++;
    }
}

void collect_identifiers() {
    char **statement;
    int i, j;

    bool failedToRead;
    char code[500];
    char displaycode[500];

    //----  allocated memory for statement token list
    statement = (char **) malloc(sizeof(char *) * 200);
    for (i = 0; i < 200; ++i) {
        statement[i] = (char *) malloc(sizeof(char) * 200);
    }

    initHashTable();
    init_identifiers();

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


        // tokenize the statement
        tokenize(statement, code, bbgetlinenumber());

        // process the statement
        char *prevId = NULL;
        for (i = 0; i < 200; ++i) {
            if (isalpha(statement[i][0]) || statement[i][0] == '_') {
                char *identifer = statement[i];

                // trim off any array/bit references
                j=0;
                while ((identifer[j] != 0) && (isalnum(identifer[j]) || identifer[j]=='_')) j++;
                identifer[j] = 0;

                // handle depending on type
                enum IdentiferType idType = ID_NONE;
                if ((prevId != NULL) && (!strncmp(prevId, "const", 5))) {
                    idType = ID_CONST;
                } else if ((prevId != NULL) && (!strncmp(prevId, "dim", 5))) {
                    idType = ID_VARIABLE;
                } else if (statement[i+1][0] == '=') {
                    idType = ID_VARIABLE;
                } else if (i==0 && (statement[i+1][0] == 0)) {
                    idType = ID_LABEL;
                }

                // add to identifier list
                Ident_add(identifer, idType);

                prevId = identifer;
            }
        }

        /*if (numconstants == (MAXCONSTANTS - 1)) {
            fprintf(stderr, "(%d) Maximum number of constants exceeded.\n", bbgetlinenumber());
            exit(1);
        }*/
    }

    //printHashTable();
}
