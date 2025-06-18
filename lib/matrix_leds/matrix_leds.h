#ifndef MATRIX_LEDS_H
#define MATRIX_LEDS_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pio_matrix.pio.h"

extern uint sm;


uint8_t obter_index(uint8_t i); //Função auxiliar para preencher a matriz de LEDS

void desenha_frame(const uint32_t matriz[][25],uint8_t frame); //Desenha frame especifico do vetor de frames

void init_led_matrix(); //Configurações para uso da matriz de LEDs

void apaga_matriz(); //Apaga os LEDs ligados da matriz de LEDs

extern const uint32_t levels[5][25]; //Vetor  de frames, com os números de 0 a 9
#endif