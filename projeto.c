//Projeto SO
//gcc -Wall -lpthread -D_REENTRANT mutex_between_procs_no.c -o mbp_n

#include "header.h"

shmem *shm;

pid_t id;
int shmid;

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

	//shm->departures_list = departures_list;
	//shm->arrival_list = arrival_list;
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

void initializer() {
	create_shm();

	return;
}

void cleanup() {
	shmdt(shm);
	shmctl(shmid, IPC_RMID, NULL);
	
	return;
}

int main(void) {

	initializer();

	id = fork();

	if(id == 0) {
		//control tower initializer
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