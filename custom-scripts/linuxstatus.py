#!/usr/bin/env python3

import json
import time
import os
import socket
from http.server import BaseHTTPRequestHandler, HTTPServer
from datetime import datetime
import fcntl
import struct

# --- funções auxiliares para coletar informações do sistema --- #

def get_datetime():  # Define a função que retorna data e hora atuais
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")  # Pega a data/hora atual e formata como string "AAAA-MM-DD HH:MM:SS"


def get_uptime():  # Define a função que retorna o tempo desde o último boot
    try:
        with open("/proc/uptime") as f:  # Abre o arquivo que contém o tempo de atividade do sistema
            return int(float(f.readline().split()[0]))  # Lê a primeira linha, separa por espaço e converte o primeiro valor para inteiro
    except:
        return 0  # Em caso de erro, retorna 0


def get_cpu_info():  # Define a função que retorna modelo, velocidade e uso da CPU
    model = "desconhecido"  # Inicializa o modelo como "desconhecido"
    speed = 0  # Inicializa a velocidade como 0
    usage = 0.0  # Inicializa o uso da CPU como 0.0%

    try:
        with open("/proc/cpuinfo") as f:  # Abre o arquivo com informações do processador
            for line in f:  # Percorre cada linha do arquivo
                if "model name" in line or "Hardware" in line:  # Se encontrar a linha com nome do modelo
                    model = line.split(":")[1].strip()  # Extrai o nome do modelo (texto após os dois-pontos)
                elif "cpu MHz" in line:  # Se encontrar a linha com velocidade do processador
                    speed = int(float(line.split(":")[1].strip()))  # Extrai o valor da velocidade e converte para inteiro
    except:
        pass  # Ignora qualquer erro ao ler o arquivo
    # cpu  4732962 0 1612435 135142710 217735 0 49852 0 0 0
    try:
        with open("/proc/stat") as f:  # Abre o arquivo com estatísticas da CPU
            cpu_line = f.readline()  # Lê a primeira linha (com dados da CPU)
            parts = list(map(int, cpu_line.strip().split()[1:]))  # Separa os valores e converte para inteiros (ignora o "cpu" inicial)
            idle, total = parts[3], sum(parts)  # Armazena tempo ocioso (4º campo) e tempo total (soma de todos)

        time.sleep(0.1)  # Aguarda 0.1 segundo para fazer nova medição

        with open("/proc/stat") as f:  # Abre novamente o arquivo
            cpu_line2 = f.readline()  # Lê a nova linha de estatísticas
            parts2 = list(map(int, cpu_line2.strip().split()[1:]))  # Converte os valores novamente
            idle2, total2 = parts2[3], sum(parts2)  # Calcula o novo tempo ocioso e total

        idle_delta = idle2 - idle  # Diferença no tempo ocioso
        total_delta = total2 - total  # Diferença no tempo total
        usage = 100.0 * (1.0 - idle_delta / total_delta) if total_delta else 0.0  # Calcula a porcentagem de uso da CPU
    except:
        usage = 0.0  # Em caso de erro, uso da CPU é 0

    return {
        "model": model,  # Retorna o modelo do processador
        "speed_mhz": speed,  # Retorna a velocidade em MHz
        "usage_percent": round(usage, 2)  # Retorna o uso da CPU com 2 casas decimais
    }

#MemTotal:        8045464 kB
#MemFree:         1021540 kB
#MemAvailable:    2845236 kB

def get_memory_info():  # Define função para obter informações de memória
    try:
        with open("/proc/meminfo") as f:  # Abre o arquivo com dados da memória
            lines = f.readlines()  # Lê todas as linhas
            mem_total = int(lines[0].split()[1]) // 1024  # Lê MemTotal (linha 0), converte para MB
            mem_free = int(lines[1].split()[1]) // 1024  # Lê MemFree (linha 1), converte para MB
            mem_available = int(lines[2].split()[1]) // 1024 if "MemAvailable" in lines[2] else mem_free  # Lê MemAvailable (linha 2) ou usa MemFree
            used = mem_total - mem_available  # Calcula a memória usada
            return {
                "total_mb": mem_total,  # Memória total em MB
                "used_mb": used  # Memória usada em MB
            }
    except:
        return {
            "total_mb": 0,  # Em caso de erro, retorna 0
            "used_mb": 0
        }


def get_os_version():  # Define função para obter a versão do sistema operacional
    try:
        with open("/proc/version") as f:  # Abre o arquivo com versão do kernel
            return f.read().strip()  # Lê o conteúdo e remove espaços em branco
    except:
        return "versão desconhecida"  # Em caso de erro, retorna texto padrão


