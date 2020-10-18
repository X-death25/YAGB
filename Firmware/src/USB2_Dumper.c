/*
USB2 GameBoy Dumper
X-death 09/2019
*/

#include <string.h>
#include <stdlib.h>

// include only used LibopenCM3 lib

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>

// Define Dumper Pinout

#define D0 GPIO9  // PB8   GB Data lines
#define D1 GPIO8  // PB9
#define D2 GPIO10 // PB10
#define D3 GPIO11 // PB11
#define D4 GPIO12 // PB12
#define D5 GPIO13 // PB13
#define D6 GPIO14 // PB14
#define D7 GPIO15 // PB15

#define OE GPIO9  //PA9    GB Control lines
#define WE GPIO10 //PA10
#define CE GPIO8  //PA8
#define RESET GPIO6  // PA6
#define A15 GPIO5          // PA5
#define AUDIO_IN	GPIO7  // PA7  GB Extra lines

#define CPU_CLK 	GPIO7  // PB7



#define CLK_CLEAR 	GPIO0  // PA0  Dumper MUX
#define CLK1 		GPIO3  // PA3
#define CLK2 		GPIO4  // PA4

#define LED_PIN 	GPIO13 // PC13  STM32 Led
#define CLEAR_ALL_PINS	0x44444444
#define AS_INPUT    0x44444444
#define AS_OUTPUT    0x33333333

// USB Special Command

#define WAKEUP     		0x10
#define READ_GB         0x11
#define READ_GB_SAVE  	0x12
#define WRITE_GB_SAVE 	0x13
#define ERASE_GB_SAVE	0x17
#define WRITE_GB_FLASH 	0x14
#define ERASE_GB_FLASH 	0x15
#define INFOS_ID 		0x18
#define DEBUG_MODE 		0x19

// Version 

#define MAX_VERSION 	1
#define MIN_VERSION 	1

// USB Specific VAR

static char serial_no[25];
static uint8_t usb_buffer_IN[64];
static uint8_t usb_buffer_OUT[64];

// GB Dumper Specific Var

static const unsigned char stmReady[] = {'R','E','A','D','Y','!'};
static unsigned char dump_running = 0;
static unsigned long address = 0;
static unsigned char Bank = 0; // Number of ROM bank always start at bank 1
static unsigned char Mapper = 0;   // Mapper Type
static unsigned char Ram_Size = 0;   // Mapper Type
static unsigned char Init_Mapper = 0;   // Mapper Type

//  USB Specific Fonction /////

