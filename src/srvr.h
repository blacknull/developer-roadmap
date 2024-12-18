/**
  ******************************************************************************
  * @file    srvr.h
  * @author  Waveshare Team
  * @version V2.0.0
  * @date    10-August-2018
  * @brief   ESP8266 WiFi server.
  *          This file provides firmware functions:
  *           + Sending web page of the tool to a client's browser
  *           + Uploading images from client part by part
  *
  ******************************************************************************
  */ 

/* Library includes ----------------------------------------------------------*/

#include <EEPROM.h>
#include <BluetoothSerial.h>

#include "basefunc.h"
CTimerMs timerPowerOn;

/* Eeprom acess --------------------------------------------------------------*/
struct CONFIG_TYPE
{
  uint8_t buf[128];     // reserve for other data want to keep in eeprom
  uint16_t idxImg;      // image index to be displayed  
  uint16_t countImg;
} config;

const int CONFIG_SIZE = 256;
void configLoad() {
    memset(&config, 0L, sizeof(config));
    EEPROM.begin(CONFIG_SIZE);
    uint8_t *p = (uint8_t*)(&config);
    for (int i = 0; i < sizeof(config); i++)
    {
        *(p + i) = EEPROM.read(i);
    }
    EEPROM.end();
}

void configSave() {
    uint8_t *p = (uint8_t*)(&config);
    EEPROM.begin(CONFIG_SIZE);
    for (int i = 0; i < sizeof(config); i++)
    {
        EEPROM.write(i, *(p + i));
    }
    EEPROM.end();
}


/* flash file system ---------------------------------------------------------*/
#include "FS.h"
#if defined(FS_SPIFFS)
    #ifdef ESP32
        #include "SPIFFS.h"
    #else
    #endif

    #define MyFs SPIFFS
#else
    #include <LittleFS.h>

    #define MyFs LittleFS
#endif

/* setup ini  ---------------------------------------------------------------*/
String BT_NAME = "ESP32_BT";
int MAX_IMG_COUNT = 32;
int EPAPER_TYPE = 34;
int SLEEP_TIME = 60 * 10;   // in seconds

void setupLoad() {
    File fileSetup = MyFs.open("/setup.ini", FILE_READ);
    if (!fileSetup) {
        Serial.println(" - failed to open setup.ini");
    }
    else {
        Serial.println(" - get setup.ini");

        String strLine = "";      
        do {
            strLine = fileSetup.readStringUntil('\n');

            if (strLine.startsWith("bt_name=")) {
                strLine.remove(0, strlen("bt_name="));
                BT_NAME = strLine;
                Serial.println("bt_name=" + BT_NAME);
            }
            else if (strLine.startsWith("epaper_type=")) {
                strLine.remove(0, strlen("epaper_type="));
                EPAPER_TYPE = strLine.toInt();
                Serial.println("epaper_type=" + String(EPAPER_TYPE));
            }
            else if (strLine.startsWith("max_photo=")) {
                strLine.remove(0, strlen("max_photo="));
                MAX_IMG_COUNT = strLine.toInt();
                Serial.println("max_photo=" + String(MAX_IMG_COUNT));
            }
            else if (strLine.startsWith("sleep_time=")) {
                strLine.remove(0, strlen("sleep_time="));
                SLEEP_TIME = strLine.toInt();
                Serial.println("sleep_time=" + String(SLEEP_TIME));
            }
        } while (strLine != "");

        fileSetup.close();
    }
}

/* image storage system -------------------------------------------*/
void storage_begin() {
    configLoad();   // load config from EEPROM to get idxImg;
    Serial.print("current idx: " + String(config.idxImg));
    Serial.println(", total images: " + String(config.countImg));

    if (config.idxImg == 0xffff || config.idxImg >= MAX_IMG_COUNT) {  // invalid num, reset to 0
        config.idxImg = 0;
        configSave();
    }

    // Print log message: initialization of storage
    Serial.print("<<<STORAGE");
    if (!MyFs.begin()) {
        Serial.println(" - failed!>>>");
        return;
    }
    else {
        Serial.println(" - OK!");
    }

    // read setup.ini
    setupLoad();

    // list img files
    int count = 0; String strExt = ".img";
    File root = MyFs.open("/");
    if (!root) {
        Serial.println("- failed to open root directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            continue;
        } else {
            String strName = file.name();
            if (strName.endsWith(strExt)) {
                Serial.println(file.name());
                count++;
            }            
        }

        file = root.openNextFile();
    }
    root.close();


    if (count <= 0) {
        Serial.println(" - no images found!>>>");
    }
    else {
        Serial.println(" - found " + String(count) + " images!>>>");
    }

    if (config.idxImg > count) {
        config.idxImg = 0;  // reset to 0
    }
    config.countImg = count;
    configSave();
}

File uploadImg;
void storage_open() {
    if (uploadImg) 
        uploadImg.close();
    
    char szImgName[256] = "";
    sprintf(szImgName, "/%03d.img", config.countImg);
    uploadImg = MyFs.open(szImgName, FILE_WRITE, true);
    if (!uploadImg) {
        Serial.println(" - failed to open file for writing: " + String(szImgName));
        return;
    }
    else {
        Serial.println(" - file opened for writing: " + String(szImgName));
    }
}