def get_process_list():  # Define função que lista os processos em execução
    processes = []  # Lista onde os processos serão armazenados
    for pid in os.listdir("/proc"):  # Percorre os diretórios de /proc
        if pid.isdigit():  # Verifica se o nome do diretório é um número (ou seja, um PID)
            try:
                with open(f"/proc/{pid}/comm") as f:  # Abre o arquivo que contém o nome do processo
                    name = f.read().strip()  # Lê o nome do processo e remove espaços
                    processes.append({ "pid": int(pid), "name": name })  # Adiciona à lista
            except:
                continue  # Ignora se não conseguir acessar o processo
    return processes  # Retorna a lista de processos


def get_disks():  # Define função que lista os discos detectados
    disks = []  # Lista onde os discos serão armazenados
    try:
        with open("/proc/partitions") as f:  # Abre o arquivo que contém partições/discos
            lines = f.readlines()[2:]  # Ignora as duas primeiras linhas (cabeçalho)
            for line in lines:  # Percorre as linhas restantes
                parts = line.strip().split()  # Separa os dados da linha
                if len(parts) == 4:  # Verifica se há exatamente 4 colunas
                    _, _, blocks, name = parts  # Extrai o número de blocos e o nome do dispositivo
                    if name.startswith("sd") or name.startswith("mmcblk"):  # Considera apenas discos (sdX ou mmcblkX)
                        disks.append({
                            "device": f"/dev/{name}",  # Caminho do dispositivo (ex: /dev/sda)
                            "size_mb": int(blocks) // 1024  # Tamanho em MB (cada bloco tem 1 KB)
                        })
    except:
        pass  # Ignora erros na leitura do arquivo
    return disks  # Retorna a lista de discos


def get_ip_address(ifname):
    # Cria um socket do tipo IPv4 e UDP
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        return socket.inet_ntoa(
            fcntl.ioctl(
                s.fileno(),               # obtém o descritor do socket
                0x8915,                   # código para obter endereço IP (SIOCGIFADDR)
                struct.pack('256s', ifname[:15].encode())  # empacota o nome da interface
            )[20:24]                      # extrai os bytes correspondentes ao endereço IP
        )
    except:
        return "desconhecido"

def get_network_adapters():
    # Inicializa a lista de adaptadores de rede
    adapters = []
    try:
        # Lista todas as interfaces de redes/class/ em /sys/class/net
        for iface in os.listdir("/synet"):
            ip = get_ip_address(iface)  # Obtém o IP da interface
            adapters.append({
                "interface": iface,     # Nome da interface
                "ip_address": ip        # Endereço IP associado
            })
    except:
        pass
    return adapters

def get_usb_devices():
    # Inicializa lista para armazenar dispositivos USB
    usb_devices = []
    base_path = "/sys/bus/usb/devices"

    try:
        # Lista todos os dispositivos no caminho base
        for dev in os.listdir(base_path):
            path = os.path.join(base_path, dev)  # Monta o caminho completo do dispositivo
            if os.path.isdir(path):              # Verifica se é um diretório (um dispositivo)
                product_path = os.path.join(path, "product")  # Caminho para o nome do dispositivo
                if os.path.exists(product_path):              # Verifica se o arquivo 'product' existe
                    try:
                        with open(product_path) as f:
                            description = f.read().strip()    # Lê a descrição do dispositivo
                        usb_devices.append({
                            "port": dev,                      # Nome da porta (ex: 1-1, 2-1.2)
                            "description": description        # Nome do dispositivo (ex: "Logitech USB Receiver")
                        })
                    except:
                        continue
    except:
        pass

    return usb_devices

# --- servidor http que responde com o json de status --- #

class StatusHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/status":
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"not found")
            return

        # coleta todas as informações para montar o json
        response = {
            "datetime": get_datetime(),
            "uptime_seconds": get_uptime(),
            "cpu": get_cpu_info(),
            "memory": get_memory_info(),
            "os_version": get_os_version(),
            "processes": get_process_list(),
            "disks": get_disks(),
            "usb_devices": get_usb_devices(),
            "network_adapters": get_network_adapters()
        }

        # envia a resposta http com json
        data = json.dumps(response, indent=2).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

def run_server(port=8080):
    # inicializa o servidor ouvindo em todas as interfaces
    print(f"\nservidor disponível em http://192.168.1.10:{port}/status")
    server = HTTPServer(("0.0.0.0", port), StatusHandler)
    server.serve_forever()

# inicia o servidor se o script for executado diretamente
if __name__ == "__main__":
    run_server()