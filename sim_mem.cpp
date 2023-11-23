#include "sim_mem.h"
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cstdio>
#include <cmath>

#define ADDRESS_SIZE 12
#define PAGE_TYPE_SIZE 2
#define TEXT_IND 0
#define DATA_IND 1
#define BSS_IND 2
#define STACK_HEAP_IND 3

char main_memory[MEMORY_SIZE];
sim_mem::sim_mem(char exe_file_name[], char swap_file_name[], int text_size, int data_size, int bss_size,
                 int heap_stack_size, int page_size) {
    this->text_size=text_size;
    this->data_size=data_size;
    this->bss_size=bss_size;
    this->heap_stack_size=heap_stack_size;
    this->page_size=page_size;
    this->is_store=-1;
    this->text_pages=(text_size/page_size);
    this->data_pages=(data_size/page_size);
    this->bss_pages=(bss_size/page_size);
    this->heap_stack_pages=(heap_stack_size/page_size);
    this->num_of_pages=text_pages+data_pages+bss_pages+heap_stack_pages;

    //open the exe file
    this->program_fd = open(exe_file_name, O_RDONLY);
    if (program_fd == -1) {
        perror("Failed to exe file");
        exit(EXIT_FAILURE);
    }

    //open the swap file
    this->swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (swapfile_fd == -1) {
        perror("Failed to open swap file");
        exit(EXIT_FAILURE);
    }

    memset(main_memory,'0',MEMORY_SIZE);

    //reset the swap file with zeros
    char swap_buffer[(page_size*(num_of_pages-(text_size/page_size)))];
    memset(swap_buffer,'0', sizeof(swap_buffer));
    ssize_t bytesWrite = write(swapfile_fd, swap_buffer, sizeof(swap_buffer));

    if (bytesWrite == -1) {
        perror("Error writing to the file");
        exit(1);
    }

    //swap available - an array to save the empty places in the swap file
    this->swap_available=(int*)malloc(sizeof(int)*(num_of_pages-(text_size/page_size)));
    memset(swap_available,0, sizeof(int)*(num_of_pages-(text_size/page_size)));

    //creat and reset the page table
    page_table =(page_descriptor**)malloc(sizeof(page_descriptor*)*4);
    page_table[0] = (page_descriptor*) malloc(sizeof(page_descriptor)*text_pages);
    for (int i = 0; i < (text_pages); i++) {
        page_table[TEXT_IND][i].valid = false;
        page_table[TEXT_IND][i].frame = -1;
        page_table[TEXT_IND][i].dirty = false;
        page_table[TEXT_IND][i].swap_index = -1;
        page_table[TEXT_IND][i].timer = -1;
    }
    page_table[1] = (page_descriptor*) malloc(sizeof(page_descriptor)*data_pages);
    for (int i = 0; i < (data_pages); i++) {
        page_table[DATA_IND][i].valid = false;
        page_table[DATA_IND][i].frame = -1;
        page_table[DATA_IND][i].dirty = false;
        page_table[DATA_IND][i].swap_index = -1;
        page_table[DATA_IND][i].timer = -1;
    }
    page_table[2] = (page_descriptor*) malloc(sizeof(page_descriptor)*bss_pages);
    for (int i = 0; i < (bss_pages); i++) {
        page_table[BSS_IND][i].valid = false;
        page_table[BSS_IND][i].frame = -1;
        page_table[BSS_IND][i].dirty = false;
        page_table[BSS_IND][i].swap_index = -1;
        page_table[BSS_IND][i].timer = -1;
    }
    page_table[3] = (page_descriptor*) malloc(sizeof(page_descriptor)*heap_stack_pages);
    for (int i = 0; i < (heap_stack_pages); i++) {
        page_table[STACK_HEAP_IND][i].valid = false;
        page_table[STACK_HEAP_IND][i].frame = -1;
        page_table[STACK_HEAP_IND][i].dirty = false;
        page_table[STACK_HEAP_IND][i].swap_index = -1;
        page_table[STACK_HEAP_IND][i].timer = -1;
    }

    this->frame_number=MEMORY_SIZE/page_size; //how many frame in the main memory
    //mem_available - an array to save the empty places in the main memory
    mem_available=(int*) malloc(sizeof(int)*frame_number);
    memset(mem_available, 0, sizeof(int) * frame_number);

}

