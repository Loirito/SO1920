#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <semaphore.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <stdbool.h>
#include <fcntl.h>

#define ARRIVAL_T 8
#define DEPARTURE_T 8
#define MAX_ARRIVAL 5
#define READING_SEM_VALUE 5
//#define MAX_DEPARTURE 8

typedef struct arrivals {
	bool can_arrive;
	bool holding;
	bool leave;
	int added_time;
	int eta;
	int init;
	int fuel;
	int flight_id;
	int priority;
} arrival_flight;
arrival_flight *arrivals_array;

typedef struct departures {
	bool can_depart;
	int init;
	int departure_time;
	int flight_id;
	int priority;
} departure_flight;
departure_flight *departures_array;

typedef struct list_arrivals *arrival_list;
typedef struct list_arrivals {
	arrival_flight flight;
	int n_slot;
	arrival_list next;
} list_arrivals;
arrival_list listarr;

typedef struct list_departures *departures_list;
typedef struct list_departures {
	departure_flight flight;
	int n_slot;
	departures_list next;
} list_departures;
departures_list listdep;

typedef struct shared_memory {
	list_departures *departures_list;
	list_arrivals *arrival_list;
	int total_flights;
	int total_landed_flights;
	double avg_arrival_time;	// this value starts counting after the eta of the flight (so the standard should be the arrival motion T units of time)
	int total_departures;
	double avg_departure_time;
	double avg_holdings_per_landing;
	double avg_holdings_per_urgent_landing;
	int total_deviated_flights;
	int total_rejections;
} shmem;
shmem *shm;

typedef struct ct_message {
	int priority;
	bool holding;
	bool aterrar;
	bool descolar;
}ct_message;

typedef struct flight_message {
	long mtype;
	int init;
	int eta;
	int takeoff;
	int fuel;
	int flight_id;
}flight_message;

void create_shm();
void create_mq();
void semaphore_creation();
//void *arrival_flight_worker();
//void *departure_flight_worker();
void initializer();
void control_tower(); 
void cleanup();