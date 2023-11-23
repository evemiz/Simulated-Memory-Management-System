
#ifndef EX4_SIM_MEM_H
#define EX4_SIM_MEM_H

#define MEMORY_SIZE 200
extern char main_memory[MEMORY_SIZE];

typedef struct page_descriptor
{
    bool valid;
    int frame;
    bool dirty;
    int swap_index;
    int timer;
} page_descriptor;

class sim_mem {
    int swapfile_fd;                //swap file fd
    int program_fd;                 //executable file fd
    int text_size;                  //the text size
    int data_size;                  //the data size
    int bss_size;                   //the bss size
    int heap_stack_size;            //the heap and stack size
    int num_of_pages;               //number of pages in the virtual memory
    int page_size;                  //the size of page (and frame)
    page_descriptor **page_table;   //pointer to page table
    int* mem_available;             //when zero - the frame in the main memory is available
    int frame_number;
    int is_store;                   //when 0 - load function, when 1 - store function
    int text_pages;
    int data_pages;
    int bss_pages;
    int heap_stack_pages;
    int* swap_available;

public:
    //constructor
    sim_mem(char exe_file_name[], char swap_file_name[], int text_size,
            int data_size, int bss_size, int heap_stack_size,
            int page_size);
    //destructor
    ~sim_mem();
    //load - try to get to an address for reading
    char load(int address);
    //store - try to get to an address for writing
    void store(int address, char value);
    //helper function to load and store functions
    char helper_load_store(int address, char c);
    //print the main memory
    static void print_memory();
    //print the swap file
    void print_swap();
    //print page table
    void print_page_table();
    //function to calculate the power of two
    static int power_of_two(int num);
    //function to convert a binary number to a decimal
    static int binary_to_decimal(const int binaryArray[], int size);
    //function to convert from logical address to a physical address
    int* address_calc(int address);
    //function to find the next frame that available
    int next_frame_available();
    //update the timer in the pages
    void timer_set(int page, int type);
    //return the frame to swap
    int frame_out();
};

#endif //EX4_SIM_MEM_H
