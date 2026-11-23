#include <stdio.h>

int sieve[200];

int main()
{
    int n = 200, count = 0;
    for (int i = 2; i < n; i++)
        sieve[i] = 1;
    for (int i = 2; i * i < n; i++) {
        if (!sieve[i])
            continue;
        for (int j = i * i; j < n; j += i)
            sieve[j] = 0;
    }
    for (int i = 0; i < n; i++) {
        if (sieve[i]) {
            printf("%4d", i);
            count++;
            if (count % 10 == 0)
                printf("\n");
        }
    }
    printf("\nprimes below %d: %d\n", n, count);
    return 0;
}
