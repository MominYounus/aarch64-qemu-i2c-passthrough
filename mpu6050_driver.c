 #include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-smbus.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

static struct i2c_client *mpu_client;

static ssize_t mpu_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	s32 accel_x_h, accel_x_l;
	s16 accel_x;
	char kernel_buf[32];
	int len;
	
	if (*ppos > 0)
		return 0;

	if (!mpu_client)
		return -ENODEV;

	/* High Byte = 0x3B, Low Byte = 0x3C */
	accel_x_h = i2c_smbus_read_byte_data(mpu_client, 0x3B);
	accel_x_l = i2c_smbus_read_byte_data(mpu_client, 0x3C);

	/* Two 8 bit values(accel_x_h and accel_y_h) in a single 16 bit(accel_x) */
	accel_x = (accel_x_h << 8) | accel_x_l;

	/* Readable string for cat command */
	len = snprintf(kernel_buf, sizeof(kernel_buf), "Accel X: %d\n", accel_x);

	/* Kernel buf to User buf */
	if(copy_to_user(user_buf, kernel_buf + *ppos, len))
		return -EFAULT;
	
	*ppos += len;
	return len;
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

static int mpu6050_probe(struct i2c_client *client)
{
	s32 ret;
	
	dev_info(&client->dev, "probe triggered for address: 0x%02x\n", client->addr);

	ret = i2c_smbus_write_byte_data(client, 0x6B, 0x00);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to woke up sensor.\n");
		return ret;
	}

	mpu_client = client; //Storing the client for read function

	ret = misc_register(&mpu_misc);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register misc device.\n");
		return ret;
	}
	
	dev_info(&client->dev, "/dev/mpu_sensor successfully created\n");
	return 0;
}

static void mpu6050_remove(struct i2c_client *client)
{
	misc_deregister(&mpu_misc);
	mpu_client = NULL;
	dev_info(&client->dev, "Removed MPU6050 and /dev/mpu_sensor.\n");
}

static const struct i2c_device_id mpu_sensor_id[] = {
	{"mpu6050", 0},
	{},
};

static struct i2c_driver mpu6050_driver = {
	.driver = {
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