sim_mem::~sim_mem() {
    close(swapfile_fd);
    close(program_fd);
    for (int i = 0; i < 4; ++i) {
        free(page_table[i]);
    }
    free(page_table);
    free(mem_available);
    free(swap_available);
}

char sim_mem::load(int address) {
    is_store=0;
    return helper_load_store(address,'\0');
}

void sim_mem::store(int address, char value) {
    is_store=1;
    helper_load_store(address,value);
}

char sim_mem::helper_load_store(int address, char c) {
    int* address_arr = address_calc(address);
    int type = address_arr[0];
    int page = address_arr[1];
    int offset = address_arr[2];
    int available_frame ;
    int page_index = page*page_size;

    //check if the address is legal
    if ((type==TEXT_IND && address>=text_size) || (type==DATA_IND && address>=1024+data_size) ||
    (type==BSS_IND && address>=2048+bss_size) || (type==STACK_HEAP_IND && address>=3072+heap_stack_size) || address<0){
        printf("ERR\n");
        free(address_arr);
        return '\0';
    }


    //in case the page is valid in the memory
    if (page_table[type][page].valid) {
        available_frame = page_table[type][page].frame;
        timer_set(page,type);
    }

        //in case the page is not valid
    else{
        available_frame = next_frame_available();       //find the next available frame to insert a page that is not in the main memory

        //in case the main memory is full - need to get a frame out
        if (available_frame == -1){
            int new_frame = frame_out();
            int find_type=0;                                        //save the page type
            int find_page=0;                                        //save the page number
            int should_swap = 0;                                    //if equal 1 so this page should save in the swap

            //find the page that we should move to the swap file and update him
            for (int j = 0; j < text_size/page_size ; ++j) {
                if (page_table[TEXT_IND][j].frame == new_frame){
                    page_table[TEXT_IND][j].valid = false;
                    mem_available[page_table[TEXT_IND][j].frame]=0;
                    page_table[TEXT_IND][j].frame = -1;
                    page_table[TEXT_IND][j].timer = -1;
                }
            }
            for (int j = 0; j < data_size/page_size ; ++j) {
                if (page_table[DATA_IND][j].frame == new_frame){
                    page_table[DATA_IND][j].valid = false;
                    mem_available[page_table[DATA_IND][j].frame]=0;
                    page_table[DATA_IND][j].frame = -1;
                    page_table[DATA_IND][j].timer = -1;
                    if (page_table[DATA_IND][j].dirty){
                        should_swap=1;
                        find_type=DATA_IND;
                        find_page=j;
                    }
                }
            }
            for (int j = 0; j < bss_size/page_size ; ++j) {
                if (page_table[BSS_IND][j].frame == new_frame){
                    page_table[BSS_IND][j].valid = false;
                    mem_available[page_table[BSS_IND][j].frame]=0;
                    page_table[BSS_IND][j].frame = -1;
                    page_table[BSS_IND][j].timer = -1;
                    if (page_table[DATA_IND][j].dirty){
                        should_swap=1;
                        find_type=BSS_IND;
                        find_page=j;
                    }
                }
            }
            for (int j = 0; j < heap_stack_size/page_size ; ++j) {
                if (page_table[STACK_HEAP_IND][j].frame == new_frame){
                    page_table[STACK_HEAP_IND][j].valid = false;
                    mem_available[page_table[STACK_HEAP_IND][j].frame]=0;
                    page_table[STACK_HEAP_IND][j].frame = -1;
                    page_table[STACK_HEAP_IND][j].timer = -1;
                    if (page_table[DATA_IND][j].dirty){
                        should_swap=1;
                        find_type=STACK_HEAP_IND;
                        find_page=j;
                    }
                }
            }
            //in case the page should save in the swap file
            if(should_swap == 1){
                char buffer[page_size];
                //copy the page into a buffer
                for (int i = 0, j = new_frame*page_size; i < page_size; ++i) {
                    buffer[i]=main_memory[j++];
                }
                //find an empty space in the swap file
                int i=0;
                while( i < (num_of_pages-(text_size/page_size))) {
                    if (swap_available[i]==0){
                        swap_available[i]=1;
                        break;
                    }
                    i++;
                }

                //change the pointer of the file
                if (lseek(swapfile_fd, i*page_size, SEEK_SET) == -1) {
                    printf("lseek");
                    return 1;
                }

                //write the data from the buffer
                ssize_t bytes_written = write(swapfile_fd, buffer, sizeof(buffer));
                if (bytes_written == -1) {
                    perror("write");
                    return 1;
                }
                page_table[find_type][find_page].swap_index=i;
                page_table[find_type][find_page].valid= false;
            }
            available_frame=next_frame_available();
        }

        //in case the page is dirty - bring the page from the swap file
        if (page_table[type][page].dirty){
            int swap_offset=page_table[type][page].swap_index*page_size;  //the swap index*page size is the index in the swap file
            if (lseek(swapfile_fd,swap_offset,SEEK_SET)==-1) {
                printf("Error moving the file offset");
                exit(1);
            }

            swap_available[swap_offset/page_size]=0;        //update this swap index to be valid

            //read from the swap file into a buffer
            char buffer[page_size];
            ssize_t bytesRead = read(swapfile_fd, buffer, sizeof(buffer));

            if (bytesRead == -1) {
                perror("Error reading from the file");
                return 1;
            }

            buffer[bytesRead] = '\0';
            int i=0;
            int j=available_frame*page_size;
            while (buffer[i]!='\0')
                main_memory[j++]=buffer[i++];
            mem_available[available_frame]=1;
            page_table[type][page].valid=true;
            page_table[type][page].frame = available_frame;
            page_table[type][page].swap_index=-1;
            timer_set(page,type);

            //"delete" the page from the swap that now in the main memory
            char buffer2[page_size];
            memset(buffer2,'0', sizeof(buffer2));
            if (lseek(swapfile_fd,swap_offset,SEEK_SET)==-1) {
                printf("Error moving the file offset");
                exit(1);
            }
            ssize_t bytesWrite = write(swapfile_fd, buffer2, sizeof(buffer2));
            if (bytesWrite == -1) {
                perror("Error writing to the file");
                return 1;
            }
        }

            //if this is a text page / data page (not dirty) / bss page (not dirty)  - bring it from the executable
        else if(type==TEXT_IND || type == DATA_IND || type == BSS_IND ){

            //if this is a text page and the operation is store - not legal
            if (is_store == 1 && type==TEXT_IND){
                free(address_arr);
                printf("ERR\n");
                return '\0';
            }
            if (type==DATA_IND)
                page_index+=text_size;
            if (type==BSS_IND)
                page_index+=(text_size+data_size);

            //change the pointer to the file
            if (lseek(program_fd,page_index,SEEK_SET)==-1) {
                printf("Error moving the file offset");
                exit(1);
            }

            //read from the file into the buffer
            char buffer[page_size];
            ssize_t bytesRead = read(program_fd, buffer, sizeof(buffer));

            if (bytesRead == -1) {
                perror("Error reading from the file");
                return 1;
            }

            buffer[bytesRead] = '\0';
            //copy the buffer data into the main memory
            int i=0;
            int j=available_frame*page_size;
            while (buffer[i]!='\0')
                main_memory[j++]=buffer[i++];

            mem_available[available_frame]=1;
            page_table[type][page].valid=true;
            page_table[type][page].frame = available_frame;
            timer_set(page,type);

        }
            //if this is a heap_stack page
        else{
            //if this page is not valid, and this is a load operation - this is not legal !
            if (!page_table[type][page].valid && is_store==0){
                free(address_arr);
                printf("ERR\n");
                return '\0';
            }

            else{
                //if this is a new heap_stack page - insert zeros into the main memory
                int i=available_frame*page_size;
                while(i<page_size) {
                    main_memory[i++] = '0';
                }
                mem_available[available_frame]=1;
                page_table[type][page].valid=true;
                page_table[type][page].frame = available_frame;
                timer_set(page,type);
            }
        }
    }
    if (is_store == 1){
        main_memory[(available_frame*page_size) + offset]=c;
        page_table[type][page].dirty=true;
    }
    free(address_arr);
    return main_memory[(available_frame*page_size) + offset];
}

