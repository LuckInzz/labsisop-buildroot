#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

// Define o nome do dispositivo que será criado em /dev/
#define DEVICE_NAME "xtea_driver"
// Define o nome da classe de dispositivo no sysfs (/sys/class/xtea)
#define CLASS_NAME "xtea"

// Licença do módulo (GPL é uma licença comum para módulos do kernel)
MODULE_LICENSE("GPL");
// Autor do módulo
MODULE_AUTHOR("Seu Nome");
// Descrição do módulo
MODULE_DESCRIPTION("Driver XTEA com suporte a chave definida pelo usuário");
// Versão do módulo
MODULE_VERSION("1.0");

// Declaração de ponteiros de caracteres estáticos para receber as partes da chave como parâmetros
static char *key0 = "00000000";
static char *key1 = "00000000";
static char *key2 = "00000000";
static char *key3 = "00000000";

// Utiliza a macro module_param para permitir que o usuário passe valores para as variáveis key0, key1, key2 e key3
// charp indica que o parâmetro é uma string (ponteiro para char)
// 0000 define as permissões de acesso ao parâmetro no sysfs (0000 significa que ninguém além do root pode alterá-lo)
module_param(key0, charp, 0000);
module_param(key1, charp, 0000);
module_param(key2, charp, 0000);
module_param(key3, charp, 0000);

// Fornece uma descrição para cada parâmetro que será exibida ao usar modinfo
MODULE_PARM_DESC(key0, "Primeira parte da chave");
MODULE_PARM_DESC(key1, "Segunda parte da chave");
MODULE_PARM_DESC(key2, "Terceira parte da chave");
MODULE_PARM_DESC(key3, "Quarta parte da chave");

// Declara um array de uint32_t para armazenar a chave convertida dos parâmetros de string
static uint32_t user_key[4];

// Variáveis para o dispositivo de caractere
static int majorNumber;                 // Major number alocado para o dispositivo
static char message[256] = {0};       // Buffer para armazenar dados lidos/escritos no dispositivo
static short size_of_message;          // Tamanho da mensagem armazenada
static int numberOpens = 0;            // Contador de quantas vezes o dispositivo foi aberto
static struct class* xteaClass  = NULL; // Ponteiro para a struct class do dispositivo
static struct device* xteaDevice = NULL; // Ponteiro para a struct device do dispositivo

// Protótipos das funções de operação do dispositivo
static int   dev_open(struct inode *, struct file *);
static int   dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

// Estrutura file_operations que associa as funções definidas acima às operações do dispositivo
static struct file_operations fops =
{
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// Função de cifra XTEA (implementação do algoritmo)
void encipher(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4]) {
    uint32_t i;
    uint32_t v0 = v[0], v1 = v[1], sum = 0;
    uint32_t delta = 0x9e3779b9;

    for (i = 0; i < num_rounds; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
    }

    v[0] = v0;
    v[1] = v1;
}

// Função de decifra XTEA (implementação do algoritmo inverso)
void decipher(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4]) {
    uint32_t i;
    uint32_t v0 = v[0], v1 = v[1], sum = 0xC6EF3720; // Valor inicial de sum para decifração
    uint32_t delta = 0x9e3779b9;

    for (i = 0; i < num_rounds; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }

    v[0] = v0;
    v[1] = v1;
}

