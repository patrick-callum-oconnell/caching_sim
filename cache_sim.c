//AUTHOR: PATRICK O'CONNELL
//DATE: 12/17/2020
//This program implements a basic cache simulation in C.
//It reads a trace file and performs basic caching functions, printing out when
//there are hits, misses, and evictions.
//
// NOTE: THIS PROGRAM IS WRITTEN FOR LINUX, NOT WINDOWS
//
//PARAMETERS:
// Program takes optional -h and -v inputs which indicate display of usage info and trace info respectively.
// Required inputs: -s (# set bits), -E(lines per set), -b (# block bits), -t (name of trace file)
//
// Here is a sample format of a given trace file:
//I 0410d8d4,8
// M 0521d7f0,4
// L 04f6b876,8
// S 7ff1205c8,8
//Where "I" indicates an instruction load, "M", "L", "S" being data modifies, loads, and saves respectively.
//To use/test this program, VALGRIND is used to automatically generate these values.
//

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef unsigned long long ADDRESS; //defines type for address for readability

/* struct that stores information about a single cache line */
struct CacheLine {

	int isValid;
	int LRU_counter;
	int last;
	ADDRESS tag;
	ADDRESS block;
};

/* takes the address as an unsigned long long, and the s and b flags
 * returns the set index as an ADDRESS
 */
ADDRESS get_set_index(ADDRESS address_num, int flag_s_set, int flag_b_set) {
	address_num = address_num >> flag_b_set;
	address_num = address_num << (64 - flag_s_set);
	address_num = address_num >> (64 - flag_s_set);
	return address_num;	
}

/* takes address as ADDRESS, s and b flags
 * returns block as ADDRESS
 */
ADDRESS get_block(ADDRESS address_num, int flag_b_set) {
	address_num = address_num << (64 - flag_b_set);
	address_num = address_num >> (64 - flag_b_set);
	return address_num;
}

/*takes address as ADDRESS, s and b flags
 * returns tag as ADDRESS
 */
ADDRESS get_tag(ADDRESS address_num, int flag_s_set, int flag_b_set) {
	address_num = address_num >> (flag_s_set + flag_b_set); //removes set bits and block bits
	return address_num;
}

//If we have set the v flag, we can use this function to print out the line with a format
//of L 20,1 miss evict hit
//Parameters determine whether or not to print miss or hit after the line
void print_line_output(char* current_line, int miss, int evict, int hit) {
	current_line = current_line + 1;
	
	char print_line[30];
	sscanf(current_line, "%30[^\n]", print_line);	

	printf("%s", print_line);
	
	if (miss == 1) {printf(" miss");}
	if (evict == 1) {printf(" eviction");}
	if (hit == 1) {printf(" hit");}
	if (hit == 2) {printf(" hit hit");}
	
	printf("\n");
}

