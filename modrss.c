#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/moduleparam.h>
#include "modrss.h"

#define GPIO4 4
#define HIGH  1
#define LOW   0

typedef struct{
        char **groups;
        uint ngroups;
        char **sockets;
        uint nsockets;
        char **data;
        uint ndata;
        uint pulse_len;
} Encoder;

static dev_t rsswitch_dev; /* Global variable for the device number         */
static struct cdev c_dev;  /* Global variable for the char device structure */
static struct class *cl;   /* Global variable for the device class          */

static void transmit(int nhigh, int nlow)
{
	/*
	 * FIXME: 350 is the pulse length in us.
	 * This should be a parameter in the future,
	 * depending on the encoder chip within the
	 * remote control.
	 */
	gpio_set_value(GPIO4, HIGH);
	udelay(350 * nhigh);
	gpio_set_value(GPIO4, LOW);
	udelay(350 * nlow);
}

static void send_tris(char *codeword)
{
	int i = 0;
	while (codeword[i] != '\0') {
		switch (codeword[i]) {
		case '0':
			send_0();
			break;
		case '1':
			send_1();
			break;
		case 'F':
			send_f();
			break;
		}
		i++;
	}
	send_sync();
}

/**
 * Sends a Tri-State "0" Bit
 *            _     _
 * Waveform: | |___| |___
 */
static void send_0(void)
{
	transmit(1, 3);
	transmit(1, 3);
}
/**
 * Sends a Tri-State "1" Bit
 *            ___   ___
 * Waveform: |   |_|   |_
 */
static void send_1(void)
{
	transmit(3, 1);
	transmit(3, 1);
}

/**
 * Sends a Tri-State "F" Bit
 *            _     ___
 * Waveform: | |___|   |_
 */
static void send_f(void)
{
	transmit(1, 3);
	transmit(3, 1);
}

/**
 * Sends a "Sync" Bit
 *                       _
 * Waveform Protocol 1: | |_______________________________
 *                       _
 * Waveform Protocol 2: | |__________
 */

static void send_sync(void)
{
	transmit(1, 31);
}

static int driver_open(struct inode *i, struct file *f)
{
	pr_info("Driver: open()\n");
	return 0;
}

static int driver_close(struct inode *i, struct file *f)
{
	pr_info("Driver: close()\n");
	return 0;
}

static ssize_t driver_read(struct file *f, char __user *ubuf, size_t len,
			   loff_t *off)
{
	pr_info("Driver: read()\n");
	gpio_set_value(GPIO4, LOW);
	return 0;
}

static ssize_t driver_write(struct file *f, const char __user *ubuf,
			    size_t len, loff_t *off)
{
	pr_info("Driver: write()\n");
	char *kbuf;
	int not_copied;

	kbuf = kmalloc(len, GFP_KERNEL);
	if (!kbuf)
		//		return -ENOMEN;
		return -1;

	not_copied = copy_from_user(kbuf, ubuf, len);

	if (strncmp("Hello", kbuf, 5) == 0)
		pr_info("OK, got Hello");

	send_1();

	return len;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.read = driver_read,
	.write = driver_write
};

static int __init modrsswitch_init(void) /* Constructor */
{
	pr_info("modrsswitch: Module registered");

	/* cat /proc/devices | grep rsswitch */
	if (alloc_chrdev_region(&rsswitch_dev, 0, 1, "rsswitch") < 0) {
		return -1;
	}
	/* ll /sys/class/chardrv */
	if ((cl = class_create(THIS_MODULE, "chardrv")) == NULL) {
		unregister_chrdev_region(rsswitch_dev, 1);
		return -1;
	}

	/* ls /dev | grep rsswitch */
	if (device_create(cl, NULL, rsswitch_dev, NULL, "rsswitch") == ERR_PTR) {
		class_destroy(cl);
		unregister_chrdev_region(rsswitch_dev, 1);
		return -1;
	}

	cdev_init(&c_dev, &fops);

	if (cdev_add(&c_dev, rsswitch_dev, 1) == -1) {
		device_destroy(cl, rsswitch_dev);
		class_destroy(cl);
		unregister_chrdev_region(rsswitch_dev, 1);
		return -1;
	}

	/* Register GPIO and set to LOW */
	int ret = 0;
	ret = gpio_request_one(GPIO4, GPIOF_OUT_INIT_LOW, "GPIO4");
	if (ret) {
		pr_err("Unable to request GPIOs: %d\n", ret);
		return ret;
	}

	return 0;
}

static void __exit modrsswitch_exit(void) /* Destructor */
{
	gpio_set_value(GPIO4, 0);
	gpio_free(GPIO4);

	cdev_del(&c_dev);
	device_destroy(cl, rsswitch_dev);
	class_destroy(cl);
	unregister_chrdev_region(rsswitch_dev, 1);
	pr_info("modrsswitch: Module unregistered");
}

module_init(modrsswitch_init);
module_exit(modrsswitch_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Hornung");
MODULE_DESCRIPTION("Remote socket switch character driver");
