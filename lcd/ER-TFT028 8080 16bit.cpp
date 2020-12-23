void ILI9341_Initial(void)
{
	delayms(5);
	RES=0;
	delayms(10);
	RES=1;
	delayms(120);

	Write_Cmd(0x01); //software reset
	delayms(5);

	Write_Cmd(0x11);
	delayms(120);

	Write_Cmd(0xCF);
	Write_Cmd_Data(0x00);
	Write_Cmd_Data(0x83);
	Write_Cmd_Data(0X30);

	Write_Cmd(0xED);
	Write_Cmd_Data(0x64);
	Write_Cmd_Data(0x03);
	Write_Cmd_Data(0X12);
	Write_Cmd_Data(0X81);

	Write_Cmd(0xE8);
	Write_Cmd_Data(0x85);
	Write_Cmd_Data(0x01);
	Write_Cmd_Data(0x79);

	Write_Cmd(0xCB);
	Write_Cmd_Data(0x39);
	Write_Cmd_Data(0x2C);
	Write_Cmd_Data(0x00);
	Write_Cmd_Data(0x34);
	Write_Cmd_Data(0x02);

	Write_Cmd(0xF7);
	Write_Cmd_Data(0x20);

	Write_Cmd(0xEA);
	Write_Cmd_Data(0x00);
	Write_Cmd_Data(0x00);


	Write_Cmd(0xC1);    //Power control
	Write_Cmd_Data(0x11);   //SAP[2:0];BT[3:0]

	Write_Cmd(0xC5);    //VCM control 1
	Write_Cmd_Data(0x34);
	Write_Cmd_Data(0x3D);

	Write_Cmd(0xC7);    //VCM control 2
	Write_Cmd_Data(0xC0);

	Write_Cmd(0x36);    // Memory Access Control
	Write_Cmd_Data(0x08);

	Write_Cmd(0x3A);     // Pixel format
	Write_Cmd_Data(0x55);  //16bit

	Write_Cmd(0xB1);       // Frame rate
	Write_Cmd_Data(0x00);
	Write_Cmd_Data(0x1D);  //65Hz

	Write_Cmd(0xB6);    // Display Function Control
	Write_Cmd_Data(0x0A);
	Write_Cmd_Data(0xA2);
	Write_Cmd_Data(0x27);
	Write_Cmd_Data(0x00);

	Write_Cmd(0xb7); //Entry mode
	Write_Cmd_Data(0x07);


	Write_Cmd(0xF2);    // 3Gamma Function Disable
	Write_Cmd_Data(0x08);

	Write_Cmd(0x26);    //Gamma curve selected
	Write_Cmd_Data(0x01);


	Write_Cmd(0xE0); //positive gamma correction
	Write_Cmd_Data(0x1f);
	Write_Cmd_Data(0x1a);
	Write_Cmd_Data(0x18);
	Write_Cmd_Data(0x0a);
	Write_Cmd_Data(0x0f);
	Write_Cmd_Data(0x06);
	Write_Cmd_Data(0x45);
	Write_Cmd_Data(0x87);
	Write_Cmd_Data(0x32);
	Write_Cmd_Data(0x0a);
	Write_Cmd_Data(0x07);
	Write_Cmd_Data(0x02);
	Write_Cmd_Data(0x07);
	Write_Cmd_Data(0x05);
	Write_Cmd_Data(0x00);

	Write_Cmd(0xE1); //negamma correction
	Write_Cmd_Data(0x00);
	Write_Cmd_Data(0x25);
	Write_Cmd_Data(0x27);
	Write_Cmd_Data(0x05);
	Write_Cmd_Data(0x10);
	Write_Cmd_Data(0x09);
	Write_Cmd_Data(0x3a);
	Write_Cmd_Data(0x78);
	Write_Cmd_Data(0x4d);
	Write_Cmd_Data(0x05);
	Write_Cmd_Data(0x18);
	Write_Cmd_Data(0x0d);
	Write_Cmd_Data(0x38);
	Write_Cmd_Data(0x3a);
	Write_Cmd_Data(0x1f);

	Write_Cmd(0x11);    //Exit Sleep
	delayms(120);
	Write_Cmd(0x29);    //Display on
	delayms(50);
	}

void Enter_Sleep(void) {
	Write_Cmd(0x28);     // Display off
	Write_Cmd(0x10);     // Enter Sleep mode
	}

void Exit_Sleep(void) {
	Write_Cmd(0x11);     // Sleep out
	delayms(120);
	Write_Cmd(0x29);     // Display on
	}



static void LCD_SetPos(unsigned char x0,unsigned char x1,unsigned int y0,unsigned int y1) {

	unsigned char YSH=y0>>8;
	unsigned char YSL=y0;
	unsigned char YEH=y1>>8;
	unsigned char YEL=y1;
	Write_Cmd(0x2A);
	Write_Cmd_Data (0x00);
	Write_Cmd_Data (x0);
	Write_Cmd_Data (0x00);
	Write_Cmd_Data (x1);
	Write_Cmd(0x2B);
	Write_Cmd_Data (YSH);
	Write_Cmd_Data (YSL);
	Write_Cmd_Data (YEH);
	Write_Cmd_Data (YEL);
	Write_Cmd(0x2C);//LCD_WriteCMD(GRAMWR);
}

void showzifu(unsigned int x,unsigned int y,unsigned char value,unsigned int dcolor,unsigned int bgcolor) {
	unsigned char i,j;
	unsigned char *temp=zifu;

	LCD_SetPos(x,x+7,y,y+11); //
	temp+=(value-32)*12;
	for(j=0;j<12;j++) {
		for(i=0;i<8;i++) {
			if((*temp&(1<<(7-i)))!=0)
				Write_Data(dcolor>>8,dcolor);
			else
				Write_Data(bgcolor>>8,bgcolor);
			}
		temp++;
		}
	}
