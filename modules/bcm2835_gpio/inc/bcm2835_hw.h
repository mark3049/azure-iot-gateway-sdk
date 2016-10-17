#ifndef __BCM2835_HW_H__
#define __BCM2835_HW_H__

#include <stdint.h>
typedef enum
{
    BCM2835_GPIO_FSEL_INPT  = 0x00,   /*!< Input 0b000 */
    BCM2835_GPIO_FSEL_OUTP  = 0x01,   /*!< Output 0b001 */
    BCM2835_GPIO_FSEL_ALT0  = 0x04,   /*!< Alternate function 0 0b100 */
    BCM2835_GPIO_FSEL_ALT1  = 0x05,   /*!< Alternate function 1 0b101 */
    BCM2835_GPIO_FSEL_ALT2  = 0x06,   /*!< Alternate function 2 0b110, */
    BCM2835_GPIO_FSEL_ALT3  = 0x07,   /*!< Alternate function 3 0b111 */
    BCM2835_GPIO_FSEL_ALT4  = 0x03,   /*!< Alternate function 4 0b011 */
    BCM2835_GPIO_FSEL_ALT5  = 0x02,   /*!< Alternate function 5 0b010 */
    BCM2835_GPIO_FSEL_MASK  = 0x07    /*!< Function select bits mask 0b111 */
} bcm2835FunctionSelect;


extern int bcm2835_init(void);
extern int bcm2835_close(void);
/* pin function select */
extern void bcm2835_gpio_fsel(uint8_t pin, uint8_t mode);

/* Set output pin */
extern void bcm2835_gpio_set(uint8_t pin);
/* Clear output pin */
extern void bcm2835_gpio_clr(uint8_t pin);

/* Read input pin */
extern uint8_t bcm2835_gpio_lev(uint8_t pin);
#endif //__BCM2835_HW__
