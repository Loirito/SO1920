#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <semaphore.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>
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

#define PIPE_NAME "/tmp/input_pipe"
#define ARRIVAL_T 8
#define DEPARTURE_T 8
#define MAX_ARRIVAL 5
#define READING_SEM_VALUE 5
#define MAX_DEPARTURE 8
#define MAX_FLIGHTS_IN_SYSTEM 100

typedef struct arrivals {
	bool can_arrive;
	bool holding;
	bool leave;
	int added_time;
	int eta;
	int init;
	int fuel;
	char flight_id[8];
	int priority;
} arrival_flight;
arrival_flight arrival_array[MAX_ARRIVAL];

typedef struct departures {
	bool can_depart;
	int init;
	int departure_time;
	char flight_id[8];
	int priority;
} departure_flight;
departure_flight departure_array[MAX_DEPARTURE];

typedef struct list_arrivals *arrival_list;
typedef struct list_arrivals {
	arrival_flight flight;
	arrival_list next;
} list_arrivals;

typedef struct list_departures *departures_list;
typedef struct list_departures {
	departure_flight flight;
	departures_list next;
} list_departures;

typedef struct shared_memory {
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
shmem *stats;

typedef struct tower {
	arrival_flight arrivalsarray[MAX_ARRIVAL];
	departure_flight departurearray[MAX_DEPARTURE];	
} towershm;
towershm *tower_shm;

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

typedef struct config {
	int tunit;
	int depduration;
	int depinterval;
	int arrduration;
	int arrinterval;
	int minholding;
	int maxholding;
	int maxdepartures;
	int maxarrivals;
}config;
config config_st;

void create_shm(void);
void create_mq(void);
void semaphore_creation(void);
void create_named_pipe(void);
void open_log(void);
void create_departure_list(departures_list *head);
void create_arrival_list(arrival_list *head);
void delete_departure_list(departures_list head);
void delete_arrival_list(arrival_list head);
void* arrival_flight_worker(void *arg);
void* departure_flight_worker(void *arg);
void* create_arrival_flight(void *arg);
float unit_conversion(int ut);
void read_pipe(void);
void read_config(void);
void initializer(void);
void control_tower(void); 
void cleanup(int signum);
