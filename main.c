/* Descrição:
 * Este projeto demonstra o uso de multicore no RP2040, conforme a tarefa.
 *
 * Núcleo 0 (Core 0):
 * - Responsável pela aquisição contínua de dados.
 * - Sensor 1: Leitor RFID MFRC522 (via SPI).
 * - Sensor 2: Potenciômetro do Joystick (via ADC no GPIO 26).
 * - Envia dados para o Core 1 via FIFO.
 *
 * Núcleo 1 (Core 1):
 * - Responsável pela Interface de Usuário (UI).
 * - Atuador 1: Display OLED SSD1306 (via I2C).
 * - Atuador 2: LEDs (Azul e Verde).
 * - Recebe dados do Core 0 via FIFO e atualiza a UI.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"  // <--- Biblioteca Multicore
#include "hardware/adc.h"   // <--- Biblioteca ADC
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "mfrc522.h"

// --- Configurações (do seu código original) ---
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define LED_AZUL 12
#define LED_VERDE 13

// --- Configurações dos Sensores ---
#define ADC_PIN 26 // GPIO 26 para o ADC (Canal 0)
#define ADC_CHANNEL 0

// --- Protocolo de Comunicação FIFO ---
// Usamos um enum para dizer ao Core 1 que tipo de dado estamos enviando
typedef enum {
    TYPE_ADC_DATA,      // Indica que o próximo uint32_t é um valor de ADC
    TYPE_RFID_UID,      // Indica que o próximo uint32_t é uma UID de 4 bytes
    TYPE_RFID_FAIL      // Indica que a leitura do RFID falhou
} data_type_t;

// ====================================================================
//               CÓDIGO DO NÚCLEO 1 (CORE 1) - INTERFACE
// ====================================================================
//

/*
 * Esta função será a entrada principal do Core 1.
 * Ela inicializa os periféricos da UI (Display, LEDs)
 * e entra em um loop para sempre, aguardando dados do Core 0.
 */
void core1_entry() {
    // --- Inicializa Periféricos do Core 1 ---
    
    // Inicializa LEDs
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_put(LED_AZUL, 0);

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, 0);

    // Inicializa display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // --- Variáveis de Estado da UI ---
    char adc_str[24];         // String para valor do ADC
    char status_msg1[24] = "Aproxime o";
    char status_msg2[24] = "  CARTAO";
    uint16_t last_adc_val = 0;
    bool cor = true;

    // --- Loop Principal do Core 1 ---
    while (true) {
        // 1. Aguarda e processa dados do Core 0
        // multicore_fifo_pop_blocking() espera até que um dado chegue
        data_type_t data_type = (data_type_t)multicore_fifo_pop_blocking();
        uint32_t payload = multicore_fifo_pop_blocking();

        switch (data_type) {
            case TYPE_ADC_DATA:
                last_adc_val = (uint16_t)payload;
                break;

            case TYPE_RFID_UID:
                // Desliga LEDs (preparação)
                gpio_put(LED_AZUL, 0);
                gpio_put(LED_VERDE, 0);

                // Descompacta a UID de 4 bytes do payload
                uint8_t uid[4];
                uid[0] = (payload >> 0) & 0xFF; // Byte 0
                uid[1] = (payload >> 8) & 0xFF; // Byte 1
                uid[2] = (payload >> 16) & 0xFF; // Byte 2
                uid[3] = (payload >> 24) & 0xFF; // Byte 3

                // Atualiza mensagens do display
                strcpy(status_msg1, "UID:");
                sprintf(status_msg2, "%02X %02X %02X %02X", uid[0], uid[1], uid[2], uid[3]);

                // Verifica UID para acionar LEDs (lógica do seu main.c)
                if (uid[0] == 0x00 && uid[1] == 0xFC && uid[2] == 0x95 && uid[3] == 0x7C) {
                    gpio_put(LED_AZUL, 1);
                } else if (uid[0] == 0xC0 && uid[1] == 0x33 && uid[2] == 0xC3 && uid[3] == 0x80) {
                    gpio_put(LED_VERDE, 1);
                }
                break;

            case TYPE_RFID_FAIL:
                strcpy(status_msg1, "Falha na");
                strcpy(status_msg2, " Leitura");
                gpio_put(LED_AZUL, 0);
                gpio_put(LED_VERDE, 0);
                break;
        }

        // 2. Redesenha a tela com os dados mais recentes
        // Esta seção é executada sempre que um *novo* dado (ADC ou RFID) é recebido
        
        // Formata string do ADC
        sprintf(adc_str, "ADC: %-4d", last_adc_val);

        ssd1306_fill(&ssd, !cor);                     // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);      // Desenha uma linha
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);      // Desenha uma linha
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);
        
        // --- Área de Dados Dinâmicos ---
        ssd1306_draw_string(&ssd, adc_str, 10, 28);      // Mostra valor do ADC
        ssd1306_draw_string(&ssd, status_msg1, 8, 41);   // Mostra status RFID (Linha 1)
        ssd1306_draw_string(&ssd, status_msg2, 8, 52);   // Mostra status RFID (Linha 2)

        ssd1306_send_data(&ssd); // Envia o buffer para o display
    }
}

