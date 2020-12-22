/*
 * \file main.c
 * \brief Libusb software for communicate with STM32
 * \author X-death for Ultimate-Consoles forum (http://www.ultimate-consoles.fr)
 * \date 2018/06
 *
 * just a simple sample for testing libusb and new code of Sega Dumper
 *
 * --------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#include <libusb.h>
#include <assert.h>

// Specific Win32 lib ( only used for debug )

//#include <conio.h>

// USB Special Command

#define WAKEUP  0x10  // WakeUP for first STM32 Communication
#define READ_GB 0x11
#define READ_GB_SAVE 0x12
#define ERASE_GB_SAVE 0x17
#define WRITE_GB_SAVE 0x13
#define WRITE_GB_FLASH 	0x14
#define ERASE_GB_FLASH 0x15
#define ERASE_SECTOR 0x16
#define INFOS_ID 0x18
#define DEBUG_MODE 0x19

// Version 

#define MAX_VERSION 	1
#define MIN_VERSION 	2


// Sega Dumper Specific Function

void pause(char const *message)
{
    int c;
 
    printf("%s", message);
    fflush(stdout);
 
    while ((c = getchar()) != '\n' && c != EOF)
    {
    }
}


int main()
{

  // LibUSB Specific Var
    
  int res                      = 0;        /* return codes from libusb functions */
  libusb_device_handle* handle = 0;        /* handle for USB device */
  int kernelDriverDetached     = 0;        /* Set to 1 if kernel driver detached */
  int numBytes                 = 0;        /* Actual bytes transferred. */
  unsigned char usb_buffer_in[64] = {0};   /* 64 byte transfer buffer IN */
  unsigned char usb_buffer_out[64] = {0};  /* 64 byte transfer buffer OUT */
  unsigned long len            = 0;        /* Number of bytes transferred. */

   // Dumper Specific Var

    unsigned long i=0;
	unsigned long j=0;
    unsigned long k=0;
	int choixMenu=0;
	unsigned long address=0;
	unsigned char *BufferROM;
	unsigned char *BufferTemp;
	unsigned char *BufferRAM;
	unsigned char *buffer_save = NULL;
	char dump_name[64];
	int game_size=0;
	unsigned long save_size=0;
	FILE *myfile;
	const char * wheel[] = { "-","\\","|","/"}; //erase wheel
	unsigned char manufacturer_id=0;
	unsigned char chip_id=0;
	unsigned short flash_id=0;
	unsigned short sector_number=0;

  // Rom Header info

	unsigned char ROM_Logo[48];
    unsigned char Game_Name[16];
	unsigned char Rom_Type=0;
    unsigned long Rom_Size=0;
	unsigned long Ram_Size=0;
	unsigned char CGB=0;
	unsigned char SGB=0;

const unsigned char GB_Logo[48] = {
	0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 
	0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 
	0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 
};
   

   // Main Program   

    printf("\n");
    printf(" ---------------------------------\n");
    printf("    GB Dumper USB2 Software     \n");
    printf(" ---------------------------------\n");
	printf("\n");

    printf("Init LibUSB... \n");

  /* Initialise libusb. */

  res = libusb_init(0);
  if (res != 0)
  {
    fprintf(stderr, "Error initialising libusb.\n");
    return 1;
  }

    printf("LibUSB Init Sucessfully ! \n");


 printf("Detecting GameBoy Dumper... \n");

  /* Get the first device with the matching Vendor ID and Product ID. If
   * intending to allow multiple demo boards to be connected at once, you
   * will need to use libusb_get_device_list() instead. Refer to the libusb
   * documentation for details. */

  handle = libusb_open_device_with_vid_pid(0, 0x1209, 0xBEBA);

  if (!handle)
  {
    fprintf(stderr, "Unable to open device.\n");
    return 1;
  }

  /* Claim interface #0. */

 res = libusb_claim_interface(handle, 0);
  if (res != 0)
  {
	res = libusb_claim_interface(handle, 1);
	 if (res != 0)
  {
    printf("Error claiming interface.\n");
    return 1;}
  }

