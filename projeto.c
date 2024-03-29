//Projeto SO
//gcc -Wall -lpthread -D_REENTRANT mutex_between_procs_no.c -o mbp_n

#include "header.h"

FILE *logfile;
pid_t id;
int statsid;
int towershmid;
int fd_named_pipe;
int mq_id;
departures_list dep_list;
departures_list static_dep_list;
arrival_list arr_list;
arrival_list static_arr_list;
struct sigaction *sigint;
struct sigaction *sigusr1;
int debugger = 0;
time_t initialtime;

pthread_t arrival_initializer;
pthread_t departure_initializer;
pthread_t pipe_thread;
pthread_t flight_threads[MAX_FLIGHTS_IN_SYSTEM];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t arrivalmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t departuremutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t arrivalcond = PTHREAD_COND_INITIALIZER;
pthread_cond_t departurecond = PTHREAD_COND_INITIALIZER;

sem_t *stats_writing_sem;
sem_t *stats_reading_sem;
sem_t *mutex_sem;



//SHARED MEMORY
void create_shm(void) {

	if((statsid = shmget(IPC_PRIVATE, sizeof(shmem), IPC_CREAT | 0777)) == -1) {
		perror("Shared memory creation unsuccessful.\n");
		exit(1);
	}

	if((towershmid = shmget(IPC_PRIVATE, sizeof(towershm), IPC_CREAT | 0777)) == -1) {
		perror("Tower shared memory creation unsuccessful.\n");
		exit(1);
	}
	stats = (shmem*) shmat(statsid, NULL, 0);
	tower_shm = (towershm*)shmat(towershmid, NULL, 0);

	printf("--------------SHARED MEMORY--------------\n");
	printf("Shared memory succesfully created at mem address %p\n", stats);
	printf("Shared memory id is %d\n", statsid);
	printf("Tower shared mem succesfully created at mem address %p\n", tower_shm);
	printf("Tower shm id is %d\n", towershmid);
	printf("Initializing shared memory values now.\n");

	stats->total_flights = 0;
	stats->total_landed_flights = 0;
	stats->avg_arrival_time = 0.0;
	stats->total_departures = 0;
	stats->avg_departure_time = 0.0;
	stats->avg_holdings_per_landing = 0.0;
	stats->avg_holdings_per_urgent_landing = 0.0;
	stats->total_deviated_flights = 0;
	stats->total_rejections = 0;

	for(int i=0; i<MAX_FLIGHTS_IN_SYSTEM; i++) {
		tower_shm->slot[i] = 0;
		tower_shm->departure[i] = 0;
		tower_shm->arrival[i] = 0;
		tower_shm->holding[i] = 0;
	}

	printf("Shared memory values successfully initialized.\n");

	return;
}

void create_mq(void) {
	if((mq_id = msgget(IPC_PRIVATE, IPC_CREAT | 0777)) < 0) {
		perror("Creating message queue.\n");
		exit(1);
	}

	printf("Successfully created MQ\n");

	return;
}

