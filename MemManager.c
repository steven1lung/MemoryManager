#include "MemManager.h"

char tlb_policy[10];
char page_policy[10];
char frame_policy[10];

int n_process;
int n_vpage;
int time_q=0;
int n_pframe;
int fifo_time=0;
int block_id=0;

int* physical_frame;

Node* free_frame;
Block* disk;
PageTable** page_table;
TLB tlb[32];
FILE* trace;

int main()
{

    parse_config();
    construct_page_table();
    trace = fopen("trace_output.txt","w");
    start();

    fclose(trace);
    return 0;
}

void start()
{
    char tmp[20];
    char process_id[2];
    char page_id_tmp[2];
    char prev_pid='Z';
    int page_index;


    int debug=0;

    FILE *fp=fopen("trace.txt","r");
    while(fscanf(fp," %[^(] ( %[^,] , %[^)]",tmp,process_id,page_id_tmp)!=EOF)
    {
        if(process_id[0]+process_id[1]==0) break;

        page_index=atoi(page_id_tmp);

        if(prev_pid!=process_id[0]) flush_tlb();
        int tlb_ret=tlb_check(page_index);


        printf("iter: %d process : %s virtual: %d\n",time_q,process_id,page_index);
        if(debug) getchar();
        // if(time_q>100) debug=1;
        // fprintf(trace,"%d ",time_q);
        if(tlb_ret==-1)
        {
            //TLB miss
            // printf("TLB miss\n");
            int page_ret=table_check(process_id,page_index);
            if(page_ret==-1)
            {
                //page fault
                // printf("page fault\n");
                fprintf(trace,"Process %s, TLB Miss, Page Fault, ",process_id);
                page_ret=Page_replace(process_id,page_index);

            }
            else
            {
                // printf("page hit\n");
                // Page hit
                fprintf(trace,"Process %s, TLB Miss, Page Hit, %d=>%d\n",process_id,page_index,page_ret);

            }
            TLB_replace(page_index,page_ret);
            fprintf(trace,"Process %s, TLB Hit, %d=>%d\n",process_id,page_index,tlb_check(page_index));
            page_table[process_id[0]-'A'][page_index].reference=1;
            page_table[process_id[0]-'A'][page_index].ref_time=time_q;
        }
        else
        {
            //TLB hit
            // printf("TLB hit %d in %s\n",page_index,process_id);
            fprintf(trace,"Process %s, TLB Hit, %d=>%d\n",process_id,page_index,tlb_ret);
            page_table[process_id[0]-'A'][page_index].reference=1;
            page_table[process_id[0]-'A'][page_index].ref_time=time_q;
        }

        prev_pid=process_id[0];


        // print_tlb();
        // print_page_table();
        // print_disk();

        time_q++;
        memset(tmp,0,20);
        memset(process_id,0,2);
        memset(page_id_tmp,0,2);
    }
    fclose(fp);
}

void print_page_table()
{
    printf("page table:\n");
    for(int i=0; i<n_vpage; i++)
    {
        printf("%d %d %d \t\t", i, page_table[0][i].PFN_DBI, page_table[0][i].present);
        printf("%d %d %d \n", i, page_table[1][i].PFN_DBI, page_table[1][i].present);
    }
}

void print_disk()
{
    printf("disk num%d:\n",block_id);
    // for(int i=0;i<block_id;i++)
    //     for(int j=0;j<n_process*n_vpage;j++){
    //         if(disk[j].blockid==i )
    //         printf("blockID: %d PID: %c Vir: %d Empty: %d\n", disk[j].blockid, disk[j].pid, disk[j].num, disk[j].empty);

    //     }
    // printf("\n\n");

    Block* tmp = disk;
    while(tmp!=NULL)
    {
        printf("blockID: %d PID: %c Vir: %d Empty: %d\n", tmp->blockid, tmp->pid, tmp->num, tmp->empty);
        tmp=tmp->next;
    }
}

