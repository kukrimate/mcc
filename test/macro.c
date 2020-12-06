#define A(x, y, z) (x + y + z)

A(1,5,2)
A((1 + 1),2,3)

// Infinite recursion test
#define X Y
#define Y Z
#define Z X

X
