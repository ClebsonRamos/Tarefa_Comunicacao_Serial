#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// Variáveis booleanas para o controle dos estados dos LEDs verde e azul, e controle do estado dos botões.
static volatile bool estado_led_azul = false, estado_led_verde = false, botao_pressionado = false;
static volatile uint32_t tempo_atual, tempo_passado = 0; // Variáveis para registro de tempo para tratamento do debouncing.

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
void mensagem_botoes(uint botao);

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
    char caractere_digitado; // Variável para armazenamento do caractere digitado.

	inicializacao_maquina_pio(PINO_MATRIZ_LED); // Inicializa matriz de LEDs NeoPixel.
	limpar_o_buffer(); // Usado aqui para inicializar o buffer da matriz de LEDs com valores nulos.
	escrever_no_buffer(); // Atribui a inicialização do buffer da matriz de LEDs.

    stdio_init_all(); // Inicialização das bibliotecas de entrada e saída padrão.
    inicializacao_dos_pinos(); // Inicialização dos pinos utilizados nesse programa.

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
        if(scanf("%c", &caractere_digitado) == 1){ // Verifica continuamente se um caractere foi digitado.
            printf("Caractere digitado: %c\n", caractere_digitado); // Exibe no serial monitor a informação do caractere digitado.
            interpretacao_do_caractere(caractere_digitado);
            if(caractere_digitado >= 48 && caractere_digitado <= 57){ // Desvio condicional caso o caractere digitado for um algarismo.
                manipulacao_matriz_led(caractere_digitado); // Exibe na matriz de LEDs o algarismo digitado.
            }else{ // Caso o caractere digitado não for um algarismo, apaga todos os LEDs da matriz.
                limpar_o_buffer();
                escrever_no_buffer();
            }
        }
        sleep_ms(100); // Delay para poupar processamento do microcontrolador.
	}

    return 0;
}

//-----PROGRAMAS AUXILIARES-----
void gpio_irq_handler(uint pino, uint32_t evento){
    if(pino == PINO_BOTAO_A && !botao_pressionado){ // Verifica se o botão A foi pressionado e se não tinha sido pressionado antes.
        tempo_atual = to_us_since_boot(get_absolute_time()); // Verifica o tempo atual, em microssegundos, para tratar o debouncing.
        if(tempo_atual - tempo_passado > 200000){
            tempo_passado = tempo_atual; // Atualiza a variável tempo_passado.
            botao_pressionado = !botao_pressionado; // Altera o estado da variável botao_pressionado.
            estado_led_verde = !estado_led_verde; // Altera o estado da variável estado_led_verde.
            gpio_put(PINO_LED_VERDE, estado_led_verde); // Ativa/desativa o LED verde
            if(estado_led_verde) // Informa o estado do LED verde no Serial Monitor.
                printf("LED verde ativado.\n");
            else
                printf("LED verde desativado.\n");
            mensagem_botoes(PINO_BOTAO_A);
            botao_pressionado = !botao_pressionado;
        }
    }else if(pino == PINO_BOTAO_B && !botao_pressionado){ // Verifica se o botão B foi pressionado e se não tinha sido pressionado antes.
        tempo_atual = to_us_since_boot(get_absolute_time()); // Verifica o tempo atual, em microssegundos, para tratar o debouncing.
        if(tempo_atual - tempo_passado > 200000){
            tempo_passado = tempo_atual; // Atualiza a variável tempo_passado.
            botao_pressionado = !botao_pressionado; // Altera o estado da variável botao_pressionado.
            estado_led_azul = !estado_led_azul; // Altera o estado da variável estado_led_azul.
            gpio_put(PINO_LED_AZUL, estado_led_azul); // Ativa/desativa o LED azul
            if(estado_led_azul) //Informa o estado do LED azul no Serial Monitor.
                printf("LED azul ativado.\n");
            else
                printf("LED azul desativado.\n");
            mensagem_botoes(PINO_BOTAO_B);
            botao_pressionado = !botao_pressionado;
        }
    }
}

void inicializacao_dos_pinos(void){ // Função para ativação dos pinos utilizados neste código.
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

void interpretacao_do_caractere(char caractere){ // Função para exibição do caractere digitado no display.
    char mensagem[2][20] = {"Caractere ", "Numero "};
    uint registro_de_tipo;
    if(caractere >= 'A' && caractere <= 'Z' || caractere >= 'a' && caractere <= 'z'){
        registro_de_tipo = 0;
        mensagem[0][10] = caractere;
    }else if(caractere >= '0' && caractere <= '9'){
        registro_de_tipo = 1;
        mensagem[1][7] = caractere;
    }
    ssd1306_fill(&ssd, false);
    ssd1306_rect(&ssd, 3, 3, 122, 58, true, false);
    ssd1306_draw_string(&ssd, mensagem[registro_de_tipo], 8, 10);
    ssd1306_send_data(&ssd);
}

void manipulacao_matriz_led(int numero){ // Função para manipulação da matriz de LEDs quando um algarismo for digitado.
    // Os caracteres para algarismos na tabela ASCII possuem indexação que vão de 48 (número 0) à 57 (número 9).
    // Subtrair 48 desse valor de indexação ajusta o valor real do algarismo.
    uint num = numero - 48;
    limpar_o_buffer();
    for(uint i = 0; i < quantidade[num]; i++)
        atribuir_cor_ao_led(coordenadas_numero[num][i], 0, 0, INTENS_LED);
    escrever_no_buffer();
}

void mensagem_botoes(uint botao){ // Função para exibição do estado dos LEDs no display.
    char mensagem[15];
    if(botao == PINO_BOTAO_A){
        if(estado_led_verde)
            strcat(mensagem, "LED Verde ON");
        else
            strcat(mensagem, "LED Verde OFF");
    }else if(botao == PINO_BOTAO_B){
        if(estado_led_azul)
            strcat(mensagem, "LED Azul ON");
        else
            strcat(mensagem, "LED Azul OFF");
    }
    ssd1306_fill(&ssd, false);
    ssd1306_rect(&ssd, 3, 3, 122, 58, true, false);
    ssd1306_draw_string(&ssd, mensagem, 8, 10);
    ssd1306_send_data(&ssd);
}
