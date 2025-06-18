#include <stdio.h>           // Lib padrão do C
#include "pico/stdlib.h"     // Definições padrões da rasp 
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "hardware/i2c.h"    // Para comunicação i2c com display oled
#include "hardware/pio.h"    // Para usar os blocos pio do rp2040
#include "hardware/timer.h"  // Para usar os timers do rp2040
#include "hardware/clocks.h" // Para pegar o clock do rp2040 e fazer alguns cálculos
#include "hardware/adc.h"    // Conversor analogico para digital, usado para medir nivel com potenciometro e boia
#include "lwip/tcp.h"

#include "lib/ssd1306/ssd1306.h" // Definições para display oled
#include "lib/ssd1306/display.h" // Definições das funções e variáveis usadas para o display oled
#include "lib/led/led.h"         // Definições para os leds usados
#include "lib/button/button.h"   // Definições para usar os botões
#include "lib/ws2812b/ws2812b.h" // Definições para usar a matriz de leds
#include "lib/buzzer/buzzer.h"   // Definições para usar o buzzer
#include "config/wifi_config_example.h" // Arquivo com credenciais wifi usadas no projeto
#include "public/html_data.h"           // Arquivo html para carregar o webserver

#include "FreeRTOS.h" // Sistema operacional FreeRtos
#include "FreeRTOSConfig.h" // Configurações usadas para este projeto do freeRTOS
#include "task.h" //Biblioteca para controle de tasks
#include "queue.h" // Biblioteca para controle de filas
#include "semphr.h" // Biblioteca para controle de semáforos e mutexes

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
volatile static int limite_minimo_nivel_agua = 10;
volatile static int limite_maximo_nivel_agua = 90;

// Fila para armazenar os valores de nivel de agua lidos
QueueHandle_t xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua;
//Mutex para proteger o acesso ao display
SemaphoreHandle_t xMutexDisplay;
// Estrutura de dados http
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
    xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua = xQueueCreate(5, sizeof(int));
    xMutexDisplay = xSemaphoreCreateMutex();
    xTaskCreate(vWebServerTask, "WebServerTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL); // Colocada com prioridade maior para o webserver ser iniciado logo
    xTaskCreate(vAcionaBombaComBaseNoNivelTask, "AcionaBombaComBaseNoNivelTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vLeituraPotenciometroTask, "LeituraPotenciometroTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vMostraDadosNoDisplayTask, "vMostraDadosNoDisplayTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();// Inicia o escalonador do FreeRTOS
    panic_unsupported();
}



// Task que controla a leitura do joystick
void vLeituraPotenciometroTask(void *pvParameters){
    (void)pvParameters; // Evita aviso de parâmetro não utilizado

    // Inicializa o adc e o pino responsável pelo potencioemtro
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
    int nivelEmPorcentagem = 0;
    while (true){
        if (xQueueReceive(xQueueLeiturasDoPotenciometroConvertidoEmNivelDeAgua, &nivelEmPorcentagem, portMAX_DELAY) == pdTRUE){
            if (nivelEmPorcentagem <= limite_minimo_nivel_agua){
                gpio_put(RELE_PIN,0); // Ativa o rele deixando os 12v passarem para a bomba
            }
            else if (nivelEmPorcentagem >= limite_maximo_nivel_agua){
                gpio_put(RELE_PIN,1); // Desativa o rele desligando a bomba
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
            if (xSemaphoreTake(xMutexDisplay,portMAX_DELAY) == pdTRUE){//Acesso seguro ao display
                sprintf(str_nivel, "%d%%", nivelAguaPorcentagem); // Formata com '%'
                ssd1306_fill(&ssd, false);                     // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 60, true, false); // Desenha um retângulo
                ssd1306_line(&ssd, 3, 25, 123, 25, true);      // Desenha uma linha
        
                ssd1306_draw_string(&ssd, "MEDIDOR NIVEL", 8, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, " DE AGUA", 20, 16);  // Desenha uma string
                ssd1306_draw_string(&ssd, "Nivel", 20, 31);           // Desenha uma string
                ssd1306_draw_string(&ssd, str_nivel, 20, 42);         // Desenha uma string
                ssd1306_send_data(&ssd);                             // Atualiza o display
                xSemaphoreGive(xMutexDisplay);
            }
            
        }
    }
     
}


void vWebServerTask(void *pvParameters){
    (void)pvParameters; // Evita aviso de parâmetro não utilizado

    if (xSemaphoreTake(xMutexDisplay,portMAX_DELAY) == pdTRUE){
        // Inicializa a biblioteca CYW43 para Wi-Fi
        if (cyw43_arch_init()){
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "WiFi => FALHA", 0, 0);
            ssd1306_send_data(&ssd);

            vTaskDelete(NULL);  // Encerrar esta task
        }

        cyw43_arch_enable_sta_mode();//Coloca em modo cliente
        // Tenta se conectar com o ssid e senha fornecidos durante 30s
        if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)){
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "WiFi => ERRO", 0, 0);
            ssd1306_send_data(&ssd);

            vTaskDelete(NULL);  // Encerrar esta task
        }

        //Obtém o ip da placa nesta rede
        uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
        char ip_str[24];
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
        ssd1306_draw_string(&ssd, ip_str, 0, 10); // Mostra o ip para acessar o webserver
        ssd1306_send_data(&ssd);
        start_http_server(); // Inicializa o webServer
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