void semaphore_creation(void) {
	sem_unlink("WRITING");
	if((stats_writing_sem = sem_open("WRITING", O_CREAT|O_EXCL, 0700, 1)) == SEM_FAILED) {
		perror("Creating writing semaphore.\n");
		exit(1);
	}
	printf("Successfully created WRITING semaphore\n");
	
	sem_unlink("READING");
	if((stats_reading_sem = sem_open("READING", O_CREAT|O_EXCL, 0700, READING_SEM_VALUE)) == SEM_FAILED) {
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

void create_named_pipe(void) {
	unlink(PIPE_NAME);
	if((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0666)<0) && (errno != EEXIST)) {
		perror("Creating pipe.\n");
		exit(1);
	}

	printf("Named pipe %s created successfully!\n", PIPE_NAME);
}

void open_log(void) {
	logfile = fopen("logfile.txt", "a+");
	if(logfile == NULL) {
		perror("Opening log file.");
	}

	printf("log's opened.\n");
	return;
}

void create_departure_list(departures_list *head) {
	head = NULL;
	return;
}

void create_arrival_list(arrival_list *head) {
	head = NULL;
	return;
}

void delete_departure_list(departures_list head) {
	departures_list temp;
	while(head != NULL){
		temp = head;
		head = head->next;
		free(temp);
	}
	printf("DEPARTURE LIST IS DELETED.\n");
	return;
}

void delete_arrival_list(arrival_list head) {
	arrival_list temp;
	while(head != NULL) {
		temp = head;
		head = head->next;
		free(temp);
	}
	printf("ARRIVAL LIST IS DELETED.\n");
	return;
}

departures_list add_departure_list(departures_list head, departure_flight flight) {
	departures_list tmp = (departures_list) malloc(sizeof(list_departures));
	departures_list tmp2;
	if(tmp == NULL) {
		perror("Inserting into departure list. (ran out of memory)");
		exit(-1);
	}

	tmp->flight = flight;
	tmp->next = NULL;
	tmp2 = head;

	if(head == NULL) {
		head = tmp;
		static_dep_list = tmp;
		return head;
	}
	else {
		while(1) {
			if(tmp->flight.init >= tmp2->flight.init &&  tmp2->next == NULL) {
				tmp2->next = tmp;
				return head;
			}

			else if(tmp->flight.init < tmp2->flight.init) {
				tmp->next = tmp2;
				head = tmp;
				return head;
			}
			else if(tmp->flight.init >= tmp2->flight.init && tmp2->next->flight.init > tmp->flight.init) {
				tmp->next = tmp2->next;
				tmp2->next = tmp;
				return head;
			}
			else if(tmp2->next->flight.init <= tmp->flight.init) {
				tmp2 = tmp2->next;
			}
		}
	}
}

arrival_list add_arrival_list(arrival_list head, arrival_flight flight) {
	arrival_list tmp = (arrival_list) malloc(sizeof(list_arrivals));
	arrival_list tmp2;

	if(tmp == NULL) {
		perror("Inserting into arrival list. (ran out of memory)");
		exit(-1);
	}

	tmp->flight = flight;
	tmp->next = NULL;
	tmp2 = head;

	if(head == NULL) {
		head = tmp;
		static_arr_list = tmp;
		return head;
	}
	else {
		while(1) {
			if(tmp->flight.init >= tmp2->flight.init && tmp2->next == NULL) {
				tmp2->next = tmp;
				return head;
			}

			else if(tmp->flight.init < tmp2->flight.init) {
				tmp->next = tmp2;
				head = tmp;
				return head;
			}

			else if(tmp->flight.init >= tmp2->flight.init && tmp->flight.init < tmp2->next->flight.init) {
				tmp->next = tmp2->next;
				tmp2->next = tmp;
				return head;
			}

			else if(tmp2->next->flight.init <= tmp->flight.init && tmp2->next != NULL) {
				tmp2 = tmp2->next;
			}
		}
	}
}

void print_departure_list(departures_list head) {
	departures_list tmp;
	tmp = head;
	while(tmp != NULL) {
		printf("[DEPARTURE] FLIGHT ID: %s\tINIT: %d\n", tmp->flight.flight_id, tmp->flight.init);
		tmp = tmp->next;
	}
	free(tmp);
	return;
}

void print_arrival_list(arrival_list head) {
	arrival_list tmp;
	tmp = head;
	while(tmp != NULL) {
		printf("[ARRIVAL] FLIGHT ID %s\tINIT: %d\n", tmp->flight.flight_id, tmp->flight.init);
		tmp = tmp->next;
	}
	free(tmp);
	return;
}

int check_arrival_flight_id(arrival_flight arrflight, arrival_list arrhead, departures_list dephead) {
	arrival_list tmp = arrhead;
	departures_list tmp2 = dephead;
	while(tmp != NULL) {
		if(strcmp(arrflight.flight_id, tmp->flight.flight_id) == 0) {
			printf("FLIGHT [%s] already exists in system.\n", arrflight.flight_id);
			return -1;
		}
		tmp = tmp->next;
	}

	while(tmp2 != NULL) {
		if(strcmp(arrflight.flight_id, tmp2->flight.flight_id) == 0) {
			printf("FLIGHT [%s] already exists in system.\n", arrflight.flight_id);
			return -1;
		}
		tmp2 = tmp2->next;
	}
	return 0;
}

int check_departure_flight_id(departure_flight flight, arrival_list arrhead, departures_list dephead) {
	arrival_list tmp = arrhead;
	departures_list tmp2 = dephead;
	while(tmp != NULL) {
		if(strcmp(flight.flight_id, tmp->flight.flight_id) == 0) {
			printf("FLIGHT [%s] already exists in system.\n", flight.flight_id);
			return -1;
		}
		tmp = tmp->next;
	}

	while(tmp2 != NULL) {
		if(strcmp(flight.flight_id, tmp2->flight.flight_id) == 0) {
			printf("FLIGHT [%s] already exists in system.\n", flight.flight_id);
			return -1;
		}
		tmp2 = tmp2->next;
	}
	return 0;
}
void add_to_total(void) {
	sem_wait(stats_writing_sem);
	stats->total_flights++;
	sem_post(stats_writing_sem);
	return;
}

void* arrival_flight_worker(void *flight) {
	arrival_flight arrflight = *((arrival_flight*)flight);
	ct_message msg;
	flight_message flightmsg;
	flightmsg.mtype = 1;
	flightmsg.eta = arrflight.eta;
	flightmsg.fuel = arrflight.fuel;
	strcpy(flightmsg.flight_id, arrflight.flight_id);
	if(msgsnd(mq_id, &flightmsg, sizeof(flightmsg), 0) == 0) printf("ARR MESSAGE SENT SUCCESSFULLY.\n");
	if(msgrcv(mq_id, &msg, sizeof(msg), 1, 0) == sizeof(msg)) {
		printf("[%s] arrival received message saying I'm in slot number %d\n", arrflight.flight_id, msg.slot);
	}
	sem_wait(mutex_sem);
	pthread_mutex_lock(&mutex);
 	fprintf(logfile, "Arrival flight [%s] created at instant %d!.\n", arrflight.flight_id, arrflight.init);
 	fflush(logfile);
 	pthread_mutex_unlock(&mutex);
 	sem_post(mutex_sem);
 	printf("Arrival flight [%s] created at instant %d!.\n", arrflight.flight_id, arrflight.init);
 	add_to_total();
 	return NULL;
}

void* departure_flight_worker(void *flight) {
	departure_flight depflight = *((departure_flight*)flight);
	ct_message msg;
	flight_message flightmsg;
	flightmsg.mtype = 2;
	flightmsg.takeoff = depflight.departure_time;
	strcpy(flightmsg.flight_id, depflight.flight_id);
	if(msgsnd(mq_id, &flightmsg, sizeof(flightmsg), 0) == 0) printf("DEP MESSAGE SENT SUCCESSFULLY.\n");
	if(msgrcv(mq_id, &msg, sizeof(msg), 2, 0) == sizeof(msg)) {
		printf("[%s] departure received message saying I'm in slot number %d\n", depflight.flight_id, msg.slot);
	}
	sem_wait(mutex_sem);
	pthread_mutex_lock(&mutex);
	fprintf(logfile, "Departure flight [%s] created at instant %d.\n", depflight.flight_id, depflight.init);
	fflush(logfile);
	pthread_mutex_unlock(&mutex);
	sem_post(mutex_sem);
	printf("Departure flight [%s] created at instant %d.\n", depflight.flight_id, depflight.init);
	add_to_total();
	return NULL;
}

void* create_arrival_flight(void *arg) {
	arrival_list temp;
	while(1){
		while(arr_list != NULL) {	
			temp = arr_list;
			pthread_mutex_lock(&arrivalmutex);
			while(temp == NULL) {
				pthread_cond_wait(&arrivalcond, &arrivalmutex);
			}
			pthread_mutex_unlock(&arrivalmutex);
			printf("INITIAL TIME: %ld.\n", initialtime);
			printf("init e este oh mano: %d\n", temp->flight.init);
			if(temp != NULL) {
				printf("TIME NOW: %ld.\n", time(NULL) - initialtime);
				if(unit_conversion(temp->flight.init) <= time(NULL) - initialtime) {
					sem_wait(stats_reading_sem);
					sem_wait(stats_writing_sem);
					pthread_create(&flight_threads[stats->total_flights], NULL, arrival_flight_worker, &temp->flight);
					stats->total_flights++;
					printf("[THREAD INITIALIZER] Created thread number %d.\n", stats->total_flights);
					sem_post(stats_writing_sem);
					sem_post(stats_reading_sem);
					arr_list = arr_list->next;
				}
				sleep(1);
			}

			sem_wait(stats_reading_sem);
			if(stats->total_flights == MAX_FLIGHTS_IN_SYSTEM) {
				printf("REACHED MAX NUMBER OF FLIGHTS POSSIBLE.\n");
				return 0;
			}
			sem_post(stats_reading_sem);
		}
	}
	return 0;
}

void* create_departure_flight(void *arg) {
	departures_list temp;
	while(1){
		while(dep_list != NULL) {	
			temp = dep_list;
			pthread_mutex_lock(&departuremutex);
			while(temp == NULL) {
				pthread_cond_wait(&departurecond, &departuremutex);
			}
			pthread_mutex_unlock(&departuremutex);
			printf("INITIAL TIME: %ld.\n", initialtime);
			printf("init e este oh mano: %d\n", temp->flight.init);
			if(temp != NULL) {
				printf("TIME NOW: %ld.\n", time(NULL) - initialtime);
				if(unit_conversion(temp->flight.init) <= time(NULL) - initialtime) {
					sem_wait(stats_reading_sem);
					sem_wait(stats_writing_sem);
					printf("I'm inside\n");
					pthread_create(&flight_threads[stats->total_flights], NULL, departure_flight_worker, &temp->flight);
					stats->total_flights++;
					printf("[THREAD INITIALIZER] Created thread number %d.\n", stats->total_flights);
					sem_post(stats_writing_sem);
					sem_post(stats_reading_sem);
					dep_list = dep_list->next;
				}
				sleep(1);
			}

			sem_wait(stats_reading_sem);
			if(stats->total_flights == MAX_FLIGHTS_IN_SYSTEM) {
				printf("REACHED MAX NUMBER OF FLIGHTS POSSIBLE.\n");
				return 0;
			}
			sem_post(stats_reading_sem);
		}
	}
	return 0;
}

float unit_conversion(int ut) {
	float conv;
	float newunit = (float)config_st.tunit/1000;
	conv = (float) ut * newunit;
	return conv;
}
	
void command_wrong_format(char *buffer) {
	perror("command in wrong format.\n");
	sem_wait(mutex_sem);
	fprintf(logfile, "WRONG COMMAND => %s\n", buffer);
	fflush(logfile);
	sem_post(mutex_sem);
	printf("WRONG COMMAND => %s\n", buffer);

	return;
}

void pipe_control(int buflen, char *buffer, char *bufcopy, char *token) {

	while(1) {

		printf("[%s] READING.\n", PIPE_NAME);
		buflen = read(fd_named_pipe, buffer, 256);
		if(buflen > 0) {
			buffer[buflen] = '\0';
		}
		strcpy(bufcopy, buffer);
		token = strtok(buffer, " ");
		if(token != NULL && strcmp(token, "DEPARTURE") == 0) {
			departure_flight depflight;
			token = strtok(NULL, " ");
			strcpy(depflight.flight_id, token);
			printf("idflight is %s.\n", depflight.flight_id);
			if(check_departure_flight_id(depflight, static_arr_list, static_dep_list) == -1) {
				command_wrong_format(bufcopy);
				pipe_control(buflen, buffer, bufcopy, token);
			}
			token = strtok(NULL, " ");
			if(strcmp(token, "init:") == 0) {
				token = strtok(NULL, " ");
				depflight.init = atoi(token);
				if(unit_conversion(depflight.init) < time(NULL) - initialtime) {
					command_wrong_format(bufcopy);
					printf("Entered flight init is lower than system time.\n");
					pipe_control(buflen, buffer, bufcopy, token);
				}
				printf("tempo de entrada no sistema: %d.\n", depflight.init);
				token = strtok(NULL, " ");
				if(strcmp(token, "takeoff:") == 0) {
					token = strtok(NULL, " ");
					depflight.departure_time = atoi(token);
					printf("takeoff time: %d.\n", depflight.departure_time);
					sem_wait(mutex_sem);
					fprintf(logfile, "NEW COMMAND => %s\n", bufcopy);
					fflush(logfile);
					sem_post(mutex_sem);
					printf("NEW COMMAND => %s\n", bufcopy);
					dep_list = add_departure_list(dep_list, depflight);
					print_departure_list(dep_list);
					pthread_mutex_lock(&departuremutex);
					if(dep_list != NULL) {
						pthread_cond_signal(&departurecond);
					}
					pthread_mutex_unlock(&departuremutex);
				}
				else command_wrong_format(bufcopy);
			}
			else command_wrong_format(bufcopy);
		}


		else if(token != NULL && strcmp(token, "ARRIVAL") == 0) {
			arrival_flight flight;
			token = strtok(NULL, " ");
			strcpy(flight.flight_id, token);
			printf("idflight is %s.\n", flight.flight_id);
			if(check_arrival_flight_id(flight, static_arr_list, static_dep_list) == -1) {
				command_wrong_format(bufcopy);
				pipe_control(buflen, buffer, bufcopy, token);
			}
			token = strtok(NULL, " ");
			if(strcmp(token, "init:") == 0) {
				token = strtok(NULL, " ");
				flight.init = atoi(token);
				if(unit_conversion(flight.init) < time(NULL) - initialtime) {
					command_wrong_format(bufcopy);
					printf("Entered flight init is lower than system time.\n");
					pipe_control(buflen, buffer, bufcopy, token);
				}
				printf("tempo de entrada no sistema: %d.\n", flight.init);
				token = strtok(NULL, " ");
				if(strcmp(token, "eta:") == 0) {
					token = strtok(NULL, " ");
					flight.eta = atoi(token);
					printf("eta: %d.\n", flight.eta);
					token = strtok(NULL, " ");
					if(strcmp(token, "fuel:") == 0) {
						token = strtok(NULL, " ");
						flight.fuel = atoi(token);
						printf("fuel: %d.\n", flight.fuel);
						sem_wait(mutex_sem);
		 				fprintf(logfile, "NEW COMMAND => %s\n", bufcopy);
		 				fflush(logfile);
		 				sem_post(mutex_sem);
		 				printf("NEW COMMAND => %s\n", bufcopy);
		 				arr_list = add_arrival_list(arr_list, flight);
		 				print_arrival_list(arr_list);
		 				pthread_mutex_lock(&arrivalmutex);
		 				if(arr_list != NULL) {
		 					pthread_cond_signal(&arrivalcond);
		 				}
		 				pthread_mutex_unlock(&arrivalmutex);
		 			}
		 			else command_wrong_format(bufcopy);
		 		}
		 		else command_wrong_format(bufcopy);
		 	}
		 	else command_wrong_format(bufcopy);
		}
		else command_wrong_format(bufcopy);
	}
}

void read_pipe(void) {

	fd_set read_set;
	char buffer[256];
	char bufcopy[256];
	int buflen = 0;
	char *token = "";

	if((fd_named_pipe = open(PIPE_NAME, O_RDWR))<0) {
			perror("Opening pipe.\n");
			exit(1);
		}
	else printf("pipe is open.\n");
	
	FD_ZERO(&read_set);
	FD_SET(fd_named_pipe, &read_set);
	printf("inside read pipe %d.\n", fd_named_pipe);

	pipe_control(buflen, buffer, bufcopy, token);
}

void read_config(void) {
	
	FILE *fp = fopen("config.txt", "r");
	if(fp == NULL) {
		perror("Opening file.");
		exit(1);
	}
	else printf("File opened successfully.\n");

	char *token;
	char line[256];

	fgets(line, sizeof(line), fp);
	config_st.tunit = atoi(line);
	printf("tunit: %d\n", config_st.tunit);

	fgets(line, sizeof(line), fp);
	token = strtok(line, ",");
	config_st.depduration = atoi(token);
	printf("depduration: %d\n", config_st.depduration);
	token = strtok(NULL, " ");
	config_st.depinterval = atoi(token);
	printf("depinterval: %d\n", config_st.depinterval);

	fgets(line, sizeof(line), fp);
	token = strtok(line, ",");
	config_st.arrduration = atoi(token);
	printf("arrduration: %d\n", config_st.arrduration);
	token = strtok(NULL, " ");
	config_st.arrinterval = atoi(token);
	printf("arrinterval: %d\n", config_st.arrinterval);

	fgets(line, sizeof(line), fp);
	token = strtok(line, ",");
	config_st.minholding = atoi(token);
	printf("minholding: %d\n", config_st.minholding);
	token = strtok(NULL, " ");
	config_st.maxholding = atoi(token);
	printf("maxholding: %d\n", config_st.maxholding);

	fgets(line, sizeof(line), fp);
	config_st.maxdepartures = atoi(line);
	printf("maxdepartures: %d\n", config_st.maxdepartures);

	fgets(line, sizeof(line), fp);
	config_st.maxarrivals = atoi(line);
	printf("maxarrivals: %d\n", config_st.maxarrivals);

	return;
}

void initializer(void) {
	sigint = (struct sigaction*)malloc(sizeof(struct sigaction));
	sigusr1 = (struct sigaction*)malloc(sizeof(struct sigaction));
	sigint->sa_handler = cleanup;
	sigint->sa_flags = 0;
	sigusr1->sa_handler = print_stats;
	sigusr1->sa_flags = 0;
	initialtime = time(NULL);
	create_departure_list(&dep_list);
	create_arrival_list(&arr_list);
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

void print_stats(int signum) {
	sem_wait(stats_reading_sem);
	printf("TOTAL FLIGHTS: %d\n", stats->total_flights);
	printf("TOTAL LANDED FLIGHTS: %d\n", stats->total_landed_flights);
	printf("AVERAGE ARRIVAL TIME: %f\n", stats->avg_arrival_time);
	printf("TOTAL DEPARTURES: %d\n", stats->total_departures);
	printf("AVERAGE DEPARTURE TIME: %f\n", stats->avg_departure_time);
	printf("AVERAGE HOLDINGS PER LANDING: %f\n", stats->avg_holdings_per_landing);
	printf("AVERAGE HOLDINGS PER URGENT LANDING: %f\n", stats->avg_holdings_per_urgent_landing);
	printf("TOTAL DEVIATED FLIGHTS: %d\n", stats->total_deviated_flights);
	printf("TOTAL REJECTIONS: %d\n", stats->total_rejections);
	sem_post(stats_reading_sem);
	return;
}

ct_message message_type(long type, ct_message msg, int slot) {
	if(type == 1) {
		printf("THIS IS AN ARRIVAL FLIGHT INSIDE MTYPE FUNC.\n");
		msg.mtype = 1;
		msg.slot = slot;
	}
	if(type == 2) {
		printf("THIS IS A DEPARTURE FLIGHT INSIDE MTYPE FUNC.\n");
		msg.mtype = 2;
		msg.slot = slot;
	}
	return msg;
}

void control_tower(void) {

	int slot = 0;
	ct_message msg;
	flight_message flightmsg;
	
	while(1) {
		printf("INSIDE CT.\n");
		if(msgrcv(mq_id, &flightmsg, sizeof(flightmsg), 2, IPC_NOWAIT) == sizeof(flightmsg)|| msgrcv(mq_id, &flightmsg, sizeof(flightmsg), 1, IPC_NOWAIT) == sizeof(flightmsg)) {
		//fprintf(logfile, "Arrival message received by flight ID [%s] now has slot %d\n", flightmsg.flight_id, ++slot);
			printf("Message received by flight ID [%s] now has slot %d\n", flightmsg.flight_id, ++slot);
			msg = message_type(flightmsg.mtype, msg, slot);
			msgsnd(mq_id, &msg, sizeof(ct_message), 0);
		}
		if(sigaction(SIGUSR1, sigusr1, NULL) < 0) {
			perror("Calling sigusr1.");
			exit(-1);
		}
		sleep(1);
	}
	return;
}

void cleanup(int signum) {
	delete_arrival_list(arr_list);
	delete_departure_list(dep_list);
	sem_wait(mutex_sem);
	fprintf(logfile, "PROGRAM FINISHED.\n");
	fflush(logfile);
	sem_post(mutex_sem);
	printf("\nPROGRAM FINISHED.\n");
	shmdt(stats);
	shmctl(statsid, IPC_RMID, NULL);
	shmdt(tower_shm);
	shmctl(towershmid, IPC_RMID, NULL);
	sem_unlink("WRITING");
	sem_unlink("READING");
	sem_unlink("MUTEX");
	fclose(logfile);
	
	exit(0);
}

int main(int argc, char *argv[]) {

	initializer();
	initialtime = time(NULL);

	id = fork();


	if(id == 0) {
		printf("Control tower created with ID %ld.\n", (long)getpid());
		fprintf(logfile, "Control Tower created with PID %ld.\n", (long)getpid());
		control_tower();
		return 0;
	}

	else if(id == -1) {
		perror("Error forking.\n");
		return 1;
	}	
	
	if(sigaction(SIGINT, sigint, NULL) < 0) {
		perror("Calling sigaction.");
		exit(-1);
	}

	if(pthread_create(&arrival_initializer, NULL, create_arrival_flight, NULL) != 0) {
		perror("Creating arrival flight initializer");
		exit(-1);
	}

	if(pthread_create(&departure_initializer, NULL, create_departure_flight, NULL) != 0) {
		perror("Creating deprature flight initializer.");
		exit(-1);
	}

	read_pipe();

	if(pthread_join(arrival_initializer, NULL) != 0) {
		perror("Joining flight initializer");
		exit(-1);
	} 

	if(pthread_join(departure_initializer, NULL) != 0) {
		perror("Joining departure initializer");
		exit(-1);
	}

	for(int i=0; i<MAX_FLIGHTS_IN_SYSTEM; i++) {
		if(pthread_join(flight_threads[i], NULL) != 0) {
			perror("Joining thread");
			exit(-1);
		}
		else {
			fprintf(logfile, "Thread closing.\n");
			printf("Thread closing.\n");
		}
	}

	return 0;

}
