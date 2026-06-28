#include "../include/mpu_ioctl.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	int range = atoi(argv[1]);
	char buffer[128] = " ";

	printf("OPENING /dev/mpu_sensor...\n");
	int fd_1 = open("/dev/mpu_sensor", O_RDWR);
	if (fd_1 < 0) {
		perror("open failed:");
		return -1;
	}

	printf("SETTING THE RANGE TO %d...\n", range);
	if (ioctl(fd_1, MPU_SET_ACCEL_RANGE, &range) < 0)
		perror("failed to set range:");

	printf("DONE......\n");

	int fd_2 = open("/dev/mpu_sensor", O_RDONLY);
	if (fd_1 < 0) {
		perror("open failed:");
		return -1;
	}

	for (int i = 0; i < 10; ++i) {
		if (read(fd_2, buffer, sizeof(buffer)) < 0) {
			perror("read failed: ");
			return -1;
		}
		printf("%s\n", buffer);
		usleep(500000);
	}

	close(fd_1);
	close(fd_2);
	return 0;
}