static const struct usb_device_descriptor dev =
{
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xff,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x1209,
    .idProduct = 0xBEBA,
    .bcdDevice = 0x0200,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor data_endp[] = {{
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x01,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 64,
        .bInterval = 1,
    }, {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x82,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 64,
        .bInterval = 1,
    }
};

static const struct usb_interface_descriptor data_iface[] = {{
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 1,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = 0xff,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,

        .endpoint = data_endp,
    }
};

static const struct usb_interface ifaces[] = {{
        .num_altsetting = 1,
        .altsetting = data_iface,
    }
};

static const struct usb_config_descriptor config =
{
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 1,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0x80,
    .bMaxPower = 0x32,

    .interface = ifaces,
};

static void usb_setup(void)
{
    /* Enable clocks for USB */
    //rcc_usb_prescale_1();
    rcc_periph_clock_enable(RCC_USB);

    // Cleaning USB Buffer

    int i=0;

    for(i = 0; i < 64; i++)
    {
        usb_buffer_IN[i]=0x00;
        usb_buffer_OUT[i]=0x00;
    }
}

static const char *usb_strings[] =
{
    "Ultimate-Consoles",
    "USB2 Yes Another GB reader",
    serial_no,
};

static char *get_dev_unique_id(char *s)
{
    volatile uint8_t *unique_id = (volatile uint8_t *)0x1FFFF7E8;
    int i;

    // Fetch serial number from chip's unique ID
    for(i = 0; i < 24; i+=2)
    {
        s[i] = ((*unique_id >> 4) & 0xF) + '0';
        s[i+1] = (*unique_id++ & 0xF) + '0';
    }
    for(i = 0; i < 24; i++)
        if(s[i] > '9')
            s[i] += 'A' - '9' - 1;

    return s;
}

//  GB Dumper Specific Fonction /////

void wait(long nb)
{
    while(nb)
    {
        __asm__("nop");
        nb--;
    }
}

void setDataInput()
{
    GPIO_CRH(GPIOB) = 0x44444444; //set pb8-15 as data IN;
//gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,D0|D1|D2|D3|D4|D5|D6|D7);

}

void setDataOutput()
{
    GPIO_CRH(GPIOB) = 0x33333333;
    //gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,D0|D1|D2|D3|D4|D5|D6|D7);
}

void setAddress (unsigned long address)
{

    GPIO_CRH(GPIOB) = 0x33333333; //set pb8-15 as data OUT

    GPIO_ODR(GPIOB) = ((address) & 0xFF) << 8;
    GPIOA_BRR |= CLK1;
    GPIOA_BSRR |= CLK1;

    GPIO_ODR(GPIOB) = (address) & 0xFF00; //address MSB
    GPIOA_BRR |= CLK2;
    GPIOA_BSRR |= CLK2;

    GPIO_CRH(GPIOB) = 0x44444444; //set pb8-15 as data IN

}


void writeGB(int address, int byte)
{
    setAddress(address);
    setDataOutput();
GPIO_ODR(GPIOB) = (byte<<8);
    GPIOA_BRR |= WE;
    wait(32);
    GPIOA_BSRR |= WE;
    GPIOA_BRR |= AUDIO_IN;
    setDataInput();
}

void writeFlash8(int address, int byte)
{
 	setAddress(address);
    setDataOutput();
    GPIOA_BRR |= WE;
    GPIO_ODR(GPIOB) = (byte<<8);
    GPIOA_BSRR |= WE;
    setDataInput();
}



unsigned int address_mapper;
unsigned long address_max;
unsigned char prev_Bank;

void currentROMBank()
{
    if(address < 0x4000)
    {
        Bank = 0;
    }
    else
    {
        Bank = (address/0x4000); //page num max 0xFF - 32mbits
    }
}

void currentRAMBank()
{
    if(address < 0xBFFF)
    {
        Bank = 0;
    }
    else
    {
        Bank = (address/0xBFFF); //page num max 0xFF - 32mbits
    }
}

void BankswitchROM()
{

    currentROMBank();

    if(Bank != prev_Bank)
    {
        //new 16k bank !
        prev_Bank = Bank;

        switch(Mapper)
        {
        case 0:
            writeGB(0x6000, 0); // Set ROM Mode
            address_mapper = (address & 0x7FFF);
            break;
        case 1:
        case 2:
        case 3:
            if(address > 0x3FFF)
            {
                writeGB(0x6000, 0); // Set ROM Mode
                writeGB(0x2000, Bank & 0x1F);
                writeGB(0x4000, Bank >> 5);
                address_mapper = 0x4000 + (address & 0x3FFF);
                //address_mapper = 0x8000 + (address & 0x3FFF);
            }
            break;
        case 6:
            if(address > 0x3FFF)
            {
                writeGB(0x2100,Bank);
                address_mapper = 0x4000 + (address & 0x3FFF);
            }
            break;

        case 16:
            gpio_clear(GPIOC, GPIO13); // Turn Led OFF
            if(address > 0x3FFF)
            {
                writeGB(0x2100,Bank);
                address_mapper = 0x4000 + (address & 0x3FFF);
            }
            break;

        case 25:
            if(address > 0x3FFF)
            {
                writeGB(0x2100,Bank);
                address_mapper = 0x4000 + (address & 0x3FFF);
            }
            break;

    	case 26:
            if(address > 0x3FFF)
            {
                writeGB(0x2100,Bank);
                address_mapper = 0x4000 + (address & 0x3FFF);
            }
            break;

        case 27:
		    if(address > 0x3FFF)
            {
                writeGB(0x2100,Bank);
                address_mapper = 0x4000 + (address & 0x3FFF);
			}

    case 30:
		    if(address > 0x3FFF)
            {
                writeGB(0x2100,Bank);
                address_mapper = 0x4000 + (address & 0x3FFF);
			}

       case 252: // Pocket Camera support
		    if(address > 0x3FFF)
            {
                writeGB(0x2100,Bank);
                address_mapper = 0x4000 + (address & 0x3FFF);
			}

            break;
        default: //0
            writeGB(0x6000, 0); // Set ROM Mode
            address_mapper = 0x4000 + (address & 0x3FFF);
        }
    }
    else
    {
        // always in the same bank
        if(address > 0x7FFF)
        {
            address_mapper = 0x4000 + (address & 0x3FFF);
        }
        else
        {
            address_mapper = (address & 0x7FFF);
        }
    }
}


void BankswitchRAM()
{

    currentRAMBank();

    if(Bank != prev_Bank)
    {
        //new 16k bank !
        prev_Bank = Bank;

        switch(Mapper)
        {

		case 03: // MBC1 + RAM + BATTERY
			
			writeGB(0x6000, 0x00); // Select MBC1 correct RAM mode
			writeGB(0x0000, 0x0A); // Enable RAM
				if(address > 0xBFFF)
            {
                writeGB(0x4000,Bank);
               // address_mapper = 0xA000;
            }
			break;

		case 06: // MBC2 + BATTERY

			gpio_clear(GPIOC, GPIO13); // Turn Led ON
			writeGB(0x0000, 0x0A); // Enable RAM
			if(address > 0xBFFF)
            {
                writeGB(0x4000,Bank);
               // address_mapper = 0xA000;
            }
			break;

   case 10: // MBC5 + RAM + BATTERY
			gpio_clear(GPIOC, GPIO13); // Turn Led ON
			writeGB(0x0000, 0x0A); // Enable RAM
				if(address > 0xBFFF)
            {
                writeGB(0x4000,Bank);
               // address_mapper = 0xA000;
            }
            break;

    case 26: // MBC5 + RAM 
			gpio_clear(GPIOC, GPIO13); // Turn Led ON
			writeGB(0x0000, 0x0A); // Enable RAM
			if(address > 0xBFFF)
            {
                writeGB(0x4000,Bank);
               // address_mapper = 0xA000;
            }
            break;

        case 27: // MBC5 + RAM + BATTERY
			gpio_clear(GPIOC, GPIO13); // Turn Led ON
			writeGB(0x0000, 0x0A); // Enable RAM
			if(address > 0xBFFF)
            {
                writeGB(0x4000,Bank);
               // address_mapper = 0xA000;
            }
            break;

   case 30: // MBC5 + RUMBLE + RAM + BATTERY
			gpio_clear(GPIOC, GPIO13); // Turn Led ON
			writeGB(0x0000, 0x0A); // Enable RAM
			if(address > 0xBFFF)
            {
                writeGB(0x4000,Bank);
               // address_mapper = 0xA000;
            }
            break;
         }
     
    }
     
}

void readGB(usbd_device *usbd_dev)
{

    unsigned char adr = 0;
    prev_Bank = 0; //init

    GPIOA_BSRR |= CLK1 | CLK2;
    GPIOA_BSRR |= OE;
    GPIOA_BSRR |= CLK_CLEAR;

    while(address < address_max)
    {
        adr = 0; //init

        BankswitchROM(); //Change bank 'n update Mapper 'only' when necessary

        while(adr < 64)
        {
            GPIO_CRH(GPIOB) = 0x33333333; //set pb8-15 as data OUT
            GPIO_ODR(GPIOB) = ((address_mapper +adr) & 0xFF) << 8;
            GPIOA_BRR |= CLK1;
            GPIOA_BSRR |= CLK1;
            GPIO_ODR(GPIOB) = (address_mapper +adr) & 0xFF00; //address MSB
            GPIOA_BRR |= CLK2;
            GPIOA_BSRR |= CLK2;
            GPIO_CRH(GPIOB) = 0x44444444; //set pb8-15 as data IN
           	wait(4);
            GPIOA_BRR |= OE;
            wait(32);
            usb_buffer_OUT[adr] = (GPIO_IDR(GPIOB) & 0xFF00) >> 8; //save into read16 global
            GPIOA_BSRR |= OE;
            adr++;
            address++;
        }
        while(usbd_ep_write_packet(usbd_dev, 0x82, usb_buffer_OUT, 64)==0); //send 64B packet
    }

}

void Mapper_Init()
{

	 switch(Mapper)
        {

		case 03: // MBC1 + RAM + BATTERY
			
			if ( Ram_Size == 8){
			writeFlash8(0x6000, 0x00);  // Select MBC1 correct RAM mode
			address_max = 8192;}					   
			if ( Ram_Size == 32){
			writeFlash8(0x6000, 0x01); // Select MBC1 correct RAM mode
			address_max = 32768;}
			writeFlash8(0x0000, 0x0A); // Enable RAM
			break;

		case 06: // MBC2 + BATTERY

			writeFlash8(0x0000, 0x0A); // Enable RAM
			address_max = 512;
			break;

		case 10: // MBC3 + RAM + BATTERY

			writeFlash8(0x0000, 0x0A); // Enable RAM
			address_max = 32768;
            break;

		case 27: // MBC5 + RAM + BATTERY
			
			if ( Ram_Size == 8){
			address_max = 8192;}					   
			if ( Ram_Size == 32){
			address_max = 32768;}
			writeFlash8(0x0000, 0x0A); // Enable RAM
			break;

		case 252 : // Pocket Camera

			writeFlash8(0x0000, 0x0A); // Enable RAM
			address_max = 128*1024;
            break;
		}

}

void readGBSave(usbd_device *usbd_dev)
{
   unsigned char adr = 0;
    prev_Bank = 0; //init

     GPIOA_BSRR |= (CLK1 | CLK2 | WE | OE) | (CLK_CLEAR << 16);
      GPIOA_BSRR |= CLK_CLEAR;
		GPIOA_BRR |= A15;
      gpio_clear(GPIOC, GPIO13); // led on


	// MBC2 Fix ???
	setDataInput();
	setAddress(0x1234);
	wait(32);
	GPIOA_BRR |= CE;
	GPIOA_BRR |= OE;
	wait(32);
	GPIOA_BSRR |= CE;
	GPIOA_BSRR |= OE;
	wait(32);
    setDataOutput();

	Mapper_Init();
	writeFlash8(0x4000,0);
	writeFlash8(0x0000, 0x0A);
	GPIOA_BSRR |= A15;
	GPIOA_BRR |= CE;
    GPIOB_BRR |= CPU_CLK; // fix for GB Camera

	address_mapper = 0;
    while(address_mapper != address_max){
        adr = 0; //init
		GPIOA_BSRR |= A15;
      //  address_mapper = 0xA000 + (address & 0x3FFF);
        // BankswitchRAM(); //upd 'only' when necessary 
        while(adr < 64){
          	setAddress(adr + 0xA000 + address );
			// setAddress(adr + 0xA000 + address)
            GPIO_CRH(GPIOB) = AS_INPUT;
            GPIO_BRR(GPIOA) |= OE;
            wait(32);
            usb_buffer_OUT[adr] = (GPIO_IDR(GPIOB) & 0xFF00) >> 8;
            GPIO_BSRR(GPIOA) |= OE;
            adr++;
        }
        address+=64;
		address_mapper+=64;
		 if ( address_mapper == 8192){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,1),address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*2){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,2);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*3){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,3);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*4){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,4);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*5){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,5);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*6){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,6);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*7){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,7);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*8){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,8);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*9){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,9);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*10){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,10);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*11){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,11);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*12){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,12);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*13){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,13);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*14){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,14);address=0;writeFlash8(0x0000, 0x0A);}
        if ( address_mapper == 8192*15){GPIOA_BRR |= A15;writeFlash8(0x6000, 0x01);writeFlash8(0x4000,15);address=0;writeFlash8(0x0000, 0x0A);}


        while(usbd_ep_write_packet(usbd_dev, 0x82, usb_buffer_OUT, 64)==0);
    }
    //disa SRAM
    writeFlash8(0x0000, 0x0);
    //  gpio_set(GPIOC, GPIO13); // led off

	/*writeFlash8(0x6000, 0x01); // Select MBC1 correct RAM mode
	writeFlash8(0x4000,3);
	writeFlash8(0x0000, 0x0A); // Enable RAM
	//writeFlash8(0x4000,0);*/

