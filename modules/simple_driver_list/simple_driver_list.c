#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/cdev.h> // Incluir para struct cdev
#include <linux/mutex.h> // Para controle de acesso concorrente

#define DEVICE_NAME "simple_driver_list"
#define CLASS_NAME "simple_class_list"
#define MAX_MSG_SIZE 256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Simple driver with linked list for messages");
MODULE_VERSION("0.3");

struct message_node {
    char data[MAX_MSG_SIZE];
    struct list_head list;
};

static int majorNumber;
static struct class *charClass = NULL;
static struct device *charDevice = NULL;
static int numberOpens = 0;
static struct list_head message_list;
static struct cdev char_device;
static struct mutex list_mutex; // Mutex para proteger o acesso à lista

// Protótipos das funções do driver
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int __init simple_list_init(void) {
    int ret; // Declarei aqui para usar
    printk(KERN_INFO "Simple List Driver: Initializing the LKM\n");

    INIT_LIST_HEAD(&message_list);
    mutex_init(&list_mutex); // Inicializa o mutex

    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        printk(KERN_ALERT "Simple List Driver: failed to register a major number\n");
        return majorNumber;
    }
    printk(KERN_INFO "Simple List Driver: registered correctly with major number %d\n", majorNumber);

    charClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(charClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Simple List Driver: failed to register device class\n");
        return PTR_ERR(charClass);
    }
    printk(KERN_INFO "Simple List Driver: device class registered correctly\n");

    cdev_init(&char_device, &fops); // Inicializa a struct cdev
    char_device.owner = THIS_MODULE;
    ret = cdev_add(&char_device, MKDEV(majorNumber, 0), 1); // Adiciona o dispositivo
    if (ret < 0) {
        class_destroy(charClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        mutex_destroy(&list_mutex); // Limpa o mutex em caso de erro
        printk(KERN_ALERT "Simple List Driver: failed to add cdev\n");
        return ret;
    }

    charDevice = device_create(charClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(charDevice)) {
        cdev_del(&char_device); // Limpa o cdev
        class_destroy(charClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        mutex_destroy(&list_mutex); // Limpa o mutex em caso de erro
        printk(KERN_ALERT "Simple List Driver: failed to create the device\n");
        return PTR_ERR(charDevice);
    }
    printk(KERN_INFO "Simple List Driver: device class created correctly\n");

    return 0;
}

static void __exit simple_list_exit(void) {
    struct message_node *node, *temp;

    mutex_lock(&list_mutex); // Protege o acesso à lista durante a saída
    list_for_each_entry_safe(node, temp, &message_list, list) {
        list_del(&node->list);
        kfree(node);
    }
    mutex_unlock(&list_mutex); // Libera o mutex

    device_destroy(charClass, MKDEV(majorNumber, 0));
    cdev_del(&char_device); // Remove o cdev
    class_unregister(charClass);
    class_destroy(charClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    mutex_destroy(&list_mutex); // Libera o mutex
    printk(KERN_INFO "Simple List Driver: goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep) {
    numberOpens++;
    printk(KERN_INFO "Simple List Driver: device has been opened %d time(s)\n", numberOpens);
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "Simple List Driver: device successfully closed\n");
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    struct message_node *node = NULL;
    char *msg_to_user = NULL;
    int bytes_read = 0;

    mutex_lock(&list_mutex); // Protege o acesso à lista para leitura

    if (list_empty(&message_list)) {
        printk(KERN_INFO "Simple List Driver: read - no messages in the list\n");
        mutex_unlock(&list_mutex); // Libera o mutex antes de retornar
        return 0; // No more messages
    }

    node = list_first_entry(&message_list, struct message_node, list);

    list_del(&node->list);

    msg_to_user = node->data;
    bytes_read = strlen(msg_to_user);

    printk(KERN_INFO "Simple List Driver: dev_read - bytes_read = %d, len = %zu, msg_to_user: '%s'", bytes_read, len, msg_to_user);

    mutex_unlock(&list_mutex); // Libera o mutex antes de chamar copy_to_user

    if (copy_to_user(buffer, msg_to_user, min((size_t)bytes_read, len))) {
        kfree(node);
        return -EFAULT;
    }

    kfree(node);
    return min((size_t)bytes_read, len);
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    struct message_node *new_node;
    char *kernel_buffer;
    size_t to_copy = min((size_t)MAX_MSG_SIZE - 1, len);

    kernel_buffer = kmalloc(to_copy + 1, GFP_KERNEL);
    if (!kernel_buffer) {
        printk(KERN_ERR "Simple List Driver: write - kmalloc failed\n");
        return -ENOMEM;
    }
    memset(kernel_buffer, 0, to_copy + 1);

    if (copy_from_user(kernel_buffer, buffer, to_copy)) {
        kfree(kernel_buffer);
        printk(KERN_ERR "Simple List Driver: write - copy_from_user failed\n");
        return -EFAULT;
    }
    kernel_buffer[to_copy] = '\0';

    new_node = kmalloc(sizeof(struct message_node), GFP_KERNEL);
    if (!new_node) {
        kfree(kernel_buffer);
        printk(KERN_ERR "Simple List Driver: write - kmalloc for node failed\n");
        return -ENOMEM;
    }
    memset(new_node->data, 0, MAX_MSG_SIZE);
    strncpy(new_node->data, kernel_buffer, MAX_MSG_SIZE - 1);
    kfree(kernel_buffer);

    mutex_lock(&list_mutex); // Protege o acesso à lista para escrita
    list_add_tail(&new_node->list, &message_list);
    mutex_unlock(&list_mutex); // Libera o mutex

    printk(KERN_INFO "Simple List Driver: wrote %zu bytes from user\n", len);
    return len;
}

module_init(simple_list_init);
module_exit(simple_list_exit);