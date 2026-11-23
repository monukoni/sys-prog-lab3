#include <stdio.h>

int x = 1;

int is_even(int n);
int is_odd(int n)
{
    return n == 0 ? 0 : is_even(n - 1);
}
int is_even(int n)
{
    return n == 0 ? 1 : is_odd(n - 1);
}

void show()
{
    printf("global x = %d\n", x);
}

int main()
{
    printf("x = %d\n", x);
    int x = 2;
    {
        int x = 3;
        printf("inner x = %d\n", x);
    }
    printf("local x = %d\n", x);
    show();
    printf("is_even(10) = %d, is_odd(7) = %d\n", is_even(10), is_odd(7));
    int sum = 0, i = 0;
    while (1) {
        if (++i > 100) break;
        sum += i;
    }
    printf("sum = %d\n", sum);
    return 0;
}
