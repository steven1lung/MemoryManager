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
int global_clock=0;
int* local_clock=0;
int* page_fault;
int* tlb_hit;
int* tlb_lookup;
int* total_mem;
physical_frame_t* physical_frame;

Node* free_frame;
Block* disk;
PageTable** page_table;
TLB tlb[32];
FILE* trace;


int main()
{

    parse_config();
    initial_all();
    trace = fopen("trace_output.txt","w");
    start();
    fclose(trace);
    analysize();

    return 0;
}

void start()
{
    char tmp[20];
    char process_id[2];
    char page_id_tmp[2];
    char prev_pid='Z';
    int page_index;


    int debug __attribute__ ((unused)) = 0;

    FILE *fp=fopen("trace.txt","r");
    while(fscanf(fp," %[^(] ( %[^,] , %[^)]",tmp,process_id,page_id_tmp)!=EOF)
    {
        if(process_id[0]+process_id[1]==0) break;

        page_index=atoi(page_id_tmp);

        total_mem[process_id[0]-'A']++;
        tlb_lookup[process_id[0]-'A']++;
        if(prev_pid!=process_id[0]) flush_tlb();
        int tlb_ret=tlb_check(page_index);





        if(debug)
        {
            print_tlb();
            print_disk();
            print_page_table();
            print_pframe();
            getchar();
        }
        // if(time_q>100) debug=1;

        if(tlb_ret==-1)
        {
            //TLB miss
            int page_ret=table_check(process_id,page_index);
            if(page_ret==-1)
            {
                //page fault
                page_fault[process_id[0]-'A']++;
                fprintf(trace,"Process %s, TLB Miss, Page Fault, ",process_id);
                page_ret=Page_replace(process_id,page_index);
                if(page_ret==-999)
                {
                    fprintf(stderr,"error in page replacement\n");
                    exit(EXIT_FAILURE);
                }

            }
            else
            {
                // Page hit
                fprintf(trace,"Process %s, TLB Miss, Page Hit, %d=>%d\n",process_id,page_index,page_ret);

            }
            TLB_replace(page_index,page_ret,process_id[0]);
            tlb_lookup[process_id[0]-'A']++;
            tlb_hit[process_id[0]-'A']++;
            fprintf(trace,"Process %s, TLB Hit, %d=>%d\n",process_id,page_index,tlb_check(page_index));
            page_table[process_id[0]-'A'][page_index].reference=1;
            page_table[process_id[0]-'A'][page_index].ref_time=time_q;
        }
        else
        {
            //TLB hit
            tlb_hit[process_id[0]-'A']++;
            fprintf(trace,"Process %s, TLB Hit, %d=>%d\n",process_id,page_index,tlb_ret);
            page_table[process_id[0]-'A'][page_index].reference=1;
            page_table[process_id[0]-'A'][page_index].ref_time=time_q;
        }

        prev_pid=process_id[0];




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
        printf("%d %d %d %d\t\t", i, page_table[0][i].PFN_DBI, page_table[0][i].present, page_table[0][i].reference);
        printf("%d %d %d %d\n", i, page_table[1][i].PFN_DBI, page_table[1][i].present, page_table[0][i].reference);
    }
}

void print_disk()
{
    printf("disk num%d:\n",block_id);
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
    for(int i=0; i<16; i+=2)
    {
        printf("%d %d %d %d\t\t",i,tlb[i].VPN,tlb[i].PFN,tlb[i].ref_time);
        printf("%d %d %d %d\n",i+1,tlb[i+1].VPN,tlb[i+1].PFN,tlb[i+1].ref_time);
    }

    printf("\n\n");


}

void tlb_clear(char pid, int page)
{
    //check if pid page is in TLB
    //if yes clear the VPN
    for(int i=0; i<32; i++)
    {
        if(tlb[i].pid==pid && tlb[i].VPN==page)
        {
            tlb[i].VPN=-1;
            return;
        }
    }
}

