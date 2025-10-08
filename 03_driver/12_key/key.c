#include "drv.h"

#define KEY_CNT  1
#define KEY_NAME "key"

#define KEY_VALUE 0XF0
#define INVAKEY   0X00

struct key_dev{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
    int key_gpio;
    atomic_t keyvalue;
};

struct key_dev key_dev;

static int keyio_init(void)
{
    key_dev.nd = of_find_node_by_path("/key");
    if (key_dev.nd == NULL) {
        return -EINVAL;
    }

    key_dev.key_gpio = of_get_named_gpio(key_dev.nd, "key-gpio", 0);
    if (key_dev.key_gpio < 0) {
        printk(KERN_ERR "can't get key0\n");
        return -EINVAL;
    }

    printk("key_gpio = %d\n", key_dev.key_gpio);

    gpio_request(key_dev.key_gpio, "key0");
    gpio_direction_input(key_dev.key_gpio);

    return 0;
}

static int key_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &key_dev;

    ret = keyio_init();
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;
    unsigned char value = 0;
    struct key_dev *dev = filp->private_data;

    if (gpio_get_value(dev->key_gpio) == 0) {
        while (!gpio_get_value(dev->key_gpio));
        atomic_set(&dev->keyvalue, KEY_VALUE);
    } else {
        atomic_set(&dev->keyvalue, INVAKEY);
    }

    value = atomic_read(&dev->keyvalue);
    ret = copy_to_user(buf, &value, sizeof(value));

    return ret;
}

static struct file_operations key_fops = {
    .owner = THIS_MODULE,
    .open = key_open,
    .read = key_read,
};

static int __init mykey_init(void)
{
    atomic_set(&key_dev.keyvalue, INVAKEY);

    if (key_dev.major) {
        key_dev.devid = MKDEV(key_dev.major, 0);
        register_chrdev_region(key_dev.devid, KEY_CNT, KEY_NAME);
    } else {
        alloc_chrdev_region(&key_dev.devid, 0, KEY_CNT, KEY_NAME);
        key_dev.major = MAJOR(key_dev.devid);
        key_dev.minor = MINOR(key_dev.devid);
    }

    key_dev.cdev.owner = THIS_MODULE;
    cdev_init(&key_dev.cdev, &key_fops);
    cdev_add(&key_dev.cdev, key_dev.devid, KEY_CNT);

    key_dev.class = class_create(THIS_MODULE, KEY_NAME);
    if (IS_ERR(key_dev.class)) {
        return PTR_ERR(key_dev.class);
    }

    key_dev.device = device_create(key_dev.class, NULL, key_dev.devid, NULL, KEY_NAME);
    if (IS_ERR(key_dev.device)) {
        return PTR_ERR(key_dev.device);
    }

    return 0;
}

static void __exit mykey_exit(void)
{
    gpio_free(key_dev.key_gpio);
    cdev_del(&key_dev.cdev);
    unregister_chrdev_region(key_dev.devid, KEY_CNT);

    device_destroy(key_dev.class, key_dev.devid);
    class_destroy(key_dev.class);
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xuemanyi");
