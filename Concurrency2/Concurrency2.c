#include <stdio.h>
#include <stdlib.h>
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
void *supper(void * args)
{
	struct philosopher* philo = (struct philosopher*)args;
	int response = 0;
	int action = 0;
	int tries;
	int justAte = 1;
	//Infinitely cycle between eating and waiting.
	while(1)
	{
		//This makes sure the philosopher only waits after they have successfully eats and not also when they fail to get both forks and put back the left fork.
		if(justAte)
		{
			//Philosopher waits.
			action = 1 + rand() % 20;
			printf("%s is thinking for %d seconds.\n", philo->name, action);
			sleep(action);
			justAte = 0;
		}
		//Hang and wait for the left fork to be available.
		response = pthread_mutex_lock(philo->leftFork);
		tries = 2;
	
		while(1)
		{
			//Try to get the right fork a couple times.
			if(tries > 0)
			{
				response = pthread_mutex_trylock(philo->rightFork);
				tries--;
			}
			else
			{
				//Set the left fork down and see if someone else can use it to start eating. 
				pthread_mutex_unlock(philo->leftFork);
				//Break to go back and grab the left fork if no one else has taken it
				break;
			}
			
			//Will only enter this if it grabs the right fork successfully.		
			if(!response)
			{
				action = 2 + rand() % 8;
				printf("%s has grabbed two forks and is eatting for %d seconds.\n", philo->name, action);
				sleep(action);
				//return the forks.
				pthread_mutex_unlock(philo->leftFork);	
				pthread_mutex_unlock(philo->rightFork);
					
				printf("%s has returned the forks.\n", philo->name);
				tries = 2;
				justAte = 1;
				break;
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
	struct philosopher philosophers[5];

	//Initialize mutex's
	for(i = 0; i < 5; i++)
	{
		response = pthread_mutex_init(&forks[i], NULL);
		
		//Check if failed to create mutex.
		if(response)
		{
			printf("Failed to create mutex. Exiting....");
			exit(1);
		}
	}

	//Initializing philosophers and setup threads
	strcpy(philosophers[0].name, "Abe");
	strcpy(philosophers[1].name,"Bob");
	strcpy(philosophers[2].name, "Carly");
	strcpy(philosophers[3].name, "Daruma");
	strcpy(philosophers[4].name,"Flume");


	for(i = 0; i < 5; i++)
	{
		philosophers[i].leftFork = &forks[i];

		//Modulus needed for last philosopher in the round robin who shares the 4th and 0th forkl indices. 
		philosophers[i].rightFork = &forks[(i + 1) % 5];
		pthread_attr_t attr;
		pthread_attr_init (&attr);
		philosophers[i].threadResponse = pthread_create(&philosophers[i].threadHandle, &attr, supper, &philosophers[i]); 
	}

	for(i = 0; i < 5; i++)
	{
		//Check if the thread creation and joining fail.
		if(!philosophers[i].threadResponse && pthread_join(philosophers[i].threadHandle, NULL))
		{
			printf("Failed to join thread for %s. Exiting...", philosophers[i].name);
			exit(1);
		} 
	}
}

int main(int argc, char** argv)
{
	setup();
	return 0;
}

