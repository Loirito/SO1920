//Projeto SO
//gcc -Wall -lpthread -D_REENTRANT mutex_between_procs_no.c -o mbp_n

#include "header.h"

shmem *shm;

pid_t id;
int shmid;
int fd[2];
int mq_id;

sem_t *writing_sem;
sem_t *reading_sem;
sem_t *mutex;

void create_shm() {

	if((shmid = shmget(IPC_PRIVATE, sizeof(shmem), IPC_CREAT | 0777)) == -1) {
		perror("Shared memory creation unsuccessful.\n");
		exit(1);
	}
	shm = (shmem*) shmat(shmid, NULL, 0);

	printf("--------------SHARED MEMORY--------------\n");
	printf("Shared memory succesfully created at mem address %p\n", shm);
	printf("Shared memory id is %d\n", shmid);
	printf("Initializing shared memory values now.\n");

	shm->departures_list = listdep;;
	shm->arrival_list = listarr;
	shm->total_flights = 0;
	shm->total_landed_flights = 0;
	shm->avg_arrival_time = 0.0;
	shm->total_departures = 0;
	shm->avg_departure_time = 0.0;
	shm->avg_holdings_per_landing = 0.0;
	shm->avg_holdings_per_urgent_landing = 0.0;
	shm->total_deviated_flights = 0;
	shm->total_rejections = 0;

	printf("Shared memory values successfully initialized.\n");

	return;
}

void create_mq() {
	if((mq_id = msgget(IPC_PRIVATE, IPC_CREAT | 0777)) < 0) {
		perror("Creating message queue.\n");
		exit(1);
	}

	printf("Successfully created MQ\n");

	return;
}

void semaphore_creation() {
	sem_unlink("WRITING");
	if((writing_sem = sem_open("WRITING", O_CREAT|O_EXCL, 0700, 0)) == SEM_FAILED) {
		perror("Creating writing semaphore.\n");
		exit(1);
	}
	printf("Successfully created WRITING semaphore\n");
	
	sem_unlink("READING");
	if((reading_sem = sem_open("READING", O_CREAT|O_EXCL, 0700, READING_SEM_VALUE)) == SEM_FAILED) {
		perror("Creating reading semaphore.\n");
		exit(1);
	}
	printf("Successfully created READING semaphore\n");

	sem_unlink("MUTEX");
	if((mutex = sem_open("MUTEX", O_CREAT|O_EXCL, 0700, 1)) == SEM_FAILED) {
		perror("Creating mutex semaphore.\n");
		exit(1);
	}
	printf("Successfully created MUTEX semaphore\n");

	return;
}

/* void *arrival_flight_worker() {
	return;
}

void *departure_flight_worker() {
	return;
} */

void initializer() {
	create_shm();
	semaphore_creation();
	pipe(fd);
	create_mq();

	return;
}

void control_tower() {

	printf("Inside control tower.\n");

	return;
}

void cleanup() {
	shmdt(shm);
	shmctl(shmid, IPC_RMID, NULL);
	sem_unlink("WRITING");
	sem_unlink("READING");
	sem_unlink("MUTEX");
	
	return;
}

int main(void) {

	initializer();

	id = fork();

	if(id == 0) {
		control_tower();
		printf("Control Tower created with PID %ld.\n", (long)getpid());
		return 0;
	}

	else if(id == -1) {
		perror("Error forking.\n");
		return 1;
	}

	else {
		printf("Sim manager here.\n");
		return 0;
	}

	return 0;

}