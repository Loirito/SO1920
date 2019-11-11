//Projeto SO
//gcc -Wall -lpthread -D_REENTRANT mutex_between_procs_no.c -o mbp_n

#include "header.h"

shmem *shm;

FILE *logfile;
pid_t id;
int shmid;
int fd_named_pipe;
int mq_id;
int tunit;
int depduration, depinterval;
int arrduration, arrinterval;
int minholding, maxholding;
int maxdepartures;
int maxarrivals; 

pthread_t thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; 

sem_t *writing_sem;
sem_t *reading_sem;
sem_t *mutex_sem;

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

	shm->departures_list = listdep;
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
	if((mutex_sem = sem_open("MUTEX", O_CREAT|O_EXCL, 0700, 1)) == SEM_FAILED) {
		perror("Creating mutex semaphore.\n");
		exit(1);
	}
	printf("Successfully created MUTEX semaphore\n");

	return;
}

void create_named_pipe() {
	unlink(PIPE_NAME);
	if((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0666)<0) && (errno != EEXIST)) {
		perror("Creating pipe.\n");
		exit(1);
	}

	printf("Named pipe %s created successfully!\n", PIPE_NAME);
}

void open_log() {
	logfile = fopen("logfile.txt", "a+");
	if(logfile == NULL) {
		perror("Opening log file.");
	}

	printf("log's opened.\n");
	return;
}

void* arrival_flight_worker(void *arg) {
	sem_wait(mutex_sem);
	pthread_mutex_lock(&mutex);
 	fprintf(logfile, "Arrival flight created!.\n");
 	pthread_mutex_unlock(&mutex);
 	sem_post(mutex_sem);
 	printf("Arrival flight created!.\n");
 	return NULL;
}

void* departure_flight_worker(void *arg) {
	sem_wait(mutex_sem);
	pthread_mutex_lock(&mutex);
	fprintf(logfile, "Departure flight created.\n");
	pthread_mutex_unlock(&mutex);
	sem_post(mutex_sem);
	printf("Departure flight created.\n");
	return NULL;
} 

void create_arrival(char *idflight, int ut, int eta, int fuel) {
	time_t begintime;
	begintime = time(NULL);
	double init = unit_conversion(ut);
	while((begintime + init) < time(NULL)) {	}
	pthread_create(&thread, NULL, arrival_flight_worker, NULL);
	return;
}

void create_departure(char *idflight, int ut, int takeoff) {
	time_t begintime;
	begintime = time(NULL);
	double init = unit_conversion(ut);
	while((begintime + init) > time(NULL)) {	}	
	pthread_create(&thread, NULL, departure_flight_worker, NULL);
	return;
}

double unit_conversion(int ut) {
	double conv;
	double newunit = (double)tunit/1000;
	conv = (double) ut * newunit;
	return conv;
}

void read_pipe() {


	while(1) {

		fd_set read_set;
		char buffer[256];
		char bufcopy[256];
		int buflen = 0;
		char *token;
		char idflight[8];
		int ut = 0, takeoff = 0, eta = 0, fuel = 0, retval = 0;

		if((fd_named_pipe = open(PIPE_NAME, O_RDONLY|O_NONBLOCK))<0) {
			perror("Opening pipe.\n");
			exit(1);
		}
		else printf("pipe is open.\n");

		FD_ZERO(&read_set);
		FD_SET(fd_named_pipe, &read_set);

		printf("inside read pipe %d.\n", fd_named_pipe);

		retval = select(fd_named_pipe + 1, &read_set, NULL, NULL, NULL);

		if(retval) {
			if(FD_ISSET(fd_named_pipe, &read_set)) {
				printf("[%s] READING.\n", PIPE_NAME);
					buflen = read(fd_named_pipe, buffer, 256);
					if(buflen > 0) {
						buffer[buflen] = '\0';
					}
					strcpy(bufcopy, buffer);
					token = strtok(buffer, " ");
					if(token != NULL && strcmp(token, "DEPARTURE") == 0) {
						token = strtok(NULL, " ");
						strcpy(idflight, token);
						printf("idflight is %s.\n", idflight);
						token = strtok(NULL, " ");
						if(strcmp(token, "init:") == 0) {
							token = strtok(NULL, " ");
							ut = atoi(token);
							printf("tempo de entrada no sistema: %d.\n", ut);
							token = strtok(NULL, " ");
							if(strcmp(token, "takeoff:") == 0) {
								token = strtok(NULL, " ");
								takeoff = atoi(token);
								printf("takeoff time: %d.\n", takeoff);
								sem_wait(mutex_sem);
								printf("inside mutex\n");
								int n = fprintf(logfile, "NEW COMMAND => %s\n", bufcopy);
								sem_post(mutex_sem);
								printf("val fprintf %d\n", n);
								printf("NEW COMMAND => %s\n", bufcopy);
								create_departure(idflight, ut, takeoff);
							}

							else {
								perror("command wrong format (takeoff:).\n");
								sem_wait(mutex_sem);
								fprintf(logfile, "WRONG COMMAND => %s\n", bufcopy);
								sem_post(mutex_sem);
								printf("WRONG COMMAND => %s\n", bufcopy);

							}
						}

						else {
							perror("command wrong format (init:).\n");
							sem_wait(mutex_sem);
							fprintf(logfile, "WRONG COMMAND => %s\n", bufcopy);
							sem_post(mutex_sem);
							printf("WRONG COMMAND => %s\n", bufcopy);
						}

					}

					else if(token != NULL && strcmp(token, "ARRIVAL") == 0) {
						token = strtok(NULL, " ");
						strcpy(idflight, token);
						printf("idflight is %s.\n", idflight);
						token = strtok(NULL, " ");
						if(strcmp(token, "init:") == 0) {
							token = strtok(NULL, " ");
							ut = atoi(token);
							printf("tempo de entrada no sistema: %d.\n", ut);
							token = strtok(NULL, " ");
							if(strcmp(token, "eta:") == 0) {
								token = strtok(NULL, " ");
								eta = atoi(token);
								printf("eta: %d.\n", eta);
								token = strtok(NULL, " ");
								if(strcmp(token, "fuel:") == 0) {
									token = strtok(NULL, " ");
									fuel = atoi(token);
									printf("fuel: %d.\n", fuel);
									sem_wait(mutex_sem);
									fprintf(logfile, "NEW COMMAND => %s\n", bufcopy);
									sem_post(mutex_sem);
									printf("NEW COMMAND => %s\n", bufcopy);
									create_arrival(idflight, ut, eta, fuel);
								}

								else {
									perror("command wrong format (fuel).\n");
									sem_wait(mutex_sem);
									fprintf(logfile, "WRONG COMMAND => %s\n", bufcopy);
									sem_post(mutex_sem);
									printf("WRONG COMMAND => %s\n", bufcopy);
								}
							}

							else {
								perror("command wrong format (eta:).\n");
								sem_wait(mutex_sem);
								fprintf(logfile, "WRONG COMMAND => %s\n", bufcopy);
								sem_post(mutex_sem);
								printf("WRONG COMMAND => %s\n", bufcopy);
							}
						}

						else {
							perror("command wrong format (init arr:).\n");
							sem_wait(mutex_sem);
							fprintf(logfile, "WRONG COMMAND => %s\n", bufcopy);
							sem_post(mutex_sem);
							printf("WRONG COMMAND => %s\n", bufcopy);
						}
					}
					else {
						perror("Command is in wrong format (DEP or ARRIVAL).\n");
						sem_wait(mutex_sem);
						fprintf(logfile, "WRONG COMMAND => %s\n", bufcopy);
						sem_post(mutex_sem);
						printf("WRONG COMMAND => %s\n", bufcopy);
					}
			}
		}

		else if (retval == -1) perror("select()");
		else printf("No data in 5s.\n");
	}
}

