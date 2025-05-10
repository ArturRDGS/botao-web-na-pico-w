#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include "hardware/adc.h"

#define BUTTON1_PIN 5      // Pino do botão 1
#define BUTTON2_PIN 6      // Pino do botão 2
#define WIFI_SSID "Rodrigues"
#define WIFI_PASS "Beta04502@"

// Declaração e estado inicial dos campos referentes aos botões e a temperatura
char button1_message[50] = "Nenhum evento no botão 1";
char button2_message[50] = "Nenhum evento no botão 2";
char temp_message[50] = "Temperatura não lida";

// Buffer para resposta HTTP
char http_response[1024];

// Função para criar a resposta HTTP com um HTML Básico
void create_http_response() {
    snprintf(http_response, sizeof(http_response),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "  <meta charset=\"UTF-8\">"
        "  <title>Monitoramento de Botões e Temperatura</title>"
        "</head>"
        "<body>"
        "  <h1>Estado dos Botões e Temperatura da Placa</h1>"
        "  <p>Botão 1: %s</p>"
        "  <p>Botão 2: %s</p>"
        "  <p>Temperatura: %s</p>"
        "  <script>"
        "      setTimeout(function(){"
        "          location.reload();"
        "      },1000);"
        "  </script>"
        "</body>"
        "</html>\r\n",
        button1_message, button2_message, temp_message);
}


// Função de callback para processar requisições HTTP   
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Apenas responde com a página HTML atualizada
    create_http_response();
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);  // Associa o callback HTTP
    return ERR_OK;
}

// Função de setup do servidor TCP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    // Liga o servidor na porta 80
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);  // Coloca o PCB em modo de escuta
    tcp_accept(pcb, connection_callback);  // Associa o callback de conexão

    printf("Servidor HTTP rodando na porta 80...\n");
}

// Monitoramento dos botões
void monitor_buttons() {
    static bool button1_last_state = false;
    static bool button2_last_state = false;

    bool button1_state = !gpio_get(BUTTON1_PIN);
    bool button2_state = !gpio_get(BUTTON2_PIN);

    if (button1_state != button1_last_state) {
        button1_last_state = button1_state;
        if (button1_state) {
            snprintf(button1_message, sizeof(button1_message), "Botão 1 foi pressionado!");
            printf("Botão 1 pressionado\n");
        } else {
            snprintf(button1_message, sizeof(button1_message), "Botão 1 foi solto!");
            printf("Botão 1 solto\n");
        }
    }

    if (button2_state != button2_last_state) {
        button2_last_state = button2_state;
        if (button2_state) {
            snprintf(button2_message, sizeof(button2_message), "Botão 2 foi pressionado!");
            printf("Botão 2 pressionado\n");
        } else {
            snprintf(button2_message, sizeof(button2_message), "Botão 2 foi solto!");
            printf("Botão 2 solto\n");
        }
    }
}

// Função de leitura da temperatura atual
void read_temperature() {
    const float conversion_factor = 3.3f / (1 << 12);
    adc_select_input(4);
    uint16_t result = adc_read();
    float voltage = result * conversion_factor;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    snprintf(temp_message, sizeof(temp_message), " %.2f °C", temperature);
}

int main() {
    // Inicializa a saída padrão
    stdio_init_all();
    sleep_ms(5000);
    printf("Iniciando servidor HTTP\n");

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }else {
        printf("Conectado.\n");
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    printf("Wi-Fi conectado!\n");

    // Configuração dos Botões

    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);

    adc_init();
    adc_set_temp_sensor_enabled(true);

    printf("Botões configurados com pull-up nos pinos %d e %d\n", BUTTON1_PIN, BUTTON2_PIN);

    // Inicia o servidor HTTP
    start_http_server();

    // Loop principal
    while (true) {
        cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo
        monitor_buttons();  // Atualiza o estado dos botões
        read_temperature(); // Realiza a leitura da temperatura
        sleep_ms(100);      // Reduz o uso da CPU
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi (não será chamado, pois o loop é infinito)
    return 0;
}