void storage_close() {
    if (uploadImg) {
        Serial.println(" - file closed: " + String(config.idxImg));
        uploadImg.close();

        config.idxImg = config.countImg;
        config.countImg++;
        config.countImg %= MAX_IMG_COUNT;
        configSave();
    }
}

 void storage_write(uint8_t *data, int len) {
    if (uploadImg) {
        Serial.print(" - writing " + String(len) + " bytes...");
        int n = uploadImg.write((const uint8_t*)data, len);
        if (n != len) 
            Serial.println(" - failed!");
        else 
            Serial.println(" - OK");
    }
}

int storage_read(uint8_t *data, int len) {
    if (uploadImg) {
        Serial.print(" - reading " + String(len) + " bytes...");
        int n = uploadImg.read((uint8_t*)data, len);
        if (n != len) 
            Serial.println(" - failed!");
        else 
            Serial.println(" - OK");
        return n;
    }
    return -1;  // error
}

/* BlueTooth service status ---------------------------------------------------*/
bool Srvr__btIsOn;// It's true when bluetooth is on
bool Srvr__btConn;// It's true when bluetooth has connected client 
int  Srvr__msgPos;// Position in buffer from where data is expected
int  Srvr__length;// Length of loaded data

/* Client ---------------------------------------------------------------------*/
BluetoothSerial Srvr__btClient; // Bluetooth client 

/* Avaialble bytes in a stream ------------------------------------------------*/
int Srvr__available()
{
    return Srvr__btIsOn ? Srvr__btClient.available() : false;
}

void Srvr__write(const char*value)
{
    // Write data to bluetooth
    if (Srvr__btIsOn) Srvr__btClient.write((const uint8_t*)value, strlen(value));
}

int Srvr__read()
{
    return Srvr__btIsOn ? Srvr__btClient.read() : -1;
}

void Srvr__flush()
{
    // Clear Bluetooth's stream
    if (Srvr__btIsOn) Srvr__btClient.flush();  
}

/* Project includes ----------------------------------------------------------*/
#include "buff.h"       // POST request data accumulator
#include "epd.h"        // e-Paper driver

bool Srvr__btSetup()                                              
{
    // Name shown in bluetooth device list of App part (PC or smartphone)
    String devName(BT_NAME);

    // Turning on
    Srvr__btIsOn = Srvr__btClient.begin(devName);

    // Show the connection result
    if (Srvr__btIsOn) Serial.println("Bluetooth is on");
    else Serial.println("Bluetooth is off");

    // There is no connection yet
    Srvr__btConn = false;

    // Return the connection result
    return Srvr__btIsOn;
}

/* The server state observation loop -------------------------------------------*/
bool Srvr__loop() 
{
    // Bluetooh connection checking
    if (!Srvr__btIsOn) return false;

    // Show and update the state if it was changed
    if (Srvr__btConn != Srvr__btClient.hasClient())
    {
        Serial.print("Bluetooth status:");
        Srvr__btConn = !Srvr__btConn;
        if(Srvr__btConn)
            Serial.println("connected"); 
        else
            Serial.println("disconnected"); 
    }

    // Exit if there is no bluetooth connection
    if (!Srvr__btConn) return false; 

    // Waiting the client is ready to send data
    while(!Srvr__btClient.available()) 
    {
        delay(1);
    }

    // Set buffer's index to zero
    // It means the buffer is empty initially
    Buff__bufInd = 0;

    // While the stream of 'client' has some data do...
    while (Srvr__available())
    {
        // Read a character from 'client'
        int q = Srvr__read();

        // Save it in the buffer and increment its index
        Buff__bufArr[Buff__bufInd++] = (byte)q;
    }
    Serial.println();

    // Initialization
    if (Buff__bufArr[0] == 'I')
    {
        Srvr__length = 0;

        // Getting of e-Paper's type
        EPD_dispIndex = Buff__bufArr[1];

        // Print log message: initialization of e-Paper (e-Paper's type)
        Serial.printf("<<<EPD %s", EPD_dispMass[EPD_dispIndex].title);


        // Initialization
        EPD_dispInit();

        // open img file
        storage_open();
        storage_write((uint8_t *)Buff__bufArr, Buff__bufInd);

        Buff__bufInd = 0;
        Srvr__flush();

        timerPowerOn.suspend();
    }

    // Loading of pixels' data
    else if (Buff__bufArr[0] == 'L')
    {
        // Print log message: image loading
        Serial.print("<<<LOAD");
        int dataSize = Buff__getWord(1);
        Srvr__length += dataSize;
                
        if ((Buff__bufInd < dataSize) || Srvr__length != Buff__getN3(3))
        {
            Buff__bufInd = 0;
            Srvr__flush();

            Serial.print(" - failed!>>>");
            Srvr__write("Error!");
            return true;
        }

        // serialize data
        storage_write((uint8_t *)Buff__bufArr, Buff__bufInd);

        // Load data into the e-Paper 
        // if there is loading function for current channel (black or red)
        if (EPD_dispLoad != 0) EPD_dispLoad();
        Serial.println("dispload dataSize = " + String(dataSize) + ", bufInd = " + String(Buff__bufInd));

        Buff__bufInd = 0;
        Srvr__flush();
    }

    // Initialize next channel
    else if (Buff__bufArr[0] == 'N')
    {
        // Print log message: next data channel
        Serial.print("<<<NEXT");

        // Instruction code for for writting data into 
        // e-Paper's memory
        int code = EPD_dispMass[EPD_dispIndex].next;
        if(EPD_dispIndex == 34)
        {
            if(flag == 0)
                code = 0x26;
            else
                code = 0x13;
        }

        // e-Paper '2.7' (index 8) needs inverting of image data bits
        EPD_invert = (EPD_dispIndex == 8);

        // If the instruction code isn't '-1', then...
        if (code != -1)
        {
            // Print log message: instruction code
            Serial.printf(" %d", code);

            // Do the selection of the next data channel
            EPD_SendCommand(code);
            delay(2);
        }

        // Setup the function for loading choosen channel's data
        EPD_dispLoad = EPD_dispMass[EPD_dispIndex].chRd;

        // serialize data
        storage_write((uint8_t *)Buff__bufArr, Buff__bufInd);

        Buff__bufInd = 0;
        Srvr__flush();
    }

    // Show loaded picture
    else if (Buff__bufArr[0] == 'S')
    {
        EPD_dispMass[EPD_dispIndex].show();

        // serialize data
        storage_write((uint8_t *)Buff__bufArr, Buff__bufInd);
        storage_close();
        
        Buff__bufInd = 0;
        Srvr__flush();

        //Print log message: show
        Serial.print("<<<SHOW");
        timerPowerOn.update();
    }

    // Send message "Ok!" to continue
    Srvr__write("Ok!");
    delay(1);

    // Print log message: the end of request processing
    Serial.print(">>>");
    return true;
}


