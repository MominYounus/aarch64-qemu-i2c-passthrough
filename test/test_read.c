#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
	int fd = open("/dev/mpu_sensor", O_RDWR, S_IRUSR);
	if (fd < 0) {
		perror("open failed: ");
		return -1;
	}
	
	char buffer[128] = " ";
	
	for (int i = 0; i < 10; ++i) {
		if (read(fd, buffer, sizeof(buffer)) < 0) {
			perror("read failed: ");
			return -1;
		}
		printf("%s\n", buffer);
		usleep(500000);
	}
	
	return 0;
}
