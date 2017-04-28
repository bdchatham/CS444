#include <stdio.h>
#include <stdlin.h>
#include <pthread.h>
#include <sys/queue.h>
#include <string.h>
#include <unistd.h>

struct philosopher
{
	pthread_t threadHandle;
	pthread_mutex_t *leftFork;
	pthread_mutex_t *rightFork;
	int threadResponse;
	char name[15];
};

//Philosopher eating cycle function.
void *supper(struct* philosopher)
{
	int reponse;
	int tries = 2;
	//Infinitely cycle between eating and waiting.
	while(1)
	{
		//Philosopher waits.
		action = 1 + rand() % 20;
		printf("%s is thinking for %d seconds.", philosopher->name, action);
		sleep(action);
		
		while(1)
		{
			//Hang for the left fork.
			response = pthread_mutex_lock(philosopher->leftFork);
		
			if(!response)//Checks if lock was successfully taken.
			{
				if(tries > 0)
				{
					response = pthread_mutex_trylock(philosopher->rightFork);
					tries--;
				}
				else
				{
					response = pthread_mutex_lock(philosopher->rightFork);
				}
					
				if(!response)
				{
					action = 2 + rand() % 8;
					printf("%s has grabbed two forks and is eatting for %d seconds.", philsopher->name, action);
					sleep(action);

					//return the forks.
					pthread_mutex_unlock(philosopher->leftFork);	
					pthread_mutex_unlock(philosopher->rightFork);

					printf("%s has returned the forks.", philosopher->name);
				}
			}
		}	
	}

	return NULL;
}

//Setup mutexes and initiate philosopher threads.
void setup()
{
	int i, response;
	pthread_mutex_t forks[5];
	struct philosophers[5];

	//Initialize mutex's
	for(i = 0; i < 5; i++)
	{
		response = pthread_mutex_init(forks[i], NULL);
		
		//Check if failed to create mutex.
		if(response)
		{
			printf("Failed to create mutex. Exitting....");
			exit(1);
		}
	}

	//Initializing philosophers and setup threads
	philosophers[0].name = "Abe";
	philosophers[1].name = "Bob";
	philosophers[2].name = "Carly";
	philosophers[3].name = "Daruma";
	philosophers[4].name = "Flume";


	for(i = 0; i < 5; i++)
	{
		philosophers[i].leftFork = &forks[i];

		//Modulus needed for last philosopher in the round robin who shares the 4th and 0th forkl indices. 
		philosoperts[i].rightFork = &forks[(i + 1) % 5];
		pthread_attr_t attr;
		pthread_attr_init (&attr);
		threadReponse = pthread_create(&philosophers.threadHandle, &attr, supper, &philosophers[i]); 
	}

	for(i = 0; i < 5; i++)
	{
		//Check if the thread creation and joining fail.
		if(!philosopher[i].threadResponse && pthread_join(philosopher.threadHandle, NULL))
		{
			printf("Failed to join thread for %s. Exiting...", philosopher[i].name);
			exit(1);
		} 
	}
}

int main(int argc, char** argv)
{
	setup();
	return 0;
}

