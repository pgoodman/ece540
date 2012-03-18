int main() {
    int x = 0;
    head_1: ++x;
    head_2: ++x;
    head_3: ++x;
    switch(x) {
    case 0: goto head_1;
    case 1: goto head_2;
    case 3: goto head_3;
    case 4: goto head_1;
    case 5: goto head_2;
    case 6: goto head_3;
    case 7: goto head_1;
    case 8: goto head_2;
    case 9: goto head_3;
    case 10: goto head_1;
    case 11: goto head_2;
    case 12: goto head_3;
    default: break;
    }
    return 0;
}

