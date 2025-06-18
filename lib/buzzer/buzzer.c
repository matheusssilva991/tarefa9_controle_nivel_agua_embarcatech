#include "buzzer.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// Inicializa o PWM no pino do buzzer
int init_buzzer(uint pin, float clk_div)
{
    gpio_set_function(pin, GPIO_FUNC_PWM); // Coloca para função pwm
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtem o slice do pino
    pwm_config config = pwm_get_default_config(); // Obtem configurações padrões para pwm
    pwm_config_set_clkdiv(&config, clk_div); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true); // Inicializa o pwm neste slice
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente

    return slice_num; // Retorna o número do slice PWM
}

// Toca uma nota com a frequência e duração especificadas
void play_tone(uint pin, uint frequency)
{
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o slice
    uint32_t clock_freq = clock_get_hz(clk_sys); // Define a frequencia de clock
    uint32_t top = clock_freq / frequency - 1; // Faz cálculo para o wrap

    pwm_set_wrap(slice_num, top); // Define o wrap
    pwm_set_gpio_level(pin, top / 2); // 50% de duty cycle
}

// Desliga o tom no pino do buzzer
void stop_tone(uint pin)
{
    uint slice_num = pwm_gpio_to_slice_num(pin);// Obtém o slice
    pwm_set_gpio_level(pin, 0); // Desliga o PWM
}