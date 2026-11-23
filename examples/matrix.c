#include <stdio.h>

int n = 4;

void mul(double a[], double b[], double c[])
{
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double s = 0;
            for (int k = 0; k < n; k++)
                s += a[i * n + k] * b[k * n + j];
            c[i * n + j] = s;
        }
}

void show(double m[])
{
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            printf("%8.2f", m[i * n + j]);
        printf("\n");
    }
}

int gcd(int a, int b)
{
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int collatz(int x)
{
    int steps = 0;
    while (x != 1) {
        x = x % 2 == 0 ? x / 2 : 3 * x + 1;
        steps++;
    }
    return steps;
}

int main()
{
    double a[16], b[16], c[16];
    for (int i = 0; i < 16; i++) {
        a[i] = i + 1;
        b[i] = (i % 5 == 0) ? 1 : 0.5;
    }
    mul(a, b, c);
    show(c);
    printf("gcd(1071, 462) = %d\n", gcd(1071, 462));
    int best = 0, arg = 0;
    for (int i = 1; i < 1000; i++) {
        int s = collatz(i);
        if (s > best) {
            best = s;
            arg = i;
        }
    }
    printf("collatz: %d steps for %d\n", best, arg);
    return 0;
}