void print_pframe()
{
    printf("physical frame : pointer at %d\n",global_clock);
    for(int i=0; i<n_pframe; i+=2)
    {
        printf("%d %c %d\t\t",i,physical_frame[i].pid,physical_frame[i].page);
        printf("%d %c %d\n",i+1,physical_frame[i+1].pid,physical_frame[i+1].page);
    }
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

        //assign the pid and page no. to the physical frame array
        physical_frame[new_frame].page=virtual;
        physical_frame[new_frame].pid=pid[0];

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

                //check page table
                if(page_table[pid[0]-'A'][virtual].present==-1)
                {
                    //first time

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

                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim,pid[0],blockid,virtual,-1);

                    tlb_clear(pid[0],victim);

                    return physical_page;
                }
                else if(page_table[pid[0]-'A'][virtual].present==0)
                {
                    //go to disk
                    int prev_block = page_table[pid[0]-'A'][virtual].PFN_DBI;

                    int physical_page=page_table[pid[0]-'A'][victim].PFN_DBI;
                    page_table[pid[0]-'A'][virtual].enter_time=time_q;
                    page_table[pid[0]-'A'][virtual].ref_time=time_q;
                    page_table[pid[0]-'A'][virtual].reference=0;
                    page_table[pid[0]-'A'][virtual].present=1;
                    page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;

                    int blockid=create_block(victim,pid[0]);
                    page_table[pid[0]-'A'][victim].present=0;
                    page_table[pid[0]-'A'][victim].PFN_DBI=blockid;
                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim,pid[0],blockid,virtual,prev_block);


                    //change block id:  prev_block to empty
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
                    tlb_clear(pid[0],victim);
                    return physical_page;
                }
            }
            else
            {
                //second chance

                int victim_pid=0;
                int victim_page=0;
                int frame_pointer=0;
                //choose victim age
                while(1)
                {

                    if(physical_frame[local_clock[pid[0]-'A']].pid!=pid[0])
                    {
                        if(++local_clock[pid[0]-'A']==n_pframe) local_clock[pid[0]-'A']=0;
                        continue;
                    }

                    if(page_table[physical_frame[local_clock[pid[0]-'A']].pid-'A'][physical_frame[local_clock[pid[0]-'A']].page].reference==1)
                    {
                        page_table[physical_frame[local_clock[pid[0]-'A']].pid-'A'][physical_frame[local_clock[pid[0]-'A']].page].reference=0;
                    }
                    else if(page_table[physical_frame[local_clock[pid[0]-'A']].pid-'A'][physical_frame[local_clock[pid[0]-'A']].page].reference==0)
                    {

                        victim_page=physical_frame[local_clock[pid[0]-'A']].page;
                        victim_pid=physical_frame[local_clock[pid[0]-'A']].pid-'A';

                        frame_pointer=local_clock[pid[0]-'A'];

                        if(++local_clock[pid[0]-'A']==n_pframe) local_clock[pid[0]-'A']=0;

                        break;
                    }


                    if(++local_clock[pid[0]-'A']==n_pframe) local_clock[pid[0]-'A']=0;
                }

                // assign pid and pagae to victim
                if(page_table[pid[0]-'A'][virtual].present==-1)
                {
                    //first reference
                    int physical_page =page_table[victim_pid][victim_page].PFN_DBI;
                    page_table[pid[0]-'A'][virtual].enter_time=time_q;
                    page_table[pid[0]-'A'][virtual].ref_time=time_q;
                    page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;
                    page_table[pid[0]-'A'][virtual].present=1;
                    page_table[pid[0]-'A'][virtual].reference=0;


                    //swap victim to disk
                    int blockid = create_block(victim_page,victim_pid+'A');

                    page_table[victim_pid][victim_page].present=0;
                    page_table[victim_pid][victim_page].PFN_DBI=blockid;

                    //change physical frame
                    physical_frame[frame_pointer].page=virtual;
                    physical_frame[frame_pointer].pid=pid[0];

                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim_page,victim_pid+'A',blockid,virtual,-1);

                    tlb_clear(victim_pid+'A',victim_page);

                    return physical_page;

                }
                else if(page_table[pid[0]-'A'][virtual].present==0)
                {
                    //go to disk

                    //get disk block id
                    int prev_block=page_table[pid[0]-'A'][virtual].PFN_DBI;

                    //assign virtual page to the selected victim frame
                    int physical_page =page_table[victim_pid][victim_page].PFN_DBI;
                    page_table[pid[0]-'A'][virtual].enter_time=time_q;
                    page_table[pid[0]-'A'][virtual].ref_time=time_q;
                    page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;
                    page_table[pid[0]-'A'][virtual].present=1;
                    page_table[pid[0]-'A'][virtual].reference=0;

                    //swap victim to disk
                    int blockid=create_block(victim_page,victim_pid+'A');
                    page_table[victim_pid][victim_page].present=0;
                    page_table[victim_pid][victim_page].PFN_DBI=blockid;
                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim_page,victim_pid+'A',blockid,virtual,prev_block);

                    //assign physical frame
                    physical_frame[frame_pointer].page=virtual;
                    physical_frame[frame_pointer].pid=pid[0];

                    tlb_clear(victim_pid+'A',victim_page);

                    //set block that swapped in to empty
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
        }
        else
        {
            //global policy
            if(!strcmp(page_policy,"FIFO"))
            {
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
                if(page_table[pid[0]-'A'][virtual].present==-1)
                {
                    //first reference

                    //put frame into the space that we found
                    int physical_page=page_table[victim_pid][victim_page].PFN_DBI;
                    page_table[pid[0]-'A'][virtual].enter_time=time_q;
                    page_table[pid[0]-'A'][virtual].ref_time=time_q;
                    page_table[pid[0]-'A'][virtual].reference=0;
                    page_table[pid[0]-'A'][virtual].present=1;
                    page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;

                    //swap victim to disk
                    int blockid=create_block(victim_page,victim_pid+'A');
                    page_table[victim_pid][victim_page].present=0;
                    page_table[victim_pid][victim_page].PFN_DBI=blockid;

                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim_page,victim_pid+'A',blockid,virtual,-1);

                    tlb_clear(victim_pid+'A',victim_page);

                    return physical_page;
                }
                else if(page_table[pid[0]-'A'][virtual].present==0)
                {
                    //find virtual page in disk block to swap in
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

                    //change block id:  prev_block to empty
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

                    tlb_clear(victim_pid+'A',victim_page);
                    return physical_page;
                }
            }
            else
            {
                //second chance

                int victim_pid=0;
                int victim_page=0;
                int frame_pointer=0;
                //choose victim page
                while(1)
                {
                    if(page_table[physical_frame[global_clock].pid-'A'][physical_frame[global_clock].page].reference==1)
                    {
                        page_table[physical_frame[global_clock].pid-'A'][physical_frame[global_clock].page].reference=0;
                    }
                    else if(page_table[physical_frame[global_clock].pid-'A'][physical_frame[global_clock].page].reference==0)
                    {

                        victim_page=physical_frame[global_clock].page;
                        victim_pid=physical_frame[global_clock].pid-'A';

                        frame_pointer=global_clock;

                        if(++global_clock==n_pframe) global_clock=0;

                        break;
                    }


                    if(++global_clock==n_pframe) global_clock=0;
                }

                // assign pid and pagae to victim
                if(page_table[pid[0]-'A'][virtual].present==-1)
                {
                    //first reference
                    int physical_page =page_table[victim_pid][victim_page].PFN_DBI;
                    page_table[pid[0]-'A'][virtual].enter_time=time_q;
                    page_table[pid[0]-'A'][virtual].ref_time=time_q;
                    page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;
                    page_table[pid[0]-'A'][virtual].present=1;
                    page_table[pid[0]-'A'][virtual].reference=0;

                    //swap victim to disk
                    int blockid = create_block(victim_page,victim_pid+'A');

                    page_table[victim_pid][victim_page].present=0;
                    page_table[victim_pid][victim_page].PFN_DBI=blockid;

                    //change physical frame
                    physical_frame[frame_pointer].page=virtual;
                    physical_frame[frame_pointer].pid=pid[0];

                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim_page,victim_pid+'A',blockid,virtual,-1);

                    tlb_clear(victim_pid+'A',victim_page);

                    return physical_page;

                }
                else if(page_table[pid[0]-'A'][virtual].present==0)
                {
                    //go to disk

                    //get disk block id
                    int prev_block=page_table[pid[0]-'A'][virtual].PFN_DBI;

                    //assign virtual page to the selected victim frame
                    int physical_page =page_table[victim_pid][victim_page].PFN_DBI;
                    page_table[pid[0]-'A'][virtual].enter_time=time_q;
                    page_table[pid[0]-'A'][virtual].ref_time=time_q;
                    page_table[pid[0]-'A'][virtual].PFN_DBI=physical_page;
                    page_table[pid[0]-'A'][virtual].present=1;
                    page_table[pid[0]-'A'][virtual].reference=0;

                    //swap victim to disk
                    int blockid=create_block(victim_page,victim_pid+'A');
                    page_table[victim_pid][victim_page].present=0;
                    page_table[victim_pid][victim_page].PFN_DBI=blockid;
                    fprintf(trace,"%d, Evict %d of Process %c to %d, %d<<%d\n",physical_page,victim_page,victim_pid+'A',blockid,virtual,prev_block);

                    //assign physical frame
                    physical_frame[frame_pointer].page=virtual;
                    physical_frame[frame_pointer].pid=pid[0];

                    tlb_clear(victim_pid+'A',victim_page);

                    //set block that swapped in to empty
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
        }
    }


    //when return -999 means something is wrong QQ
    return -999;
}

