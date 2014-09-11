#include <linux/cdev.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>


#define GPIO4  4  /* The default GPIO pin */
#define REPEAT 1  /* Times to repeat the codeword */
#define HIGH   1
#define LOW    0

/*
 * Each encoder chip used in the power socket remote controls can be described
 * with the following struct.
*/
struct Encoder {
	char **groups;    /* Group identifiers                  */
	uint ngroups;     /* Number of groups                   */
	char **sockets;   /* Socket identifiers                 */
	uint nsockets;    /* Number of sockets within one group */
	char **data;      /* On or off                          */
	uint ndata;       /* On or off => 2                     */
	uint pulse_len;   /* This might differ                  */
};

static dev_t rsswitch_dev;   /* Device number         */
static struct cdev c_dev;    /* Char device structure */
static struct class *cl;     /* Device class          */

/* The 433 MHz sender must be connected to one of these pins */
static uint valid_gpios[] = {4, 17, 21, 22, 23, 24, 25};

static int send_pin = GPIO4; /* The default pin */
module_param(send_pin, int, 0644);
MODULE_PARM_DESC(send_pin, "GPIO the 433 MHz sender is connected to");

static void transmit(int nhigh, int nlow)
{
	/*
	 * FIXME: 350 is the pulse length in us. his should be a parameter in
	 * the future, depending on the encoder chip within the remote control.
	 */
	gpio_set_value(send_pin, HIGH);
	udelay(350 * nhigh);
	gpio_set_value(send_pin, LOW);
	udelay(350 * nlow);
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

static void send_tris(char *codeword)
{
	int i = 0;
	while (codeword[i] != '\0') {
		switch (codeword[i]) {
		case '0':
			send_0();
			pr_debug("modrss: send_0()\n");
			break;
		case '1':
			send_1();
			pr_debug("modrss: send_1()\n");
			break;
		case 'F':
			send_f();
			pr_debug("modrss: send_F()\n");
			break;
		}
		i++;
	}
	send_sync();
}

/**
 * Configure struct for the PT2260 encoder
 * @param pt2260     Pointer to a pt2260 instance
 */
static int pt2260_init(struct Encoder *pt2260)
{
	char * const groups[]  = {"1FFF", "F1FF", "FF1F", "FFF1"};
	char * const sockets[] = {"1FF0", "F1F0", "FF10"};
	char * const data[]    = {"0001", "0010"};
	int i;

	/* Four possible switch groups */
	pt2260->ngroups = 4;
	pt2260->groups = kmalloc(pt2260->ngroups * sizeof(char *), GFP_KERNEL);
	if (pt2260->groups == NULL) {
		pr_err("modrss: Cannot kmalloc\n");
		return -1;
	}
	/* Three possible switches per group */
	pt2260->nsockets = 3;
	pt2260->sockets = kmalloc(pt2260->nsockets * sizeof(char *),
				  GFP_KERNEL);
	if (pt2260->sockets == NULL) {
		pr_err("modrss: Cannot kmalloc\n");
		return -1;
	}

	/* Data is either "On" or "Off" */
	pt2260->ndata = 2;
	pt2260->data = kmalloc(pt2260->ndata * sizeof(char *), GFP_KERNEL);
	if (pt2260->data == NULL) {
		pr_err("modrss: Cannot kmalloc\n");
		return -1;
	}

	for (i = 0; i < pt2260->ngroups; i++)
		pt2260->groups[i] = groups[i];

	for (i = 0; i < pt2260->nsockets; i++)
		pt2260->sockets[i] = sockets[i];

	for (i = 0; i < pt2260->ndata; i++)
		pt2260->data[i] = data[i];

	return 0;
}

/**
 * Configure struct for the PT2262 encoder
 * @param *pt2262     Pointer to a pt2262 instance
 */
static int pt2262_init(struct Encoder *pt2262)
{
	char * const groups[]   = {"FFFF", "0FFF", "F0FF", "00FF",
						"FF0F", "0F0F", "F00F", "000F",
						"FFF0", "0FF0", "F0F0", "00F0",
						"FF00", "0F00", "F000", "0000"};
	char * const sockets[]  = {"F0FF", "FF0F", "FFF0", "FFFF"};
	char * const data[]     = {"FFF0", "FF0F"};
	int i;

	/* 16 possible switch groups (A-P in Intertechno code) */
	pt2262->ngroups = 16;
	pt2262->groups = kmalloc(pt2262->ngroups * sizeof(char *), GFP_KERNEL);
	if (pt2262->groups == NULL) {
		pr_err("modrss: Cannot kmalloc\n");
		return -1;
	}

	/* Four possible switches per group */
	pt2262->nsockets = 4;
	pt2262->sockets = kmalloc(pt2262->nsockets * sizeof(char *),
				  GFP_KERNEL);
	if (pt2262->sockets == NULL) {
		pr_err("modrss: Cannot kmalloc\n");
		return -1;
	}

	/* Data is either "On" or "Off" */
	pt2262->ndata = 2;
	pt2262->data = kmalloc(pt2262->ndata * sizeof(char *), GFP_KERNEL);
	if (pt2262->data == NULL) {
		pr_err("modrss: Cannot kmalloc\n");
		return -1;
	}

	for (i = 0; i < pt2262->ngroups; i++)
		pt2262->groups[i] = groups[i];

	for (i = 0; i < pt2262->nsockets; i++)
		pt2262->sockets[i] = sockets[i];

	for (i = 0; i < pt2262->ndata; i++)
		pt2262->data[i] = data[i];

	return 0;
}

/**
 * Emulate an encoder chip
 * @param *enc          Pointer to an encoder instance
 * @param uint group    Socket group
 * @param uint socket   Socket within group
 * @param uint data     Data to send
 */
static int socket_ctrl(struct Encoder *enc, uint group, uint socket, uint data)
{
	int i;

	/* Calculate the codeword size */
	size_t s = strlen(enc->groups[group]) +
		strlen(enc->sockets[socket]) +
		strlen(enc->data[data]);

	/* Generate the codeword including '\0' */
	char codeword[s];
	snprintf(codeword, s + 1, "%s%s%s",
		 enc->groups[group],
		 enc->sockets[socket],
		 enc->data[data]);

	pr_debug("codeword: %s\n", codeword);

	/* Send the codeword */
	for (i = 0; i < REPEAT; i++)
		send_tris(codeword);

	return 0;
}

static int socket_send(uint dev, uint group, uint socket, uint data)
{
	struct Encoder encoder;

	switch (dev) {
	case 0:
		pt2260_init(&encoder);
		break;
	case 1:
		pt2262_init(&encoder);
		break;
	default:
		pr_err("modrss: Unknown encoder type.\n");
		return -1;
	}

	socket_ctrl(&encoder, group, socket, data);

	return 0;
}

static int driver_open(struct inode *i, struct file *f)
{
	return 0;
}

static int driver_close(struct inode *i, struct file *f)
{
	return 0;
}

static ssize_t driver_read(struct file *f, char __user *ubuf, size_t len,
			   loff_t *off)
{
	return 0;
}

static ssize_t driver_write(struct file *f, const char __user *ubuf,
			    size_t len, loff_t *off)
{
	char *kbuf;
	uint encoder, group, socket, data;
	int stat, i, data_len;

	kbuf = kmalloc(len, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, ubuf, len)) {
		pr_err("Error: Unable to read user input\n");
		kfree(kbuf);
		return -EFAULT;
	}

	data_len = strlen(kbuf);

	/* Check for valid hex values from user space */
	for (i = 0; i < data_len; i++) {
		if ((kbuf[i] >= 'a') && (kbuf[i] <= 'f')) {
			kbuf[i] = kbuf[i] - 'a';
		} else if ((kbuf[i] >= 'A') && (kbuf[i] <= 'F')) {
			kbuf[i] = kbuf[i] - 'A';
		} else if ((kbuf[i] >= '0') && (kbuf[i] <= '9')) {
			kbuf[i] = kbuf[i] - '0';
		} else {
			pr_err("modrss: Only characters 0-9, a-f, and A-F.\n");
			return -1;
		}
	}

	pr_debug("modrss: socket_ctrl(%d, %d, %d, %d)\n", (uint)kbuf[0],
		 (uint)kbuf[1], (uint)kbuf[2], (uint)kbuf[3]);

	socket_send((uint)kbuf[0], (uint)kbuf[1], (uint)kbuf[2], (uint)kbuf[3]);

	return len;
}