void print_tlb()
{
    printf("TLB:\n");
    for(int i=0; i<32; i++)
    {
        printf("%d %d %d %d\n",i,tlb[i].VPN,tlb[i].PFN,tlb[i].ref_time);
    }

    printf("\n\n");


}


int Page_replace(const char* pid,const int virtual )
{
    if(free_frame!=NULL)
    {
        //there are still free frames
        int new_frame = free_frame->num;
        free_frame=free_frame->next;
        // printf("assign process %c page %d to %d\n",pid[0],virtual,new_frame);
        page_table[pid[0]-'A'][virtual].PFN_DBI=new_frame;
        page_table[pid[0]-'A'][virtual].present=1;
        page_table[pid[0]-'A'][virtual].reference=0;
        page_table[pid[0]-'A'][virtual].ref_time=time_q;
        page_table[pid[0]-'A'][virtual].enter_time=time_q;
        fprintf(trace,"%d, Evict -1 of Process %s to -1, %d<<-1\n",new_frame,pid,virtual);
        return new_frame;
    }
    else
    {
        // no more free frames
        if(!strcmp(frame_policy,"LOCAL"))
        {
            //local policy
            if(!strcmp(page_policy,"FIFO"))
            {
                //FIFO
                int victim=0;
                for(int i=1; i<n_vpage; ++i)
                {
                    if( page_table[pid[0]-'A'][i].present==1 && page_table[pid[0]-'A'][i].enter_time < page_table[pid[0]-'A'][victim].enter_time)
                    {
                        victim=i;
                    }
                }
                //put frame into the space that we found
                int physical_page=page_table[pid[0]-'A'][victim].PFN_DBI;
                page_table[pid[0]-'A'][virtual].enter_time=time_q;
                page_table[pid[0]-'A'][virtual].ref_time=time_q;
                page_table[pid[0]-'A'][virtual].reference=0;
                page_table[pid[0]-'A'][virtual].present=1;
                page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;

                //swap victim to disk
                int blockid=create_block(victim,pid[0]);
                page_table[pid[0]-'A'][victim].present=0;
                page_table[pid[0]-'A'][victim].PFN_DBI=blockid;
                fprintf(trace,"%d, Evict %d of Process %s to %d, %d<<%d\n",physical_page,victim,pid,blockid,virtual,-1);
                return physical_page;
            }
            else
            {
                //second chance
            }
        }
        else
        {

            //global policy
            if(!strcmp(page_policy,"FIFO"))
            {

                // print_tlb();
                //FIFO

                int victim_page=0;
                int victim_pid=0;
                for(int j=0; j<n_process; ++j)
                    for(int i=1; i<n_vpage; ++i)
                    {
                        if( page_table[j][i].present==1 && page_table[j][i].enter_time < page_table[victim_pid][victim_page].enter_time)
                        {
                            victim_pid=j;
                            victim_page=i;
                        }
                    }
                //check page table
                // printf("victim : %d of %c ",victim_page,victim_pid+'A');
                if(page_table[pid[0]-'A'][virtual].present==-1)
                {
                    //first time
                    // printf("first reference\n");
                    //put frame into the space that we found
                    int physical_page=page_table[victim_pid][victim_page].PFN_DBI;
                    page_table[pid[0]-'A'][virtual].enter_time=time_q;
                    page_table[pid[0]-'A'][virtual].ref_time=time_q;
                    page_table[pid[0]-'A'][virtual].reference=0;
                    page_table[pid[0]-'A'][virtual].present=1;
                    page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;

                    //swap victim to disk
                    int blockid=create_block(victim_page,victim_pid+'A');

                    // printf("store %c %d to disk %d\n",victim_pid+'A',victim_page,blockid);
                    // printf("store %c %d to physical %d\n",pid[0],virtual,physical_page);


                    page_table[victim_pid][victim_page].present=0;
                    page_table[victim_pid][victim_page].PFN_DBI=blockid;
                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim_page,victim_pid+'A',blockid,virtual,-1);

                    return physical_page;
                }
                else if(page_table[pid[0]-'A'][virtual].present==0)
                {

                    //go to disk
                    // printf("refernece before from disk %c %d\n",pid[0],virtual);
                    // fprintf(trace,"%c %d is in disk ",pid[0],virtual);
                    int prev_block = page_table[pid[0]-'A'][virtual].PFN_DBI;

                    int physical_page=page_table[victim_pid][victim_page].PFN_DBI;
                    page_table[pid[0]-'A'][virtual].enter_time=time_q;
                    page_table[pid[0]-'A'][virtual].ref_time=time_q;
                    page_table[pid[0]-'A'][virtual].reference=0;
                    page_table[pid[0]-'A'][virtual].present=1;
                    page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;

                    int blockid=create_block(victim_page,victim_pid+'A');
                    page_table[victim_pid][victim_page].present=0;
                    page_table[victim_pid][victim_page].PFN_DBI=blockid;
                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim_page,victim_pid+'A',blockid,virtual,prev_block);



                    // printf("chaasdasdnged block %c %d from %d to 1\n",pid[0],virtual,disk[(pid[0]-'A')*n_vpage+virtual].empty);
                    //change block id:  prev_block to 1
                    Block* tmp = disk;
                    while(tmp!=NULL)
                    {
                        if(tmp->blockid==prev_block)
                        {
                            tmp->empty=1;
                            break;
                        }
                        tmp=tmp->next;
                    }

                    return physical_page;
                }





            }
            else
            {
                //second chance
            }
        }
    }
}

