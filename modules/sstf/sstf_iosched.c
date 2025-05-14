/*
 * SSTF IO Scheduler
 *
 * For Kernel 4.13.9
 */

#include <linux/blkdev.h> // Inclui o header para operações de bloco de dispositivos.
#include <linux/elevator.h> // Inclui o header para a estrutura do elevador (escalonador de E/S).
#include <linux/bio.h> // Inclui o header para a estrutura de buffer de E/S (bio).
#include <linux/module.h> // Inclui o header para programação de módulos do kernel.
#include <linux/slab.h> // Inclui o header para alocação de memória do kernel (slab allocator).
#include <linux/init.h> // Inclui o header para funções de inicialização.
#include <linux/list.h> // Inclui o header para manipulação de listas encadeadas do kernel.
#include <linux/kernel.h> // Inclui o header para funcionalidades básicas do kernel.
#include <linux/printk.h> // Inclui o header para funções de impressão no kernel.

/*
 * struct sstf_data - Estrutura de dados específica para o escalonador SSTF.
 *
 * Esta estrutura armazena informações necessárias para o funcionamento do
 * escalonador SSTF para cada fila de requisições.
 */
struct sstf_data {
    struct list_head queue;          // Lista encadeada para armazenar as requisições pendentes.
    sector_t last_sector;           // Guarda o número do último setor que foi atendido.
    bool first_request_dispatched; // Flag para indicar se a primeira requisição já foi despachada.
    unsigned long add_sequence;      // Contador para registrar a ordem em que as requisições são adicionadas.
    unsigned long long total_arrival_distance; // Contador para a distância total percorrida na ordem de chegada.
    sector_t previous_arrival_sector;   // Guarda o setor da requisição anterior na ordem de chegada.
    unsigned long add_count;           // Contador de requisições adicionadas.
    unsigned long long total_dispatch_distance; // Contador para a distância total percorrida na ordem de atendimento.
    sector_t previous_dispatch_sector;  // Guarda o setor da requisição anterior na ordem de atendimento.
    unsigned long dsp_sequence;      // Contador para registrar a ordem em que as requisições são despachadas.
};

/*
 * sstf_merged_requests - Função chamada quando uma nova requisição é mesclada com uma existente.
 */
static void sstf_merged_requests(struct request_queue *q, struct request *rq,
                                 struct request *next)
{
    list_del_init(&next->queuelist); // Remove a requisição 'next' da lista, pois ela foi mesclada com 'rq'.
    printk(KERN_INFO "[SSTF] merged R %llu with %llu\n", (unsigned long long)blk_rq_pos(rq), (unsigned long long)blk_rq_pos(next)); // Imprime mensagem de mesclagem.
}

/*
 * sstf_dispatch - Função para selecionar e despachar a próxima requisição a ser atendida. - Modificado
 */
static int sstf_dispatch(struct request_queue *q, int force) {
    struct sstf_data *nd = q->elevator->elevator_data; // Obtém a estrutura de dados SSTF.
    struct request *rq, *next, *best = NULL; // Declara ponteiros para percorrer a lista.
    sector_t best_dist = (sector_t)-1; // Inicializa a melhor distância.
    char direction = 'R'; // Define a direção padrão como leitura.
    sector_t current_sector;

    list_for_each_entry_safe(rq, next, &nd->queue, queuelist) { // Percorre a lista de requisições de forma segura.
        sector_t pos = blk_rq_pos(rq); // Obtém a posição do setor da requisição atual.
        // Calcula a distância absoluta entre o setor da requisição atual (pos) e o último setor atendido (nd->last_sector).
        sector_t dist = pos > nd->last_sector ? pos - nd->last_sector : nd->last_sector - pos;

        if (dist < best_dist) { // Se a distância atual for menor que a melhor distância.
            best = rq; // A requisição atual se torna a melhor.
            best_dist = dist; // A distância atual se torna a melhor.
        }
    }

    if (best) { // Se uma melhor requisição foi encontrada.
        list_del_init(&best->queuelist); // Remove a melhor requisição da lista.
        elv_dispatch_sort(q, best); // Despacha a melhor requisição.
        current_sector = blk_rq_pos(best); // Obtém o setor da requisição despachada.
        if (nd->dsp_sequence > 1) { // Se não for a primeira requisição despachada.
            // Calcula a distância total percorrida desde a última requisição despachada.
            nd->total_dispatch_distance += abs((long long)current_sector - (long long)nd->previous_dispatch_sector);
        } else {
            // Se for a primeira requisição despachada, calcula a distância desde o setor 0.
            nd->total_dispatch_distance += current_sector; // Distância desde o setor 0 na primeira requisição.
        }
        nd->previous_dispatch_sector = current_sector; // Atualiza o setor da última requisição despachada.
        nd->last_sector = current_sector; // Atualiza o último setor atendido.
        printk(KERN_EMERG "[SSTF] dsp %lu %c %llu\n",
               nd->dsp_sequence++, direction, (unsigned long long)nd->last_sector); // Imprime mensagem de despacho.
        return 1; // Indica que uma requisição foi despachada.
    }
    return 0; // Indica que nenhuma requisição foi despachada.
}

/*
 * sstf_add_request - Função chamada quando uma nova requisição é adicionada à fila. - Modificado
 */