void analysize()
{
    FILE* out=fopen("analysis.txt","w");

    double m =100;
    double t = 20;

    for(int i=0; i<n_process; i++)
    {
        double eat=0;
        double rate=0;
        double hit_rate=0;
        //caclulate the results
        rate = (double)(page_fault[i])/(double)(total_mem[i]);
        hit_rate = (double)(tlb_hit[i])/(double)(tlb_lookup[i]);
        eat = hit_rate*(m+t)+(1-hit_rate)*(2*m+t);

        fprintf(out,"Process %c, Effective Access Time = %.3lf\n",i+'A',eat);
        if(i==n_process-1) fprintf(out,"Process %c, Page Fault Rate: %.3lf",i+'A',rate);
        else fprintf(out,"Process %c, Page Fault Rate: %.3lf\n",i+'A',rate);
    }
    fclose(out);
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

}

void TLB_replace(int virtual,int physical,char pid)
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
            tlb[victim].pid=pid;

        }
        else
        {
            //random
            srand(time(NULL));
            int victim = rand()%32;
            tlb[victim].PFN=physical;
            tlb[victim].VPN=virtual;
            tlb[victim].pid=pid;
            tlb[victim].ref_time=time_q;
        }
    }
    else
    {
        //there is some space in TLB
        tlb[full].PFN=physical;
        tlb[full].VPN=virtual;
        tlb[full].ref_time=time_q;
        tlb[full].pid=pid;
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
    int w __attribute__((unused));
    char n_proc[10];
    char n_v[10];
    char n_p[10];
    fp=fopen("sys_config.txt","r");
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

void initial_all()
{
    //initial TLB entries to -1
    memset(tlb,-1,sizeof(TLB)*32);

    //initial physical frame to -1
    physical_frame=(physical_frame_t*)malloc(sizeof(physical_frame_t)*n_pframe);
    memset(physical_frame,-1,sizeof(physical_frame_t)*n_pframe);
    global_clock=0;

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
            page_table[i][j].reference=0;
        }
    }

    //initial disk
    disk=NULL;

    //initial clock pointer
    global_clock=0;
    local_clock=(int*)malloc(sizeof(int)*n_process);
    memset(local_clock,0,sizeof(int)*n_process);

    //initialize page fault counter
    page_fault = (int*)malloc(sizeof(int)*n_process);
    memset(page_fault,0,sizeof(int)*n_process);

    //initialize total mem access counter
    total_mem = (int*)malloc(sizeof(int)*n_process);
    memset(total_mem,0,sizeof(int)*n_process);

    //iniitialize tlb hit counter
    tlb_hit = (int*)malloc(sizeof(int)*n_process);
    memset(tlb_hit,0,sizeof(int)*n_process);

    //iniitialize tlb lookup counter
    tlb_lookup = (int*)malloc(sizeof(int)*n_process);
    memset(tlb_lookup,0,sizeof(int)*n_process);

}



void flush_tlb()
{
    memset(tlb,-1,sizeof(TLB)*32);
}