// Clean Buffer
  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}


 printf("GameBoy Dumper Found ! \n");
 printf("Reading cartridge...\n");


 // At this step we can try to read the buffer wake up GB Dumper

  usb_buffer_out[0] = WAKEUP;// Affect request to  WakeUP Command

  libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); // Send Packets to Sega Dumper
  
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0); // Receive packets from Sega Dumper

		
    printf("\nGB Dumper %.*s",6, (char *)usb_buffer_in);
	printf("\n");
    printf("Hardware Firmware version : %d", usb_buffer_in[20]);
    printf(".%d\n", usb_buffer_in[21]);
    printf("Software Firmware version : %d",MAX_VERSION);
    printf(".%d\n",MIN_VERSION);


  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}

// Now try to detect cartridge type : GB or GBA or Empty

printf("\nTry to read cartridge type ...\n");

address=260;
game_size=260+64;
usb_buffer_out[0] = READ_GB;
usb_buffer_out[1] = address & 0xFF ;
usb_buffer_out[2] = (address & 0xFF00)>>8;
usb_buffer_out[3]=(address & 0xFF0000)>>16;
usb_buffer_out[5]=game_size & 0xFF;
usb_buffer_out[6]=(game_size & 0xFF00)>>8;
usb_buffer_out[7]=(game_size & 0xFF0000)>>16;
usb_buffer_out[4] = 0; // Slow Mode
libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
libusb_bulk_transfer(handle, 0x82,usb_buffer_in,64, &numBytes, 60000);

for (i = 0; i < 48; i++)
    {
        ROM_Logo [i] = usb_buffer_in[i];
		
    }

if(memcmp((unsigned char *)ROM_Logo,(unsigned char *)GB_Logo,48) == 0)
	{
		printf("Nintendo GameBoy cartridge detected !");
	}
else {printf("Bad header , cartridge dirty or empty Flash ROM");}


/*
printf("\nDisplaying USB IN buffer\n\n");

   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }
*/

// Now try to read ROM GB Header


address=308;
game_size=308+64;
usb_buffer_out[0] = READ_GB;
usb_buffer_out[1] = address & 0xFF ;
usb_buffer_out[2] = (address & 0xFF00)>>8;
usb_buffer_out[3]=(address & 0xFF0000)>>16;
usb_buffer_out[5]=game_size & 0xFF;
usb_buffer_out[6]=(game_size & 0xFF00)>>8;
usb_buffer_out[7]=(game_size & 0xFF0000)>>16;
usb_buffer_out[4] = 0; // Slow Mode
libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
libusb_bulk_transfer(handle, 0x82,usb_buffer_in,64, &numBytes, 60000);