static void sstf_add_request(struct request_queue *q, struct request *rq) {
    struct sstf_data *nd = q->elevator->elevator_data; // Obtém a estrutura de dados SSTF.
    char direction = 'R'; // Define a direção padrão como leitura.
    sector_t current_sector = blk_rq_pos(rq);

    if (nd->add_count > 0) {
        nd->total_arrival_distance += abs((long long)current_sector - (long long)nd->previous_arrival_sector);
    } else {
        nd->total_arrival_distance += current_sector; // Distância desde o setor 0 na primeira chegada.
    }
    nd->previous_arrival_sector = current_sector; // Atualiza o setor da última requisição adicionada.

    printk(KERN_EMERG "[SSTF] add %lu %c %llu\n",
           nd->add_sequence++, direction, (unsigned long long)current_sector); // Imprime mensagem de adição.
    list_add_tail(&rq->queuelist, &nd->queue); // Adiciona a requisição ao final da lista.
    nd->add_count++; // Incrementa o contador de requisições adicionadas.
}

/*
 * sstf_init_queue - Função para inicializar a fila de requisições com os dados do SSTF. - Modificado
 */
static int sstf_init_queue(struct request_queue *q, struct elevator_type *e) {
    struct sstf_data *nd; // Declara ponteiro para a estrutura de dados SSTF.
    struct elevator_queue *eq; // Declara ponteiro para a estrutura do elevador da fila.

    eq = elevator_alloc(q, e); // Aloca a estrutura do elevador para a fila.
    if (!eq) // Se a alocação falhar.
        return -ENOMEM; // Retorna erro de falta de memória.

    nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node); // Aloca memória para a estrutura de dados SSTF.
    if (!nd) { // Se a alocação falhar.
        kobject_put(&eq->kobj); // Libera a estrutura do elevador alocada.
        return -ENOMEM; // Retorna erro de falta de memória.
    }
    eq->elevator_data = nd; // Associa a estrutura de dados SSTF à estrutura do elevador.

    INIT_LIST_HEAD(&nd->queue); // Inicializa a lista de requisições pendentes.
    nd->last_sector = 0; // Inicializa o último setor atendido.
    nd->first_request_dispatched = false; // Inicializa a flag do primeiro despacho.
    nd->add_sequence = 1;     // Inicializa o contador de chegada.
    nd->total_arrival_distance = 0; // Inicializa a distância total de chegada.
    nd->previous_arrival_sector = 0; // Inicializa o setor anterior de chegada.
    nd->add_count = 0;           // Inicializa o contador de adição.
    nd->total_dispatch_distance = 0; // Inicializa a distância total de atendimento.
    nd->previous_dispatch_sector = 0; // Inicializa o setor anterior de atendimento.
    nd->dsp_sequence = 1;     // Inicializa o contador de atendimento.

    spin_lock_irq(q->queue_lock); // Obtém o spinlock da fila com interrupções desabilitadas.
    q->elevator = eq; // Associa a estrutura do elevador à fila.
    spin_unlock_irq(q->queue_lock); // Libera o spinlock da fila com interrupções habilitadas.
    printk(KERN_INFO "[SSTF] init_queue done\n"); // Imprime mensagem de inicialização concluída.
    return 0; // Retorna sucesso.
}

/*
 * sstf_exit_queue - Função chamada quando a fila de requisições associada ao SSTF é destruída. - Modificado
 */
static void sstf_exit_queue(struct elevator_queue *e)
{
    struct sstf_data *nd = e->elevator_data; // Obtém a estrutura de dados SSTF.

    BUG_ON(!list_empty(&nd->queue)); // Verifica se a lista de requisições está vazia.
    printk(KERN_INFO "[SSTF] Total Arrival Distance: %llu\n", nd->total_arrival_distance);
    printk(KERN_INFO "[SSTF] Total Dispatch Distance: %llu\n", nd->total_dispatch_distance);
    kfree(nd); // Libera a memória alocada para a estrutura de dados SSTF.
    printk(KERN_INFO "[SSTF] exit_queue called\n"); // Imprime mensagem de saída da fila.
}

/*
 * elevator_sstf - Estrutura que define o tipo de elevador SSTF e suas operações.
 */
static struct elevator_type elevator_sstf = {
    .ops.sq = { // Define as operações específicas da fila de requisições para o SSTF.
        .elevator_merge_req_fn         = sstf_merged_requests, // Função chamada quando uma requisição é mesclada.
        .elevator_dispatch_fn         = sstf_dispatch, // Função para selecionar e despachar a próxima requisição.
        .elevator_add_req_fn             = sstf_add_request, // Função chamada quando uma nova requisição é adicionada.
        .elevator_init_fn         = sstf_init_queue, // Função para inicializar a fila de requisições.
        .elevator_exit_fn         = sstf_exit_queue, // Função chamada quando a fila é destruída.
    },
    .elevator_name = "sstf", // Define o nome do escalonador.
    .elevator_owner = THIS_MODULE, // Define o proprietário do módulo.
};

/*
 * sstf_init - Função de inicialização do módulo do kernel.
 */
static int __init sstf_init(void)
{
    printk(KERN_INFO "[SSTF] init module\n"); // Imprime mensagem de inicialização do módulo.
    return elv_register(&elevator_sstf); // Registra o tipo de elevador SSTF.
}

/*
 * sstf_exit - Função de saída do módulo do kernel.
 */
static void __exit sstf_exit(void)
{
    elv_unregister(&elevator_sstf); // Remove o registro do tipo de elevador SSTF.
    printk(KERN_INFO "[SSTF] exit module\n"); // Imprime mensagem de saída do módulo.
}

module_init(sstf_init); // Define a função de inicialização do módulo.
module_exit(sstf_exit); // Define a função de saída do módulo.

MODULE_AUTHOR(""); // Define o autor do módulo.
MODULE_LICENSE("GPL"); // Define a licença do módulo.
MODULE_DESCRIPTION("SSTF IO scheduler with final distance calculation"); // Define a descrição do módulo.