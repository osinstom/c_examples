//
// Created by p4dev on 18.12.18.
//

#include <stdio.h>

void basic_test(void);
void decompose(double, long*, double*);
void max_min(int array[], int N, int *small, int *big);
int* maxiumum(int*, int*);

int main(int argc, char *argv[])
{
    basic_test();
}

void basic_test()
{
    puts("Hello world");
    printf("Hello world!\n");
    int i = 6;
    int *p = &i;

    printf("%d\n", *p);
//    printf("%d\n", p);
//    printf("%d\n", &i);

    /*
     * Pointers as Arguments
     */

    long l = 24L;
    double d = 7.15;
    decompose(3.145, &l, &d);
    printf("Decompose, %li %f\n", l, d);


//    MAX-MIN
    int array[] = { 1, 9, 3, 24, 55, 2, 44, 2, 5};
    int size = (int) (sizeof(array) / sizeof(array[0]));
    int min, max;
    max_min(array, size, &min, &max);
    printf("Min: %d, Max: %d\n", min, max);

    /*
     * Pointers as Return Values
     * Note! Never return a pointer to an automatic local variable!
     */
    int *y;
    int j = 3, k = 5;
    y = maxiumum(&j, &k);
    printf("Max: %d\n", *y);
}

void decompose(double x, long *int_part, double *frac_part)
{
    *int_part = (long) x;
    *frac_part = x - *int_part;
}

/*
 * Takes array a with size N and finds min & max values.
 */
void max_min(int a[], int N, int *min, int *max)
{

    *max = a[0];
    *min = a[0];
    int i;
    for (i = 0; i<N;i++)
    {
        if (a[i] > *max)
            *max = a[i];
        if (a[i] < *min)
            *min = a[i];
    }

}

int* maxiumum(int *a, int *b) {
    if (*a > *b)
        return a;
    else
        return b;
}