//
}

void EraseGBSave(void)
{
	unsigned short adr = 0;

	// Set correct MBC1 mode : mode 0

	GPIOA_BRR |= A15;
	setAddress(0x6000);
	setDataOutput();
	GPIO_ODR(GPIOB) = (0x00<<8);
	GPIOA_BRR |= WE;
	wait(32);
    GPIOA_BSRR |= WE;

	// Bankswitch to RAM

	 setAddress(0);
     setDataOutput();
	 GPIO_ODR(GPIOB) = (0x0A<<8);
	 GPIOA_BRR |= WE;
	 wait(32);
     GPIOA_BSRR |= WE;

	// Prepare Write Mode

	GPIOA_BSRR |= OE;
	GPIOA_BSRR |= CE;
	GPIOA_BSRR |= A15;

while(adr < 8192)
        {

			setAddress(0xA000  + adr);
			setDataOutput();
			GPIOA_BRR |= WE;
	        GPIOA_BRR |= CE;
	        GPIO_ODR(GPIOB) = (0xFF<<8);
			wait(32);
			GPIOA_BSRR |= WE;
	        GPIOA_BSRR |= CE;
			adr++;	          
   		}

    //disa SRAM
    writeFlash8(0x0000, 0x0);
}

void writeGBSave()
{

unsigned short adr = 0;
unsigned char byte = 0;

 if ( Init_Mapper == 0)
{

	// Set correct MBC1 mode : mode 0

	GPIOA_BRR |= A15;
	setAddress(0x6000);
	setDataOutput();
	GPIO_ODR(GPIOB) = (0x00<<8);
	GPIOA_BRR |= WE;
	wait(32);
    GPIOA_BSRR |= WE;

	// Bankswitch to RAM

	 setAddress(0);
     setDataOutput();
	 GPIO_ODR(GPIOB) = (0x0A<<8);
	 GPIOA_BRR |= WE;
	 wait(32);
     GPIOA_BSRR |= WE;
	 Init_Mapper =1;
}

	// Prepare Write Mode

	GPIOA_BSRR |= OE;
	GPIOA_BSRR |= CE;
	GPIOA_BSRR |= A15;

while(adr < 32)
        {

			setAddress(0xA000 + address + adr);
			setDataOutput();
			GPIOA_BRR |= WE;
	        GPIOA_BRR |= CE;
			byte = usb_buffer_IN[32+adr];
	        GPIO_ODR(GPIOB) = (byte<<8);
			wait(32);
			GPIOA_BSRR |= WE;
	        GPIOA_BSRR |= CE;
			adr++;	          
   		}  

/*  gpio_clear(GPIOC, GPIO13); // LED on
    unsigned short adr = 0;
    unsigned char byte = 0;
    setDataOutput();

    GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
    GPIOB_BSRR |= OE;
    GPIOA_BSRR |= CLK_CLEAR;

    // SRAM rom > 16Mbit
    GPIOB_BSRR |= D0;
    GPIOA_BRR  |= TIME;
    GPIOA_BSRR |= TIME;

    while(adr < 32)
    {
        setAddress((address+adr));
        setDataOutput();
        //directWrite8(0xCC);
        byte = usb_buffer_IN[32+adr];
        directWrite8(byte);
        GPIOA_BRR  |= CE;
        GPIOC_BRR  |= WE_SRAM;
        // wait(16); //utile ?
        //directWrite8(usb_buffer_IN[5+adr]);

        GPIOC_BSRR |= WE_SRAM;
        GPIOA_BSRR |= CE;
        setDataInput();
        adr++;
    }*/
}


