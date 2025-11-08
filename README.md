# Monitoramento Multicore com Sensores

## Descrição
Este projeto implementa um sistema de monitoramento em tempo real utilizando os dois núcleos do microcontrolador **RP2040** na placa **BitDogLab**. O sistema lê continuamente os sensores **BMP280** (temperatura e pressão) e **AHT20** (temperatura e umidade), processa os dados em um núcleo e atualiza a interface de usuário em outro núcleo, com comunicação via **FIFO**.

O **Core 0** é responsável pela aquisição contínua e envio de dados.  
O **Core 1** recebe os dados, atualiza o **display OLED**, controla os **LEDs RGB** e ativa o **buzzer** em caso de alerta.

## Funcionalidades
- **Leitura paralela de dois sensores** (BMP280 e AHT20)
- **Comunicação entre núcleos via FIFO**
- **Atualização em tempo real do display OLED**
- **Controle de LEDs RGB** (verde = normal, vermelho = alerta)
- **Alerta sonoro com buzzer** (T ≥ 32°C ou U ≥ 55%)
- **Modo BOOTSEL** ao pressionar o botão B

## Hardware Utilizado
- **Microcontrolador**: RP2040 (Raspberry Pi Pico)
- **Placa**: BitDogLab
- **Sensores**:
  - BMP280 (temperatura e pressão)
  - AHT20 (temperatura e umidade)
- **Display**: OLED SSD1306 (128x64) via I²C
- **LEDs RGB**: Indicadores de status (GPIOs 11, 12, 13)
- **Buzzer**: Alerta sonoro com PWM (GPIO 21)
- **Botão B**: Entrada em modo BOOTSEL (GPIO 6)

## Configuração
1. **Conexões I²C**:
   - Sensores: `SDA → GPIO 0`, `SCL → GPIO 1` (i2c0)
   - Display: `SDA → GPIO 14`, `SCL → GPIO 15` (i2c1)
2. **LEDs RGB**:
   - Vermelho: GPIO 13
   - Verde: GPIO 11
   - Azul: GPIO 12
3. **Buzzer**: GPIO 21 (PWM)
4. **Botão B**: GPIO 6 (com pull-up interno)

## Estrutura do Código
- **`main_multicore.c`**: 
  - `main()` → Core 0: leitura de sensores + envio via FIFO
  - `core1_entry()` → Core 1: interface (display, LEDs, buzzer)
- **`bmp280.c/h`**: Leitura e conversão de temperatura/pressão
- **`aht20.c/h`**: Leitura de temperatura e umidade
- **`ssd1306.c/h`**: Controle do display OLED
- **`font.h`**: Fonte para exibição de texto

## Uso
1. Compile com o **Pico SDK**.
2. Grave o firmware na placa BitDogLab.
3. O sistema inicia automaticamente:
   - **Core 0** lê os sensores a cada 500ms
   - **Core 1** atualiza o display e verifica alertas
4. **Pressione o botão B** para entrar no modo BOOTSEL (atualização de firmware).

## Alertas
- **Condição de alerta**:  
  `Temperatura ≥ 32.0°C` **ou** `Umidade ≥ 55.0%`
- **Ações**:
  - LED vermelho aceso
  - Buzzer ligado (tom contínuo em 3500 Hz)
  - Display mostra valores em tempo real
- **Condição normal**:
  - LED verde aceso
  - Buzzer desligado

## Exemplo de Saída no Display
CEPEDI   TIC37
EMBARCATECH
BMP280  AHT10
T:33.1C     U:60.2%

