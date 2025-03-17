#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stm32l476xx.h>

// Define Speed Frequencies
#define SYS_CLK_FREQ 4000000 // 4 MHz (MSI default)
#define SYSTICK_TRIGGER_SLOW (SYS_CLK_FREQ / 3) // 1 second interval
#define SYSTICK_TRIGGER_MEDIUM (SYS_CLK_FREQ / 5)
#define SYSTICK_TRIGGER_FAST (SYS_CLK_FREQ / 7)
const uint32_t SHIFT_SPEEDS[] = {SYSTICK_TRIGGER_SLOW, SYSTICK_TRIGGER_MEDIUM, SYSTICK_TRIGGER_FAST};
uint8_t current_speed = 0; // 0 = slow, 1 = medium, 2 = fast

#define startSysTickTimer_MACRO (SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk)

// Define LED Direction Control/Interrupts
uint8_t led_direction = 0; // 0 = left to right, 1 = right to left
uint8_t current_led = 0; // Current LED index
volatile uint8_t systick_flag = 0;

// Debounce
#define SYSTICK_DEBOUNCE_TIMER (SYS_CLK_FREQ / 100);
volatile uint8_t debounce_timer = 0;
void debounce() {
    while (!(GPIOC->IDR & GPIO_IDR_ID13));  // Wait until the button is released

    for (uint32_t i = 0; i < 20000; i++) {
        if (!(GPIOC->IDR & GPIO_IDR_ID13)) {
            i = 0;  // Restart debounce timer if button is pressed again
        }
    }
}

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} LED;

const LED LED_ARRAY[] = {
    {GPIOB, (0x1 << 8)}, // D15: PB8
    {GPIOB, (0x1 << 9)}, // D14: PB9
    {GPIOA, (0x1 << 5)}, // D13: PA5
    {GPIOA, (0x1 << 6)}, // D12: PA6
    {GPIOA, (0x1 << 7)}, // D11: PA7
    {GPIOB, (0x1 << 6)}, // D10: PB6
    {GPIOA, (0x1 << 9)}, // D8: PA9
    {GPIOA, (0x1 << 8)}  // D7: PA8
};

void configureLed() {
    // GPIO Enable
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    // LED setup for GPIOA, PA5, PA6, PA7, PA8, and PA9
    GPIOA->MODER &= ~(GPIO_MODER_MODE5 | GPIO_MODER_MODE6 | GPIO_MODER_MODE7 | GPIO_MODER_MODE8 | GPIO_MODER_MODE9);
    GPIOA->MODER |= (GPIO_MODER_MODE5_0 | GPIO_MODER_MODE6_0 | GPIO_MODER_MODE7_0 | GPIO_MODER_MODE8_0 | GPIO_MODER_MODE9_0);

    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT5 | GPIO_OTYPER_OT6 | GPIO_OTYPER_OT7 | GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9);
    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED5 | GPIO_OSPEEDR_OSPEED6 | GPIO_OSPEEDR_OSPEED7 | GPIO_OSPEEDR_OSPEED8 | GPIO_OSPEEDR_OSPEED9);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD5 | GPIO_PUPDR_PUPD6 | GPIO_PUPDR_PUPD7 | GPIO_PUPDR_PUPD8 | GPIO_PUPDR_PUPD9);

    // LED setup for GPIOB: PB8, PB9, and PB6
    GPIOB->MODER &= ~(GPIO_MODER_MODE8 | GPIO_MODER_MODE9 | GPIO_MODER_MODE6);
    GPIOB->MODER |= (GPIO_MODER_MODE8_0 | GPIO_MODER_MODE9_0 | GPIO_MODER_MODE6_0);

    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9 | GPIO_OTYPER_OT6);
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED8 | GPIO_OSPEEDR_OSPEED9 | GPIO_OSPEEDR_OSPEED6);
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8 | GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD6);
}

void configureSwitch() {
    // Button setup for GPIOA, PA2 and PA3
    GPIOA->MODER &= ~(GPIO_MODER_MODE2 | GPIO_MODER_MODE3);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD2 | GPIO_PUPDR_PUPD3);
    GPIOA->PUPDR |= (GPIO_PUPDR_PUPD2_1 | GPIO_PUPDR_PUPD3_1); // Enable pull-down resistors

    // User Button Setup
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN; // Enable port C clock

    GPIOC->MODER &= ~(GPIO_MODER_MODE13); // input pin (PC13=00)
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD13); // no pullup,pulldown (PC13=00)
}

void SysTick_Handler(void) {
    systick_flag = 1;
}

void configureSysTickInterrupt(uint32_t reload_value) {
    SysTick->CTRL = 0;					// disable SysTick timer
    NVIC_SetPriority(SysTick_IRQn, 7);	// set priority level

    SysTick->LOAD = reload_value;		// set counter reload value
    SysTick->VAL = 0;					// reset SysTick timer value
    SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk	// use system clock
    				| SysTick_CTRL_TICKINT_Msk 	// enable SysTick interrupts
					| SysTick_CTRL_ENABLE_Msk;	// enable SysTick timer
}

uint8_t shift_led() {
    // Turn off the current LED
    LED_ARRAY[current_led].port->ODR &= ~LED_ARRAY[current_led].pin;

    // Move to the next LED
    if (led_direction == 0) {
        current_led = (current_led == 7) ? 0 : current_led + 1; // Left to Right Direction
    } else {
        current_led = (current_led == 0) ? 7 : current_led - 1; // Right to Left direction
    }

    // Turn on the new LED
    LED_ARRAY[current_led].port->ODR |= LED_ARRAY[current_led].pin;

    // Check if the sequence rolled off at either end
    if ((current_led == 0 && led_direction == 1) || (current_led == 7 && led_direction == 0)) {
        // Increment speed and cycle back to slow if necessary
        current_speed = (current_speed + 1) % 3;

        // Update SysTick reload value
        SysTick->LOAD = SHIFT_SPEEDS[current_speed];
    }

    return current_led;
}

int main(void) {
    configureLed();
    configureSwitch();
    configureSysTickInterrupt(SHIFT_SPEEDS[current_speed]); // Start with slow speed

    // Turn on the first LED at startup
	LED_ARRAY[current_led].port->ODR |= LED_ARRAY[current_led].pin;

    while (1) {

    	if (!(GPIOC->IDR & GPIO_IDR_ID13)) { // If user button is pressed
    		debounce();
    		led_direction ^= 1;
    	}

        // Shift the LED if the SysTick flag is set
        if (systick_flag) {
            shift_led();
            systick_flag = 0;
        }
    }

    return 0;
}