// Função de inicialização do módulo (executada quando o módulo é carregado)
static int __init xtea_init(void){
    int ret;

    printk(KERN_INFO "xtea_driver: Inicializando\n");

    // Converte as strings da chave (key0, key1, key2, key3) para inteiros de 32 bits (hexadecimal)
    // kstrtou32 retorna 0 em caso de sucesso e um erro em caso de falha
    ret  = kstrtou32(key0, 16, &user_key[0]);
    ret |= kstrtou32(key1, 16, &user_key[1]);
    ret |= kstrtou32(key2, 16, &user_key[2]);
    ret |= kstrtou32(key3, 16, &user_key[3]);

    // Verifica se houve algum erro na conversão das chaves
    if (ret) {
        printk(KERN_ALERT "xtea_driver: Erro ao converter chaves\n");
        return -EINVAL; // Retorna um código de erro de argumento inválido
    }

    // Imprime a chave que foi definida (em formato hexadecimal)
    printk(KERN_INFO "xtea_driver: Chave definida: %08x %08x %08x %08x\n",
           user_key[0], user_key[1], user_key[2], user_key[3]);

    // Registra o dispositivo de caractere para obter um major number dinamicamente
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        printk(KERN_ALERT "xtea_driver: Falha ao registrar major number\n");
        return majorNumber; // Retorna o código de erro da função register_chrdev
    }

    // Cria a classe do dispositivo no sysfs
    xteaClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(xteaClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME); // Desfaz o registro do major number em caso de falha
        printk(KERN_ALERT "xtea_driver: Falha ao criar classe\n");
        return PTR_ERR(xteaClass); // Retorna o código de erro do ponteiro
    }

    // Cria o nó do dispositivo no diretório /dev/ (ex: /dev/xtea_driver)
    xteaDevice = device_create(xteaClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(xteaDevice)) {
        class_destroy(xteaClass);           // Destrói a classe criada
        unregister_chrdev(majorNumber, DEVICE_NAME); // Desfaz o registro do major number
        printk(KERN_ALERT "xtea_driver: Falha ao criar device\n");
        return PTR_ERR(xteaDevice); // Retorna o código de erro do ponteiro
    }

    printk(KERN_INFO "xtea_driver: Dispositivo criado com sucesso\n");
    return 0; // Indica que a inicialização foi bem-sucedida
}

// Função de saída do módulo (executada quando o módulo é removido)
static void __exit xtea_exit(void){
    device_destroy(xteaClass, MKDEV(majorNumber, 0)); // Destrói o device criado
    class_unregister(xteaClass);                     // Desregistra a classe
    class_destroy(xteaClass);                        // Destrói a classe
    unregister_chrdev(majorNumber, DEVICE_NAME);      // Desfaz o registro do major number
    printk(KERN_INFO "xtea_driver: Módulo removido\n");
}

// Função chamada quando o dispositivo é aberto por um processo do usuário
static int dev_open(struct inode *inodep, struct file *filep){
    numberOpens++; // Incrementa o contador de aberturas
    printk(KERN_INFO "xtea_driver: Dispositivo aberto %d vezes\n", numberOpens);
    return 0; // Indica sucesso
}

// Função chamada quando um processo do usuário tenta ler do dispositivo
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
    // Copia dados do kernel space (message) para o user space (buffer)
    int error_count = copy_to_user(buffer, message, size_of_message);
    if (error_count==0){
        printk(KERN_INFO "xtea_driver: Enviou %d caracteres para o usuário\n", size_of_message);
        return (size_of_message = 0); // Reseta o tamanho da mensagem após a leitura
    } else {
        printk(KERN_INFO "xtea_driver: Falha ao enviar %d caracteres para o usuário\n", error_count);
        return -EFAULT; // Retorna um código de erro de falha na cópia
    }
}

// Função chamada quando um processo do usuário tenta escrever no dispositivo
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
    uint32_t data[2]; // Buffer para armazenar 8 bytes de dados

    // Verifica se a quantidade de dados escrita é suficiente para a operação XTEA (8 bytes)
    if (len < sizeof(data)) {
        printk(KERN_WARNING "xtea_driver: Dados insuficientes para cifrar\n");
        return -EINVAL; // Retorna um código de erro de argumento inválido
    }

    // Copia dados do user space (buffer) para o kernel space (data)
    if (copy_from_user(data, buffer, sizeof(data))) {
        return -EFAULT; // Retorna um código de erro de falha na cópia
    }

    // Cifra os dados usando a função encipher e a chave definida pelo usuário
    encipher(32, data, user_key);

    // Copia o texto cifrado para o buffer de mensagem do kernel
    memcpy(message, data, sizeof(data));
    size_of_message = sizeof(data); // Atualiza o tamanho da mensagem

    printk(KERN_INFO "xtea_driver: Mensagem cifrada armazenada\n");
    return len; // Retorna o número de bytes escritos pelo usuário
}

// Função chamada quando o dispositivo é fechado por um processo do usuário
static int dev_release(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "xtea_driver: Dispositivo fechado\n");
    return 0; // Indica sucesso
}

// Macros para registrar as funções de inicialização e saída do módulo
module_init(xtea_init);
module_exit(xtea_exit);