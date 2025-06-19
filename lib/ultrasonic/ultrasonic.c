#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/timer.h"

// Velocidade do som em cm/µs. (343 m/s = 34300 cm/s = 0.0343 cm/µs)
const float SOUND_SPEED_CM_PER_US = 0.0343;
// Timeout em microssegundos para esperar pelo eco. Cerca de 38ms é o máximo para o HC-SR04 (~6.5m, teórico)
const uint32_t TIMEOUT_US = 40000;


// --- Funções do Sensor ---
void setup_ultrasonic_pins(uint trigPin, uint echoPin) {
    gpio_init(trigPin);
    gpio_init(echoPin);
    gpio_set_dir(trigPin, GPIO_OUT);
    gpio_set_dir(echoPin, GPIO_IN);
}

// Função para medir a duração do pulso em microssegundos
// Retorna 0 em caso de timeout/erro
uint64_t get_pulse_duration_us(uint trigPin, uint echoPin) {
    // Garante que o gatilho esteja baixo e estabilizado
    gpio_put(trigPin, 0);
    sleep_us(2);

    // Envia o pulso de gatilho de 10µs
    gpio_put(trigPin, 1);
    sleep_us(10);
    gpio_put(trigPin, 0);

    absolute_time_t start_wait, end_wait;

    // Aguarda o início do pulso de eco (com timeout)
    start_wait = get_absolute_time();
    while (gpio_get(echoPin) == 0) {
        if (absolute_time_diff_us(start_wait, get_absolute_time()) > TIMEOUT_US) {
            return 0; // Retorna 0 para indicar timeout
        }
    }
    absolute_time_t pulse_start = get_absolute_time();

    // Aguarda o fim do pulso de eco (com timeout)
    end_wait = get_absolute_time();
    while (gpio_get(echoPin) == 1) {
        if (absolute_time_diff_us(end_wait, get_absolute_time()) > TIMEOUT_US) {
            return 0; // Retorna 0 para indicar timeout
        }
    }
    absolute_time_t pulse_end = get_absolute_time();

    // Retorna a duração do pulso em microssegundos
    return absolute_time_diff_us(pulse_start, pulse_end);
}

// --- Funções de Conversão ---

// Converte a duração do pulso (µs) para distância em centímetros
float microseconds_to_cm(uint64_t pulse_duration_us) {
    // Distância = (duração * velocidade_som) / 2 (porque o som vai e volta)
    return (float)pulse_duration_us * SOUND_SPEED_CM_PER_US / 2.0f;
}

// Converte a duração do pulso (µs) para distância em polegadas
float microseconds_to_inches(uint64_t pulse_duration_us) {
    // 1 polegada = 2.54 cm
    return microseconds_to_cm(pulse_duration_us) / 2.54f;
}