//this function calculate the power of two of a given num - for the logical address translation
int sim_mem::power_of_two(int num) {
    if (num==1)
        return 0;
    int degree=1;
    int temp=2;
    while(true){
        if (temp >= num)
            return degree;
        degree++;
        temp = (int)pow(2,degree);
    }
}

//this function get a binary number (int an int array) and turn it into a decimal integer
int sim_mem::binary_to_decimal(const int *binaryArray, int size) {
    int decimalNumber = 0;
    int power = 0;
    for (int i = size - 1; i >= 0; i--) {
        decimalNumber += binaryArray[i] * (int)pow(2, power);
        power++;
    }
    return decimalNumber;
}

/*
 *the address_calc function translate logical address into a physical address, the return value is an array that will contain this 3 :
 *1. the type of page index (0 - text, 1 - data, 2 - bss, 3 - heapStack) --> arr[0]
 *2. the page number  --> arr[1]
 *3. the offset  --> arr[2]
 */
int *sim_mem::address_calc(int address) {
    int* arr = (int*) malloc(sizeof(int)*3);
    //converting the address to binary
    int binary_address[ADDRESS_SIZE];
    int index = 0;
    int temp_address=address;
    while (temp_address > 0) {
        binary_address[index] = temp_address % 2;
        temp_address /= 2;
        index++;
    }
    while (index<ADDRESS_SIZE)
        binary_address[index++]=0;

    //calculate the offset
    int offset_size = power_of_two(this->page_size);
    int offset[offset_size];
    int i=offset_size-1;
    int j=0;
    while (i>=0)
        offset[i--]=binary_address[j++];
    arr[2]= binary_to_decimal(offset,offset_size);

    //calculate the page number
    int page_len_in_address = ADDRESS_SIZE - offset_size - PAGE_TYPE_SIZE;
    int page[page_len_in_address];
    i=page_len_in_address-1;
    j=offset_size;
    while (i>=0)
        page[i--]=binary_address[j++];
    arr[1]= binary_to_decimal(page,page_len_in_address);

    //calculate the page's type
    int outer[PAGE_TYPE_SIZE];
    i=0;
    j=ADDRESS_SIZE-1;
    while(i < PAGE_TYPE_SIZE)
        outer[i++]=binary_address[j--];
    arr[0]= binary_to_decimal(outer, PAGE_TYPE_SIZE);

    return arr;
}

