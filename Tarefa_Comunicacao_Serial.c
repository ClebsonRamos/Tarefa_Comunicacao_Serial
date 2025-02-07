#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "inclusao/ssd1306.h"
#include "inclusao/font.h"

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

// Definição do número de LEDs e pino.
#define CONTADOR_LED 25
#define PINO_MATRIZ_LED 7
#define PINO_LED_VERDE 11
#define PINO_LED_AZUL 12
#define PINO_BOTAO_A 5
#define PINO_BOTAO_B 6
#define TEMPO_LIMITE_DEBOUNCING 200000
#define I2C_PORTA i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C
#define INTENS_LED 200

//-----VARIÁVEIS GLOBAIS-----
// Definição de pixel GRB
struct pixel_t {
	uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t LED_da_matriz;

// Declaração do buffer de pixels que formam a matriz.
LED_da_matriz leds[CONTADOR_LED];

// Variáveis para uso da máquina PIO.
PIO maquina_pio;
uint variavel_maquina_de_estado;

const uint8_t quantidade[10] = {12, 8, 11, 10, 9, 11, 12, 8, 13, 12}; // Quantidade de LEDs que serão ativados para cada número.
const uint8_t coordenadas_numero[10][13] = { // Vetor com a identificação dos LEDs que serão ativados para cada número.
    {1, 2, 3, 6, 8, 11, 13, 16, 18, 21, 22, 23}, // 0
    {1, 2, 3, 7, 12, 16, 17, 22}, // 1
    {1, 2, 3, 6, 11, 12, 13, 18, 21, 22, 23}, // 2
    {1, 2, 3, 8, 11, 12, 18, 21, 22, 23}, // 3
    {1, 8, 11, 12, 13, 16, 18, 21, 23}, // 4
    {1, 2, 3, 8, 11, 12, 13, 16, 21, 22, 23}, // 5
    {1, 2, 3, 6, 8, 11, 12, 13, 16, 21, 22, 23}, // 6
    {1, 8, 11, 16, 18, 21, 22, 23}, // 7
    {1, 2, 3, 6, 8, 11, 12, 13, 16, 18, 21, 22, 23}, // 8
    {1, 2, 3, 8, 11, 12, 13, 16, 18, 21, 22, 23} // 9
};

static volatile bool estado_led_azul = false, estado_led_verde = false;
static volatile uint32_t tempo_atual, tempo_passado = 0;

ssd1306_t ssd; // Inicialização da estrutura do display

//-----PROTÓTIPOS-----
void inicializacao_maquina_pio(uint pino);
void atribuir_cor_ao_led(const uint indice, const uint8_t r, const uint8_t g, const uint8_t b);
void limpar_o_buffer(void);
void escrever_no_buffer(void);

void gpio_irq_handler(uint pino, uint32_t evento);
void inicializacao_dos_pinos(void);
void interpretacao_do_caractere(char caractere);
void manipulacao_matriz_led(int numero);

// Inicializa a máquina PIO para controle da matriz de LEDs.
void inicializacao_maquina_pio(uint pino){
	uint programa_pio, i;
	// Cria programa PIO.
	programa_pio = pio_add_program(pio0, &ws2818b_program);
	maquina_pio = pio0;

	// Toma posse de uma máquina PIO.
	variavel_maquina_de_estado = pio_claim_unused_sm(maquina_pio, false);
	if (variavel_maquina_de_estado < 0) {
		maquina_pio = pio1;
		variavel_maquina_de_estado = pio_claim_unused_sm(maquina_pio, true); // Se nenhuma máquina estiver livre, panic!
	}

	// Inicia programa na máquina PIO obtida.
	ws2818b_program_init(maquina_pio, variavel_maquina_de_estado, programa_pio, pino, 800000.f);

	// Limpa buffer de pixels.
	for (i = 0; i < CONTADOR_LED; ++i) {
		leds[i].R = 0;
		leds[i].G = 0;
		leds[i].B = 0;
	}
}

// Atribui uma cor RGB a um LED.
void atribuir_cor_ao_led(const uint indice, const uint8_t r, const uint8_t g, const uint8_t b){
	leds[indice].R = r;
	leds[indice].G = g;
	leds[indice].B = b;
}

// Limpa o buffer de pixels.
void limpar_o_buffer(void){
	for (uint i = 0; i < CONTADOR_LED; ++i)
		atribuir_cor_ao_led(i, 0, 0, 0);
}

// Escreve os dados do buffer nos LEDs.
void escrever_no_buffer(void){
	// Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
	for (uint i = 0; i < CONTADOR_LED; ++i){
		pio_sm_put_blocking(maquina_pio, variavel_maquina_de_estado, leds[i].G);
		pio_sm_put_blocking(maquina_pio, variavel_maquina_de_estado, leds[i].R);
		pio_sm_put_blocking(maquina_pio, variavel_maquina_de_estado, leds[i].B);
	}
	sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

//-----PROGRAMA PRINCIPAL-----
int main(void){
    char caractere_digitado;
	// Inicializa matriz de LEDs NeoPixel.
	inicializacao_maquina_pio(PINO_MATRIZ_LED);
	limpar_o_buffer();
	escrever_no_buffer();

    stdio_init_all();
    inicializacao_dos_pinos();

    // Inicialização das interrupções atribuídas aos botões.
    gpio_set_irq_enabled_with_callback(PINO_BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(PINO_BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORTA); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

	// Loop principal.
	while(true){
        printf("Caractere: ");
        if(scanf("%c", &caractere_digitado) == 1){
            printf("Caractere digitado: %c\n", caractere_digitado);
            interpretacao_do_caractere(caractere_digitado);
            if(caractere_digitado >= 48 && caractere_digitado <= 57){
                manipulacao_matriz_led((int)caractere_digitado);
            }else{
                limpar_o_buffer();
                escrever_no_buffer();
            }
        }
        sleep_ms(100);
	}
}

//-----PROGRAMAS AUXILIARES-----
void gpio_irq_handler(uint pino, uint32_t evento){
    if(gpio_get(PINO_BOTAO_A)){
        tempo_atual = to_us_since_boot(get_absolute_time());
        if(tempo_atual - tempo_passado > TEMPO_LIMITE_DEBOUNCING){
            tempo_passado = tempo_atual;
            estado_led_verde = !estado_led_verde;
            gpio_put(PINO_LED_VERDE, estado_led_verde);
            if(estado_led_verde)
                printf("LED verde ativado.\n");
            else
                printf("LED verde desativado.\n");
        }
    }else if(gpio_get(PINO_BOTAO_B)){
        tempo_atual = to_us_since_boot(get_absolute_time());
        if(tempo_atual - tempo_passado > TEMPO_LIMITE_DEBOUNCING){
            estado_led_azul = !estado_led_azul;
            gpio_put(PINO_LED_AZUL, estado_led_azul);
            if(estado_led_azul)
                printf("LED azul ativado.\n");
            else
                printf("LED azul desativado.\n");
        }
    }
}

void inicializacao_dos_pinos(void){
    gpio_init(PINO_LED_AZUL);
    gpio_init(PINO_LED_VERDE);
    gpio_set_dir(PINO_LED_AZUL, GPIO_OUT);
    gpio_set_dir(PINO_LED_VERDE, GPIO_OUT);
    gpio_put(PINO_LED_AZUL, false);
    gpio_put(PINO_LED_VERDE, false);

    gpio_init(PINO_BOTAO_A);
    gpio_init(PINO_BOTAO_B);
    gpio_set_dir(PINO_BOTAO_A, GPIO_IN);
    gpio_set_dir(PINO_BOTAO_B, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_A);
    gpio_pull_up(PINO_BOTAO_B);

    i2c_init(I2C_PORTA, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Defina a função do pino GPIO para I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Defina a função do pino GPIO para I2C
    gpio_pull_up(I2C_SDA); // Pull up na linha de dados
    gpio_pull_up(I2C_SCL); // Pull up na linha de clock
}

void interpretacao_do_caractere(char caractere){
    ssd1306_fill(&ssd, false);
    ssd1306_rect(&ssd, 3, 3, 122, 58, true, false);
    ssd1306_draw_char(&ssd, caractere, 8, 10);
    ssd1306_send_data(&ssd);
}

void manipulacao_matriz_led(int numero){
    for(uint i = 0; i < quantidade[numero]; i++)
        atribuir_cor_ao_led(coordenadas_numero[numero][i], 0, 0, INTENS_LED);
    escrever_no_buffer();
}
