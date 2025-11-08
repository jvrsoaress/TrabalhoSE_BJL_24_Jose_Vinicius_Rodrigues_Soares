/*
 * ATIVIDADE: Utilização de dois núcleos no RP2040 (BitDogLab)
 * Objetivo: Paralelismo entre leitura de sensores e interface com display, LEDs e buzzer
 * 
 * Core 0: Lê BMP280 (temperatura) e AHT20 (umidade) a cada 500ms
 * Core 1: Atualiza display OLED, controla LEDs RGB e buzzer com base nos dados
 * Comunicação: FIFO do RP2040 (push no Core 0 → pop no Core 1)
 * 
 * Adicionado:
 *   - Buzzer com PWM (GPIO 21)
 *   - Alerta: T ≥ 32°C ou U ≥ 55% → LED vermelho + buzzer
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "font.h"
#include <math.h>

/* ===========================================================================
 *                            DEFINIÇÕES DE HARDWARE
 * =========================================================================== */

#define I2C_PORT_SENSORS i2c0
#define I2C_SDA_SENSORS  0
#define I2C_SCL_SENSORS  1

#define I2C_PORT_DISPLAY i2c1
#define I2C_SDA_DISPLAY  14
#define I2C_SCL_DISPLAY  15
#define OLED_ADDR        0x3C

#define LED_R 13
#define LED_G 11
#define LED_B 12

#define BUZZER_PIN 21
#define BUZZER_FREQUENCY 3500

#define BOTAO_B 6

/* ===========================================================================
 *                        VARIÁVEIS GLOBAIS COMPARTILHADAS
 * =========================================================================== */

ssd1306_t ssd;
struct bmp280_calib_param params;
char str_temp[8] = "T:--.-C";
char str_umi[8]  = "U:--.-%";
bool cor = true;

#define ALERTA_TEMPERATURA_MAX 32.0f
#define ALERTA_UMIDADE_MAX     55.0f

/* ===========================================================================
 *                     FUNÇÕES DO BUZZER (PWM)
 * =========================================================================== */

void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096));
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0);
}

static inline void buzzer_on() {
    pwm_set_gpio_level(BUZZER_PIN, 2048);
}

static inline void buzzer_off() {
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

/* ===========================================================================
 *                  FUNÇÕES DO CORE 1 (Interface)
 * =========================================================================== */

void gpio_irq_handler(uint gpio, uint32_t events) {
    reset_usb_boot(0, 0);
}

void core1_entry(void);

/* ===========================================================================
 *                         CORE 1: INTERFACE DE USUÁRIO
 * =========================================================================== */
void core1_entry() {
    i2c_init(I2C_PORT_DISPLAY, 400 * 1000);
    gpio_set_function(I2C_SDA_DISPLAY, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISPLAY, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISPLAY);
    gpio_pull_up(I2C_SCL_DISPLAY);

    ssd1306_init(&ssd, 128, 64, false, OLED_ADDR, I2C_PORT_DISPLAY);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    gpio_init(LED_R); gpio_set_dir(LED_R, GPIO_OUT);
    gpio_init(LED_G); gpio_set_dir(LED_G, GPIO_OUT);
    gpio_init(LED_B); gpio_set_dir(LED_B, GPIO_OUT);
    gpio_put(LED_G, 1);

    pwm_init_buzzer(BUZZER_PIN);
    buzzer_off();

    printf("Core 1: Display, LEDs e Buzzer inicializados.\n");

    while (true) {
        uint32_t temp_raw = multicore_fifo_pop_blocking();
        uint32_t umi_raw  = multicore_fifo_pop_blocking();

        float temperatura = temp_raw / 100.0f;
        float umidade     = umi_raw  / 100.0f;

        bool alerta = (temperatura >= ALERTA_TEMPERATURA_MAX) || (umidade >= ALERTA_UMIDADE_MAX);

        if (alerta) {
            gpio_put(LED_R, 1); gpio_put(LED_G, 0); gpio_put(LED_B, 0);
            buzzer_on();
        } else {
            gpio_put(LED_R, 0); gpio_put(LED_G, 1); gpio_put(LED_B, 0);
            buzzer_off();
        }

        sprintf(str_temp, "%.1fC", temperatura);
        sprintf(str_umi,  "%.1f%%", umidade);

        ssd1306_fill(&ssd, !cor);
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);
        ssd1306_draw_string(&ssd, "BMP280  AHT10", 10, 28);
        ssd1306_line(&ssd, 63, 25, 63, 60, cor);
        ssd1306_draw_string(&ssd, str_temp, 14, 41);
        ssd1306_draw_string(&ssd, str_umi,  73, 41);
        ssd1306_send_data(&ssd);

        cor = !cor;

        printf("Core 1: T=%.1f°C | U=%.1f%% | %s\n",
               temperatura, umidade, alerta ? "ALERTA! (LED + BUZZER)" : "Normal");

        sleep_ms(100);
    }
}

/* ===========================================================================
 *                         CORE 0: AQUISIÇÃO DE SENSORES
 * =========================================================================== */
int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("=== SISTEMA MULTICORE COM BUZZER - BITDOGLAB ===\n");

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    i2c_init(I2C_PORT_SENSORS, 400 * 1000);
    gpio_set_function(I2C_SDA_SENSORS, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_SENSORS, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_SENSORS);
    gpio_pull_up(I2C_SCL_SENSORS);

    bmp280_init(I2C_PORT_SENSORS);
    bmp280_get_calib_params(I2C_PORT_SENSORS, &params);
    aht20_reset(I2C_PORT_SENSORS);
    aht20_init(I2C_PORT_SENSORS);

    printf("Core 0: Sensores BMP280 e AHT20 inicializados.\n");

    multicore_launch_core1(core1_entry);
    printf("Core 0: Core 1 iniciado (interface + buzzer).\n");

    AHT20_Data dados_aht;
    int32_t raw_temp, raw_press;

    while (true) {
        bmp280_read_raw(I2C_PORT_SENSORS, &raw_temp, &raw_press);
        int32_t temp_x100 = bmp280_convert_temp(raw_temp, &params);

        bool ok = aht20_read(I2C_PORT_SENSORS, &dados_aht);
        int32_t umi_x100 = ok ? (int32_t)(dados_aht.humidity * 100) : -9999;

        multicore_fifo_push_blocking(temp_x100);
        multicore_fifo_push_blocking(umi_x100);

        printf("Core 0: Enviado → T=%.1f°C | U=%.1f%%\n",
               temp_x100 / 100.0f, umi_x100 / 100.0f);

        sleep_ms(500);
    }

    return 0;
}