/*this function run over the mem_available number and the output will be the first available frame,
 * if there is no available frame - the output will be -1
 */
int sim_mem::next_frame_available() {
    int i;
    for (i = 0; i < this->frame_number ; ++i) {
        if (mem_available[i]==0){
            return i;
        }
    }
    return -1;
}

/*this function runs over all the available pages and update their timer
 * the page's timer with the same page and type like the arguments will update to memory_size - this page is the one we use right now
 * all the timers of the other pages will be decreased by one
 * The timer will help us "decide" which frame to take off from the main memory according to the method of LRU
 */
void sim_mem::timer_set(int page, int type) {
    for (int j = 0; j < text_size/page_size ; ++j) {
        if (j==page && TEXT_IND==type)
            page_table[TEXT_IND][j].timer=MEMORY_SIZE;
        else
            page_table[TEXT_IND][j].timer --;
    }
    for (int j = 0; j < data_size/page_size ; ++j) {
        if (j==page && DATA_IND==type)
            page_table[DATA_IND][j].timer=MEMORY_SIZE;
        else
            page_table[DATA_IND][j].timer --;
    }
    for (int j = 0; j < bss_size/page_size ; ++j) {
        if (j==page && BSS_IND==type)
            page_table[BSS_IND][j].timer=MEMORY_SIZE;
        else
            page_table[BSS_IND][j].timer --;
    }
    for (int j = 0; j < heap_stack_size/page_size ; ++j) {
        if (j==page && STACK_HEAP_IND==type)
            page_table[STACK_HEAP_IND][j].timer=MEMORY_SIZE;
        else
            page_table[STACK_HEAP_IND][j].timer --;
    }
}
/*this function will help with tha LRU algorithm - choosing a frame from the main memory to replace it with a new page
 * this function running over all the available pages (that are in the main memory) and find the page with the minimum value
 * of timer - this page is the page that has not been used for the longest time, so we would like to remove it from the main memory
 */
