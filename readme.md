# mcc
This is my C99 hobby compiler.

# structure
Each stage so far is implemented as a (mostly) standalone library in `lib`
with the driver program(s) in `src`.

# project state
The pre-processor is supposed to be fully working, obviously there are bugs to
iron out. Simple program's pre-processed via mcc's preprocessor can compile with
other compilers (glibc's headers mostly work). The parser is currently being worked
on.

# copying
You probably can get a better compiler, under a less-restrictive license elsewhere.
If you are still interested in using this one, you can do so under version 2 of the GPL.
