#include "include/mpu_ioctl.h"
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/i2c-smbus.h>
#include <linux/i2c.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

/* Registers: [High Byte and Low Byte]

 * 0x3B / 0x3C -> Accelerometer X
 * 0x3D / 0x3E -> Accelerometer Y
 * 0x3F / 0x40 -> Accelerometer Z
 * 0x41 / 0x42 -> Temperature (We will skip this for now, but notice it sits
   right in the middle!)
 * 0x43 / 0x44 -> Gyroscope X
 * 0x45 / 0x46 -> Gyroscope Y
 * 0x47 / 0x48 -> Gyroscope Z
 * 0x1C Accel_config
 */
#define REG_ACCEL_XOUT_H            0x3B
#define REG_ACCEL_XOUT_L            0x3C
#define REG_ACCEL_YOUT_H            0x3D
#define REG_ACCEL_YOUT_L            0x3E
#define REG_ACCEL_ZOUT_H            0x3F
#define REG_ACCEL_ZOUT_L            0x40

#define REG_GYRO_XOUT_H             0x43
#define REG_GYRO_XOUT_L             0x44
#define REG_GYRO_YOUT_H             0x45
#define REG_GYRO_YOUT_L             0x46
#define REG_GYRO_ZOUT_H             0x47
#define REG_GYRO_ZOUT_L             0x48

#define REG_ACCEL_CONFIG            0x1C
#define REG_PWR_MGMT                0x6B
#define PWR_MGMT_DEVICE_RESET       0x80 /* Bit 7 = 1: Reset the device */
#define PWR_MGMT_CLK_PILL           0x01 /* Bit 2:0 = 1: Auto-select the best clock source*/
#define PWR_MGMT_SLEEP              0x40 /* Bit 6 = 1: Put the device to sleep */

#define ACCEL_RANGE_2G              0x00
#define	ACCEL_RANGE_4G              0x08
#define	ACCEL_RANGE_8G              0x10
#define	ACCEL_RANGE_16G             0x18


static struct i2c_client *mpu_client;
static DEFINE_MUTEX(mpu_mutex);

/* Interrupt */
static struct delayed_work mpu_work; // Autonomous Worker

