//
// Ed's 93c56 / 93c66 PROM programmer
//
// prom is in 16 bit mode only
// In maxon radio, ORG pin is hardwired to make chip operate in 16 bit mode
//
// Programmer can:-
//   read prom into memory
//   write buffer into prom
//   erase entire device
//   load intel hex file into memory with or without offset
//   dump buffer contents
//   

// Maximum number of bytes in data buffer
// Biggest PROM ever - 1K when using an Arduino Uno
#define BUFF_SIZE 1024

// Length of serial port buffer
#define SERBUFF_LEN 80

// Value to initialise data buffer with
#define INIT_BUFF  0x00

//defining GPIO pins for eeprom   
#define DATA_IN  2              // EEPROM PIN#4 - DO
#define DATA_OUT 3              // EEPROM PIN#3 - DI
#define CLOCK 4                 // EEPROM PIN#2 - CLK
#define CHIP_SEL 5              // EEPROM PIN#1 - CS

#define READ   0b00000110       //read instruction
#define WRITE  0b00000101       //write instruction
#define WREN   0b00000100       //erase write enable instruction
#define WREN2  0b11000000       // second byte of write enable
#define WRDS   0b00000100       // write disable
#define WRDS2  0b00000000       // second byte of write disable
#define ERAL   0b00000100       // erase all
#define ERAL2  0b10000000       // second byte of erase

typedef struct
{
    char *name;
    void (*fn)(void);
} cmdType;

void cmdReadDevice(void);
void cmdSetOffset(void);
void cmdLoadMemory(void);
void cmdProgDevice(void);
void cmdSetPromSize(void);
void cmdFillBuff(void);
void cmdDumpBuff(void);
void cmdListCommands(void);
void cmdNFill(void);
void cmdErase(void);
void cmdIntelDump(void);

// Address offset for loading data
long int offset;

// size of PROM
int promSize;

char serialBuff[SERBUFF_LEN];
char *paramPtr;

unsigned char dataBuffer[BUFF_SIZE];

cmdType cmdList[] =
{
    { "dump", cmdDumpBuff },
    { "erase", cmdErase },
    { "fill", cmdFillBuff },
    { "help", cmdListCommands },
    { "idump", cmdIntelDump },
    { "load", cmdLoadMemory },
    { "nfill", cmdNFill },
    { "offset", cmdSetOffset },
    { "prog", cmdProgDevice },
    { "read", cmdReadDevice },
    { "size", cmdSetPromSize },
    { "?", cmdListCommands },
    { "", NULL }
};

void writeEnable()
{
    digitalWrite(CHIP_SEL ,HIGH);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, WREN);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, WREN2);

    digitalWrite(CHIP_SEL, LOW);

    delay(2);
}

void writeDisable()
{
    digitalWrite(CHIP_SEL ,HIGH);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, WRDS);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, WRDS2);

    digitalWrite(CHIP_SEL, LOW);

    delay(2);
}

void eraseDevice()
{
    digitalWrite(CHIP_SEL ,HIGH);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, ERAL);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, ERAL2);

    digitalWrite(CHIP_SEL, LOW);

    // datasheet says max time is 15ms
    delay(20);
}

void readWord(int addr, unsigned char *hi, unsigned char *lo)
{
    digitalWrite(CHIP_SEL ,HIGH);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, READ);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, addr);

    // Read 16 bits of data, eight at a time
    *hi = shiftIn(DATA_IN, CLOCK, MSBFIRST);        
    *lo = shiftIn(DATA_IN, CLOCK, MSBFIRST);
   
    digitalWrite(CHIP_SEL, LOW);
}

void writeWord(int addr, unsigned char hi, unsigned char lo)
{
    digitalWrite(CHIP_SEL ,HIGH);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, WRITE);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, addr);

    // Write 16 bits of data, eight at a time
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, hi);
    shiftOut(DATA_OUT, CLOCK, MSBFIRST, lo);
   
    digitalWrite(CHIP_SEL, LOW);

    // datasheet says max time is 1ms per byte
    delay(5);
}

unsigned char buffToByte(char *buff)
{           
    char txtBuff[16];
    char *endPtr;
     
    memcpy(txtBuff, buff, 2);
    txtBuff[2] = '\0';
    return (unsigned char)strtol(txtBuff, &endPtr, 16);
}

unsigned char buffToWord(char *buff)
{           
    char txtBuff[16];
    char *endPtr;
     
    memcpy(txtBuff, buff, 4);
    txtBuff[4] = '\0';
    return (unsigned int)strtol(txtBuff, &endPtr, 16);
}

