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

// Un-comment to enable support for "big" devices - needs 2K buffer, so won't run on an Arduino Uno
#define __BIG_PROM

// Maximum number of bytes in data buffer
// Biggest PROM ever - 1K bytes when using an Arduino Uno
//                     2K with an Arduino Mega
#ifdef __BIG_PROM
#define BUFF_SIZE 2048
#else
#define BUFF_SIZE 1024
#endif

// Length of serial port buffer
#define SERBUFF_LEN 80

// Value to initialise data buffer with
#define INIT_BUFF  0x00

//defining GPIO pins for eeprom   
#define DATA_IN  2              // EEPROM PIN#4 - DO
#define DATA_OUT 3              // EEPROM PIN#3 - DI
#define CLOCK    4              // EEPROM PIN#2 - CLK
#define CHIP_SEL 5              // EEPROM PIN#1 - CS
#define PWR      6              // Turn PROM on and off

#define READ   0b00000110       //read instruction
#define WRITE  0b00000101       //write instruction
#define WREN   0b00000100       //erase write enable instruction
#define WREN2  0b00000011       // second part of write enable
#define WRDS   0b00000100       // write disable
#define WRDS2  0b00000000       // second part of write disable
#define ERAL   0b00000100       // erase all
#define ERAL2  0b00000010       // second part of erase

#define DEV_9346 0
#define DEV_9356 1
#define DEV_9366 2
#define DEV_9376 3
#ifdef __BIG_PROM
#define DEV_9386 4
#endif

// Flow control when loading Intel hex file
#define XON    0x11
#define XOFF   0x13
#define ESC    0x1b

typedef struct
{
    char *promName;
    int addrLen;
    int promSize;
} promType;

typedef struct
{
    char *cmdName;
    void (*fn)(void);
} cmdType;

void cmdReadDevice(void);
void cmdSetOffset(void);
void cmdSetPageLen(void);
void cmdLoadMemory(void);
void cmdProgDevice(void);
void cmdSetPromType(void);
void cmdFillBuff(void);
void cmdDumpBuff(void);
void cmdListCommands(void);
void cmdNFill(void);
void cmdErase(void);
void cmdIntelDump(void);

// Address offset for loading data
int offset;

// Buffer when reading serial port
char serialBuff[SERBUFF_LEN];
char *paramPtr;

// Data buffer for prom contents
unsigned char dataBuffer[BUFF_SIZE];

int promId;

int pageLen;

promType proms[] =
{
    { "9346", 6, 128 },
    { "9356", 8, 256 },
    { "9366", 8, 512 },
    { "9376", 10, 1024 },
#ifdef __BIG_PROM
    { "9386", 10, 2048 },
#endif
    { NULL, 0 },
};

// Command table
cmdType cmdList[] =
{
    { "device", cmdSetPromType },
    { "dump", cmdDumpBuff },
    { "erase", cmdErase },
    { "fill", cmdFillBuff },
    { "help", cmdListCommands },
    { "idump", cmdIntelDump },
    { "load", cmdLoadMemory },
    { "nfill", cmdNFill },
    { "offset", cmdSetOffset },
    { "pagelen", cmdSetPageLen },
    { "prog", cmdProgDevice },
    { "read", cmdReadDevice },
    { "?", cmdListCommands },
    { NULL, NULL }
};

void powerUp()
{
    digitalWrite(PWR, HIGH);
    delay(500);
}

void powerDown()
{
    digitalWrite(PWR, LOW);
}

// Shift a number of bits out of DATA_OUT pin using CLOCK pin
// Data goes out most significant bit first
void shiftBitsOut(unsigned short int value, int bitLength)
{
    if(bitLength == 0)
    {
        bitLength = 16;
        while((value & 0x8000) != 0x8000)
        {
            value = value << 1;
            bitLength--;
        }
    }
  
    while(bitLength)
    {
        if((value & 0x8000) == 0x8000)
        {
            digitalWrite(DATA_OUT, HIGH);
        }
        else
        {
            digitalWrite(DATA_OUT, LOW);
        }

        digitalWrite(CLOCK, HIGH);
        digitalWrite(CLOCK, LOW);

        value = value << 1;
        bitLength--;
    }
}

void writeEnable()
{
    unsigned short int opCode;
    unsigned short int addr;

    opCode = WREN << (proms[promId].addrLen);
    addr = WREN2 << (proms[promId].addrLen - 2);

    opCode = addr | opCode;

    digitalWrite(CHIP_SEL ,HIGH);

    shiftBitsOut(opCode, 0);
    
    digitalWrite(CHIP_SEL, LOW);

    delay(2);
}