/* load img data from storage file and show -----------------------------------*/
void storage_show() {
    char szImgFile[128] = "";
    sprintf(szImgFile, "/%03d.img", config.idxImg);

    File storageImg = MyFs.open(szImgFile, FILE_READ);
    if (!storageImg) {
        Serial.printf("Error to open: %s\n", szImgFile);
        return;
    }

    // Read the file into buffer
    do {
        // Read first char of packet from file
        int q = storageImg.read();

        // Save it in the buffer and increment its index
        Buff__bufInd = 0;
        Buff__bufArr[Buff__bufInd++] = (byte)q;

        // check packet
        if ('I' == q) {
            q = storageImg.read();
            Buff__bufArr[1] = q;

            // Getting of e-Paper's type
            EPD_dispIndex = q;
            Serial.printf("<<<EPD %s", EPD_dispMass[EPD_dispIndex].title);

            // Initialization
            EPD_dispInit();

            Buff__bufInd = 0;
        } else if ('L' == q) { // img data packet
            // Print log message: image loading
            Serial.print("<<<LOAD");

            uint16_t dataSize = 0;
            storageImg.read((uint8_t *)&dataSize, sizeof(uint16_t));
            Serial.println(" dataSize = " + String(dataSize));

            // Read data from file and save it in the buffer
            int n = storageImg.read((uint8_t *)(Buff__bufArr + 3), dataSize - 3);
            Buff__bufInd = dataSize;

            // Load data into the e-Paper
            // if there is loading function for current channel (black or red)
            if (EPD_dispLoad != 0)
                EPD_dispLoad();

            Buff__bufInd = 0;
        } else if ('N' == q) {
            // Print log message: next data channel
            Serial.print("<<<NEXT");

            // Instruction code for for writting data into
            // e-Paper's memory
            int code = EPD_dispMass[EPD_dispIndex].next;
            if (EPD_dispIndex == 34)
            {
                if (flag == 0)
                    code = 0x26;
                else
                    code = 0x13;
            }

            // e-Paper '2.7' (index 8) needs inverting of image data bits
            EPD_invert = (EPD_dispIndex == 8);

            // If the instruction code isn't '-1', then...
            if (code != -1)
            {
                // Print log message: instruction code
                Serial.printf(" %d", code);

                // Do the selection of the next data channel
                EPD_SendCommand(code);
                delay(2);
            }

            // Setup the function for loading choosen channel's data
            EPD_dispLoad = EPD_dispMass[EPD_dispIndex].chRd;

            Buff__bufInd = 0;
        } else if ('S' == q) {
            EPD_dispMass[EPD_dispIndex].show();
            Buff__bufInd = 0;

            // Print log message: show
            Serial.print("<<<SHOW");
        } else {
            Buff__bufInd = 0;

            // Print log message: unknown packet
            Serial.print("<<<UNK Data Packet.");
        }
    } while (storageImg.available());

    storageImg.close();

    Serial.println(">>>");
}

