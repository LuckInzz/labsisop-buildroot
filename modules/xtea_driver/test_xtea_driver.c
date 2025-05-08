#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 512  // Tamanho do buffer de comunicação

int main() {
    int ret, fd;
    char receive[BUFFER_LENGTH];     // Buffer para leitura do driver
    char stringToSend[BUFFER_LENGTH]; // Buffer para comando de envio

    printf("Iniciando teste do driver XTEA...\n");

    // Abre o dispositivo xtea_driver para leitura e escrita
    fd = open("/dev/xtea_driver", O_RDWR);
    if (fd < 0) {
        perror("Erro ao abrir o dispositivo /dev/xtea_driver");
        return errno;
    }

    // Exemplo de comando: enc followed by key[0..3], tamanho, e os dados
    printf("Digite o comando no formato:\n");
    printf("enc <key0> <key1> <key2> <key3> <size> <dados_hexadecimais>\n");
    printf("Exemplo:\n");
    printf("enc f0e1d2c3 b4a59687 78695a4b 3c2d1e0f 16 aabbccddeeff00112233445566778899aabbccddeeff\n\n");

    // Lê a linha completa (com espaços)
    printf("Digite o comando:\n");
    scanf(" %[^\n]%*c", stringToSend);

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