void read_config() {
	
	FILE *fp = fopen("config.txt", "r");
	if(fp == NULL) {
		perror("Opening file.");
		exit(1);
	}
	else printf("File opened successfully.\n");

	char *token;
	char line[256];

	fgets(line, sizeof(line), fp);
	tunit = atoi(line);
	printf("tunit: %d\n", tunit);

	fgets(line, sizeof(line), fp);
	token = strtok(line, ",");
	depduration = atoi(token);
	printf("depduration: %d\n", depduration);
	token = strtok(NULL, " ");
	depinterval = atoi(token);
	printf("depinterval: %d\n", depinterval);

	fgets(line, sizeof(line), fp);
	token = strtok(line, ",");
	arrduration = atoi(token);
	printf("arrduration: %d\n", arrduration);
	token = strtok(NULL, " ");
	arrinterval = atoi(token);
	printf("arrinterval: %d\n", arrinterval);

	fgets(line, sizeof(line), fp);
	token = strtok(line, ",");
	minholding = atoi(token);
	printf("minholding: %d\n", minholding);
	token = strtok(NULL, " ");
	maxholding = atoi(token);
	printf("maxholding: %d\n", maxholding);

	fgets(line, sizeof(line), fp);
	maxdepartures = atoi(line);
	printf("maxdepartures: %d\n", maxdepartures);

	fgets(line, sizeof(line), fp);
	maxarrivals = atoi(line);
	printf("maxarrivals: %d\n", maxarrivals);

	return;
}

void initializer() {
	open_log();
	read_config();
	create_shm();
	semaphore_creation();
	create_mq();
	create_named_pipe();
	sem_wait(mutex_sem);
	fprintf(logfile, "PROGRAM STARTED.\n");
	sem_post(mutex_sem);
	printf("PROGRAM STARTED.\n");
}

void control_tower() {

	printf("Inside control tower.\n");

	return;
}

void cleanup() {
	sem_wait(mutex_sem);
	fprintf(logfile, "PROGRAM FINISHED.\n");
	sem_post(mutex_sem);
	printf("PROGRAM FINISHED.\n");
	shmdt(shm);
	shmctl(shmid, IPC_RMID, NULL);
	sem_unlink("WRITING");
	sem_unlink("READING");
	sem_unlink("MUTEX");
	fclose(logfile);
	
	return;
}

int main(int argc, char *argv[]) {

	initializer();

	id = fork();


	if(id == 0) {
		control_tower();
		fprintf(logfile, "Control Tower created with PID %ld.\n", (long)getpid());
		return 0;
	}

	else if(id == -1) {
		perror("Error forking.\n");
		return 1;
	}
	sem_wait(mutex_sem);
	fprintf(logfile, "why doesn't this work.\n");
	sem_post(mutex_sem);
	read_pipe();

	if(pthread_join(thread, NULL)) {
		perror("Joining thread.");
		exit(1);
	}
	else {
		fprintf(logfile, "Thread closing.\n");
		printf("Thread closing.\n");
	}
	cleanup();

	return 0;

}