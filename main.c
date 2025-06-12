#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
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

void vTestTask(void *pvParameters);

int main()
{
    stdio_init_all();

    xTaskCreate(vTestTask, "TestTask", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}

void vTestTask(void *pvParameters) {
    (void)pvParameters; // Evita aviso de parâmetro não utilizado
    ssd1306_t ssd; // Declaração do display OLED


    // Inicializa a biblioteca CYW43 para Wi-Fi
    cyw43_arch_init();
    cyw43_arch_gpio_put(CYW43_LED_PIN, 1); // Liga o LED integrado

    init_leds(); // Inicializa os LEDs
    init_btns();          // Inicializa os botões
    init_btn(BTN_SW_PIN); // Inicializa o botão do joystick
    init_buzzer(BUZZER_A_PIN, 4.0); // Inicializa o buzzer (pino BUZZER_A_PIN com divisor de clock 4.0)
    ws2812b_init(); // Inicializa a matriz de LEDs WS2812B

    // Inicializa o display OLED
    init_display(&ssd);

    ssd1306_fill(&ssd, false); // Limpa a tela
    ssd1306_send_data(&ssd);   // Envia os dados para o display

    // Loop infinito
    while (1) {
        ssd1306_fill(&ssd, false); // Limpa a tela
        ssd1306_draw_string(&ssd, "Hello, World!", 0, 0); // Desenha a string no display
        ssd1306_send_data(&ssd); // Envia os dados para o display

        cyw43_arch_gpio_put(CYW43_LED_PIN, !cyw43_arch_gpio_get(CYW43_LED_PIN)); // Inverte o estado do LED

        printf("Hello, World!\n"); // Imprime no console

        ws2812b_clear(); // Limpa a matriz de LEDs
        // Define uma cor para o primeiro LED
        ws2812b_set_led(0, 255, 0, 0); // Define o primeiro LED como vermelho
        ws2812b_write();

        vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 segundo
    }
}