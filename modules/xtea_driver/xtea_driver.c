#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/cdev.h>

#define DEVICE_NAME "xtea_driver"
#define BUF_SIZE 4096
#define XTEA_NUM_ROUNDS 32

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Seu Nome");
MODULE_DESCRIPTION("Driver de criptografia XTEA");

static int major;
static struct cdev xtea_cdev;
static struct class *xtea_class;

static char result_buf[BUF_SIZE];
static size_t result_size = 0;
static int result_ready = 0;

// Função XTEA - criptografia
void xtea_encipher(uint32_t num_rounds, uint32_t v[2], const uint32_t key[4]) {
    uint32_t i, v0 = v[0], v1 = v[1], sum = 0, delta = 0x9E3779B9;
    for (i = 0; i < num_rounds; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
    }
    v[0] = v0; v[1] = v1;
}

// Função XTEA - descriptografia
void xtea_decipher(uint32_t num_rounds, uint32_t v[2], const uint32_t key[4]) {
    uint32_t i, v0 = v[0], v1 = v[1], delta = 0x9E3779B9, sum = delta * num_rounds;
    for (i = 0; i < num_rounds; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }
    v[0] = v0; v[1] = v1;
}

// Função de leitura do dispositivo
static ssize_t xtea_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    if (!result_ready)
        return 0;

    if (count > result_size)
        count = result_size;

    if (copy_to_user(buf, result_buf, count))
        return -EFAULT;

    result_ready = 0; // limpa flag após leitura
    return count;
}

// Função auxiliar para converter string hexadecimal para binário
static int hexstr_to_bin(const char *hex, uint8_t *bin, size_t bin_size) {
    int i;
    for (i = 0; i < bin_size; i++) {
        if (sscanf(hex + i * 2, "%2hhx", &bin[i]) != 1)
            return -EINVAL;
    }
    return 0;
}

// Função de escrita no dispositivo
static ssize_t xtea_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char *kbuf;
    char *token;
    char *cmd, *key_str[4], *size_str, *data_str;
    uint32_t key[4];
    uint8_t *raw_data;
    uint32_t *blocks;
    size_t i, block_count;

    // Aloca e copia o buffer da aplicação
    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    kbuf[count] = '\0';

    // Tokeniza a entrada
    token = strsep(&kbuf, " ");
    if (!token) goto invalid_format;
    cmd = token;

    for (i = 0; i < 4; i++) {
        token = strsep(&kbuf, " ");
        if (!token) goto invalid_format;
        key_str[i] = token;
    }

    token = strsep(&kbuf, " ");
    if (!token) goto invalid_format;
    size_str = token;

    data_str = kbuf;
    if (!data_str) goto invalid_format;

    // Converte chave
    for (i = 0; i < 4; i++) {
        if (kstrtou32(key_str[i], 16, &key[i]))
            goto invalid_format;
    }

    // Converte tamanho
    unsigned long data_size;
    if (kstrtoul(size_str, 10, &data_size))
        goto invalid_format;

    if (data_size > BUF_SIZE || data_size % 8 != 0)
        goto invalid_format;

    raw_data = kmalloc(data_size, GFP_KERNEL);
    if (!raw_data) {
        kfree(kbuf);
        return -ENOMEM;
    }

    if (hexstr_to_bin(data_str, raw_data, data_size)) {
        kfree(raw_data);
        kfree(kbuf);
        return -EINVAL;
    }

    block_count = data_size / 8;
    blocks = (uint32_t *)raw_data;

    for (i = 0; i < block_count; i++) {
        if (strcmp(cmd, "enc") == 0)
            xtea_encipher(XTEA_NUM_ROUNDS, &blocks[i * 2], key);
        else if (strcmp(cmd, "dec") == 0)
            xtea_decipher(XTEA_NUM_ROUNDS, &blocks[i * 2], key);
        else {
            kfree(raw_data);
            kfree(kbuf);
            return -EINVAL;
        }
    }

    // Prepara buffer de saída
    result_size = data_size * 2;
    for (i = 0; i < data_size; i++)
        sprintf(&result_buf[i * 2], "%02x", raw_data[i]);

    result_ready = 1;

    kfree(raw_data);
    kfree(kbuf);
    return count;

invalid_format:
    kfree(kbuf);
    return -EINVAL;
}

// Estrutura de operações do dispositivo
static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = xtea_read,
    .write = xtea_write,
};

// Função de inicialização do módulo
static int __init xtea_init(void) {
    dev_t dev;
    int ret;

    // Aloca major dinamicamente
    ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (ret < 0) return ret;

    major = MAJOR(dev);
    cdev_init(&xtea_cdev, &fops);
    xtea_cdev.owner = THIS_MODULE;

    ret = cdev_add(&xtea_cdev, dev, 1);
    if (ret < 0) return ret;

    xtea_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(xtea_class)) return PTR_ERR(xtea_class);

    device_create(xtea_class, NULL, dev, NULL, DEVICE_NAME);
    printk(KERN_INFO "xtea_driver carregado. Major: %d\n", major);
    return 0;
}

// Função de saída do módulo
static void __exit xtea_exit(void) {
    dev_t dev = MKDEV(major, 0);
    device_destroy(xtea_class, dev);
    class_destroy(xtea_class);
    cdev_del(&xtea_cdev);
    unregister_chrdev_region(dev, 1);
    printk(KERN_INFO "xtea_driver descarregado.\n");
}

module_init(xtea_init);
module_exit(xtea_exit);