int main(int argc, char *argv[]) {
	
	//checks to make sure correct # of arguments passed
	//tells user how to input command line if entered incorrectly
	if ((argc != 10) && (argc != 9)) {
	
		printf("Invalid arguments.\n");
		printf("%d", argc);
		printf(" arguments used.\n");
		printf("Arguments required:\n");
		printf("-v (optional), -s int, -E int, -b int, -t string\n");
		return 0;
	}
	int flag_v_set = 0;
	int flag_s_set = 1;
	int flag_E_set = 1;
	int flag_b_set = 1;
	char* trace_file;

	char c;

	//parses command line arguments and assigns the values to our variables	
	while ((c = getopt(argc, argv, "v::s:E:b:t:")) != -1) {
		switch(c) {
			case 'v':
				flag_v_set = 1;
				break;
			case 's':
				flag_s_set = atoi(optarg);
				break;
			case 'E':
				flag_E_set = atoi(optarg);
				break;
			case 'b':
				flag_b_set = atoi(optarg);
				break;
			case 't':
				trace_file = optarg;
				break;
			default:
				printf("Invalid command line argument(s).");
				exit(1);
		}
	}

	//console message to user about the process
	printf("Input accepted. Beginning memory allocation...\n");

	int i, j; //iteration variables used multiple times later

	//allocates memory for the cache	
	int num_sets = 1;
	//loop makes num_sets =  2^flag_s_set
	//without me having to use pow()
	for (i = 0; i < flag_s_set; i++) {
		num_sets <<= 1; //
	}	

	int num_lines = flag_E_set;

	//creates 2d array in which to store the cache sets
	//CacheLine** refers to a set, Cacheline* refers to the lines in that set
	struct CacheLine** cache = (struct CacheLine**) malloc(num_sets * sizeof(struct CacheLine*));

	for(i = 0; i < num_sets; i++) { 
		
		//allocates space for every cache line
		cache[i] = (struct CacheLine*)  malloc(num_lines * sizeof(struct CacheLine));

		for (j = 0; j < num_lines; j++) {
			//init some cache metadata
			cache[i][j].isValid = 0;
			cache[i][j].LRU_counter = 0;
			cache[i][j].last = 1;
		}			
	}

	//initializes counters for cache operations
	int hit_total = 0;
	int miss_total = 0;
	int evict_total = 0;

	//reads file line by line, performing the cache operations
	FILE *fptr;
	fptr = fopen(trace_file, "r");
	
	//loop to read through every line of the file, stops when fgets returns NULL
	//which only happens due to an error or at the end of file (EOF)
	int max_length = 30; //max length of string
	char current_line[max_length];
	char* current_operation;
	char* address_string;
	int LRU, min_LRU_count; //ints for use in finding the least recently used line
	int hasChanged;

	ADDRESS address_num, block, tag;
	int set_index; //is used for indexing, so better used as an int

	printf("Caching operation(s) initiated.\n");

    //Now we go through the file itself and perform the caching operations
	while (fgets(current_line, max_length, fptr) != NULL) {
	

		current_operation = (char*) malloc(sizeof(char));
		address_string = (char*) malloc(strlen(current_line) * sizeof(char));
	
		sscanf(current_line, "%s %9[^,]", current_operation, address_string);
		address_num = strtoull(address_string, NULL, 16);

		//printf("Address num: %llu\n", address_num);
		
		//if not a L, S, or M operation, doesn't do anything, otherwise
		//this if statement performs the caching operation
		if (strcmp(current_operation, "I") != 0) {
			//gets relevant data from the operation address
			set_index = (int) get_set_index(address_num, flag_s_set, flag_b_set);
			block = get_block(address_num, flag_b_set);
			tag = get_tag(address_num, flag_s_set, flag_b_set);
				
			//now, perform the caching operation
			//first, determines LRU line in case we need to evict
			LRU = 0;
			min_LRU_count = cache[set_index][0].LRU_counter;

			if (num_lines > 1) {
				if (cache[set_index][0].last == 0) { LRU = 1; }
				for (i = 0; i < num_lines; i++) {
					if ((cache[set_index][i].LRU_counter < min_LRU_count) && (cache[set_index][0].last != i)) {
						LRU = i; //sets the current line to be the LRU line
						min_LRU_count = cache[set_index][i].LRU_counter;
					}
				}	
			}
			
			//now actually goes through the lines to read the tags
			//this loop handles possibility of invalid lines and hits
			hasChanged = 0;
			for (i = 0; (i < num_lines) && (hasChanged == 0); i++) {
				//if invalid cache, loads values in
				if (cache[set_index][i].isValid == 0) {
					cache[set_index][i].isValid = 1;
				
					if (strcmp(current_operation, "M") == 0) {
						if (flag_v_set == 1) {print_line_output(current_line, 1, 0, 1);} //indicates miss and hit
						miss_total++;
						hit_total++;
					} else {
						if (flag_v_set == 1) {print_line_output(current_line, 1, 0, 0);} //indicates only a miss
						miss_total++;
					}
						
					cache[set_index][i].tag = tag;
					cache[set_index][i].block = block;						
					
					cache[set_index][i].LRU_counter++;
					cache[set_index][0].last = i;
					hasChanged = 1;

				}
				else if (cache[set_index][i].tag == tag) {
					if (strcmp(current_operation, "M") == 0) {
						if (flag_v_set == 1) {print_line_output(current_line, 0, 0, 2);} //hit hit
						hit_total = hit_total + 2;
					} else {
						if (flag_v_set == 1) {print_line_output(current_line, 0, 0, 1);} //hit
						hit_total++;
					}
												
					if (strcmp(current_operation, "S") != 0) {
						cache[set_index][i].block = block; //if not a save, needs to load data in
						//no change in tag needed b/c in this if statement we know the tag is the same
					}
					//cache[set_index][i].LRU_counter++;
					cache[set_index][0].last = i;
					hasChanged = 1;

				
				}						
			}

	
			//THIS LOOP HANDLES MISSES AND EVICTIONS
			//if hasChanged is still 0, then it is a miss, so we alter the LRU line that we determined in an earlier loop
			if (hasChanged == 0) {						
				
				if (strcmp(current_operation, "M") == 0) {
					if (flag_v_set == 1) {print_line_output(current_line, 1, 1, 1);} //miss evict hit
					miss_total++;
					evict_total++;						
					hit_total++;
				} else {
					if (flag_v_set == 1) {print_line_output(current_line, 1, 1, 0);} //miss evict
					miss_total++;
					evict_total++;
				}
				cache[set_index][LRU].tag = tag;
				cache[set_index][LRU].block = block;
				cache[set_index][LRU].LRU_counter++;	
				cache[set_index][0].last = LRU;		
			} 								
		}
		
		free(current_operation);
		free(address_string);
	}		
		
	fclose(fptr);	
	//free memory
	for(i = 0; i < num_sets; i++) { 		
		//frees space for every cache line
		free(cache[i]);
	}	
	free(cache);

	printf("Caching process completed. Memory has been freed.\n");

    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions); //prints a final total of hits, misses, evictions

    return 0;
}
