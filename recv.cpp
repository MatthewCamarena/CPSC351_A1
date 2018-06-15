#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h"    /* For the message struct */


/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

/* The pointer to the shared memory */
void *sharedMemPtr;

/* The name of the received file */
const char recvFileName[] = "recvfile";


/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
 * @param sharedMemPtr - the pointer to the shared memory
 */

void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
	// generate key
	const key_t key = ftok("keyfile.txt", 'a');
	if(key == (key_t)-1)
	{
		perror("ftok");
		exit(-1);
	}

	// allocate shared memory
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666 | IPC_CREAT);
	if (shmid == -1)
	{
		perror("shmget");
		exit(-1);
	}

	// attach to shared memory (a value of 0 for the address parameter indicates that OS should choose the address)
	sharedMemPtr = shmat(shmid, (void*)0, 0);
	if (sharedMemPtr == (void*)-1)
	{
		perror("shmat");
		exit(-1);
	}

	// create message queue
	msqid = msgget(key, 0666 | IPC_CREAT);
	if (msqid == -1)
	{
		perror("msgget");
		exit(-1);
	}
}
 

/**
 * The main loop
 */
void mainLoop()
{
	/* The size of the mesage */
	int msgSize = 0;
	
	/* Open the file for writing */
	FILE* fp = fopen(recvFileName, "w");
		
	/* Error checks */
	if(!fp)
	{
		perror("fopen");	
		exit(-1);
	}

	message rcvMsg;
	if (msgrcv(msqid, &rcvMsg, sizeof(rcvMsg), SENDER_DATA_TYPE, 0) == -1)
	{
		perror("msgrcv");
		exit(-1);
	}
	else
	{
		msgSize = rcvMsg.size;
		if (msgSize == 0)
		{
			// message of size 0 indicates end of transfer
			fclose(fp);
			return;
		}
	}

	/* Keep receiving until the sender set the size to 0, indicating that
 	 * there is no more data to send
 	 */	

	while(msgSize != 0)
	{	
		/* If the sender is not telling us that we are done, then get to work */
		if(msgSize != 0)
		{
			/* Save the shared memory to file */
			if(fwrite(sharedMemPtr, sizeof(char), msgSize, fp) < 0)
			{
				perror("fwrite");
			}

			// send message to sender indicating that we are ready to read the next file chunk
			message readyMsg;
			readyMsg.mtype = RECV_DONE_TYPE;
			if (msgsnd(msqid, &readyMsg, 0, 0) == -1)
			{
				perror("msgsnd");
				exit(-1);
			}

			// receive next message chunk, if any
			message rcvMsg;
			if (msgrcv(msqid, &rcvMsg, sizeof(rcvMsg), SENDER_DATA_TYPE, 0) == -1)
			{
				perror("msgrcv next chunk");
				exit(-1);
			}
			else
			{
				msgSize = rcvMsg.size;
				/*
				if (msgSize == 0)
				{
					// message of size 0 indicates end of transfer
					fclose(fp);

					return;
				}
				*/
			}
		}
		/* We are done */
		else
		{
			/* Close the file */
			fclose(fp);

			return;
		}
	}
}



/**
 * Perfoms the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr)
{
	// detach from shared memory
	if (shmdt(sharedMemPtr) == -1)
	{
		perror("shmdt");
		exit(-1);
	}

	// deallocate shared memory chunk
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		perror("shmctl");
		exit(-1);
	}
	
	// deallocate message queue
	if (msgctl(msqid, IPC_RMID, NULL) == -1)
	{
		perror("msgctl");
		exit(-1);
	}
}

/**
 * Handles the exit signal
 * @param signal - the signal type
 */

void ctrlCSignal(int signal)
{
	/* Free system V resources */
	cleanUp(shmid, msqid, sharedMemPtr);
	exit(0);
}

int main(int argc, char** argv)
{
	// override default signal handler
	signal(SIGINT, ctrlCSignal); 
				
	/* Initialize */
	init(shmid, msqid, sharedMemPtr);
	
	/* Go to the main loop */
	mainLoop();

	/* Cleanup */ 
	cleanUp(shmid, msqid, sharedMemPtr);
	
	return 0;
}