/*
printf("\nDisplaying USB IN buffer\n\n");

   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }
*/
printf("\nReading ROM Header...\n");
printf("\nGame Title : ");
for (i=0; i<16; i++) {Game_Name[i]=usb_buffer_in[i];} // ROM Name
printf("%.*s",16,(char *) Game_Name);
Rom_Type = usb_buffer_in[19];
printf("\nCartridge Type : %02X ",Rom_Type);
if ( Rom_Type == 0x00){ printf("(ROM ONLY)");}
if ( Rom_Type == 0x01){ printf("(MBC1)");}
if ( Rom_Type == 0x02){ printf("(MBC1+RAM)");}
if ( Rom_Type == 0x03){ printf("(MBC1+RAM+BATTERY)");}
if ( Rom_Type == 0x05){ printf("(MBC2)");}
if ( Rom_Type == 0x06){ printf("(MBC2+BATTERY)");}
if ( Rom_Type == 0x08){ printf("(ROM+RAM)");}
if ( Rom_Type == 0x09){ printf("(ROM+RAM+BATTERY)");}
if ( Rom_Type == 0x0B){ printf("(MMM01)");}
if ( Rom_Type == 0x0C){ printf("(MMM01+RAM)");}
if ( Rom_Type == 0x0D){ printf("(MMM01+RAM+BATTERY)");}
if ( Rom_Type == 0x0F){ printf("(MBC3+TIMER+BATTERY)");}
if ( Rom_Type == 0x10){ printf("(MBC3+TIMER+RAM+BATTERY)");}
if ( Rom_Type == 0x11){ printf("(MBC3)");}
if ( Rom_Type == 0x12){ printf("(MBC3+RAM)");}
if ( Rom_Type == 0x13){ printf("(MBC3+RAM+BATTERY)");}
if ( Rom_Type == 0x19){ printf("(MBC5)");}
if ( Rom_Type == 0x1A){ printf("(MBC5+RAM)");}
if ( Rom_Type == 0x1B){ printf("(MBC5+RAM+BATTERY)");}
if ( Rom_Type == 0x1C){ printf("(MBC5+RUMBLE)");}
if ( Rom_Type == 0x1D){ printf("(MBC5+RUMBLE+RAM)");}
if ( Rom_Type == 0x1E){ printf("(MBC5+RUMBLE+RAM+BATTERY)");}
if ( Rom_Type == 0x20){ printf("(MBC6)");}
if ( Rom_Type == 0x22){ printf("(MBC7+SENSOR+RUMBLE+RAM+BATTERY)");}
if ( Rom_Type == 0xFC){ printf("(POCKET CAMERA)");}
if ( Rom_Type == 0xFD){ printf("(BANDAI TAMA5)");}
if ( Rom_Type == 0xFE){ printf("(HuC3)");}
if ( Rom_Type == 0xFF){ printf("(HuC1+RAM+BATTERY)");}
Rom_Size= (32768 << usb_buffer_in[20]); // Rom Size
printf("\nGame Size is :  %ld Ko",Rom_Size/1024);
if ( Rom_Type != 0x06) {
Ram_Size = usb_buffer_in[21];
printf("\nRam Size : ");
if ( Ram_Size == 0x00){ printf("None");}
if ( Ram_Size == 0x01){ printf("2 Ko");save_size=2*1024;}
if ( Ram_Size == 0x02){ printf("8 Ko");save_size=8*1024;}
if ( Ram_Size == 0x03){ printf("32 Ko ");save_size=32*1024;}
if ( Ram_Size == 0x04){ printf("128 Ko");save_size=128*1024;}
if ( Ram_Size == 0x05){ printf("64 Ko");save_size=64*1024;}
}
else{Ram_Size = 512; printf("\nRam Size : 512 bytes ");save_size=512;}
CGB = usb_buffer_in[15];
if ( CGB  == 0xC0){ printf("\nGame only works on GameBoy Color");}
else { printf("\nGameBoy / GameBoy Color compatible game");}
SGB = usb_buffer_in[18];
if ( SGB  == 0x00){ printf("\nNo Super GameBoy enhancements");}
if ( SGB  == 0x03){ printf("\nGame have Super GameBoy enhancements");}

// MBC5 custom inits


// Clean Buffer
  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}
/*
if (Rom_Type == 0x19 || 0x1A || 0x1B || 0x1C || 0x1D || 0x1E)
{printf("\nMBC5 detected please wait until init is completed ...\n");
address=0;
game_size = Rom_Size;
usb_buffer_out[0] = READ_GB;           				
usb_buffer_out[1]=address & 0xFF;
usb_buffer_out[2]=(address & 0xFF00)>>8;
usb_buffer_out[3]=(address & 0xFF0000)>>16;
usb_buffer_out[4]=0;
usb_buffer_out[5]=game_size & 0xFF;
usb_buffer_out[6]=(game_size & 0xFF00)>>8;
usb_buffer_out[7]=(game_size & 0xFF0000)>>16;
usb_buffer_out[8]=Rom_Type;
BufferTemp = (unsigned char*)malloc(game_size); 
     for (i=0; i<game_size; i++)
        {
            BufferTemp[i]=0x00;
        }			


						libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
						res = libusb_bulk_transfer(handle, 0x82,BufferTemp,game_size*2 , &numBytes, 60000);
  							if (res != 0)
  								{
    								printf("Error \n");
    								return 1;
  								}     
 						printf("MBC5 init completed sucessfully !\n");}
*/

