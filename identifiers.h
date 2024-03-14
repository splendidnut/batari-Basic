/***************************************************************************
 * Neolithic Compiler - Simple C Cross-compiler for the 6502
 *
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
// Created by User on 8/27/2021.
//

#ifndef NEOLITHIC_IDENTIFIERS_H
#define NEOLITHIC_IDENTIFIERS_H

extern char * Ident_lookup(char *lookupName);
extern char * Ident_add(char *name, int value);
extern void initHashTable();
extern void printHashTable();

extern void collect_identifiers();

#endif //NEOLITHIC_IDENTIFIERS_H