void print_free_frames()
{
    Node* tmp = free_frame;
    while(tmp!=NULL)
    {
        printf("%d \n",tmp->num);
        tmp=tmp->next;
    }
}
//find the missing block number 15
int create_block(int num, char pid)
{

    Block* tmp = disk;
    while(tmp!=NULL)
    {
        if(tmp->empty==1) break;
        tmp=tmp->next;
    }

    if(tmp!=NULL)
    {
        tmp->num=num;
        tmp->pid=pid;
        tmp->empty=0;
        return tmp->blockid;
    }
    else
    {
        Block* new = (Block*)malloc(sizeof(Block));
        new->num=num;
        new->pid=pid;
        new->empty=0;
        new->next=NULL;
        new->blockid=block_id;

        if(disk==NULL) disk=new;
        else
        {
            tmp=disk;
            while(tmp->next!=NULL) tmp=tmp->next;
            tmp->next=new;
        }

        return block_id++;
    }


    // int check_block=-9;
    // int break_loop=false;
    // for(int i=0;i<block_id;++i)
    // {
    //     for(int j=0;j<n_process*n_vpage;j++){
    //         if( disk[j].empty==1 && disk[j].blockid==i){
    //             check_block=j;
    //             break_loop=true;
    //             break;
    //         }
    //     }
    //     if(break_loop==true) break;
    // }
    // // printf("after create block loop:\n");print_disk();
    // if(check_block==-9){
    //     int row = pid-'A';
    //     int col=num;
    //     printf("current %d %c %d index: %d\n", block_id,pid,num,row*n_vpage+col);
    //     printf("change %d %c %d\n",disk[row*n_vpage+col].blockid,disk[row*n_vpage+col].pid,disk[row*n_vpage+col].num);
    //     disk[row*n_vpage+col].blockid=block_id;
    //     disk[row*n_vpage+col].num=num;
    //     disk[row*n_vpage+col].pid=pid;
    //     disk[row*n_vpage+col].empty=0;
    //     block_id++;
    //     return block_id-1;
    // }
    // else{
    //     printf("found empty block %d\n",check_block);
    //     disk[check_block].num=num;
    //     disk[check_block].pid=pid;
    //     disk[check_block].empty=0;
    //     return disk[check_block].blockid;
    // }

}

