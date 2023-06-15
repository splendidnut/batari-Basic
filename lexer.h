//
// Created by User on 1/7/2023.
//

#ifndef BATARI_BASIC_LEXER_H
#define BATARI_BASIC_LEXER_H

enum { MAX_LINE_LENGTH = 300 };


extern void preprocess(char *data);
extern void tokenize(char **statement, const char *code, int lineNum);

#endif //BATARI_BASIC_LEXER_H
