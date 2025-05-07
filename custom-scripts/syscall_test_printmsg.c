#include <stdio.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SYSCALL_PRINTMSG 387 // Use o número da sua syscall

int main(int argc, char** argv){
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <mensagem>\n", argv[0]);
        return 1;
    }

    const char *msg = argv[1];
    long ret;

    printf("Chamando a syscall 'printmsg' com a mensagem: '%s'\n", msg);

    ret = syscall(SYSCALL_PRINTMSG, msg);

    if (ret == 0) {
        printf("Syscall 'printmsg' executada com sucesso.\n");
    } else {
        perror("Erro na syscall 'printmsg'");
        printf("Valor de retorno: %ld\n", ret);
    }

    return 0;
}