void reset_command()
{
setDataOutput();
writeFlash8(0x5555,0xAA);
writeFlash8(0x2AAA,0x55);
writeFlash8(0x5555,0xF0);
}


void EraseFlash()
{

writeFlash8(0x5555,0xAA);
writeFlash8(0x2AAA,0x55);
writeFlash8(0x5555,0x80);
writeFlash8(0x5555,0xAA);
writeFlash8(0x2AAA,0x55);
writeFlash8(0x5555,0x10);

wait(170000); // ~20Ms
gpio_clear(GPIOC, GPIO13);
}

void writeGBFlash()
{

}
void infosId()
{
	/*GPIOA_BSRR |= CLK1 | CLK2;
    GPIOA_BSRR |= OE;
    GPIOA_BSRR |= CLK_CLEAR;
	GPIOA_BSRR |= WE;

	GPIO_CRH(GPIOB) = 0x33333333; //set pb8-15 as data 


    // Reset flash
       writeGB(0x555, 0xaa);
	writeGB(0x2aa, 0x55);
	writeGB(0x555, 0xF0);
	wait(100);
 

	// ID command sequence
    writeGB(0x555, 0xaa);
    writeGB(0x2aa, 0x55);
    writeGB(0x555, 0x90);

	

	        GPIO_CRH(GPIOB) = 0x33333333; //set pb8-15 as data OUT
            GPIO_ODR(GPIOB) = ((0) & 0xFF) << 8;
            GPIOA_BRR |= CLK1;
            GPIOA_BSRR |= CLK1;
            GPIO_ODR(GPIOB) = (0) & 0xFF00; //address MSB
            GPIOA_BRR |= CLK2;
            GPIOA_BSRR |= CLK2;
            GPIO_CRH(GPIOB) = 0x44444444; //set pb8-15 as data IN
           	wait(4);
            GPIOA_BRR |= OE;
            wait(32);
            usb_buffer_OUT[1] = (GPIO_IDR(GPIOB) & 0xFF00) >> 8; //save into read16 global
            GPIOA_BSRR |= OE;

    // Reset flash
       writeGB(0x555, 0xaa);
	writeGB(0x2aa, 0x55);
	writeGB(0x555, 0xF0);
	wait(100);
 

	// ID command sequence
    writeGB(0x555, 0xaa);
    writeGB(0x2aa, 0x55);
    writeGB(0x555, 0x90);

			GPIO_CRH(GPIOB) = 0x33333333; //set pb8-15 as data OUT
            GPIO_ODR(GPIOB) = ((2) & 0xFF) << 8;
            GPIOA_BRR |= CLK1;
            GPIOA_BSRR |= CLK1;
            GPIO_ODR(GPIOB) = (0) & 0xFF00; //address MSB
            GPIOA_BRR |= CLK2;
            GPIOA_BSRR |= CLK2;
            GPIO_CRH(GPIOB) = 0x44444444; //set pb8-15 as data IN
           	wait(4);
            GPIOA_BRR |= OE;
            wait(32);
            usb_buffer_OUT[3] = (GPIO_IDR(GPIOB) & 0xFF00) >> 8; //save into read16 global
            GPIOA_BSRR |= OE;
  
*/

}