// Função de callback chamada pelo LwIP depois que uma parte dos dados da resposta HTTP foi enviada com sucesso.
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len){
    // `arg`: A estrutura 'http_state' que foi associada ao PCB via tcp_arg().
    // `tpcb`: O PCB TCP da conexão atual.
    // `len`: O número de bytes que foram enviados com sucesso nesta chamada.

    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len; // Adiciona a quantidade de bytes recém-enviados ao contador total de bytes enviados.
    // Verifica se todos os bytes da resposta foram enviados.
    if (hs->sent >= hs->len){
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}


// Processa as requisições HTTP recebidas dos clientes e gera as respostas.Chamada sempre que dados são recebidos em uma conexão TCP.
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    // `arg`: Ponteiro genérico (pode ser usado para manter estado da conexão, mas aqui é sobrescrito).
    // `tpcb`: O PCB TCP da conexão atual.
    // `p`: Um ponteiro para uma estrutura `pbuf` que contém os dados recebidos (a requisição HTTP).
    // `err`: O código de erro, se houver, na recepção dos dados.
    if (!p){ // Verifica se o ponteiro pbuf é nulo.
        // Se p for nulo, significa que o cliente fechou a conexão ou houve um erro.    
        tcp_close(tpcb); // Fecha a conexão TCP.
        return ERR_OK;
    }

    char *req = (char *)p->payload; // Converte o payload (conteúdo) do pbuf para uma string (a requisição HTTP).
    // Aloca memória para a struct http_state que manterá o estado da resposta.
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs){ // Verifica se a alocação de memória falhou.
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0; // Inicializa o contador de bytes enviados para esta resposta.

    // Verifica se a requisição é "GET /bomba/on"
    if (strstr(req, "GET /bomba/on")){
        gpio_put(RELE_PIN, 0); // Ativa o relé (LOW liga a bomba)
        const char *txt = "Bomba Ligada";
        // Formata a resposta HTTP (cabeçalhos e corpo).
        hs->len = snprintf(hs->response, sizeof(hs->response),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "%s",
                        (int)strlen(txt), txt);
    }
    // Verifica se a requisição é "GET /bomba/off"
    else if (strstr(req, "GET /bomba/off")){
        gpio_put(RELE_PIN, 1); // Desativa o relé (HIGH desliga a bomba)
        const char *txt = "Bomba Desligada";
        // Formata a resposta HTTP (cabeçalhos e corpo).
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
        int estado_bomba_para_json = !gpio_get(RELE_PIN); // Pega o estado atual do rele, nivel 0 da gpio indica rele ativado
        char json_payload[96]; // Buffer para a string JSON
        // Formata os dados em uma string JSON.
        int json_len = snprintf(json_payload, sizeof(json_payload),
                                 "{\"bomba_agua\":%d,\"nivel_agua\":%d}\r\n",
                                 estado_bomba_para_json, nivel_porcentagem); // Enviando como 'nivel_agua' e 'bomba_agua'
         // Formata a resposta HTTP com cabeçalhos JSON e o corpo JSON.
         hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           json_len, json_payload);

    }
    // Se nenhuma das requisições acima corresponder, serve a página HTML padrão.
    else{
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n" // Tipo de conteúdo HTML
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           (int)strlen(html_data), html_data);
    }

     // --- Envio da Resposta ---

    // Associa a estrutura 'hs' (que contém a resposta a ser enviada) ao PCB.
    // Isso permite que 'http_sent' (o callback de envio) acesse esses dados.
    tcp_arg(tpcb, hs);
    // Registra uma função de callback para quando os dados da resposta forem enviados.
    // 'http_sent' será chamada para cada bloco de dados enviados.
    tcp_sent(tpcb, http_sent);

    // Escreve os dados da resposta no buffer de envio do TCP.
    // `TCP_WRITE_FLAG_COPY`: Copia os dados para o buffer interno do LwIP.
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    // Força o LwIP a enviar os dados agora.
    tcp_output(tpcb);

    pbuf_free(p); // Libera o pbuf que continha a requisição recebida (pois já foi processado).
    return ERR_OK; // Retorna OK, indicando que o processamento foi bem-sucedido.
}

// Função chamada pelo LwIP sempre que uma nova conexão TCP é estabelecida com o seu servidor.
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err){
    // `arg`: Ponteiro genérico (não usado neste exemplo, mas pode ser útil para passar contexto).
    // `newpcb`: O novo PCB que representa a conexão recém-aceita com o cliente.
    // `err`: O código de erro, se houver, ao aceitar a conexão.

    // Registra uma função de callback para quando dados forem recebidos nesta nova conexão.
    // Toda vez que o cliente enviar dados (a requisição HTTP), o LwIP chamará 'http_recv'.
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}


// Inicialização do servidor HTTP. Configura a "escuta" na porta 80 para novas conexões.
static void start_http_server(void){
    // Cria um novo PCB (Protocol Control Block) TCP.
    // O PCB é a estrutura de dados central do LwIP que representa uma conexão TCP.
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    // Associa o PCB a um endereço IP e porta.
    // IP_ADDR_ANY: Significa que o servidor escutará em qualquer interface de rede disponível.
    // 80: É a porta padrão para o protocolo HTTP.
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    // Coloca o PCB em modo de escuta (listening).
    // Isso faz com que o LwIP comece a monitorar a porta 80 para novas tentativas de conexão.
    pcb = tcp_listen(pcb);
    // Registra uma função de callback para quando uma nova conexão for aceita.
    // Quando um cliente tenta se conectar, o LwIP chamará 'connection_callback'.
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