void TLB_replace(int virtual,int physical)
{

    //check if TLB is full
    int full=-1;
    for(int i=0; i<32; ++i)
    {
        if(tlb[i].VPN==-1)
        {
            full=i;
            break;
        }
    }

    if(full==-1)
    {
        //TLB is full
        if(!strcmp(tlb_policy,"LRU"))
        {
            //LRU
            int victim=0;
            for(int i=0; i<31; ++i)
            {
                if(tlb[i].ref_time<tlb[victim].ref_time)
                {
                    victim=i;
                }
            }
            tlb[victim].PFN=physical;
            tlb[victim].VPN=virtual;
            tlb[victim].ref_time=time_q;

        }
        else
        {
            //random
            srand(time(NULL));
            int victim = rand()%32;
            tlb[victim].PFN=physical;
            tlb[victim].VPN=virtual;
            tlb[victim].ref_time=time_q;
        }
    }
    else
    {
        //there is some space in TLB
        tlb[full].PFN=physical;
        tlb[full].VPN=virtual;
        tlb[full].ref_time=time_q;
    }



}

int table_check(const char* pid,const int index)
{

    if(page_table[pid[0]-'A'][index].present==1)
    {
        int ret =  page_table[pid[0]-'A'][index].PFN_DBI;
        return ret;
    }
    else return -1;

}

int tlb_check(const int index)
{
    for(int i=0; i<32; i++)
    {
        if(tlb[i].VPN==index)
        {
            tlb[i].ref_time=time_q;
            return tlb[i].PFN;
        }
    }
    return -1;
}

void parse_config()
{
    FILE *fp;
    char tmp[50];
    int w;
    char n_proc[10];
    char n_v[10];
    char n_p[10];
    fp=fopen("ans/sys_config.txt","r");
    w=fscanf(fp," %[^:] : %[^\n]",tmp,tlb_policy);
    w=fscanf(fp," %[^:] : %[^\n]",tmp,page_policy);
    w=fscanf(fp," %[^:] : %[^\n]",tmp,frame_policy);
    w=fscanf(fp," %[^:] : %[^\n]",tmp,n_proc);
    w=fscanf(fp," %[^:] : %[^\n]",tmp,n_v);
    w=fscanf(fp," %[^:] : %[^\n]",tmp,n_p);

    n_process=atoi(n_proc);
    n_vpage=atoi(n_v);
    n_pframe=atoi(n_p);
    printf("TLB Replacement Policy: %s\n",tlb_policy);
    printf("Page Replacement Policy: %s\n",page_policy);
    printf("Frame Allocation Policy: %s\n",frame_policy);
    printf("Number of Processes: %d\n",n_process);
    printf("Number of Virtual Page: %d\n",n_vpage);
    printf("Number of Physical Frame: %d\n",n_pframe);

    fclose(fp);
}

void construct_page_table()
{
    //initial TLB entries to -1
    memset(tlb,-1,sizeof(TLB)*32);

    //initial physical frame to -1
    physical_frame=(int*)malloc(sizeof(int)*n_pframe);
    memset(physical_frame,-1,n_pframe);

    //initial free frame list
    free_frame=(Node*)malloc(sizeof(Node));
    free_frame->num=0;
    free_frame->next=NULL;
    Node* last = free_frame;
    for(int i=1; i<n_pframe; ++i)
    {
        Node* tmp = (Node*)malloc(sizeof(Node));
        tmp->num=i;
        tmp->next=NULL;
        last->next=tmp;
        last=last->next;
    }

    //initial page table
    page_table = (PageTable**)malloc(sizeof(PageTable*)*n_process);
    for(int i=0; i<n_process; ++i)
    {
        page_table[i]=(PageTable*)malloc(sizeof(PageTable)*n_vpage);
        memset(page_table[i],-1,sizeof(PageTable)*n_vpage);
        for(int j=0; j<n_vpage; ++j)
        {
            page_table[i][j].enter_time=INT_MAX;
        }
    }

    //initial disk
    disk=NULL;
    // disk=(Block*)malloc(sizeof(Block)*n_process*n_vpage);
    // memset(disk,-1,sizeof(Block)*n_process*n_vpage);
}



void flush_tlb()
{
    memset(tlb,-1,sizeof(TLB)*32);
}