// ====================================================================
//               CÓDIGO DO NÚCLEO 0 (CORE 0) - AQUISIÇÃO
// ====================================================================
//

int main() {
    stdio_init_all();
    sleep_ms(2000); // Aguarda o monitor serial (opcional)
    printf("Iniciando sistema Dual-Core RFID/ADC...\n");

    // --- Inicializa Periféricos do Core 0 ---

    // Inicializa ADC (Sensor 2)
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHANNEL);

    // Inicializa MFRC522 (Sensor 1)
    MFRC522Ptr_t mfrc = MFRC522_Init();
    PCD_Init(mfrc, spi0);
    PCD_AntennaOn(mfrc);
    printf("Core 0: MFRC522 e ADC inicializados.\n");

    // --- Lança o Core 1 ---
    printf("Core 0: Lançando Core 1 para cuidar da UI...\n");
    multicore_launch_core1(core1_entry);

    // --- Loop Principal do Core 0 ---
    while (true) {
        
        // Tarefa 1: Ler ADC (Leitura contínua)
        uint16_t adc_value = adc_read();
        
        // Envia dados do ADC para o Core 1
        multicore_fifo_push_blocking(TYPE_ADC_DATA);
        multicore_fifo_push_blocking(adc_value);

        // Tarefa 2: Verificar RFID (Leitura não-bloqueante)
        if (PICC_IsNewCardPresent(mfrc)) {
            printf("Core 0: Cartão detectado!\n");

            if (PICC_ReadCardSerial(mfrc)) {
                // Sucesso na leitura
                printf("Core 0: UID lida com sucesso.\n");
                
                // Compacta os 4 bytes da UID em um único uint32_t
                // mfrc->uid.uidByte[0] -> byte menos significativo
                // mfrc->uid.uidByte[3] -> byte mais significativo
                uint32_t uid_compactada = 0;
                uid_compactada |= (uint32_t)(mfrc->uid.uidByte[3]) << 24;
                uid_compactada |= (uint32_t)(mfrc->uid.uidByte[2]) << 16;
                uid_compactada |= (uint32_t)(mfrc->uid.uidByte[1]) << 8;
                uid_compactada |= (uint32_t)(mfrc->uid.uidByte[0]);

                // Envia dados do RFID para o Core 1
                multicore_fifo_push_blocking(TYPE_RFID_UID);
                multicore_fifo_push_blocking(uid_compactada);

            } else {
                // Falha na leitura
                printf("Core 0: Falha ao ler UID do cartão.\n");
                multicore_fifo_push_blocking(TYPE_RFID_FAIL);
                multicore_fifo_push_blocking(0); // Payload nulo
            }

            // Pausa para evitar múltiplas leituras do mesmo cartão
            // O Core 1 continuará executando e atualizando o display
            // com o último valor de ADC recebido antes desta pausa.
            sleep_ms(2000); 

            // Limpa os LEDs e reseta a UI para "Aproxime"
            // (Enviando um novo dado de ADC, o Core 1 limpa a tela)
            adc_value = adc_read();
            multicore_fifo_push_blocking(TYPE_ADC_DATA);
            multicore_fifo_push_blocking(adc_value);

        }
        
        // Pequena pausa para não sobrecarregar o FIFO
        // e dar tempo para o loop do Core 1 executar
        sleep_ms(100); 
    }
}