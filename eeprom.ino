// Maximum number of bytes in data buffer
#define BUFF_SIZE 1024

// Length of serial port buffer
#define SERBUFF_LEN 80

//defining pins for eeprom   
#define DATA_IN  2    // EEPROM PIN#4
#define DATA_OUT 3    // EEPROM PIN#3
#define CLOCK 4       // EEPROM PIN#2
#define CHIP_SEL 5    // EEPROM PIN#1

#define READ   0b1100          //read instruction
#define WRITE  0b1010          //write instruction
#define EWEN   0b10011         //erase write enable instruction

char serialBuff[SERBUFF_LEN];

char *paramPtr;

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

// Address offset for loading data
long int offset;

// size of PROM
int promSize;

unsigned char dataBuffer[BUFF_SIZE];

cmdType cmdList[] =
{
    { "dump", cmdDumpBuff },
    { "fill", cmdFillBuff },
    { "help", cmdListCommands },
    { "load", cmdLoadMemory },
    { "offset", cmdSetOffset },
    { "prog", cmdProgDevice },
    { "read", cmdReadDevice },
    { "size", cmdSetPromSize },
    { "?", cmdListCommands },
    { "", NULL }
};

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

void cmdDumpBuff()
{
    int addr;
    int c;
    char txtBuff[16];

    addr = 0;
    do
    {
        sprintf(txtBuff, "%04x   ", addr);
        Serial.print(txtBuff);

        for(c = 0; c < 16; c++)
        {
            sprintf(txtBuff, "%02x ", dataBuffer[addr + c]);
            Serial.print(txtBuff);
        }
        Serial.print("\n");

        addr = addr + 16;
    }
    while(addr < promSize);
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
        digitalWrite(CHIP_SEL ,HIGH);
        shiftOut(DATA_OUT, CLOCK, MSBFIRST, READ);
        shiftOut(DATA_OUT, CLOCK, MSBFIRST, addr);

        // Read 16 bits of data, eight at a time
        dataBuffer[addr] = shiftIn(DATA_IN, CLOCK, MSBFIRST);
        addr++;
        
        dataBuffer[addr] = shiftIn(DATA_IN, CLOCK, MSBFIRST);
        addr++;
   
        digitalWrite(CHIP_SEL, LOW);
    }
    while(addr < promSize);
}

// Set device size
void cmdSetPromSize()
{
    char txtbuff[16];
    char *endPtr;
    
    if(paramPtr == NULL)
    {
        sprintf(txtbuff, "0x%04x\n", promSize);
        Serial.print("Prom size: ");
        Serial.print(txtbuff);
    }
    else
    {
        promSize = strtol(paramPtr, &endPtr, 16);
    }
}

// Set load offset
void cmdSetOffset()
{
    char txtbuff[16];
    char *endPtr;
    
    if(paramPtr == NULL)
    {
        sprintf(txtbuff, "0x%04x\n", offset);
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
    Serial.print("Programme device\n");
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
    promSize = 256;
    memset(dataBuffer, 0xff, BUFF_SIZE);

    while(!Serial);

    Serial.print("Ed's PROM programmer\n");
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