// Clean Buffer
  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}
	
printf("\n\n --- MENU ---\n");
printf(" 1) Dump GB ROM\n");
printf(" 2) Dump GB Save\n"); 
printf(" 3) Write GB Save\n"); 
printf(" 4) Erase GB Save\n"); 
printf(" 5) Write GB Flash\n");
printf(" 6) Erase GB Flash\n");
printf(" 7) Flash Sector Erase\n");
printf(" 8) Flash Memory Detection \n");
printf(" 0) Debug\n"); 


printf("\nYour choice: \n");
scanf("%d", &choixMenu);

switch(choixMenu)
{

case 1: // READ GB ROM
 
    	choixMenu=0;
				printf(" 1) Auto (from header)\n");
        		printf(" 2) Manual\n");
				printf(" Your choice: ");
        		scanf("%d", &choixMenu);
					if(choixMenu!=1)
					{
            			printf(" Enter number of KB to dump: ");
            			scanf("%d", &game_size);
						game_size *= 1024;
					}
					else {game_size = Rom_Size; }
				if (Rom_Type == 0x19 || 0x1A || 0x1B || 0x1C || 0x1D || 0x1E){game_size=game_size*2;}		    
				printf("Sending command Dump ROM \n");
        		printf("Dumping please wait ...\n");
				address=0;
				if (Rom_Type == 0x19 || 0x1A || 0x1B || 0x1C || 0x1D || 0x1E){printf("\nRom Size : %ld Ko \n",game_size/1024/2);}
				else{printf("\nRom Size : %ld Ko \n",game_size/1024);}
        // Cleaning Buffer
   
// Cleaning ROM Buffer
address=0;
BufferROM = (unsigned char*)malloc(game_size); 
     for (i=0; i<game_size; i++)
        {
            BufferROM[i]=0x00;
        }			

						usb_buffer_out[0] = READ_GB;           				
						usb_buffer_out[1]=address & 0xFF;
            			usb_buffer_out[2]=(address & 0xFF00)>>8;
            			usb_buffer_out[3]=(address & 0xFF0000)>>16;
            			usb_buffer_out[4]=0;
						usb_buffer_out[5]=game_size & 0xFF;
            			usb_buffer_out[6]=(game_size & 0xFF00)>>8;
            			usb_buffer_out[7]=(game_size & 0xFF0000)>>16;
						usb_buffer_out[8]=Rom_Type;

/*
						usb_buffer_out[6]=usb_buffer_in[20]; // for calculate number of rom bank
						usb_buffer_out[7]=usb_buffer_in[21]; // for calculate number of ram bank*/

						libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
						printf("ROM dump in progress...\n"); 
						res = libusb_bulk_transfer(handle, 0x82,BufferROM,game_size, &numBytes, 60000);
  							if (res != 0)
  								{
    								printf("Error \n");
    								return 1;
  								}     
 						printf("\nDump ROM completed !\n");
						if ( CGB  == 0xC0){myfile = fopen("dump_gbc.gbc","wb");}
         				else {myfile = fopen("dump_gb.gb","wb");}
						if (Rom_Type == 0x19 || 0x1A || 0x1B || 0x1C || 0x1D || 0x1E){fwrite(BufferROM+game_size/2,1,game_size/2, myfile);}
						else {fwrite(BufferROM, 1,game_size, myfile);}
       					fclose(myfile);
 break;


case 2: // READ GB SAVE
 
    			choixMenu=0;
				printf(" 1) Auto (from header)\n");
        		printf(" 2) Manual\n");
				printf(" Your choice: ");
        		scanf("%d", &choixMenu);
					if(choixMenu!=1)
					{
            			printf(" Enter number of byte to dump: ");
            			scanf("%d", &save_size );
						Ram_Size *= 1024;
					}
						    
				printf("Sending command Dump SAVE \n");
        		printf("Dumping please wait ...\n");
				address=0;
				printf("\nSave Size : %ld Ko \n",save_size/1024);
				BufferRAM = (unsigned char*)malloc(save_size);
   
// Cleaning RAM Buffer
     for (i=0; i<save_size; i++)
        {
            BufferRAM[i]=0x00;
        }
	
		address=0;

						usb_buffer_out[0] = READ_GB_SAVE;           				
						usb_buffer_out[1]=address & 0xFF;
            			usb_buffer_out[2]=(address & 0xFF00)>>8;
            			usb_buffer_out[3]=(address & 0xFF0000)>>16;
            			usb_buffer_out[4]=0;
						usb_buffer_out[8]=Rom_Type;
						usb_buffer_out[9]=save_size/1024;


libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
						printf("RAM dump in progress...\n"); 
						res = libusb_bulk_transfer(handle, 0x82,BufferRAM,save_size, &numBytes, 60000);
  							if (res != 0)
  								{
    								printf("Error \n");
    								return 1;
  								}     
 						printf("\nDump Save completed !\n");
						//myfile = fopen("dump_gb.sav","wb");
						if ( CGB  == 0xC0){myfile = fopen("dump_gbc.sav","wb");}
         				else {myfile = fopen("dump_gb.sav","wb");}
						fwrite(BufferRAM, 1,save_size, myfile);
       					fclose(myfile);
						break;

 case 3:  // WRITE SRAM

        printf(" Save file: ");
        scanf("%60s", dump_name);
        myfile = fopen(dump_name,"rb");
        fseek(myfile,0,SEEK_END);
        save_size = ftell(myfile);
        buffer_save = (unsigned char*)malloc(save_size);
        rewind(myfile);
        fread(buffer_save, 1, save_size, myfile);
        fclose(myfile);

        // Write SRAM

        i=0;
        j=0;
		k=0;
        address=0;
        while ( j < save_size)
        {
            // Fill usb out buffer with save data
            for (i=32; i<64; i++)
            {
                usb_buffer_out[i] = buffer_save[k];
                k++;
            }
            i=0;
            j+=32;
            usb_buffer_out[0] = WRITE_GB_SAVE; // Select write in 8bit Mode
            usb_buffer_out[1]=address & 0xFF;
            usb_buffer_out[2]=(address & 0xFF00)>>8;
            usb_buffer_out[3]=(address & 0xFF0000)>>16;
            usb_buffer_out[7] = 0x00;
            libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
            while ( usb_buffer_in[7] != 0xAA)
            {
                libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000);
            }
            address+=32;
        }

        printf("SRAM Sucessfully Writted ...\n");
        break;



