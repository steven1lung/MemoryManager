#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <limits.h>

typedef struct TLB
{
    char pid;
    int VPN; //virtual frame number
    int PFN; //physical frame number
    int ref_time;
} TLB;

typedef struct node
{
    int num;
    struct node* next;
} Node;

typedef struct block
{
    int num;
    int empty;
    int blockid;
    char pid;
    struct block* next;
} Block;

typedef struct PageTable
{
    int PFN_DBI; //virtual page number and disk block number
    bool reference;
    int present;
    int ref_time;
    int enter_time;
} PageTable;

void parse_config();
void initial_all();
int tlb_check(const int index);
int table_check(const char* pid,const int index);
void flush_tlb();
void TLB_replace(int virtual,int physical,char pid);
void start();
int Page_replace(const char* pid,const int virtual );
int create_block(int num, char pid);
void print_free_frames();
void print_page_table();
void print_disk();
void tlb_clear(char pid, int page);