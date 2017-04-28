#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <string.h>
#include <unistd.h>
//#include "mt19937ar.c"



int buffer_available = 1;


/*
 A C-program for MT19937, with initialization improved 2002/1/26.
 Coded by Takuji Nishimura and Makoto Matsumoto.
 
 Before using, initialize the state by using init_genrand(seed)
 or init_by_array(init_key, key_length).
 
 Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 
 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 3. The names of its contributors may not be used to endorse or promote
 products derived from this software without specific prior written
 permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

static unsigned long mt[N]; /* the array for the state vector  */
static int mti=N+1; /* mti==N+1 means mt[N] is not initialized */

/* initializes mt[N] with a seed */
void init_genrand(unsigned long s)
{
    mt[0]= s & 0xffffffffUL;
    for (mti=1; mti<N; mti++) {
        mt[mti] =
        (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array mt[].                        */
        /* 2002/01/09 modified by Makoto Matsumoto             */
        mt[mti] &= 0xffffffffUL;
        /* for >32 bit machines */
    }
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
void init_by_array(unsigned long init_key[], int key_length)
{
    int i, j, k;
    init_genrand(19650218UL);
    i=1; j=0;
    k = (N>key_length ? N : key_length);
    for (; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1664525UL))
        + init_key[j] + j; /* non linear */
        mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
        i++; j++;
        if (i>=N) { mt[0] = mt[N-1]; i=1; }
        if (j>=key_length) j=0;
    }
    for (k=N-1; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1566083941UL))
        - i; /* non linear */
        mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
        i++;
        if (i>=N) { mt[0] = mt[N-1]; i=1; }
    }
    
    mt[0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */
}

/* generates a random number on [0,0xffffffff]-interval */
unsigned long genrand_int32(void)
{
    unsigned long y;
    static unsigned long mag01[2]={0x0UL, MATRIX_A};
    
    /* mag01[x] = x * MATRIX_A  for x=0,1 */
    srand(time(NULL));
    int random = rand();
    if (mti >= N) { /* generate N words at one time */
        int kk;
        
        if (mti == N+1)   /* if init_genrand() has not been called, */
            init_genrand(random);
        //init_genrand(5489UL); /* a default initial seed is used */
        
        for (kk=0;kk<N-M;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (;kk<N-1;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
        mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];
        
        mti = 0;
    }
    
    y = mt[mti++];
    
    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);
    
    return y;
}

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

    unsigned int random;
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
        // Make an asm call here to rdrand and store the value
        asm("rdrand %0" : "=r" (random));
        
        if(range == 1){
             x = low+ random % (high - low);
            
        }else{
            x = 0+ random % (100000);
            
            if(x<0){
                
                x=x*-1;
            }
        }
    }
    else{
        //use mt1993
        if(range == 1){
            
            x = (int)genrand_int32()%(high+1-low);
            
            if(x<0){
                x=x*-1;
            }
        }else{
            x = 0+ (int)genrand_int32() % (100000);
            
            if(x<0){
                x=x*-1;
                
            }
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
                printf("sleep time: %d\n",new_item.sleep_time );
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
        //producer();
    }
    else{
        if(strcmp("consumer", string)==0){
            //printf("consumer\n");
           // consumer();
        }
        else{
            printf("Incorrect input\n");
        }
    }
    pthread_exit(0);
}



int main(int argc, char **argv)
{
    int i;
    for(i=0; i<20;i++){
        int x = random_num_gen(1, 2,9);
        printf("%d\n", x);
    }
        
    if (argc < 3) {
        printf("Need one producer and at least one consumer. Input should be \"producer consumer consumer...\"");
        exit(-1);
    }
    
    int num_args = argc - 1;
    
   // struct sum_runner_struct args[num_args];
    
    char* args[num_args];
    pthread_t tids[num_args];
    
   // pthread_t tids[num_args];
    
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
