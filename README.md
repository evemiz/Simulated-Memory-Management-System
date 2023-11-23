# Simulated-Memory-Management-System
==Description==
This code provides a framework for simulating a memory management system, including virtual main memory - the RAM, page tables - a table with two levels, the first level is for the type of page(TEXT/DATA/BSS/HEAP_STACK) and the second level is for the data of the pages(if the page is valid, dirty, his frame and his swap index), and swap file to save all the dirty pages if the main memory is full.

Program DATABASE:
The page_descriptor struct defines the properties of a page in the memory. It contains the following fields:
valid: Indicates whether the page is in the main memory
frame: The frame number where the page is stored in main memory, if it's not there the frame will be -1.
dirty: Indicates whether the page has been modified - used with store function.
swap_index: The index of the page in the swap file, if it's not there - the index will be -1.
timer: A timer value used for page swapping with LRU algorithm.

functions:
1. constructor - initializes a sim_mem object, sets up the necessary variables and the page table.
2. destructor - cleans up resources and releases file descriptors.
3. load - tries to get the character at the specified memory address and returns the character
4. store - tries to change the char at a specified memory address to a givven char.
5. helper_load_store - a helper function for the load and store functions, performs the actual loading or storing operation at the given address.
6. print_memory - prints the main memory.
7. print_swap - prints the swap file.
8. print_page_table - prints the page table.
9. power_of_two - calculates and returns the power of two for a given num.
10.binary_to_decimal - converts a binary number to its decimal equivalent.
11.address_calc - converts a logical address to a physical address.
12.next_frame_available - finds the next available frame in the main memory.
13.timer_set - updates the timer value for a page in the page table.
14.frame_out - find the frame that should swapped out from the main memory according to LRU algorithm.

==Program Files==
- main.cpp - contains a short main 
- sim_mem.h - contains the header file of the simulated memory
- sim_mem.cpp - contains the cpp file of the simulated memory
- Makefile


==How to compile?==
compile: make
run: ./sim_mem

==Input & Output==
you can use the functions load and store to get or change chars in specific address, and you can print the memory, the swap file and the page table to follow the memory.


