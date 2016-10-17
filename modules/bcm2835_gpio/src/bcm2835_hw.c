
#include "bcm2835_hw.h"
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include "azure_c_shared_utility/xlogging.h"

/*! This means pin HIGH, true, 3.3volts on a pin. */
#define HIGH 0x1
/*! This means pin LOW, false, 0volts on a pin. */
#define LOW  0x0

/*! On RPi2 with BCM2836, and all recent OSs, the base of the peripherals is read from a /proc file */
#define BMC2835_RPI2_DT_FILENAME "/proc/device-tree/soc/ranges"
/*! Offset into BMC2835_RPI2_DT_FILENAME for the peripherals base address */
#define BMC2835_RPI2_DT_PERI_BASE_ADDRESS_OFFSET 4
/*! Offset into BMC2835_RPI2_DT_FILENAME for the peripherals size address */
#define BMC2835_RPI2_DT_PERI_SIZE_OFFSET 8

/*! Peripherals block base address on RPi 1 */
#define BCM2835_PERI_BASE               0x20000000
/*! Size of the peripherals block on RPi 1 */
#define BCM2835_PERI_SIZE               0x01000000

/*! Offsets for the bases of various peripherals within the peripherals block
  /   Base Address of the System Timer registers
*/
#define BCM2835_ST_BASE			0x3000
/*! Base Address of the Pads registers */
#define BCM2835_GPIO_PADS               0x100000
/*! Base Address of the Clock/timer registers */
#define BCM2835_CLOCK_BASE              0x101000
/*! Base Address of the GPIO registers */
#define BCM2835_GPIO_BASE               0x200000
/*! Base Address of the SPI0 registers */
#define BCM2835_SPI0_BASE               0x204000
/*! Base Address of the BSC0 registers */
#define BCM2835_BSC0_BASE 		0x205000
/*! Base Address of the PWM registers */
#define BCM2835_GPIO_PWM                0x20C000
/*! Base Address of the BSC1 registers */
#define BCM2835_BSC1_BASE		0x804000
#define BCM2835_GPFSEL0                      0x0000 /*!< GPIO Function Select 0 */
#define BCM2835_GPSET0                       0x001c /*!< GPIO Pin Output Set 0 */
#define BCM2835_GPCLR0                       0x0028 /*!< GPIO Pin Output Clear 0 */
#define BCM2835_GPLEV0                       0x0034 /*!< GPIO Pin Level 0 */

uint32_t *bcm2835_peripherals_base = (uint32_t *)BCM2835_PERI_BASE;
uint32_t bcm2835_peripherals_size = BCM2835_PERI_SIZE;

/* Virtual memory address of the mapped peripherals block 
 */
uint32_t *bcm2835_peripherals = (uint32_t *)MAP_FAILED;

/* And the register bases within the peripherals block
 */
volatile uint32_t *bcm2835_gpio        = (uint32_t *)MAP_FAILED;
static int module_init_count = 0;
static void *mapmem(const char *msg, size_t size, int fd, off_t off)
{
    void *map = mmap(NULL, size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, off);
    if (map == MAP_FAILED)
	//fprintf(stderr, "bcm2835_init: %s mmap failed: %s\n", msg, strerror(errno));
    return map;
}

static void unmapmem(void **pmem, size_t size)
{
    if (*pmem == MAP_FAILED) return;
    munmap(*pmem, size);
    *pmem = MAP_FAILED;
}
static void bcm2835_peri_write(volatile uint32_t* paddr, uint32_t value)
{
	__sync_synchronize();
	*paddr = value;
	__sync_synchronize();
}
static uint32_t bcm2835_peri_read(volatile uint32_t* paddr)
{
	uint32_t ret;
	__sync_synchronize();
	ret = *paddr;
	__sync_synchronize();
	return ret;
}

void bcm2835_peri_set_bits(volatile uint32_t* paddr, uint32_t value, uint32_t mask)
{
	uint32_t v = bcm2835_peri_read(paddr);
	v = (v & ~mask) | (value & mask);
	bcm2835_peri_write(paddr, v);
}

void bcm2835_gpio_fsel(uint8_t pin, uint8_t mode)
{
	if(bcm2835_gpio == MAP_FAILED) return;
	/* Function selects are 10 pins per 32 bit word, 3 bits per pin */
	volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPFSEL0/4 + (pin/10);
	uint8_t   shift = (pin % 10) * 3;
	uint32_t  mask = BCM2835_GPIO_FSEL_MASK << shift;
	uint32_t  value = mode << shift;
	bcm2835_peri_set_bits(paddr, value, mask);
}

