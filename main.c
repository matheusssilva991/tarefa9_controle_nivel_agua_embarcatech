#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "lwip/tcp.h"
// #include "pico/bootrom.h" // Biblioteca para inicialização do bootrom

#include "lib/ssd1306/ssd1306.h"
#include "lib/ssd1306/display.h"
#include "lib/led/led.h"
#include "lib/button/button.h"
#include "lib/ws2812b/ws2812b.h"
#include "lib/buzzer/buzzer.h"
#include "config/wifi_config.h"
#include "public/html_data.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#define CYW43_LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43
#define ANALOG_PIN_X 27 // Pino do ADC para o eixo X do joystick
#define ANALOG_PIN_Y 26 // Pino do ADC para o eixo Y do joystick

// Estrutura de dados
struct http_state
{
    char response[4096];
    size_t len;
    size_t sent;
};

// Prototipos das funções
void vTestTask(void *pvParameters);
void vWebServerTask(void *pvParameters);
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);

// Variáveis globais
ssd1306_t ssd; // Declaração do display OLED

int main()
{
    stdio_init_all();

    // Inicializa o display OLED
    init_display(&ssd);

    ssd1306_fill(&ssd, false); // Limpa a tela
    ssd1306_send_data(&ssd);   // Envia os dados para o display

    xTaskCreate(vTestTask, "TestTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vWebServerTask, "WebServerTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}

void vTestTask(void *pvParameters)
{
    (void)pvParameters; // Evita aviso de parâmetro não utilizado

    init_leds();                    // Inicializa os LEDs
    init_btns();                    // Inicializa os botões
    init_btn(BTN_SW_PIN);           // Inicializa o botão do joystick
    init_buzzer(BUZZER_A_PIN, 4.0); // Inicializa o buzzer (pino BUZZER_A_PIN com divisor de clock 4.0)
    ws2812b_init();                 // Inicializa a matriz de LEDs WS2812B

    // Loop infinito
    while (1)
    {
        ssd1306_fill(&ssd, false);                        // Limpa a tela
        ssd1306_draw_string(&ssd, "Hello, World!", 0, 0); // Desenha a string no display
        ssd1306_send_data(&ssd);                          // Envia os dados para o display

        cyw43_arch_gpio_put(CYW43_LED_PIN, !cyw43_arch_gpio_get(CYW43_LED_PIN)); // Inverte o estado do LED

        printf("Hello, World!\n"); // Imprime no console

        ws2812b_clear(); // Limpa a matriz de LEDs
        // Define uma cor para o primeiro LED
        ws2812b_set_led(0, 255, 0, 0); // Define o primeiro LED como vermelho
        ws2812b_write();

        vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 segundo
    }
}

void vWebServerTask(void *pvParameters)
{
    (void)pvParameters; // Evita aviso de parâmetro não utilizado

    // Temporário para evitar aviso de parâmetro não utilizado
    adc_init();
    adc_gpio_init(ANALOG_PIN_X); // Inicializa o pino do ADC para o eixo X
    adc_gpio_init(ANALOG_PIN_Y); // Inicializa o pino do ADC para o eixo Y

    // Inicializa a biblioteca CYW43 para Wi-Fi
    if (cyw43_arch_init())
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => FALHA", 0, 0);
        ssd1306_send_data(&ssd);

        vTaskDelete(NULL);  // Encerrar esta task
    }

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => ERRO", 0, 0);
        ssd1306_send_data(&ssd);

        vTaskDelete(NULL);  // Encerrar esta task
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
    ssd1306_draw_string(&ssd, ip_str, 0, 10);
    ssd1306_send_data(&ssd);

    start_http_server();

    while (1)
    {
        // Mantém o servidor HTTP ativo
        vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 segundo
    }
}

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs)
    {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    if (strstr(req, "GET /led/on"))
    {
        gpio_put(RED_LED_PIN, 1);
        const char *txt = "Ligado";
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           (int)strlen(txt), txt);
    }
    else if (strstr(req, "GET /led/off"))
    {
        gpio_put(RED_LED_PIN, 0);
        const char *txt = "Desligado";
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           (int)strlen(txt), txt);
    }
    else if (strstr(req, "GET /estado"))
    {
        adc_select_input(0);
        uint16_t x = adc_read();
        adc_select_input(1);
        uint16_t y = adc_read();
        int botao = !gpio_get(BTN_A_PIN);
        int joy = !gpio_get(BTN_SW_PIN);

        char json_payload[96];
        int json_len = snprintf(json_payload, sizeof(json_payload),
                                "{\"led\":%d,\"x\":%d,\"y\":%d,\"botao\":%d,\"joy\":%d}\r\n",
                                gpio_get(RED_LED_PIN), x, y, botao, joy);

        printf("[DEBUG] JSON: %s\n", json_payload);

        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           json_len, json_payload);
    }
    else
    {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           (int)strlen(html_data), html_data);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);

    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}