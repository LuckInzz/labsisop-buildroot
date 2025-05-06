#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>  // necessário para copy_to_user
#include "listSleeping.h"

asmlinkage long sys_listSleepingProcesses(char __user *buf, int size) {
	struct task_struct *proces;
	char kbuf[1024]; // buffer no kernel
	int bufsz = 0;            // tamanho atual preenchido no buffer
	int ret;

	for_each_process(proces) {
		if (proces->state == TASK_INTERRUPTIBLE || proces->state == TASK_UNINTERRUPTIBLE) {
			char line[128];
			int written;

			// Prepara a linha
			written = snprintf(line, sizeof(line),
				"Process: %s | PID: %d | State: %ld\n",
				proces->comm,
				task_pid_nr(proces),
				proces->state);

			// Checa se há espaço suficiente no kbuf (incluindo o null terminator)
			if (bufsz + written + 1 > sizeof(kbuf)) {
				break; // sem espaço suficiente, evita overflow no kbuf
			}

			// Copia a linha para o buffer final
			memcpy(kbuf + bufsz, line, written);
			bufsz += written;
		}
	}

	// Adiciona o null terminator ao final do buffer do kernel
	if (bufsz < sizeof(kbuf)) {
		kbuf[bufsz] = '\0';
		bufsz++; // Incrementa o tamanho para incluir o null terminator
	}

	// Verifica se o buffer do usuário é suficiente
	if (bufsz > size) {
		return -EINVAL; // Retorna um código de erro mais específico (Invalid argument)
	}

	// Copia do buffer do kernel para o espaço do usuário
	ret = copy_to_user(buf, kbuf, bufsz);
	if (ret != 0) {
		return -EFAULT; // Retorna um código de erro para falha na cópia (Bad address)
	}

	return bufsz - ret; // retorna quantidade de bytes COPIADOS
}