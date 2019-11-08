%Projeto SO
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>


pid_t id;
int shmid;

void initializer() {

}

int main(void) {

	id = fork();

	if(id == 0) {
		//control tower initializer
		printf("Control Tower created with PID %ld.\n", (long)getpid());
	}

	else if(id == -1) {
		perror("Error forking.\n");
	}

	else {
		printf("Sim manager here.\n");
	}

	return 0;

}