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
    int ret;
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

    if (*ppos > 0)
        return 0;

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
    ret = snprintf(kernel_buf, sizeof(kernel_buf), "A: %d %d %d | G %d %d %d\n",
                   accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);

    /* Kernel buf to User buf */
    if (copy_to_user(user_buf, kernel_buf + *ppos, min(count, (size_t)len))) {
        goto unlock_and_exit;
        ret = -EFAULT;
    }

    *ppos += len;

 unlock_and_exit:
    mutex_unlock(&mpu_mutex);
    return ret;
}

static const struct file_operations mpu_fops = {
    .owner = THIS_MODULE,
    .read = mpu_read,
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

    ret = i2c_smbus_write_byte_data(client, 0x6B, 0x00);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to woke up sensor.\n");
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
    mpu_client = NULL;
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