// Read a line from serial port up to '\n' character
// seperate command and parameter into two strings by replacing space with '\0'
// set paramPtr to point to parameter if present
void serialReadline()
{
    int c;
    int inchar;

    paramPtr = NULL;

    c = 0;
    do
    {
        if(Serial.available() > 0)
        {
            inchar = Serial.read();
            if(inchar == '\n')
            {
                serialBuff[c] = '\0';
            }
            else
            {
                serialBuff[c] = inchar;
            }
            
            // Command parameters will follow a space...
            if(inchar == ' ')
            {
                serialBuff[c] = '\0';
                paramPtr = &serialBuff[c + 1];
            }

            c++;
        }

        if(c == SERBUFF_LEN)
        {
            serialBuff[SERBUFF_LEN - 1] = '\0';
        }
    }
    while(c < SERBUFF_LEN && inchar != '\n');
}

void cmdListCommands()
{
    int c;

    c = 0;
    while(cmdList[c].name[0] != '\0')
    {
        Serial.print(cmdList[c].name);
        Serial.print("\n");

        c++;
    }
}

void cmdErase()
{
    writeEnable();
    eraseDevice();
    writeDisable();
}

void cmdNFill()
{
    int addr;
    unsigned char dByte;

    dByte = 0;
    for(addr = 0; addr < promSize; addr++)
    {
        dataBuffer[addr] = dByte;
        dByte++;
    }
}

void cmdDumpBuff()
{
    int addr;
    int c;
    char txtBuff[32];
    unsigned char dByte;

    addr = 0;
    do
    {
        sprintf(txtBuff, "%04X   ", addr);
        Serial.print(txtBuff);

        for(c = 0; c < 16; c++)
        {
            sprintf(txtBuff, "%02X ", dataBuffer[addr + c]);
            Serial.print(txtBuff);
        }

        txtBuff[0] = ':';
        c = 0;
        do
        {
            dByte = dataBuffer[addr + c];
            c++;
            
            if(isprint(dByte))
            {
                txtBuff[c] = dByte;
            }
            else
            {
                txtBuff[c] = '.';
            }
        }
        while(c < 16);
        txtBuff[17] = ':';
        txtBuff[18] = '\0';

        Serial.print("   ");
        Serial.print(txtBuff);
        Serial.print("\n");

        addr = addr + 16;
    }
    while(addr < promSize);
}

void cmdIntelDump()
{
    int addr;
    int c;
    unsigned char cs;
    unsigned char hi;
    unsigned char lo;
    unsigned char dByte;
    char txtBuff[16];

    addr = 0;
    do
    {
        hi = (addr & 0xff00) >> 8;
        lo = addr & 0x00ff;
        
        sprintf(serialBuff, ":10%02X%02X00", hi, lo);
        cs = 0 - 16 - hi - lo;
        for(c = 0; c < 16; c++)
        {
            dByte = dataBuffer[addr + c];
            sprintf(txtBuff, "%02X", dByte);
            strcat(serialBuff, txtBuff);

            cs = cs - dByte;
        }
        sprintf(txtBuff, "%02X\n", cs);
        strcat(serialBuff, txtBuff);
        
        addr = addr + 16;

        Serial.print(serialBuff);
    }
    while(addr < promSize);

    Serial.print(":00000001FF\n");
}

void cmdFillBuff()
{
    unsigned char fillByte;
    char *endPtr;

    if(paramPtr == NULL)
    {
        Serial.print("Missing parameter\n");
    }
    else
    {
        fillByte = (unsigned char)strtol(paramPtr, &endPtr, 16);
        memset(dataBuffer, fillByte, BUFF_SIZE);
    }
}

// Read device contents and send in hex to serial port
void cmdReadDevice()
{
    int addr;

    addr = 0;
    do
    {
        readWord(addr, &dataBuffer[addr], &dataBuffer[addr + 1]);
        addr = addr + 2;
    }
    while(addr < promSize);
}

// Set device size
void cmdSetPromSize()
{
    char txtbuff[16];
    char *endPtr;
    int sz;
    
    if(paramPtr == NULL)
    {
        sprintf(txtbuff, "0x%04X\n", promSize);
        Serial.print("Prom size: ");
        Serial.print(txtbuff);
    }
    else
    {
        sz = strtol(paramPtr, &endPtr, 16);
        if(sz > BUFF_SIZE)
        {
            Serial.print("Size too big\n");
        }
        else
        {
            promSize = sz;    
        }
    }
}

// Set load offset
void cmdSetOffset()
{
    char txtbuff[16];
    char *endPtr;
    
    if(paramPtr == NULL)
    {
        sprintf(txtbuff, "0x%04X\n", offset);
        Serial.print("Load offset: ");
        Serial.print(txtbuff);
    }
    else
    {
        offset = strtol(paramPtr, &endPtr, 16);
    }
}