void writeDisable()
{
    unsigned short int opCode;
    unsigned short int addr;

    opCode = WRDS << (proms[promId].addrLen);
    addr = WRDS2 << (proms[promId].addrLen - 2);

    opCode = addr | opCode;

    digitalWrite(CHIP_SEL ,HIGH);

    shiftBitsOut(opCode, 0);
    
    digitalWrite(CHIP_SEL, LOW);

    delay(2);
}

void eraseDevice()
{
    unsigned short int opCode;
    unsigned short int addr;

    opCode = ERAL << (proms[promId].addrLen);
    addr = ERAL2 << (proms[promId].addrLen - 2);

    opCode = addr | opCode;

    digitalWrite(CHIP_SEL ,HIGH);

    shiftBitsOut(opCode, 0);
    
    digitalWrite(CHIP_SEL, LOW);

    // datasheet says max time is 15ms
    delay(20);
}

void readWord(unsigned short int addr, unsigned char *hi, unsigned char *lo)
{
    unsigned short int opCode;

    opCode = (READ << proms[promId].addrLen) | addr;
    
    digitalWrite(CHIP_SEL ,HIGH);

    shiftBitsOut(opCode, proms[promId].addrLen + 3);

    // Read 16 bits of data, eight at a time
    *hi = shiftIn(DATA_IN, CLOCK, MSBFIRST);        
    *lo = shiftIn(DATA_IN, CLOCK, MSBFIRST);
   
    digitalWrite(CHIP_SEL, LOW);
}

void writeWord(unsigned short int addr, unsigned char hi, unsigned char lo)
{
    unsigned short int opCode;

    opCode = (WRITE << proms[promId].addrLen) | addr;
    
    digitalWrite(CHIP_SEL ,HIGH);

    shiftBitsOut(opCode, proms[promId].addrLen + 3);

    // Write 16 bits of data, eight at a time
    opCode = (hi << 8) | lo;
    shiftBitsOut(opCode, 16);

    digitalWrite(CHIP_SEL, LOW);

    // datasheet says max time is 1ms per byte
    delay(5);
}

void readDevice()
{
    unsigned short int opCode;
    int addr;

    // Start at device address 0
    opCode = (READ << proms[promId].addrLen) | 0;
    
    digitalWrite(CHIP_SEL ,HIGH);

    shiftBitsOut(opCode, proms[promId].addrLen + 3);

    // Read entire device
    addr = 0;
    while(addr < proms[promId].promSize)
    {
        dataBuffer[addr] = shiftIn(DATA_IN, CLOCK, MSBFIRST); 
        addr++;
               
        dataBuffer[addr] = shiftIn(DATA_IN, CLOCK, MSBFIRST);
        addr++;
    }
   
    digitalWrite(CHIP_SEL, LOW);
}

void writeDevice()
{
    unsigned short int opCode;
    int devAddr;
    int buffAddr;

    devAddr = 0;
    opCode = (WRITE << proms[promId].addrLen) | devAddr;

    buffAddr = 0;
    while(buffAddr < proms[promId].promSize)
    {
        digitalWrite(CHIP_SEL ,HIGH);

        shiftBitsOut(opCode, proms[promId].addrLen + 3);
        shiftOut(DATA_OUT, CLOCK, MSBFIRST, dataBuffer[buffAddr]);
        buffAddr++;
        
        shiftOut(DATA_OUT,CLOCK, MSBFIRST, dataBuffer[buffAddr]);
        buffAddr++;

        digitalWrite(CHIP_SEL, LOW);

        // datasheet says max time is 1ms per byte
        delay(5);

        devAddr++;
    }
}

unsigned char buffToByte(char *buff)
{           
    char txtBuff[16];
    char *endPtr;
     
    memcpy(txtBuff, buff, 2);
    txtBuff[2] = '\0';
    return (unsigned char)strtol(txtBuff, &endPtr, 16);
}

unsigned short int buffToWord(char *buff)
{           
    char txtBuff[16];
    char *endPtr;
     
    memcpy(txtBuff, buff, 4);
    txtBuff[4] = '\0';
    return (unsigned short int)strtol(txtBuff, &endPtr, 16);
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
                if(inchar == ' ')
                {
                    serialBuff[c] = '\0';
                    c++;
                    paramPtr = &serialBuff[c];
                }
                else
                {
                    if(isprint(inchar))
                    {
                        serialBuff[c] = inchar;
                        c++;
                    }
                }
            }
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
    while(cmdList[c].cmdName != NULL)
    {
        Serial.print(cmdList[c].cmdName);
        Serial.print("\n");

        c++;
    }
}

void cmdErase()
{
    powerUp();
    writeEnable();
    eraseDevice();
    writeDisable();
    powerDown();
}

