#include <stdio.h>

int fact(int n)
{
    if (n <= 1)
        return 1;
    return n * fact(n - 1);
}

int fib(int n)
{
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

int main()
{
    int i;
    for (i = 0; i <= 10; i++)
        printf("%2d! = %d\n", i, fact(i));
    printf("fib(20) = %d\n", fib(20));
    return 0;
}