case 4: // Erase GB SAVE
 
    			choixMenu=0;
				printf("Erasing GB Save...");
				buffer_save = (unsigned char*)malloc(1024*32);
				i=0;
				// Cleaning buffer save
            	for (i=0; i<1024*32; i++)
            		{
                		buffer_save[i] = 0xFF;         
            		}
				 i=0;
        j=0;
		k=0;
        address=0;
        while ( j < 1024*32)
        {
            // Fill usb out buffer with save data
            for (i=32; i<64; i++)
            {
                usb_buffer_out[i] = buffer_save[k];
                k++;
            }
            i=0;
            j+=32;
            usb_buffer_out[0] = WRITE_GB_SAVE; // Select write in 8bit Mode
            usb_buffer_out[1]=address & 0xFF;
            usb_buffer_out[2]=(address & 0xFF00)>>8;
            usb_buffer_out[3]=(address & 0xFF0000)>>16;
            usb_buffer_out[7] = 0x00;
            libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
            while ( usb_buffer_in[7] != 0xAA)
            {
                libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000);
            }
            address+=32;
        }
        	    
        printf("\nSRAM Sucessfully Erased ...\n");

				break;

 case 5: //WRITE FLASH

		// Sending ROM to Buffer

		printf(" Enter ROM file: ");
        scanf("%60s", dump_name);
        myfile = fopen(dump_name,"rb");
        fseek(myfile,0,SEEK_END);
        game_size = ftell(myfile);
        BufferROM = (unsigned char*)malloc(game_size);
        rewind(myfile);
        fread(BufferROM, 1, game_size, myfile);
        fclose(myfile);

        i=0;
        address = 0;
    	j=0;
		k=0;

		printf("Writing Flash please wait ...\n");
   
 		// Fill usb out buffer with ROM data

    while ( j < game_size)
        {
          
            for (i=32; i<64; i++)
            {
                usb_buffer_out[i] = BufferROM[k];
                k++;
            }
            i=0;
            j+=32;

            usb_buffer_out[0] = WRITE_GB_FLASH; // Select write in 8bit Mode
            usb_buffer_out[1]=address & 0xFF;
            usb_buffer_out[2]=(address & 0xFF00)>>8;
            usb_buffer_out[3]=(address & 0xFF0000)>>16;
            usb_buffer_out[7] = 0x00;
			usb_buffer_out[8]=0x1A;
            libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
            while ( usb_buffer_in[7] != 0xAA)
            {
                libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000);
            }
            address+=32;
        }

        printf("Flash Sucessfully Writted ...\n");
		break;

 case 6: //ERASE FLASH

        usb_buffer_out[0] = ERASE_GB_FLASH;
        libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
        i=0;
        while(usb_buffer_in[0]!=0x01)
        {
            libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000);   //wait status
            printf("\r ERASE flash in progress: %s ", wheel[i]);
            fflush(stdout);
            i++;
            if(i==4)
            {
                i=0;
            }
        }

        printf("\r ERASE SMD flash in progress: 100%%");
        fflush(stdout);

        break;

