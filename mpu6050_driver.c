#include "include/mpu_ioctl.h"
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/i2c-smbus.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

/* Registers: [High Byte and Low Byte]

 * 0x3B / 0x3C -> Accelerometer X
 * 0x3D / 0x3E -> Accelerometer Y
 * 0x3F / 0x40 -> Accelerometer Z
 * 0x41 / 0x42 -> Temperature (We will skip this for now, but notice it sits
   right in the middle!)
 * 0x43 / 0x44 -> Gyroscope X
 * 0x45 / 0x46 -> Gyroscope Y
 * 0x47 / 0x48 -> Gyroscope Z

 */

static struct i2c_client *mpu_client;
static DEFINE_MUTEX(mpu_mutex);

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

    if (!mpu_client)
        return -ENODEV;

    if (mutex_lock_interruptible(&mpu_mutex))
        return -ERESTARTSYS;

    /* Accelrometer */
    /* X Axis */
    /* High Byte = 0x3B, Low Byte = 0x3C */
    accel_x_h = i2c_smbus_read_byte_data(mpu_client, 0x3B);
    accel_x_l = i2c_smbus_read_byte_data(mpu_client, 0x3C);

    /* Y Axis */
    /* High Byte = 0x3D, Low Byte = 0x3E */
    accel_y_h = i2c_smbus_read_byte_data(mpu_client, 0x3D);
    accel_y_l = i2c_smbus_read_byte_data(mpu_client, 0x3E);

    /* Z Axis */
    /* High Byte = 0x3F, Low Byte = 0x40 */
    accel_z_h = i2c_smbus_read_byte_data(mpu_client, 0x3F);
    accel_z_l = i2c_smbus_read_byte_data(mpu_client, 0x40);

    if (accel_x_h < 0 || accel_x_l < 0 || accel_y_h < 0 || accel_y_l < 0 ||
        accel_z_h < 0 || accel_z_l < 0) {
        ret = -EREMOTEIO;
        goto unlock_and_exit;
    }

    /* Gyroscope */
    /* X Axis */
    /* High Byte = 0x43, Low Byte = 0x44 */
    gyro_x_h = i2c_smbus_read_byte_data(mpu_client, 0x43);
    gyro_x_l = i2c_smbus_read_byte_data(mpu_client, 0x44);

    /* Y Axis */
    /* High Byte = 0x45, Low Byte = 0x46 */
    gyro_y_h = i2c_smbus_read_byte_data(mpu_client, 0x45);
    gyro_y_l = i2c_smbus_read_byte_data(mpu_client, 0x46);

    /* Z Axis */
    /* High Byte = 0x47, Low Byte = 0x48 */
    gyro_z_h = i2c_smbus_read_byte_data(mpu_client, 0x47);
    gyro_z_l = i2c_smbus_read_byte_data(mpu_client, 0x48);

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
            reg_value = 0x00;

        else if (requested_range == 4)
            reg_value = 0x08;

        else if (requested_range == 8)
            reg_value = 0x10;

        else if (requested_range == 16)
            reg_value = 0x18;

        else
            return -EINVAL;

        mutex_lock(&mpu_mutex);

        ret = i2c_smbus_write_byte_data(mpu_client, 0x1C, reg_value);
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

static int mpu6050_probe(struct i2c_client *client) {
    s32 ret;

    dev_info(&client->dev, "probe triggered for address: 0x%02x\n",
             client->addr);

    /* Reset the Device
     * writing 0x80 to PWR_MGT(0x6B)
     */
    ret = i2c_smbus_write_byte_data(client, 0x6B, 0x80);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to Reset.\n");
        return ret;
    }

    // mpu6050 needs few miliseconds to reboot after the reset
    msleep(100);

    /* wake up the sensor, let's set the clock source to the x axis
     * writing 0x01 to Power Management PWR_MGT(0x6B)
     */
    ret = i2c_smbus_write_byte_data(client, 0x6B, 0x01);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to woke up sensor.\n");
        return ret;
    }

    /*let's try setting the Accelrometer explicitly to +/- 2g
     * writing 0x00 to ACCEL_CONFIG(0x1C)
     */
    ret = i2c_smbus_write_byte_data(client, 0x1C, 0x00);
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
    return 0;
}

static void mpu6050_remove(struct i2c_client *client) {
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
