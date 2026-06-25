#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "../include/mpu_ioctl.h"

int main()
{
	int range = 8;

	printf("OPENING /dev/mpu_sensor...\n");
	int fd = open("/dev/mpu_sensor", O_RDWR);

	printf("SETTING THE RANGE TO %d...\n", range);
	if (ioctl(fd, MPU_SET_ACCEL_RANGE, &range) < 0)
		perror("failed to set range:");

	printf("DONE......\n");
	
	close(fd);
	return 0;
}
