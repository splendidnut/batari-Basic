//
// Created by User on 1/7/2023.
//

#include <string.h>
#include <stdio.h>
#include "lexer.h"


int lexerLine = 0;      // track which line has passed thru the lexer.

/**
 * Read line from source code stream
 *
 * - Make a copy of the original source line
 * - Separate tokens with whitespace
 *
 * @param data - copied then overwritten with tokens separated out
 * @return True if no more data (failed to read)
 */
void preprocess(char *data) {
    char originalStr[300];

    // copy the original source line
    strncpy(originalStr, data, 298);
    originalStr[298] = '\0';            // SAFETY: make sure string is zero-terminated

    // rewrite line with tokens separated out
    int foundchars = 0;
    int done = 0;
    char c,lastChar = 0;
    char lastOp = 0;
    char *src = originalStr;
    char *dst = data;

    while (*src && !done) {
        lastChar = c;
        c = *src;
        if ((c == 0x0A) || (c == 0x0D)) break;
        switch (c) {
            case '<':
            case '(':
            case ')':
            case '+':
            case '-':
            case ':':
            case ',':
                *(dst++) = ' ';
                *(dst++) = c;
                *(dst++) = ' ';
                lastOp = c;
                break;

                // handle '<>'
            case '>':
                if (lastOp == '<') {
                    dst--;
                } else {
                    *(dst++) = ' ';
                }
                *(dst++) = c;
                *(dst++) = ' ';
                lastOp = c;
                break;

                // handle double ops
            case '|':
            case '&':
                if (lastOp == c) {
                    dst--;
                } else {
                    *(dst++) = ' ';
                }
                *(dst++) = c;
                *(dst++) = ' ';
                lastOp = c;
                break;

                // process '=' and check for preceding '<' or '>'
            case '=':
                if ((lastOp == '<') || (lastOp == '>')) {
                    dst--;
                } else {
                    *(dst++) = ' ';
                }
                *(dst++) = c;
                *(dst++) = ' ';
                lastOp = '=';
                break;
            case ';':
                if (dst > data && (*(dst-1) == ' ')) dst--;
                *(dst++) = '\n';  // truncate ';' line comments
                *(dst++) = '\0';
                done = 1;
                lastOp = 0;
                break;
            case ' ':
            case 0x09:
                if (lastChar != ' ')
                    *(dst++) = ' ';
                lastChar = ' ';
                break;
            default: {
                *(dst++) = c;
                foundchars = 1;
                lastOp = 0;
            }
        }
        src++;    // next input character
    }
    lexerLine++;

    // now trim off trailing whitespace
    if ((dst > data) && (*(dst-1) == ' ')) dst=dst-1;

    if (done && !foundchars) {
        *data = '\0';       // if nothing of interest is found, clear out original string
        return;
    }
    *(dst++) = '\0';    // make sure data is properly terminated
}


/**
 * Tokenize -  Break apart a statement line into individual token strings
 */
void tokenize(char **statement, const char *code, int lineNum) {
    char single;
    int multiplespace = 0;
    int srcIdx = 0;
    int dstIdk = 0;
    int tokenCnt = 0;

    // look for spaces, reject multiples
    while (code[srcIdx] != '\0') {
        single = code[srcIdx++];
        if (single == ' ') {
            if (!multiplespace) {
                tokenCnt++;
                dstIdk = 0;
            }
            multiplespace++;
        } else {
            multiplespace = 0;
            if (dstIdk < 199)    // avoid overrun with long horizontal separators
                statement[tokenCnt][dstIdk++] = single;
        }

    }
    if (tokenCnt > 190) {
        fprintf(stderr, "(%d) Warning: long line\n", lineNum);
    }
}
