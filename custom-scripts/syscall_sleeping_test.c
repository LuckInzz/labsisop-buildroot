#include <stdio.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h> // Para acessar códigos de erro como EINVAL

#define SYSCALL_LISTSLEEPING 386

int main() {
	char buf[1024];	// Buffer to hold the list of sleeping processes (corrigi o tamanho para corresponder ao erro)
	long ret;

	ret = syscall(SYSCALL_LISTSLEEPING, buf, sizeof(buf));

	if (ret > 0) {
		printf("Sleeping processes (%ld bytes):\n%s\n", ret, buf);
	} else {
		printf("Syscall failed with return value: %ld\n", ret);
		if (ret == -EINVAL) {
			perror("Error: User buffer too small");
		} else if (ret == -EFAULT) {
			perror("Error: Failed to copy data to user buffer");
		} else {
			perror("Error: Unknown syscall error");
		}
	}
	return 0;
}