static const struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = driver_open,
	.release = driver_close,
	.read    = driver_read,
	.write   = driver_write
};

static struct miscdevice modrss_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rsswitch",
	.fops = &fops
};

static int __init modrsswitch_init(void)
{
	int ret, i, valid;

	pr_debug("modrss: Module registered");

	misc_register(&modrss_dev);

	valid = 0;
	/* Check for valid GPIO */
	for (i = 0; i < ARRAY_SIZE(valid_gpios); i++) {
		if (send_pin == valid_gpios[i]) {
			valid = 1;
			break;
		}
	}

	if (valid) {
		send_pin = GPIO4;
		pr_err("modrss: GPIO %d not supported, using default GPIO %d\n",
		       send_pin, GPIO4);
	}

	/* Register GPIO and set to LOW */
	ret = gpio_request_one(send_pin, GPIOF_OUT_INIT_LOW, "send_pin");
	if (ret) {
		pr_err("modrss: Unable to request GPIO: %d\n", ret);
		return ret;
	}

	pr_debug("modrss: Using GPIO %d\n", send_pin);

	return 0;
}

static void __exit modrsswitch_exit(void)
{
	gpio_set_value(send_pin, 0);
	gpio_free(send_pin);

	cdev_del(&c_dev);
	device_destroy(cl, rsswitch_dev);
	class_destroy(cl);
	unregister_chrdev_region(rsswitch_dev, 1);
	pr_debug("modrss: Module unregistered");
}

module_init(modrsswitch_init);
module_exit(modrsswitch_exit);

MODULE_AUTHOR("Michael Hornung");
MODULE_DESCRIPTION("Remote socket switch character driver");
MODULE_LICENSE("GPL");
