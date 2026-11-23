#include <stdio.h>
#include <math.h>

/* корінь методом Ньютона */
double my_sqrt(double x)
{
    double r = x / 2.0;
    int steps = 0;
    do {
        r = (r + x / r) / 2;
        steps++;
    } while (fabs(r * r - x) > 1e-12 && steps < 100);
    return r;
}

double average(double v[], int n)
{
    double s = 0;
    for (int i = 0; i < n; i++)
        s += v[i];
    return s / n;
}

int main()
{
    double v[5] = {1.5, 2.25, 3.0, 10.0, 0.125};
    for (int i = 1; i <= 5; i++)
        printf("sqrt(%d) = %.6f (math: %.6f)\n", i, my_sqrt(i), sqrt(i));
    printf("average = %.4f\n", average(v, 5));
    printf("pow = %.3f\n", pow(2, 10) / 3);
    int k = 7.9;
    double d = 7 / 2;
    double e = 7 / 2.0;
    printf("%d %.1f %.1f %d\n", k, d, e, (int)(e * 3));
    return 0;
}
