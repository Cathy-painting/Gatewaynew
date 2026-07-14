#include "bsp_led.h"

static uint8_t led_state = 0x00;

void bsp_led_write8(uint8_t value){
	led_state=value;
	
	HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,GPIO_PIN_SET);

	for(uint8_t i=0; i<8; i++){
				//	0000 0000    value
		    //  0000 0010    2
		    //  0000 0001    1
			if(value & (1<<i)){
				 HAL_GPIO_WritePin(GPIOC,GPIO_PIN_8 << i,GPIO_PIN_RESET);
			
			}else{
				 HAL_GPIO_WritePin(GPIOC,GPIO_PIN_8 << i,GPIO_PIN_SET);
			}
		
		}

		HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,GPIO_PIN_RESET);

}

void bsp_led_on(uint8_t led){
	//led=3 
  if(led <1 || led > 8)
	{
	return;
	}
//0000 0000	
//0000 0001
	led_state |= (1<<(led-1));
	bsp_led_write8(led_state);
}

void bsp_led_off(uint8_t led){
	 //led= 4
	if(led<1 || led>8)
	{
	return;
	}
//0000 1101 yuan
//0000 1000 mie
//1111 0111
	
//0000 0001
	led_state &= ~(1<< (led-1));
	bsp_led_write8(led_state);

}

void bsp_led_toggle(uint8_t led){
	
  if(led<1||led>8){
		return;
	}
//0000 0101
//0100 0100
//0000 0001
	led_state ^= (1<<(led-1));
	
  bsp_led_write8(led_state);
}











