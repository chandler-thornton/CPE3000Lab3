#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stm32l476xx.h>

/**
************************************************************
* @file main.c
* @brief Main program body
* @author Andy Phan, Chandler Thornton
* @version 1.0
* ----------------------------------------------------------
* GPIO Interfacing Lab
*
* Configure the Nucleo-L476RG board so that when the left or right
* switch is pressed, there will be an external interrupt that will
* cause the array of 8 LEDs will light up one by one from left
* to right or right to left, respectively. Every time the LED array
* reaches the end of either side, it will roll off and continue to
* the other side speeding up. The speed will increase from slow,
* medium, to fast and start at slow again to cycle.
*
*
************************************************************
*/

#define SYS_CLK_FREQ 400000
#define SYSTICK_TRIGGER_SLOW (SYS_CLK_FREQ * 4) //4
#define SYSTICK_TRIGGER_MEDIUM (SYS_CLK_FREQ * 2) //2
#define SYSTICK_TRIGGER_FAST (SYS_CLK_FREQ * 1) // 1
#define SYSTICK_DEBOUNCE_TIMER (SYS_CLK_FREQ / 0.03)

#define startSysTickTimer_MACRO (SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk);

const uint32_t SHIFT_SPEEDS[] = {SYSTICK_TRIGGER_SLOW,
								SYSTICK_TRIGGER_MEDIUM,
								SYSTICK_TRIGGER_FAST};
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

//===============================================================
// GPIO_Init()
//
// @param: none
// @return: none
//
// Configures LED GPIO pins
//===============================================================
void GPIO_Init()
{
	//GPIO enable
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN
						| RCC_AHB2ENR_GPIOBEN
						| RCC_AHB2ENR_GPIOCEN
						| RCC_AHB2ENR_GPIOHEN;

	//LED setup
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
}

//==========================================================
// button_init()
//
// @param: none
// @return: none
//
// Configures button GPIO and external interrupt for PA0,1
//==========================================================
void button_init()
{

	GPIOA->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1);
	GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD0 | GPIO_PUPDR_PUPD1);

	//Button setup
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;
	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI1;

	EXTI->IMR1 |= EXTI_IMR1_IM0 | EXTI_IMR1_IM1;

    EXTI->RTSR1 |= (EXTI_RTSR1_RT0 | EXTI_RTSR1_RT1);
	EXTI->FTSR1 &= ~(EXTI_FTSR1_FT0 | EXTI_FTSR1_FT1);

	EXTI->PR1 |= EXTI_PR1_PIF0 | EXTI_PR1_PIF1;

	NVIC_SetPriority(EXTI0_IRQn, 0);
	NVIC_EnableIRQ(EXTI0_IRQn);
	NVIC_SetPriority(EXTI1_IRQn, 0);
	NVIC_EnableIRQ(EXTI1_IRQn);
}

//==========================================================
// EXTI0_IRQHandler()
//
// @param: none
// @return: none
//
// Sets left button flag & clears EXTI0 pending register
//==========================================================
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR1 & EXTI_PR1_PIF0)
    {
    	led_direction = 0;
    	EXTI->PR1 |= EXTI_PR1_PIF0; // Clear pending flag}
    }
}

//==========================================================
// EXTI1_IRQHandler()
//
// @param: none
// @return: none
//
// Sets right button flag & clears EXTI1 pending register
//==========================================================
void EXTI1_IRQHandler(void)
{
    if (EXTI->PR1 & EXTI_PR1_PIF1)
    {
    	led_direction = 1;
    	EXTI->PR1 |= EXTI_PR1_PIF1; // Clear pending flag
    }
}

//==========================================================
// SysTick_Handler()
//
// @param: none
// @return: none
//
// Sets systick_flag that is polled in main loop
//=========================================================
void SysTick_Handler(void)
{
	systick_flag = 1;
}

//==========================================================
// configureSysTickInterrupt()
//
// @param: uint32_t theReloadValue - the reload value for the SysTick timer
// @return: none
//
// Configure hardware so that the SysTick timer will trigger
//=========================================================
void configureSysTickInterrupt(uint32_t reload_value)
{
	SysTick->CTRL = 0;
	NVIC_SetPriority(SysTick_IRQn, 0);

	SysTick->LOAD = SYSTICK_TRIGGER_SLOW;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

//==========================================================
// shift_led()
//
// @param: none
// @return: none
//
// Cycles through LEDs based on direction input using array of LEDs
//==============================================================
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

//===============================================================
// increment_speed()
//
// @param: none
// @return: none
//
// Cycle from slow to fast LED movement with each full cycle
//===============================================================
void increment_speed()
{
	if(((current_led == 8) && !led_direction)
			|| ((current_led == 0) && led_direction))
	{
		current_speed = (current_speed == 2) ? 0 : current_speed + 1;
	}

	SysTick->LOAD = SHIFT_SPEEDS[current_speed];
}

int main(void)
{
	/*
	 * Initialize SysTick
	 */
	configureSysTickInterrupt(SHIFT_SPEEDS[current_speed]);
	startSysTickTimer_MACRO;

	/*
	 * Initialize GPIO
	 */
	GPIO_Init();
	button_init();

    while (1)
    {
    	if(systick_flag)
    	{
    		shift_led();
    		increment_speed();
    		systick_flag = 0;
    	}
    }

    return 0;
}