static ssize_t mpu_read(struct file *file, char __user *user_buf, size_t count,
                        loff_t *ppos) {
    int len;
    s32 ret;
    s32 accel_x_h, accel_x_l;
    s32 accel_y_h, accel_y_l;
    s32 accel_z_h, accel_z_l;
    s32 gyro_x_h, gyro_x_l;
    s32 gyro_y_h, gyro_y_l;
    s32 gyro_z_h, gyro_z_l;
    s16 accel_x;
    s16 accel_y;
    s16 accel_z;
    s16 gyro_x;
    s16 gyro_y;
    s16 gyro_z;
    char kernel_buf[128];

    if (mutex_lock_interruptible(&mpu_mutex))
        return -ERESTARTSYS;

    if (!mpu_client) {
        ret = -ENODEV;
        goto unlock_and_exit;
    }

    /* Accelrometer */
    /* X Axis */
    /* High Byte = REG_ACCEL_XOUT_H, Low Byte = REG_ACCEL_XOUT_L */
    accel_x_h = i2c_smbus_read_byte_data(mpu_client, REG_ACCEL_XOUT_H);
    accel_x_l = i2c_smbus_read_byte_data(mpu_client, REG_ACCEL_XOUT_L);

    /* Y Axis */
    /* High Byte = 0x3D, Low Byte = 0x3E */
    accel_y_h = i2c_smbus_read_byte_data(mpu_client, REG_ACCEL_YOUT_H);
    accel_y_l = i2c_smbus_read_byte_data(mpu_client, REG_ACCEL_YOUT_L);

    /* Z Axis */
    /* High Byte = 0x3F, Low Byte = 0x40 */
    accel_z_h = i2c_smbus_read_byte_data(mpu_client, REG_ACCEL_ZOUT_H);
    accel_z_l = i2c_smbus_read_byte_data(mpu_client, REG_ACCEL_ZOUT_L);

    if (accel_x_h < 0 || accel_x_l < 0 || accel_y_h < 0 || accel_y_l < 0 ||
        accel_z_h < 0 || accel_z_l < 0) {
        ret = -EREMOTEIO;
        goto unlock_and_exit;
    }

    /* Gyroscope */
    /* X Axis */
    /* High Byte = 0x43, Low Byte = 0x44 */
    gyro_x_h = i2c_smbus_read_byte_data(mpu_client, REG_GYRO_XOUT_H);
    gyro_x_l = i2c_smbus_read_byte_data(mpu_client, REG_GYRO_XOUT_L);

    /* Y Axis */
    /* High Byte = 0x45, Low Byte = 0x46 */
    gyro_y_h = i2c_smbus_read_byte_data(mpu_client, REG_GYRO_YOUT_H);
    gyro_y_l = i2c_smbus_read_byte_data(mpu_client, REG_GYRO_YOUT_L);

    /* Z Axis */
    /* High Byte = 0x47, Low Byte = 0x48 */
    gyro_z_h = i2c_smbus_read_byte_data(mpu_client, REG_GYRO_ZOUT_H);
    gyro_z_l = i2c_smbus_read_byte_data(mpu_client, REG_GYRO_ZOUT_L);

    if (gyro_x_h < 0 || gyro_x_l < 0 || gyro_y_h < 0 || gyro_y_l < 0 ||
        gyro_z_h < 0 || gyro_z_l < 0) {
        ret = -EREMOTEIO;
        goto unlock_and_exit;
    }

    /* Two 8 bit values(accel_x_h and accel_y_h) in a single 16 bit(accel_x) */
    /* Accelrometer */
    accel_x = (accel_x_h << 8) | accel_x_l;
    accel_y = (accel_y_h << 8) | accel_y_l;
    accel_z = (accel_z_h << 8) | accel_z_l;

    /* Gyroscope */
    gyro_x = (gyro_x_h << 8) | gyro_x_l;
    gyro_y = (gyro_y_h << 8) | gyro_y_l;
    gyro_z = (gyro_z_h << 8) | gyro_z_l;

    /* Readable string for cat command */
    len = snprintf(kernel_buf, sizeof(kernel_buf), "A: %d %d %d | G %d %d %d\n",
                   accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);

    ret = simple_read_from_buffer(user_buf, count, ppos, kernel_buf, len);

 unlock_and_exit:
    mutex_unlock(&mpu_mutex);
    return ret;
}

static long mpu_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int requested_range;
    u8 reg_value;
    s32 ret;

    if (_IOC_TYPE(cmd) != MPU_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case MPU_SET_ACCEL_RANGE:

        if (get_user(requested_range, (int __user *)arg))
            return -EFAULT;

        /* ACCEL_CONFIG (0x1C)
         * 0x00 sets it to ±2g
         * 0x08 sets it to ±4g
         * 0x10 sets it to ±8g
         * 0x18 sets it to ±16g
         */

        if (requested_range == 2)
            reg_value = ACCEL_RANGE_2G;

        else if (requested_range == 4)
            reg_value = ACCEL_RANGE_4G;

        else if (requested_range == 8)
            reg_value = ACCEL_RANGE_8G;

        else if (requested_range == 16)
            reg_value = ACCEL_RANGE_16G;

        else
            return -EINVAL;

        mutex_lock(&mpu_mutex);

        ret = i2c_smbus_write_byte_data(mpu_client, REG_ACCEL_CONFIG, reg_value);
        if (ret < 0) {
            mutex_unlock(&mpu_mutex);
            return -EREMOTEIO;
        }

        mutex_unlock(&mpu_mutex);
        break;

    default:
        return -ENOTTY;
    }

    return 0;
}

