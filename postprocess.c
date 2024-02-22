// Provided under the GPL v2 license. See the included LICENSE.txt for details.

#include <string.h>
#include <unistd.h>

#include "linker.h"

int main(int argc, char *argv[]) {
    char includesPath[500];
    int i;
    while ((i = getopt(argc, argv, "i:")) != -1) {
        switch (i) {
            case '?':
                includesPath[0] = '\0';
                break;
            case 'i':
                strcpy(includesPath, optarg);
                break;
            default:
                break;
        }
    }

    char slashChar = includesPath[strlen(includesPath) - 1];
    if ((slashChar == '\\') || (slashChar == '/')) {
        strcat(includesPath, "includes/");
    } else {
        strcat(includesPath, "/includes/");
    }

    link_files(includesPath);
}
