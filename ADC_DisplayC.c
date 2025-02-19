#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define LED_VERDE 11  
#define LED_AZUL 12  
#define LED_VERMELHO 13  
#define JOYSTICK_X_PIN 27  // GPIO para eixo X
#define JOYSTICK_Y_PIN 26  // GPIO para eixo Y
#define JOYSTICK_PB 22 // GPIO para botão do Joystick
#define Botao_A 5 // GPIO para botão A
#define dead_angle 100


ssd1306_t ssd; // Inicializa a estrutura do display
static volatile uint32_t last_time = 0;
static volatile bool estado_LED = true;
static volatile bool estado_BORDA = false;
void setup_pwm(uint gpio, float duty_cycle);
void gpio_irq_handler(uint gpio, uint32_t events);


int main()
{
  stdio_init_all();
  gpio_init(JOYSTICK_PB);
  gpio_set_dir(JOYSTICK_PB, GPIO_IN);
  gpio_pull_up(JOYSTICK_PB); 
  gpio_init(Botao_A);
  gpio_set_dir(Botao_A, GPIO_IN);
  gpio_pull_up(Botao_A);
  gpio_init(LED_VERDE);
  gpio_set_dir(LED_VERDE, GPIO_OUT);
  gpio_put(LED_VERDE, 0); 
  gpio_init(LED_AZUL);
  gpio_set_dir(LED_AZUL, GPIO_OUT);
  gpio_put(LED_AZUL, 0); 
  gpio_init(LED_VERMELHO);
  gpio_set_dir(LED_VERMELHO, GPIO_OUT);
  gpio_put(LED_VERMELHO, 0);

  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA); // Pull up the data line
  gpio_pull_up(I2C_SCL); // Pull up the clock line
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  adc_init();
  adc_gpio_init(JOYSTICK_X_PIN);
  adc_gpio_init(JOYSTICK_Y_PIN);  
  gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL,true);

  static volatile uint pos_quadrado[2]={61,29}; 
  static float duty_cycle_VERMELHO,duty_cycle_AZUL;

  bool cor = true;
  while (true)
  {
    adc_select_input(1); 
    uint16_t vrx_value = adc_read(); 
    adc_select_input(0); 
    uint16_t vry_value = adc_read();  
    if((vrx_value-2047)>dead_angle) {
        duty_cycle_VERMELHO = (vrx_value-2047)/2047.0;
        pos_quadrado[0] = 61+duty_cycle_VERMELHO*58;
    }else if((2047-vrx_value)>dead_angle){
        duty_cycle_VERMELHO = (2047-vrx_value)/2047.0;
        pos_quadrado[0] = 61-duty_cycle_VERMELHO*58;
    }else{
        pos_quadrado[0] = 61;
        duty_cycle_VERMELHO = 0;
    }
    if ((vry_value-2047)>dead_angle) {
        duty_cycle_AZUL = (vry_value-2047)/2047.0;
        pos_quadrado[1] = 29-duty_cycle_AZUL*27;
    }else if((2047-vry_value)>dead_angle) {
        duty_cycle_AZUL = (2047-vry_value)/2047.0;
        pos_quadrado[1] = 29+duty_cycle_AZUL*27;
    }else{
        pos_quadrado[1] = 29;
        duty_cycle_AZUL = 0;
    }
    if(estado_LED){
        setup_pwm(LED_VERMELHO, duty_cycle_VERMELHO);
        setup_pwm(LED_AZUL, duty_cycle_AZUL);
    }else{
        setup_pwm(LED_VERMELHO, 0);
        setup_pwm(LED_AZUL, 0); 
    }
    ssd1306_fill(&ssd, !cor); // Limpa o display
    if(estado_BORDA){
        ssd1306_rect(&ssd, 0, 0, 128, 64, cor, !cor); // Desenha borda
    }
    ssd1306_draw_string(&ssd,"0",pos_quadrado[0],pos_quadrado[1]);// Desenha o quadrado
    ssd1306_send_data(&ssd); 
    sleep_ms(100);
  }
  return 0;
}

void setup_pwm(uint gpio, float duty_cycle) {
  gpio_set_function(gpio, GPIO_FUNC_PWM);// Configura GPIO para PWM
  uint slice = pwm_gpio_to_slice_num(gpio);// Obtém o slice associado ao pino GPIO
  pwm_set_clkdiv(slice, 250.0f);// Configura divisor de clock para 250 (clock base de 500kHz)
  pwm_set_wrap(slice, 10000);// Define o contador máximo (wrap) para 10.000
  pwm_set_gpio_level(gpio, (uint16_t)(10000 * (duty_cycle / 100.0)));// Define o nível do duty cycle (0,12% de 10.000 -> 12)
  pwm_set_enabled(slice, true);// Ativa o PWM
}

void gpio_irq_handler(uint gpio, uint32_t events){
uint32_t current_time = to_us_since_boot(get_absolute_time());// Obtém o tempo atual em microssegundos
if (current_time - last_time > 200000){ // Verifica se passou tempo suficiente desde o último evento com 200 ms de debouncing 
    last_time = current_time; // Atualiza o tempo do último evento
    if(gpio == Botao_A){
      estado_LED = !estado_LED; //Altera o estado dos leds
      }
    else if(gpio == JOYSTICK_PB){
      estado_BORDA = !estado_BORDA;
      setup_pwm(LED_VERDE, estado_BORDA);
    }
}
}