#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <string.h>
#include <unistd.h>




int buffer_available = 1;

struct item{
    int number;
    int sleep_time;
};

struct item buffer[32];

// keeps track of where the producer inserts new items
int p_iterator = 0;


// function for the producer to add items to buffer array
void add_item(struct item add_item)
{
    
    buffer[p_iterator] = add_item;
    p_iterator++;
}


// function for the consumer to remove the first item in the buffer array
struct item remove_item()
{
    struct item ret_item = buffer[0];
    int i;
    for(i = 0; i<p_iterator-1; i++){
        buffer[i] = buffer[i+1];
    }
    p_iterator--;
    return ret_item;
}



// generates a random number. when range is 1, it means that there is a range specified (e.g. when the consumer sleeps from 2 to 9 seconds)
// low and high can just be zero when range is not 1
int random_num_gen( int range, int low, int high){
    int x;
    srand(time(NULL));
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    
    char vendor[13];
    
    eax = 0x01;
    
    __asm__ __volatile__(
                         "cpuid;"
                         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(eax)
                         );
    
    if(ecx & 0x40000000){
        // use rdrand
        if(range == 1){
             x = low+ rand() % (high - low);
        }else{
            x = 0+ rand() % (100000);
        }
    }
    else{
        //use mt19937
        if(range == 1){
            x = low+ rand() % (high - low);
        }else{
            x = 0+ rand() % (100000);
        }
    }
    
    return x;
}



void consumer()
{
    //int x = 100;
    while(1){
        if(buffer_available == 1){
            
            if(p_iterator>0){
                buffer_available = 0;
                struct item new_item = remove_item();
                //printf("iterator: %d\n", p_iterator);
                buffer_available = 1;
                //printf("sleep time: %d\n",new_item.sleep_time );
                sleep(new_item.sleep_time);
                printf("%d\n",new_item.number);
            }
        }
       // x--;
    }
    
}

void producer()
{
    //int x = 100;
    while(1){
        if(buffer_available == 1){
            
            if(p_iterator<=31){
                buffer_available = 0;
                struct item new_item;
                new_item.number = random_num_gen(0,0,0);
                new_item.sleep_time = random_num_gen(1, 2, 9);
                add_item(new_item);
                buffer_available = 1;
                int r = random_num_gen(1, 3, 7);
                sleep(r);
            }
        }
       // x--;
    }
}


// function that is called when a new thread is created
void* thread(void* arg)
{
    //printf("%s",arg);
    //printf("thread\n");
    char* string = (char*)arg;
    if(strcmp("producer", string)==0){
        //printf("producer\n");
        producer();
    }
    else{
        if(strcmp("consumer", string)==0){
            //printf("consumer\n");
            consumer();
        }
        else{
            printf("Incorrect input\n");
        }
    }
    pthread_exit(0);
}



int main(int argc, char **argv)
{
    
        
    if (argc < 3) {
        printf("Need one producer and at least one consumer. Input should be \"producer consumer consumer...\"");
        exit(-1);
    }
    
    int num_args = argc - 1;
    
   // struct sum_runner_struct args[num_args];
    
    char* args[num_args];
    pthread_t tids[num_args];
    
   // pthread_t tids[num_args];
    int i;
    for (i = 0; i < num_args; i++) {
       // printf("args: %s", argv[i+1]);
        args[i] = argv[i + 1];
       // printf("args[i]: %s", args[i]);
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&tids[i], &attr, thread, args[i]);
    }
    
    for (i = 0; i < num_args; i++) {
        pthread_join(tids[i], NULL);
        
    }
   
}
