#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 512   // Tamanho do buffer de comunicação

int main() {
    int ret, fd;
    char receive[BUFFER_LENGTH];     // Buffer para leitura do driver
    char stringToSend[BUFFER_LENGTH]; // Buffer para comando de envio
    char command[10];
    unsigned int size;
    char data_hex[BUFFER_LENGTH];

    printf("Iniciando teste do driver XTEA (com chave por parâmetro)...\n");

    // Abre o dispositivo xtea_driver para leitura e escrita
    fd = open("/dev/xtea_driver", O_RDWR);
    if (fd < 0) {
        perror("Erro ao abrir o dispositivo /dev/xtea_driver");
        return errno;
    }

    // Exemplo de comando: enc/dec, tamanho, e os dados
    printf("Digite o comando no formato:\n");
    printf("<enc|dec> <size> <dados_hexadecimais>\n");
    printf("Exemplo de encriptação:\n");
    printf("enc 16 aabbccddeeff00112233445566778899aabbccddeeff\n");
    printf("Exemplo de decriptação:\n");
    printf("dec 32 89abcdef0123456789abcdef01234567aabbccddeeff00112233445566778899\n\n");

    printf("Digite o comando:\n");
    if (scanf("%9s %u %s", command, &size, data_hex) != 3) {
        fprintf(stderr, "Formato de comando inválido.\n");
        close(fd);
        return EXIT_FAILURE;
    }

    snprintf(stringToSend, BUFFER_LENGTH, "%s %u %s", command, size, data_hex);

    // Envia o comando para o driver
    printf("Enviando comando ao driver: [%s]\n", stringToSend);
    ret = write(fd, stringToSend, strlen(stringToSend));
    if (ret < 0) {
        perror("Falha ao escrever no driver");
        close(fd);
        return errno;
    }

    // Lê a resposta do driver (texto encriptado ou erro)
    ret = read(fd, receive, BUFFER_LENGTH);
    if (ret < 0) {
        perror("Falha ao ler a resposta do driver");
        close(fd);
        return errno;
    }

    // Exibe o conteúdo retornado
    printf("Resposta do driver: [%s]\n", receive);

    // Encerra
    close(fd);
    printf("Fim do teste.\n");

    return 0;
}