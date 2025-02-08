TAREFA COMUNICAÇÃO SERIAL

Este programa tem a funcionalidade de interpretar um caractere digitado via teclado no terminal serial monitor do ambiente de desenvolvimento Visual Studio Code e apresentar uma resposta tanto no serial monitor quanto na tela OLED presente na placa BitDogLab. Caso o caractere digitado seja um algarismo, a matriz de LEDs RGB presente na placa BitDogLab apresentará um padrão representando o algarismo digitado.
O botão A presente na BitDogLab foi programado para alterar o estado do LED RGB verde (ligado/desligado) e o botão B para alterar o estado do LED RGB azul. A alteração do estado dos LEDs via botões será informado com uma mensagem tanto no serial monitor quanto na tela OLED.


COMPONENTES

Para executar este programa, é necessário:
  * 1 placa Raspberry Pi Pico W, com um microcontrolador RP2040;
  * 2 LEDs (azul e verde);
  * 1 computador com o VS Code instalado;
  * 1 cabo USB para comunicação com o terminal serial monitor.


FUNCIONALIDADE DO PROGRAMA

O programa recebe um caractere digitado pelo teclado via terminal serial monitor e exibe uma mensagem no terminal e no display OLED informando qual caractere foi digitado. Caso o caractere digitado seja um algarismo, essa algarismo também é exibido na matriz de LEDs da placa BitDogLab.
Os botões A e B presentes nesse equipamento alternam o estado dos LEDs verde e azul, respectivamente. A função responsável por realizar essa alteração foi programada como uma rotina de interrupção na execução do código.


LINK DO VÍDEO DE APRESENTAÇÃO DO PROJETO

https://drive.google.com/file/d/1V4HZtlW5pEooCiEL_AjL7lYv1aOkjbdJ/view?usp=sharing
