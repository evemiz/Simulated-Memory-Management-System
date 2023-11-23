#include "sim_mem.h"

int main() {
    sim_mem s((char*)"exec_file",(char*)"swap_file",128,128,64,64,64);
    s.store(1024,'*');
    s.store(1088,'!');
    s.load(0);
    s.load(64);
    s.load(2048);
    s.store(3072,'%');
    s.store(1025,'%');
    s.print_swap();
    s.print_page_table();
    s.print_memory();
    return 0;
}
