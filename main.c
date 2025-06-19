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
#include "lib/matrix_leds/matrix_leds.h"
#include "lib/buzzer/buzzer.h"
#include "config/wifi_config_example.h"
#include "public/html_data.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define CYW43_LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43

#define PIN_ADC_LEITURA_POTENCIOMETRO 28 // Pino do ADC para ler os valores alterados no potencimetro pela boia
#define RELE_PIN 16 // Gpio que ativará(low) e desativará(high) o relé para acionar a bomba

#define ANALOG_PIN_X 27 // Pino do ADC para o eixo X do joystick
#define ANALOG_PIN_Y 26 // Pino do ADC para o eixo Y do joystick

// OBS: Na hora da montagem do reservatório precisamos ver até onde o potênciometro irá girar no nível mínimo  e máximo para ajustar os valores abaixo
// Definições para o potenciômetro de boia
#define ADC_MIN_LEITURA_POTENCIOMETRO 22   // Valor mínimo lido do potenciômetro (quando o reservatório está vazio)
#define ADC_MAX_LEITURA_POTENCIOMETRO 4070 // Valor máximo lido do potenciômetro (quando o reservatório está cheio)

// Limites que serão alterados pela interface Web, por padrão deixei assim so para testar
volatile static uint8_t limite_minimo_nivel_agua = 10;
volatile static uint8_t limite_maximo_nivel_agua = 90;
volatile static bool reset_limits=false;

// Fila para armazenar os valores de nivel de agua lidos
QueueHandle_t xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua;
SemaphoreHandle_t xMutexDisplay;
// Estrutura de dados
struct http_state
{
    char response[4096];
    size_t len;
    size_t sent;
};

// Prototipos das funções
void vWebServerTask(void *pvParameters);
void vMostraDadosNoDisplayTask(void *pvParameters);
void vLeituraPotenciometroTask(void *pvParameters);
void vAcionaBombaComBaseNoNivelTask(void * pvParameters);
void vMatrixLedsTask(void *pvParameters);
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);
void button_callback(uint gpio, uint32_t events);

// Variáveis globais
ssd1306_t ssd; // Declaração do display OLED

