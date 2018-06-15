
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h"    /* For the message struct */

/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

/* The pointer to the shared memory */
void* sharedMemPtr;

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
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
	
	// get id of shared memory segment
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

	// get id of message queue
	msqid = msgget(key, 0666 | IPC_CREAT);
	if (msqid == -1)
	{
		perror("msgget");
		exit(-1);
	}
}

/**
 * Performs the cleanup functions
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
}

/**
 * The main send function
 * @param fileName - the name of the file
 */
void send(const char* fileName)
{
	/* Open the file for reading */
	FILE* fp = fopen(fileName, "r");
	
	/* Was the file open? */
	if(!fp)
	{
		perror("fopen");
		exit(-1);
	}
	
	/* Read the whole file */
	while(!feof(fp))
	{
		/* A buffer to store message we will send to the receiver. */
		message sndMsg;
		sndMsg.mtype = SENDER_DATA_TYPE; 
		
		/* A buffer to store message received from the receiver. */
		message rcvMsg;

		/* Read at most SHARED_MEMORY_CHUNK_SIZE from the file and store them in shared memory. 
 		 * fread will return how many bytes it has actually read (since the last chunk may be less
 		 * than SHARED_MEMORY_CHUNK_SIZE).
 		 */
		if((sndMsg.size = fread(sharedMemPtr, sizeof(char), SHARED_MEMORY_CHUNK_SIZE, fp)) < 0)
		{
			perror("fread");
			exit(-1);
		}

		// send message to receiver indicating that data is ready
		if (msgsnd(msqid, &sndMsg, sizeof(sndMsg), 0) == -1)
		{
			perror("msgsnd");
			exit(-1);
		}

		// wait until receiver says that he has finished saving the memory chunk
		if (msgrcv(msqid, &rcvMsg, sizeof(rcvMsg), RECV_DONE_TYPE, 0) == -1)
		{
			perror("msgrcv");
			exit(-1);
		}
	}

	// we have finished sending the file, tell receiver that we have nothing more to send
	message sndMsgFinish;
	sndMsgFinish.mtype = SENDER_DATA_TYPE;
	sndMsgFinish.size = 0;
	if (msgsnd(msqid, &sndMsgFinish, sizeof(sndMsgFinish), 0) == -1)
	{
		perror("msgsnd finish");
		exit(-1);
	}
	
	/* Close the file */
	fclose(fp);
}


int main(int argc, char** argv)
{
	
	/* Check the command line arguments */
	if(argc < 2)
	{
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}
		
	/* Connect to shared memory and the message queue */
	init(shmid, msqid, sharedMemPtr);
	
	/* Send the file */
	send(argv[1]);
	
	/* Cleanup */
	cleanUp(shmid, msqid, sharedMemPtr);
		
	return 0;
}
