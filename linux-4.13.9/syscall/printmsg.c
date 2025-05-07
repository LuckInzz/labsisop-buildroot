#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/string.h>
#include <linux/uaccess.h> // Para strncpy_from_user

#include "printmsg.h"

#define MAX_MSG_SIZE 256

asmlinkage long sys_printmsg(const char __user *user_msg) {
    char kernel_msg[MAX_MSG_SIZE];
    long copied_bytes;

    if (!user_msg) {
        printk(KERN_ERR "sys_printmsg: Ponteiro de mensagem do usuário é NULL\n");
        return -EFAULT; // Bad address
    }

    copied_bytes = strncpy_from_user(kernel_msg, user_msg, sizeof(kernel_msg) - 1);

    if (copied_bytes < 0) {
        printk(KERN_ERR "sys_printmsg: Erro ao copiar a mensagem do usuário (%ld)\n", copied_bytes);
        return copied_bytes;
    }

    kernel_msg[copied_bytes] = '\0'; // Garante que a string seja terminada corretamente

    printk(KERN_INFO "Mensagem do usuário: %s\n", kernel_msg);

    return 0; // Sucesso
}