/* Set output pin */
void bcm2835_gpio_set(uint8_t pin)
{
	if(bcm2835_gpio == MAP_FAILED) return;
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPSET0/4 + pin/32;
    uint8_t shift = pin % 32;
    bcm2835_peri_write(paddr, 1 << shift);
}
/* Clear output pin */
void bcm2835_gpio_clr(uint8_t pin)
{
	if(bcm2835_gpio == MAP_FAILED) return;
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPCLR0/4 + pin/32;
    uint8_t shift = pin % 32;
    bcm2835_peri_write(paddr, 1 << shift);
}

/* Read input pin */
uint8_t bcm2835_gpio_lev(uint8_t pin)
{
	if(bcm2835_gpio == MAP_FAILED) return LOW;
	volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPLEV0/4 + pin/32;
    uint8_t shift = pin % 32;
    uint32_t value = bcm2835_peri_read(paddr);
    return (value & (1 << shift)) ? HIGH : LOW;
}

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"


/* Initialise this library. */
int bcm2835_init(void)
{
	int  memfd;
	int  ok;
	FILE *fp;

	if(++module_init_count!=1) return 1;
    /* Figure out the base and size of the peripheral address block
    // using the device-tree. Required for RPi2, optional for RPi 1
    */
    if ((fp = fopen(BMC2835_RPI2_DT_FILENAME , "rb")))
    {
        unsigned char buf[4];
		fseek(fp, BMC2835_RPI2_DT_PERI_BASE_ADDRESS_OFFSET, SEEK_SET);
		if (fread(buf, 1, sizeof(buf), fp) == sizeof(buf))
			  bcm2835_peripherals_base = (uint32_t *)(buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0);
			fseek(fp, BMC2835_RPI2_DT_PERI_SIZE_OFFSET, SEEK_SET);
		if (fread(buf, 1, sizeof(buf), fp) == sizeof(buf))
			bcm2835_peripherals_size = (buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0);
		fclose(fp);
    }
    /* else we are prob on RPi 1 with BCM2835, and use the hardwired defaults */

    /* Now get ready to map the peripherals block 
     * If we are not root, try for the new /dev/gpiomem interface and accept
     * the fact that we can only access GPIO
     * else try for the /dev/mem interface and get access to everything
     */
    memfd = -1;
    ok = 0;
    if (geteuid() == 0)
    {
		/* Open the master /dev/mem device */
	    if ((memfd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) 
		{
		  	LogError("bcm2835_init: Unable to open /dev/mem\n") ;
			goto exit;
		}
      
		/* Base of the peripherals block is mapped to VM */
		bcm2835_peripherals = mapmem("gpio", bcm2835_peripherals_size, memfd, (uint32_t)bcm2835_peripherals_base);
		if (bcm2835_peripherals == MAP_FAILED) goto exit;

		/* Now compute the base addresses of various peripherals, 
		// which are at fixed offsets within the mapped peripherals block
		// Caution: bcm2835_peripherals is uint32_t*, so divide offsets by 4
		*/
		bcm2835_gpio = bcm2835_peripherals + BCM2835_GPIO_BASE/4;

		ok = 1;
    }
    else
    {
		/* Not root, try /dev/gpiomem */
		/* Open the master /dev/mem device */
		if ((memfd = open("/dev/gpiomem", O_RDWR | O_SYNC) ) < 0) 
		{
			LogError("bcm2835_init: Unable to open /dev/gpiomem");
			goto exit;
		}
      
		/* Base of the peripherals block is mapped to VM */
		bcm2835_peripherals_base = 0;
		bcm2835_peripherals = mapmem("gpio", bcm2835_peripherals_size, memfd, (uint32_t)bcm2835_peripherals_base);
		if (bcm2835_peripherals == MAP_FAILED) goto exit;
		bcm2835_gpio = bcm2835_peripherals;
		ok = 1;
    }

exit:
    if (memfd >= 0)
        close(memfd);

    if (!ok)
		bcm2835_close();

    return ok;
}


int bcm2835_close(void)
{
	if(--module_init_count!=0) return 1;
    unmapmem((void**) &bcm2835_peripherals, bcm2835_peripherals_size);
    bcm2835_peripherals = MAP_FAILED;
    bcm2835_gpio = MAP_FAILED;
    return 1; /* Success */
}    

