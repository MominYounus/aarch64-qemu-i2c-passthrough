#ifndef MPU_IOCTL_H
#define MPU_IOCTL_H

#include <linux/ioctl.h>

#define MPU_IOC_MAGIC 'M'
#define MPU_SET_ACCEL_RANGE _IOW(MPU_IOC_MAGIC, 1, int)

#endif
