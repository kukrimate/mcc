#include <stdio.h>

int
main(void)
{
	int my_int;

	my_int = 5;
	*(short *) &my_int = 4;
	printf("my_int: %d\n", my_int);
}