void cmdNFill()
{
    unsigned short int addr;
    unsigned char dByte;

    dByte = 0;
    for(addr = 0; addr < proms[promId].promSize; addr++)
    {
        dataBuffer[addr] = dByte;
        dByte++;
    }
}

void cmdDumpBuff()
{
    unsigned short int addr;
    int c;
    char txtBuff[32];
    unsigned char dByte;
    int lineCount;
    char inchar;

    lineCount = pageLen;

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

        if(pageLen && addr != proms[promId].promSize)
        {
            lineCount--;
            if(lineCount == 0)
            {
                lineCount = pageLen;

                Serial.print("--more--");

                do
                {
                    inchar = Serial.read();
                }
                while(inchar == -1);
                Serial.print("\n");

                if(inchar == 'n' || inchar == 'q' || inchar == ESC)
                {
                    addr = proms[promId].promSize;
                }
            }
        }
    }
    while(addr < proms[promId].promSize);
}

void cmdIntelDump()
{
    unsigned short int addr;
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
    while(addr < proms[promId].promSize);

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
    powerUp();
    readDevice();
    powerDown();
}

// Set prom type
void cmdSetPromType()
{
    char txtBuff[16];
    int c;
    
    if(paramPtr == NULL)
    {
        Serial.print("Device: ");
        Serial.print(proms[promId].promName);
        Serial.print("\n");

        sprintf(txtBuff, "Size: %d KB\n", proms[promId].promSize);
        Serial.print(txtBuff);

        Serial.print("Supported devices: ");
        c = 0;
        do
        {
            Serial.print(proms[c].promName);
            c++;
            if(proms[c].promName != NULL)
            {
                Serial.print(", ");
            }
        }
        while(proms[c].promName != NULL);
        Serial.print("\n");

        Serial.print("Buffer: ");

        sprintf(txtBuff, "%d bytes\n", BUFF_SIZE);
        Serial.print(txtBuff);
    }
    else
    {
        c = 0;
        while(strcmp(paramPtr, proms[c].promName) && proms[c].promName != NULL)
        {
            c++;
        }

        if(proms[c].promName == NULL)
        {
            Serial.print("Invalid device\n");
        }
        else
        {
            promId = c;
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

// Set load offset
void cmdSetPageLen()
{
    char txtbuff[16];
    char *endPtr;
    
    if(paramPtr == NULL)
    {
        sprintf(txtbuff, "%d\n", pageLen);
        Serial.print("Page length: ");
        Serial.print(txtbuff);
    }
    else
    {
        pageLen = strtol(paramPtr, &endPtr, 10);
    }
}

// Read serial port data and store in buffer
// add offset to addresses in hex file
void cmdLoadMemory()
{
    char txtBuff[16];
    int c;
    boolean eof;
    short int addr;
    unsigned char byteCount;
    unsigned char recordType;
    char *endPtr;

    eof = false;

    do
    {
        Serial.write(XON);
         
        serialReadline();

        Serial.write(XOFF);
        
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

                    addr = buffToWord(&serialBuff[3]);

                    if(offset >= 0)
                    {
                        addr = addr + offset;
                    }
                    else
                    {
                        if(addr > abs(offset))
                        {
                            addr = addr + offset;
                        }
                        else
                        {
                            addr = 0;
                        }
                    }

                    // data starts at ninth character
                    c = 9;   

                    while(byteCount && eof == false)
                    {
                        if(addr >= BUFF_SIZE)
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
    powerUp();
    writeEnable();
    writeDevice();
    writeDisable();
    powerDown();
}

void setup()
{
    pinMode(CLOCK, OUTPUT);
    pinMode(DATA_OUT, OUTPUT);
    pinMode(DATA_IN, INPUT);
    pinMode(CHIP_SEL, OUTPUT);
    pinMode(PWR, OUTPUT);

    digitalWrite(CHIP_SEL, LOW);
    digitalWrite(CLOCK, LOW);
    digitalWrite(PWR, LOW);
    
    Serial.begin(9600);

    offset = 0;
    promId = DEV_9356;
    pageLen = 0;
    memset(dataBuffer, INIT_BUFF, BUFF_SIZE);

    while(!Serial);

    Serial.print("Ed's 93cX6 PROM programmer\n");
}

void loop()
{
    int cmd;

    Serial.print("> ");
    serialReadline();
    Serial.print("\n");

    if(serialBuff[0] != '\0')
    {
        cmd = 0;
        while(strcmp(cmdList[cmd].cmdName, serialBuff) && cmdList[cmd].cmdName != NULL)
        {
            cmd++;
        }

        if(cmdList[cmd].cmdName == NULL)
        {
            Serial.print("Bad command\n");
        }
        else
        {
            cmdList[cmd].fn();
        }
    }

    Serial.print("\n");
}