int main()
{
    stdio_init_all();

    button_init_predefined(true,true,true);

    button_setup_irq(true,true,true,button_callback);

    // Inicializa o display OLED
    init_display(&ssd);

    init_leds();

    ssd1306_fill(&ssd, false); // Limpa a tela
    ssd1306_send_data(&ssd);   // Envia os dados para o display
    xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua = xQueueCreate(5, sizeof(int));
    xMutexDisplay = xSemaphoreCreateMutex();
    xTaskCreate(vWebServerTask, "WebServerTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vAcionaBombaComBaseNoNivelTask, "AcionaBombaComBaseNoNivelTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vLeituraPotenciometroTask, "LeituraPotenciometroTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vMostraDadosNoDisplayTask, "vMostraDadosNoDisplayTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vMatrixLedsTask, "vMatrixLedsTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}

void button_callback(uint gpio, uint32_t events){
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if(current_time - last_debounce_time > debounce_delay){

        if(gpio == BUTTON_A){
            reset_limits=true;
        }
        else if(gpio == BUTTON_B){
            
        }
        else if(gpio == BUTTON_SW){
            
        }

        last_debounce_time = current_time;
    }
}



// Task que controla a leitura do joystick
void vLeituraPotenciometroTask(void *pvParameters){
    (void)pvParameters; // Evita aviso de parâmetro não utilizado

    // Inicializa o adc e os pinos responsáveis pelo eixo x e y do joystick
    adc_gpio_init(PIN_ADC_LEITURA_POTENCIOMETRO);
    adc_init();
    int porcentagem = 0;
    while (true){
        // ja estou armazenando na fila a porcentagem em relação ao valor lido
        // 0% == 22 lido pelo adc
        // 100% == 4077
        adc_select_input(2); // GPIO 26 = ADC0
        porcentagem = (int)(((float)(adc_read() - ADC_MIN_LEITURA_POTENCIOMETRO) / (ADC_MAX_LEITURA_POTENCIOMETRO - ADC_MIN_LEITURA_POTENCIOMETRO)) * 100.0f);
        xQueueSend(xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua, &porcentagem, 0); // Envia o valor da leitura do potenciometro em porcentagem para fila
        vTaskDelay(pdMS_TO_TICKS(100));              // 10 Hz de leitura
    }
}



void vAcionaBombaComBaseNoNivelTask(void * pvParameters){
    (void)pvParameters; // Evita aviso de parâmetro não utilizado

    gpio_init(RELE_PIN);
    gpio_set_dir(RELE_PIN,GPIO_OUT);
    gpio_put(RELE_PIN,1);// Começa com o Relé desligado, pois ele no nivel alto da gpio é desligado ja que é um rele com optoacoplador
    uint8_t nivelEmPorcentagem = 0;
    int slice_num = init_buzzer(BUZZER_A_PIN,4.0);

    if (!gpio_get(RELE_PIN)){
        set_led_green();
    }else if(gpio_get(RELE_PIN)){
        set_led_yellow();
    }

    while (true){
        if (reset_limits)
        {
            limite_maximo_nivel_agua=90;
            limite_minimo_nivel_agua=10;
            reset_limits=false;
            printf("Limites reinicializados\n");
        }
        
        if (xQueueReceive(xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua, &nivelEmPorcentagem, portMAX_DELAY) == pdTRUE){
            if (nivelEmPorcentagem <= limite_minimo_nivel_agua){
                gpio_put(RELE_PIN,0); // Ativa o rele deixando os 12v passarem para a bomba
            }
            else if (nivelEmPorcentagem >= limite_maximo_nivel_agua){
                gpio_put(RELE_PIN,1); // Desativa o rele desligando a bomba

            }
            
        }

        if (!gpio_get(RELE_PIN) && gpio_get(RED_LED_PIN)){
            set_led_green();

            play_tone(BUZZER_A_PIN, 300);
            vTaskDelay(pdMS_TO_TICKS(250)); // Toca o buzzer por 250ms
            stop_tone(BUZZER_A_PIN);

        }else if(gpio_get(RELE_PIN) && !gpio_get(RED_LED_PIN)){
            set_led_yellow();
            for (uint8_t i = 0; i < 2; i++)
            {
                play_tone(BUZZER_A_PIN, 900);
                vTaskDelay(pdMS_TO_TICKS(150)); // Toca o buzzer por 250ms
                stop_tone(BUZZER_A_PIN);
                vTaskDelay(pdMS_TO_TICKS(150)); // Toca o buzzer por 250ms
            }
            
        }
        
    }
}

void vMostraDadosNoDisplayTask(void *pvParameters){
    (void)pvParameters; // Evita aviso de parâmetro não utilizado

    int nivelAguaPorcentagem = 0;
    char str_nivel[5]; // Buffer para armazenar a string
    while (true){
        if (xQueueReceive(xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua, &nivelAguaPorcentagem, portMAX_DELAY) == pdTRUE){
            if (xSemaphoreTake(xMutexDisplay,portMAX_DELAY) == pdTRUE){
                sprintf(str_nivel, "%d%%", nivelAguaPorcentagem); // Formata com '%'
                ssd1306_fill(&ssd, false);                     // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 60, true, false); // Desenha um retângulo
                ssd1306_line(&ssd, 3, 25, 123, 25, true);      // Desenha uma linha
        
                ssd1306_draw_string(&ssd, "MEDIDOR NIVEL", 8, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, " DE AGUA", 20, 16);  // Desenha uma string
                ssd1306_draw_string(&ssd, "Nivel: ", 10, 31);           // Desenha uma string
                ssd1306_draw_string(&ssd, str_nivel, 58, 31);         // Desenha uma string
                ssd1306_draw_string(&ssd, gpio_get(RELE_PIN) ? "Bomba: OFF" : "Bomba: ON", 10,41);
                ssd1306_send_data(&ssd);                             // Atualiza o display
                xSemaphoreGive(xMutexDisplay);
            }
            
        }
    }
}

void vMatrixLedsTask(void *pvParameters){
    (void)pvParameters; // Evita aviso de parâmetro não utilizado
    init_led_matrix();
    apaga_matriz();
    uint8_t nivelEmPorcentagem = 0;

    while (true)
    {
        if(xQueueReceive(xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua, &nivelEmPorcentagem, portMAX_DELAY) == pdTRUE){
            if (nivelEmPorcentagem >= 80){
                desenha_frame(levels,4);
            }else if(nivelEmPorcentagem >= 60){
                desenha_frame(levels,3);
            }else if(nivelEmPorcentagem >= 40){
                desenha_frame(levels,2);
            }else if(nivelEmPorcentagem >= 20){
                desenha_frame(levels,1);
            }else{
                desenha_frame(levels,0);
            }
            
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    

}


void vWebServerTask(void *pvParameters){
    (void)pvParameters; // Evita aviso de parâmetro não utilizado

    if (xSemaphoreTake(xMutexDisplay,portMAX_DELAY) == pdTRUE){
        // Inicializa a biblioteca CYW43 para Wi-Fi
        if (cyw43_arch_init())
        {
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "WiFi => FALHA", 0, 0);
            ssd1306_send_data(&ssd);

            vTaskDelete(NULL);  // Encerrar esta task
        }

        cyw43_arch_enable_sta_mode();
        if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
        {
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "WiFi => ERRO", 0, 0);
            ssd1306_send_data(&ssd);

            vTaskDelete(NULL);  // Encerrar esta task
        }

        uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
        char ip_str[24];
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        printf("IP: %s\n",ip_str);
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
        ssd1306_draw_string(&ssd, ip_str, 0, 10);
        ssd1306_send_data(&ssd);
        start_http_server();
        vTaskDelay(pdMS_TO_TICKS(2000)); // pra dar tempo de ver o ip
        xSemaphoreGive(xMutexDisplay);
    }
    

    while (1){
        // Mantém o servidor HTTP ativo
        cyw43_arch_poll();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 segundo
    }
     cyw43_arch_deinit();// Esperamos que nunca chegue aqui
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

    if (strstr(req, "GET /bomba/on")){
        gpio_put(RELE_PIN, 0); // Ativa o relé (LOW liga a bomba)
        const char *txt = "Bomba Ligada";
        hs->len = snprintf(hs->response, sizeof(hs->response),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "%s",
                        (int)strlen(txt), txt);
    }
    else if (strstr(req, "GET /bomba/off")){
        gpio_put(RELE_PIN, 1); // Desativa o relé (HIGH desliga a bomba)
        const char *txt = "Bomba Desligada";
        hs->len = snprintf(hs->response, sizeof(hs->response),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "%s",
                        (int)strlen(txt), txt);
    }
    else if (strstr(req, "GET /estado")){  // Se a requisição for para obter o estado dos sensores(potenciometro com boia)
        adc_select_input(2); // Seleciona o canal ADC 2 para o potenciômetro da boia
        // Converte para porcentagem
        int nivel_porcentagem = (int)(((float)(adc_read() - ADC_MIN_LEITURA_POTENCIOMETRO) / (ADC_MAX_LEITURA_POTENCIOMETRO - ADC_MIN_LEITURA_POTENCIOMETRO)) * 100.0f);
        int estado_bomba_para_json = !gpio_get(RELE_PIN);
        char json_payload[96]; // Buffer para a string JSON
        int json_len = snprintf(json_payload, sizeof(json_payload),
                                 "{\"bomba_agua\":%d,\"nivel_agua\":%d}\r\n",
                                 estado_bomba_para_json, nivel_porcentagem); // Enviando como 'nivel_agua' e 'bomba_agua'
         hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           json_len, json_payload);

    }
    else{
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