static const struct file_operations mpu_fops = {
    .owner = THIS_MODULE,
    .read = mpu_read,
    .unlocked_ioctl = mpu_ioctl,
};

static struct miscdevice mpu_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mpu_sensor",
    .fops = &mpu_fops,
};

/*
 * This function will reads the sensor autonomously in the background.
 * This function reads the Z-Axis of Accelerometer.
 */
static void mpu_work_handler(struct work_struct *work) {
    s32 z_h, z_l;
    s16 z_val;

    mutex_lock(&mpu_mutex);

    if (!mpu_client)
        return;

    z_h = i2c_smbus_read_byte_data(mpu_client, REG_ACCEL_ZOUT_H);
    if (z_h < 0) {
        mutex_unlock(&mpu_mutex);
        return;
    }

    z_l = i2c_smbus_read_byte_data(mpu_client, REG_ACCEL_ZOUT_L);
    if (z_l < 0) {
        mutex_unlock(&mpu_mutex);
        return;
    }

    z_val = (z_h << 8) | z_l;

    dev_info(&mpu_client->dev, "[Autonomous Read] Z-Axis: %d\n", z_val);

    mutex_unlock(&mpu_mutex);

    schedule_delayed_work(&mpu_work, msecs_to_jiffies(1000));
}

static int mpu6050_probe(struct i2c_client *client) {
    s32 ret;

    dev_info(&client->dev, "probe triggered for address: 0x%02x\n",
             client->addr);

    /* Reset the Device
     * writing 0x80 to PWR_MGT(0x6B)
     */
    ret = i2c_smbus_write_byte_data(client, REG_PWR_MGMT, PWR_MGMT_DEVICE_RESET);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to Reset.\n");
        return ret;
    }

    // mpu6050 needs few miliseconds to reboot after the reset
    msleep(100);

    /* wake up the sensor, let's set the clock source to the x axis
     * writing 0x01 to Power Management PWR_MGT(0x6B)
     */
    ret = i2c_smbus_write_byte_data(client, REG_PWR_MGMT, PWR_MGMT_CLK_PILL);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to woke up sensor.\n");
        return ret;
    }

    /*let's try setting the Accelrometer explicitly to +/- 2g
     * writing 0x00 to ACCEL_CONFIG(0x1C)
     */
    ret = i2c_smbus_write_byte_data(client, REG_ACCEL_CONFIG, ACCEL_RANGE_2G);
    if (ret < 0) {
        dev_err(&client->dev, "Failed set  sensor.\n");
        return ret;
    }

    mpu_client = client; // Storing the client for read function

    ret = misc_register(&mpu_misc);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to register misc device.\n");
        return ret;
    }

    dev_info(&client->dev, "/dev/mpu_sensor successfully created\n");

    /* Delayed Work */
    // Initializes the work struct and ties it to mpu_work_handler.
    INIT_DELAYED_WORK(&mpu_work, mpu_work_handler);

    // Starts the first execution to happen 1 second from now on.
    schedule_delayed_work(&mpu_work, msecs_to_jiffies(1000));

    return 0;
}

static void mpu6050_remove(struct i2c_client *client) {
    cancel_delayed_work_sync(&mpu_work);

    misc_deregister(&mpu_misc);

    mutex_lock(&mpu_mutex);

    mpu_client = NULL;

    mutex_unlock(&mpu_mutex);

    dev_info(&client->dev, "Removed MPU6050 and /dev/mpu_sensor.\n");
}

static const struct i2c_device_id mpu_sensor_id[] = {
    {"mpu6050", 0},
    {},
};

static struct i2c_driver mpu6050_driver = {
    .driver =
        {
            .name = "mpu6050",
        },
    .probe = mpu6050_probe,
    .remove = mpu6050_remove,
    .id_table = mpu_sensor_id,
};

module_i2c_driver(mpu6050_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mohammed Momin");
MODULE_DESCRIPTION("MPU6050 I2C Live Chardev Device");
