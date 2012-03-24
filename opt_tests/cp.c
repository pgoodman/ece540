/*
 * cp.c
 *
 *  Created on: Mar 24, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */


int main(void) {
    int x = 10;
    int y = x;
    int k = x;
    int j = 0;

    for(; x < 100; ) {
        int z = y;
        if(x) {
            y = x;
            k = y;
            j = x;
        } else {
            j = y;
        }

        x = y + y + z + k + j;
    }

    return j;
}