// Read serial port data and store in buffer
// add offset to addresses in hex file
void cmdLoadMemory()
{
    char txtBuff[16];
    int c;
    boolean eof;
    unsigned int addr;
    unsigned char byteCount;
    unsigned char recordType;
    char *endPtr;

    eof = false;

    do
    {
        serialReadline();
        
        // must start with ':'
        //                   01234567890"
        // must be at least ":LLAAAARRCC"
        if(serialBuff[0] == ':' && strlen(serialBuff) >= 11)
        {
            recordType = buffToByte(&serialBuff[7]);
            
            switch(recordType)
            {
                // Data record
                case 0x00:
                    byteCount = buffToByte(&serialBuff[1]);

                    addr = buffToWord(&serialBuff[3]) + offset;
                    if(addr > BUFF_SIZE)
                    {
                        addr = 0;
                    }

                    // data starts at ninth character
                    c = 9;   

                    while(byteCount && eof == false)
                    {
                        if(addr > BUFF_SIZE)
                        {
                            Serial.print("Buffer overflow\n");
                            eof = true;
                        }
                        else
                        {
                            dataBuffer[addr] = buffToByte(&serialBuff[c]);

                            c = c + 2;
                            addr++;
                            byteCount--;
                        }
                    }
                    break;

                // End-of-file record
                case 0x01:
                    eof = true;
                    break;

                // Ignore other record types
                default:;
            }
        }
    }
    while(eof == false);
}

// Programme device with buffer data
void cmdProgDevice()
{
    int addr;

    writeEnable();

    addr = 0;
    do
    {
        writeWord(addr, dataBuffer[addr], dataBuffer[addr + 1]);
        addr = addr + 2;
    }
    while(addr < promSize);

    writeDisable();
}

void setup()
{
    pinMode(CLOCK ,OUTPUT);
    pinMode(DATA_OUT ,OUTPUT);
    pinMode(DATA_IN ,INPUT);
    pinMode(CHIP_SEL ,OUTPUT);
    digitalWrite(CHIP_SEL ,LOW);
    
    Serial.begin(9600);

    offset = 0;
    promSize = 256;   // A 93C56
    memset(dataBuffer, INIT_BUFF, BUFF_SIZE);

    while(!Serial);

    Serial.print("Ed's 93cX6 PROM programmer\n");
}

void loop()
{
    int cmd;

    do
    {
        Serial.print("> ");
        serialReadline();
        Serial.print("\n");

        cmd = 0;
        while(strcmp(cmdList[cmd].name, serialBuff) && cmdList[cmd].name[0] != '\0')
        {
            cmd++;
        }

        if(cmdList[cmd].name[0] == '\0')
        {
            if(serialBuff[0] != '\0')
            {
                Serial.print("Bad command\n");
            }
        }
        else
        {
            cmdList[cmd].fn();
        }

        Serial.print("\n");
    }
    while(1);
}

#ifdef _DICKDICK

//int high = 0;
int low =0;     //high and low address
byte dataword[]={"hello world!"};    //data to send in eeprom

void loop_off_the_internet()
{
    char buf[8];
    int i;
    byte a;
    byte b;

    //digitalWrite(CHIP_SEL ,HIGH);                 
    //shiftOut(DATA_OUT,CLOCK,MSBFIRST,EWEN);     //sending EWEN instruction
    //digitalWrite(CHIP_SEL ,LOW);
    delay(10);
 
    low=0;
 
    // For 8bit organization change it to i<=255
    for (i=0; i<=127; i++)
    {
        digitalWrite(CHIP_SEL ,HIGH);
        shiftOut(DATA_OUT,CLOCK,MSBFIRST,READ); //sending READ instruction
        shiftOut(DATA_OUT,CLOCK,MSBFIRST,low);   //sending low address
        a = shiftIn(DATA_IN,CLOCK,MSBFIRST); //sendind data
   
        // The Following Line is for x16Bit Organization
        // Comment it for 8 bit organization
        b = shiftIn(DATA_IN,CLOCK,MSBFIRST); //sendind data
   
        digitalWrite(CHIP_SEL ,LOW);
   
        // For 8bit Organization change it to low++;
        low = low + 2;       //incrementing low address
   
        // The Following Line is for x16bit Organization
        sprintf(buf, "%02X%02X", a,b);
   
        // The Following Line is for x8bit Organization
        //sprintf(buf, "%02X", a);
   
        Serial.print(buf);
    }
    
    while(1);
}

#endif
