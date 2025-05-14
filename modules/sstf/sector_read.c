/*
 * Stress test for SSTF IO scheduler
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFFER_LENGTH 512
#define DISK_SZ 1073741824  // 1GB disco (ajuste conforme necessário)
#define FORKS 5            // Número de processos
#define READS 10            // Requisições por processo

int main(int argc, char *argv[])
{
	int fd, i;
	unsigned int pos;
	char buf[BUFFER_LENGTH];
    int num_forks = FORKS;
    int num_reads = READS;
    if (argc > 1) num_forks = atoi(argv[1]);
    if (argc > 2) num_reads = atoi(argv[2]);

	// Limpa caches para evitar leituras cacheadas
	printf("Cleaning disk cache...\n");
	system("echo 3 > /proc/sys/vm/drop_caches");

	// Configura a fila de escalonamento do disco
	printf("Configuring scheduling queues...\n");
	system("echo 2 > /sys/block/sdb/queue/nomerges");
	system("echo 4 > /sys/block/sdb/queue/max_sectors_kb"); 
	system("echo 0 > /sys/block/sdb/queue/read_ahead_kb");
	system("echo sstf > /sys/block/sdb/queue/scheduler");

	// Cria processos para gerar requisições concorrentes
	printf("Forking %d processes with %d reads each...\n", num_forks, num_reads);

	for (i = 0; i < num_forks; i++) {
		if (fork() == 0) {
			srand(getpid()); // semente única por processo

			fd = open("/dev/sdb", O_RDWR);
			if (fd < 0) {
				perror("Failed to open the device...");
				exit(errno);
			}

			for (int j = 0; j < num_reads; j++) {
				pos = rand() % (DISK_SZ >> 9); // setor aleatório
				lseek(fd, pos * 512, SEEK_SET);
				read(fd, buf, 100);
			}

			close(fd);
			exit(0);
		}
	}

	printf("Test completed.\n");
	return 0;
}
