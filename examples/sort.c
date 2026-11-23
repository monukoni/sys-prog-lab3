#include <stdio.h>

void print(int a[], int n)
{
    for (int i = 0; i < n; i++)
        printf("%d ", a[i]);
    printf("\n");
}

void bubble(int a[], int n)
{
    int swapped = 1;
    while (swapped) {
        swapped = 0;
        for (int i = 0; i + 1 < n; i++) {
            if (a[i] > a[i + 1]) {
                int t = a[i];
                a[i] = a[i + 1];
                a[i + 1] = t;
                swapped = 1;
            }
        }
        n--;
    }
}

int main()
{
    int a[10] = {5, -3, 9, 0, 12, 7, -8, 1, 4, 2};
    print(a, 10);
    bubble(a, 10);
    print(a, 10);
    return 0;
}
