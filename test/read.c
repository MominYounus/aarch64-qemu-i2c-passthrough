#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#define NUM_THREADS 4

void *read_sensor(void *thread_id)
{
	long id = (long)thread_id;
		
	int fd = open("/dev/mpu_sensor", O_RDONLY, S_IRUSR);
	char buffer[128];

	while (read(fd, &buffer, sizeof(buffer)) > 0){
	  printf("%s\n", buffer);
		usleep(100000);
	}
	printf("\n");
	printf("From thread[%ld]\n", id);
	
	close(fd);
	return NULL;
}

int main()
{
	pthread_t threads[NUM_THREADS];

	for (long i = 0; i < NUM_THREADS; ++i) {
		if (pthread_create(&threads[i], NULL ,read_sensor, (void*)i) != 0) {
			perror("pthread failed:");
			return 1;
		}
	}

	for (int i = 0; i < NUM_THREADS; ++i) 
		pthread_join(threads[i], NULL);
	
	return 0;
}
 