/// USB Specific Function


//void SendNextPaquet(usbd_device *usbd_dev, uint8_t ep){
//	readGB();
//	usbd_ep_write_packet(usbd_dev,ep,usb_buffer_OUT,64);
//	address += 64;
//}


/*
* This gets called whenever a new IN packet has arrived from PC to STM32
 */

static void usbdev_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
    (void)ep;
    (void)usbd_dev;

    usb_buffer_IN[0] = 0;
    usbd_ep_read_packet(usbd_dev, 0x01,usb_buffer_IN, 64); // Read Paquets from PC

    address = (usb_buffer_IN[3]<<16) | (usb_buffer_IN[2]<<8) | usb_buffer_IN[1];
    address_max = (usb_buffer_IN[7]<<16) | (usb_buffer_IN[6]<<8) | usb_buffer_IN[5];


    if(usb_buffer_IN[0] == WAKEUP)
    {
        memcpy((unsigned char *)usb_buffer_OUT, (unsigned char *)stmReady, sizeof(stmReady));
		usb_buffer_OUT[20] = MAX_VERSION;
        usb_buffer_OUT[21] = MIN_VERSION;
        usbd_ep_write_packet(usbd_dev, 0x82, usb_buffer_OUT, 64);
        int i=0;
        for(i = 0; i < 64; i++)
        {
            usb_buffer_IN[i]=0x00;
            usb_buffer_OUT[i]=0x00;
        }
    }

    if (usb_buffer_IN[0] == READ_GB && usb_buffer_IN[4] != 1 )    // READ GB 
    {
        //dump_running = 0;
        Mapper =  usb_buffer_IN[8];
        readGB(usbd_dev);
        //usbd_ep_write_packet(usbd_dev, 0x82, usb_buffer_OUT, 64);
    }

	if (usb_buffer_IN[0] == READ_GB_SAVE && usb_buffer_IN[4] != 1 )    // READ GB Save 
    {
        //dump_running = 0;
        Mapper =  usb_buffer_IN[8];
		Ram_Size =  usb_buffer_IN[9];
		readGBSave(usbd_dev);
			// Set correct MBC1 mode : mode 0
    }

	if (usb_buffer_IN[0] == ERASE_GB_SAVE )    // Erase GB Save 
    {
		Bank = usb_buffer_IN[5];
		Mapper =  usb_buffer_IN[8];
		EraseGBSave();
		usb_buffer_OUT[7]=0xAA;
		usbd_ep_write_packet(usbd_dev, 0x82, usb_buffer_OUT, 64);

    }

	if (usb_buffer_IN[0] == WRITE_GB_SAVE)     // WRITE GB Save
    {
        dump_running = 0;
        writeGBSave();
        usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
        usb_buffer_OUT[7]=0xAA;
    }


	if (usb_buffer_IN[0] == ERASE_GB_FLASH )    // Erase GB Flash 
    {
		reset_command();
        EraseFlash();
		usb_buffer_OUT[0]=0x01;
		usbd_ep_write_packet(usbd_dev, 0x82, usb_buffer_OUT, 64);

    }

	if (usb_buffer_IN[0] == INFOS_ID)   // Chip Information
   {
		/*dump_running = 0;
		GPIOA_BRR |= WE;
		usb_buffer_OUT[6]=0xFF;
		infosId();
		usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
		usb_buffer_OUT[6]=0x00;*/
   }


}

