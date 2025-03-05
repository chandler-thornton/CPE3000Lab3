#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stm32l476xx.h>

/**
******************************************
* @file main.c
* @brief Main program body
* @author Chandler Thornton + Andy Phan
* @version 1.0
* ----------------------------------------
* Lab 3
******************************************
*/

#define SYS_CLK_FREQ 400000
#define SYSTICK_TRIGGER_SLOW (SYS_CLK_FREQ * 4)
#define SYSTICK_TRIGGER_MEDIUM (SYS_CLK_FREQ * 2)
#define SYSTICK_TRIGGER_FAST (SYS_CLK_FREQ * 1)
#define SYSTICK_DEBOUNCE_TIMER (SYS_CLK_FREQ / 100);

#define startSysTickTimer_MACRO (SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk)

const uint32_t SHIFT_SPEEDS[] = {SYSTICK_TRIGGER_SLOW, SYSTICK_TRIGGER_MEDIUM, SYSTICK_TRIGGER_FAST};
uint8_t current_speed = 0;

uint8_t led_direction = 0;
uint8_t current_led;

volatile uint8_t leftbutton_flag = 0;
volatile uint8_t rightbutton_flag = 0;
volatile uint8_t systick_flag = 0;

volatile uint8_t debounce_timer = 0;

typedef struct{
	GPIO_TypeDef* port;
	uint16_t pin;
} LED;

const LED LED_ARRAY[]= {
		{GPIOC, (0x1 << 3)},
		{GPIOC, (0x1 << 2)},
		{GPIOH, (0x1 << 1)},
		{GPIOH, (0x1 << 0)},
		{GPIOC, (0x1 << 13)},
		{GPIOB, (0x1 << 7)},
		{GPIOA, (0x1 << 15)},
		{GPIOA, (0x1 << 14)}
};

void GPIO_Init()
{
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN
						| RCC_AHB2ENR_GPIOBEN
						| RCC_AHB2ENR_GPIOCEN
						| RCC_AHB2ENR_GPIOHEN;

	GPIOA->MODER &= ~(GPIO_MODER_MODE14 | GPIO_MODER_MODE15);
	GPIOA->MODER |= (GPIO_MODER_MODE14_0 | GPIO_MODER_MODE15_0);
	GPIOB->MODER &= ~(GPIO_MODER_MODE7);
	GPIOB->MODER |= (GPIO_MODER_MODE7_0);
	GPIOC->MODER &= ~(GPIO_MODER_MODE2
						| GPIO_MODER_MODE3
						| GPIO_MODER_MODE13);
	GPIOC->MODER |= (GPIO_MODER_MODE2_0
						| GPIO_MODER_MODE3_0
						| GPIO_MODER_MODE13_0);
	GPIOH->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1);
	GPIOH->MODER |= (GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0);

	GPIOA->OTYPER &= ~(GPIO_MODER_MODE14 | GPIO_MODER_MODE15);
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT7);
	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT2
						| GPIO_OTYPER_OT3
						| GPIO_OTYPER_OT13);
	GPIOH->OTYPER &= ~(GPIO_OTYPER_OT0 | GPIO_OTYPER_OT1);

	GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED14 | GPIO_OSPEEDR_OSPEED15);
	GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED7);
	GPIOC->OSPEEDR &= ~(GPIO_OTYPER_OT2
						| GPIO_OTYPER_OT3
						| GPIO_OTYPER_OT13);
	GPIOH->OSPEEDR &= ~(GPIO_OTYPER_OT0 | GPIO_OTYPER_OT1);

	GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD14 | GPIO_PUPDR_PUPD15);
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD7);
	GPIOC->PUPDR &= ~(GPIO_OTYPER_OT2
						| GPIO_OTYPER_OT3
						| GPIO_OTYPER_OT13);
	GPIOH->PUPDR &= ~(GPIO_OTYPER_OT0 | GPIO_OTYPER_OT1);


	//Button setup
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

	GPIOA->MODER &= ~(GPIO_MODER_MODE2 | GPIO_MODER_MODE3);
	GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD2 | GPIO_PUPDR_PUPD3);
}

void SysTick_Handler(void)
{
	systick_flag = 1;
}

void configureSysTickInterrupt(uint32_t reload_value)
{
	SysTick->CTRL = 0;
	NVIC_SetPriority(SysTick_IRQn, 7);

	SysTick->LOAD = SYSTICK_TRIGGER_SLOW;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

void EXTI2_IRQHandler(void)
{
    if (EXTI->PR1 & EXTI_PR1_PIF2)
    {
        EXTI->PR1 = EXTI_PR1_PIF2; // Clear pending flag
    }
}

void EXTI3_IRQHandler(void)
{
    if (EXTI->PR1 & EXTI_PR1_PIF3)
    {
        EXTI->PR1 = EXTI_PR1_PIF3; // Clear pending flag
    }
}
uint8_t shift_led()
{
	LED_ARRAY[current_led].port->ODR &= ~LED_ARRAY[current_led].pin;

	if(!led_direction)
	{
		current_led = (current_led == 8) ? 0 : current_led + 1;
	}
	else
	{
		current_led = (current_led == 0) ? 8 : current_led - 1;
	}

	LED_ARRAY[current_led].port->ODR |= LED_ARRAY[current_led].pin;
	return current_led;
}

void increment_speed()
{
	if(((current_led == 8) && !led_direction) || ((current_led == 0) && led_direction))
	{
		current_speed = (current_speed == 2) ? 0 : current_speed + 1;
	}

	SysTick->LOAD = SHIFT_SPEEDS[current_speed];
}

volatile uint32_t button_press_count = 0; 		//Stores final count value @ end of sequence (250k cycles)
volatile uint32_t button_press_sequence = 0;	//Count value per sequence
volatile uint32_t button_press_timer = 0;		//Length of sequence
volatile bool button_state = 0;
volatile bool button_pressed = 0;

void debounce_2()
{
    volatile bool current_button_state = (GPIOA->IDR & GPIO_IDR_ID3) ? 1 : 0;

    if (current_button_state != button_state)
    {
        for (volatile int i = 0; i < 500; i++);

        current_button_state = (GPIOA->IDR & GPIO_IDR_ID3) ? 1 : 0;

        if (button_state == 0 && current_button_state == 1)
        {
            led_direction = 1;
        }
    }

    button_state = current_button_state;
}

int main(void)
{
	/*
	 * Initialize SysTick
	 */
	configureSysTickInterrupt(s[current_speed]);
	startSysTickTimer_MACRO;

	/*
	 * Initialize GPIO
	 */
	GPIO_Init();

    EXTI->IMR1 |= (EXTI_IMR1_IM2 | EXTI_IMR1_IM3);
    EXTI->RTSR1 &= ~(EXTI_RTSR1_RT2 | EXTI_RTSR1_RT3);
	EXTI->FTSR1 |= (EXTI_FTSR1_FT2 | EXTI_FTSR1_FT3);
	NVIC_EnableIRQ(EXTI2_IRQn);
	NVIC_SetPriority(EXTI2_IRQn, 2);
	NVIC_EnableIRQ(EXTI3_IRQn);
	NVIC_SetPriority(EXTI3_IRQn, 2);

    while (1)
    {
    	if(systick_flag)
    	{
    		shift_led();
    		increment_speed();
    		systick_flag = 0;
    	}

    	debounce_2();
    }

    return 0;
}