int sim_mem::frame_out() {
    int min= MEMORY_SIZE;
    int save_frame=-1;
    for (int j = 0; j < text_size/page_size ; ++j) {
        if (page_table[TEXT_IND][j].timer<min && page_table[TEXT_IND][j].valid){
            min=page_table[TEXT_IND][j].timer;
            save_frame=page_table[TEXT_IND][j].frame;
        }
    }
    for (int j = 0; j < data_size/page_size ; ++j) {
        if (page_table[DATA_IND][j].timer<min && page_table[DATA_IND][j].valid){
            min=page_table[DATA_IND][j].timer;
            save_frame=page_table[DATA_IND][j].frame;
        }
    }
    for (int j = 0; j < bss_size/page_size ; ++j) {
        if (page_table[BSS_IND][j].timer<min && page_table[BSS_IND][j].valid){
            min=page_table[BSS_IND][j].timer;
            save_frame=page_table[BSS_IND][j].frame;
        }
    }
    for (int j = 0; j < heap_stack_size/page_size ; ++j) {
        if (page_table[STACK_HEAP_IND][j].timer<min && page_table[STACK_HEAP_IND][j].valid){
            min=page_table[STACK_HEAP_IND][j].timer;
            save_frame=page_table[STACK_HEAP_IND][j].frame;
        }
    }
    return save_frame;
}

void sim_mem::print_memory() {
    int i;
    printf("\n Physical memory\n");
    for (i = 0; i < MEMORY_SIZE ; i++) {
        printf("[%c]\n",main_memory[i]);
    }
}


void sim_mem::print_swap()  {
    char* str = (char*)(malloc(this->page_size * sizeof(char)));
    int i;
    printf("\n Swap m"
           ""
           "emory\n");
    lseek(swapfile_fd,0,SEEK_SET);      //goto the start of the file
    while (read(swapfile_fd ,str, this->page_size) == this->page_size){
        for (i = 0; i < page_size; ++i) {
            printf("%d - [%c]\t",i,str[i]);
        }
        printf("\n");
    }
    free(str);
}

void sim_mem::print_page_table() {
    int i;

    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for ( i = 0; i < text_pages ; ++i) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[0][i].valid,
               page_table[0][i].dirty,
               page_table[0][i].frame,
               page_table[0][i].swap_index);
    }

    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for ( i = 0; i < data_pages ; ++i) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[1][i].valid,
               page_table[1][i].dirty,
               page_table[1][i].frame,
               page_table[1][i].swap_index);
    }

    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for ( i = 0; i < bss_pages ; ++i) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[2][i].valid,
               page_table[2][i].dirty,
               page_table[2][i].frame,
               page_table[2][i].swap_index);
    }

    printf("Valid\t Dirty\t Frame\t Swap index\n");
    for ( i = 0; i < heap_stack_pages ; ++i) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\n",
               page_table[3][i].valid,
               page_table[3][i].dirty,
               page_table[3][i].frame,
               page_table[3][i].swap_index);
    }
}