/*
* This gets called whenever a new OUT packet has been send from STM32 to PC
*/

//static void usbdev_data_tx_cb(usbd_device *usbd_dev, uint8_t ep)
//{
//	(void)ep;
//	(void)usbd_dev;
//if ( dump_running == 1 )
//{
//SendNextPaquet(usbd_dev,0x82);
//}
//
//}


static void usbdev_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
    (void)wValue;
    (void)usbd_dev;
    usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, usbdev_data_rx_cb); //in
    usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, NULL); //out
}


//  Main Fonction /////

uint8_t usbd_control_buffer[256];

int main(void)
{

    int i=0;
    usbd_device *usbd_dev;

    // Init Clock
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);

    /* Enable alternate function peripheral clock. */
    rcc_periph_clock_enable(RCC_AFIO);

    // Led ON

    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    gpio_clear(GPIOC, GPIO13); // LED on/off

    // Force USB Re-enumeration after bootloader is executed

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
    gpio_clear(GPIOA, GPIO12);
    for( i = 0; i < 0x800000; i++)
    {
        __asm__("nop");    //1sec
    }
    get_dev_unique_id(serial_no);

    // Init USB2 GB Dumper

    usb_setup();
    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings,
                         3, usbd_control_buffer, sizeof(usbd_control_buffer));
    usbd_register_set_config_callback(usbd_dev, usbdev_set_config);

    for (i = 0; i < 0x800000; i++)
        __asm__("nop");

    // GPIO Init

    //Full GPIO
    AFIO_MAPR = AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF;

    GPIO_CRL(GPIOA) = 0x33333313; //always ouput (ce, clear etc) expact MARK 3
    GPIO_CRH(GPIOA) = 0x34444333;
    GPIO_CRL(GPIOB) = 0x33433444;
    GPIO_CRH(GPIOB) = 0x33333333;


      /*  gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,OE | WE | CE );
      gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,AUDIO_IN | CPU_CLK | RESET );
      GPIOB_BSRR |= RESET | AUDIO_IN | CPU_CLK;*/

    GPIOA_BSRR |= OE;
    GPIOA_BSRR |= CLK_CLEAR;
    GPIOA_BRR |= A15;
    GPIOA_BSRR |= AUDIO_IN;
    GPIOB_BSRR |= CPU_CLK;
    GPIOA_BSRR |= RESET;
	GPIOA_BRR |= WE;

    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN);
    gpio_set(GPIOC, GPIO13); // Turn Led OFF

    dump_running = 0;

	// First Read test

	// Custom RAM test

	/*writeFlash8(0x6000, 0x01); // Select MBC1 correct RAM mode
	writeFlash8(0x4000,3);
	writeFlash8(0x0000, 0x0A); // Enable RAM
	//writeFlash8(0x4000,0);

	//GPIOA_BSRR |= A15;
	//GPIOA_BRR |= CE;

	while (1);*/






    while(1)
    {
        usbd_poll(usbd_dev);

    }

}








