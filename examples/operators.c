#include <stdio.h>

int counter = 0;

int next()
{
    counter++;
    return counter;
}

int main()
{
    int a = 17, b = 5, c;
    printf("%d %d %d %d\n", a / b, a % b, -a / b, -a % b);
    printf("%d %d %d\n", a > b, a == b, !(a > b));
    c = a > b && b > 0;
    printf("c = %d\n", c);
    c = (a < b) + (a != b) * 10;
    printf("c = %d\n", c);
    int x = 3;
    int y = x++;
    y += ++x;
    printf("x = %d y = %d\n", x, y);
    x = y = 4;
    x += 3; y *= x; y -= 1; y /= 3; y %= 5;
    printf("x = %d y = %d\n", x, y);
    double d = 10;
    d /= 4;
    int i = 10;
    i /= 4;
    i += 2.7;
    printf("d = %.2f i = %d\n", d, i);
    if (0 || next() > 0)
        printf("called, counter = %d\n", counter);
    if (1 || next() > 0)
        printf("short-circuit, counter = %d\n", counter);
    printf("%d\n", - -a);
    printf("%d %d\n", 2 + 3 * 4 - 6 / 2, (2 + 3) * (4 - 6) / 2);
    printf("%c%c%c [%5d] [%-5d] [%05d] [%x] [%8.3f] %%\n", 'O', 'K', '\n' + 23, 42, 42, -42, 255, 3.14159);
    int m = a > b ? a : b;
    printf("max = %d\n", m);
    int k = 0;
    do k += 2; while (k < 9);
    printf("k = %d\n", k);
    int t = 0;
    for (int j = 0; j < 10; j++) {
        if (j % 2) continue;
        if (j > 6) break;
        t += j;
    }
    printf("t = %d\n", t);
    if (a > 100)
        printf("big\n");
    else if (a > 10)
        if (b > 10) printf("both\n");
        else printf("dangling else -> inner if\n");
    return 0;
}
