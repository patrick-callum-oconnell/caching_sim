# caching_sim
A program that generates a cache and simulates caching operations, written in C.

It reads a trace file and performs basic caching functions, printing out when
there are hits, misses, and evictions. The cache itself is generated within the main() function,
and the actual size and format of the cache and its attributes that will be allocated is determined by the user
in the command line. So, the cache can be arbitrarily large or small.

Memory is efficiently allocated and deallocated, so this program should be fast even with extremely large inputs.

NOTE: THIS PROGRAM IS WRITTEN FOR LINUX, NOT WINDOWS

PARAMETERS:
Program takes optional -h and -v inputs which indicate display of usage info and trace info respectively.
Required inputs: -s (# set bits), -E(lines per set), -b (# block bits), -t (name of trace file)

Here is a sample format of a few lines in a given trace file:
I 0410d8d4,8
 S 7ff1205c8,8
 L 04f6b876,8
 M 0521d7f0,4
Where "I" indicates an instruction load, "S", "L", "M" being data saves, loads, modifies respectively.
To use/test this program, VALGRIND is used to automatically generate these values.