case 7: //ERASE Sector Test

		printf(" Enter sector number to erase : \n ");
        scanf("%d", &sector_number);
		usb_buffer_out[0] = ERASE_SECTOR;
		usb_buffer_out[1] = sector_number;
		libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
		libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000); 
		while ( usb_buffer_in[7] != 0xAA)
            {
                libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000);
            }
		printf("Sector Erased sucesfully \n");
		break;		
	    

case 8: // Vendor / ID Info


						//printf("Detecting Flash...\n");
						printf("Try to Detect AMD compatible Flash...\n");
						usb_buffer_out[0] = INFOS_ID;
						usb_buffer_out[1] = 0x01;  // AMD Manufacturer ID
						libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
						libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000); 
						manufacturer_id = usb_buffer_in[1];
						chip_id = usb_buffer_in[3];
						printf("Manufacturer ID : %02X    ",usb_buffer_in[1]);
						printf("Chip ID : %02X \n",usb_buffer_in[3]);
						flash_id = (manufacturer_id<<8) | chip_id;
						//printf("Flash ID : %04X \n",flash_id);
						switch(flash_id)
						  {
								case 0x01AD :   printf("Memory : AM29F016 \n");printf("Capacity : 16Mb \n");break;
								case 0x0141 :   printf("Memory : AM29F032 \n");printf("Capacity : 32Mb \n");break;
								default : 		printf("No AMD or compatible Flash detected \n");			break;
                          }
						scanf("%d");
								
						break; 



case 0:  // DEBUG

        while (1)
        {
            printf("\n\nEnter ROM Address ( decimal value) :\n \n");
            scanf("%ld",&address);
			usb_buffer_out[0] = READ_GB;
			usb_buffer_out[1] = address & 0xFF ;
			usb_buffer_out[2] = (address & 0xFF00)>>8;
			usb_buffer_out[3]=(address & 0xFF0000)>>16;
			usb_buffer_out[4] = 0; // Slow Mode

           libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
libusb_bulk_transfer(handle, 0x82,usb_buffer_in,64, &numBytes, 60000);


printf("\nDisplaying USB IN buffer\n\n");

   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }
        }
  

}

		
return 0;

}





