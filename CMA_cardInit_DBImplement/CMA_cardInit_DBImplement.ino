#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include <EDB.h>


#define RST_PIN_rc522         48           // Configurable, see typical pin layout 
#define SS_PIN_rc522          53         // Configurable, see typical pin layout 

MFRC522 mfrc522(SS_PIN_rc522, RST_PIN_rc522);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;
byte readCard[4];
int readSuccessful;

/*
 *Display Segment 
 */
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 

/*
 * RTC Segment
 */
#define RTC_ADDR 0x68
DateTime now;
RTC_DS3231 rtc;


/*
 * SD Card Segment
 */
#define RST_PIN_SD            1
#define SS_PIN_SD             41
File myFile;
File xFile;

/*
 * Arduino Database
 */
#define TABLE_SIZE 10000
#define RECORDS_TO_CREATE 100
char* db_cards = "/db_cards.db";
char* db_usage = "/db_usagelog.db";

File dbFile;

// The read and write handlers for using the SD Library
inline void writer (unsigned long address, const byte* data, unsigned int recsize) {
    //digitalWrite(13, HIGH);
    dbFile.seek(address);
    dbFile.write(data,recsize);
    dbFile.flush();
    //digitalWrite(13, LOW);
}

inline void reader (unsigned long address, byte* data, unsigned int recsize) {
    //digitalWrite(13, HIGH);
    dbFile.seek(address);
    dbFile.read(data,recsize);
    //digitalWrite(13, LOW);
}

// Create an EDB object with the appropriate write and read handlers
EDB db(&writer, &reader);

struct UserData {
    int id;
    unsigned long uid;
}
userData;

//Function Declarations
unsigned long getID();  //Get ID of RFID card
int check_dbfiles();  //Check for database file (User data)
String get_datestring(); //Get the date as a string
String get_timestring(); //Get the time as a string
String get_dayofweekstr();  //Get the day of the week as a string
int search_id(unsigned long uidd);
void recordLimit(); //Display record limit size
void createRecord(int id, unsigned long uid); //Create UID Entry in databse (User Data)
void printAllRecords(); //Print All records
void printError(EDB_Status err); //Utility Function

int x=0;
void setup() {
    //Serial communications with PC
    Serial.begin(9600); // Initialize serial communications with the PC
    while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
    
    SPI.begin();        // Init SPI bus

    //MFRC522 Card Initialization
    mfrc522.PCD_Init(); // Init MFRC522 card

    // Prepare the key (used both as key A and as key B)
    // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    //SD Card Initialization
    if (!SD.begin(SS_PIN_SD)) {
    Serial.println("initialization failed!");
    while (1);
    }
    Serial.println("initialization DONE");

    //OLED Display Initialization
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
    }
    display.clearDisplay();

    //RTC Initialize
    if (! rtc.begin()){
      Serial.println("Couldn't find RTC Module");
      while (1);
  }

  //Database Initialize
  check_dbfiles(); //Check for database files
  recordLimit();
}



void loop() {
  
  
  if(mfrc522.PICC_IsNewCardPresent()) {
  unsigned long uid = getID();
  if(uid != -1){
    
    Serial.print("Card detected, UID: "); 
    Serial.println(uid,HEX);
    Serial.println(get_datestring());
    display.clearDisplay();
    display.setTextSize(4);             // Normal 1:1 pixel scale
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(0,0);
    display.println(uid,HEX);
    display.display();

    createRecord(x,uid);
    printAllRecords();
    x+=1;
    
    myFile = SD.open("test1.txt", FILE_WRITE);
    myFile.println(uid,HEX);
    myFile.close();
    myFile = SD.open("test1.txt");
    if (myFile) {
    Serial.println("test1.txt:");

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
    
      Serial.write(myFile.read());
      
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test1.txt");
  }
    }
  }




  

}


unsigned long getID(){
  if ( ! mfrc522.PICC_ReadCardSerial()) { 
    return -1;
  }
  unsigned long hex_num=0;
  hex_num += (unsigned long)mfrc522.uid.uidByte[0] << 24;
  //Serial.println(mfrc522.uid.uidByte[0],HEX);
  hex_num += (unsigned long)mfrc522.uid.uidByte[1] << 16;
  //Serial.println(mfrc522.uid.uidByte[1],HEX);
  hex_num += (unsigned long)mfrc522.uid.uidByte[2] <<  8;
  //Serial.println(mfrc522.uid.uidByte[2],HEX);
  hex_num += (unsigned long)mfrc522.uid.uidByte[3];
  //Serial.println(mfrc522.uid.uidByte[3],HEX);
  mfrc522.PICC_HaltA(); // Stop reading
  return hex_num;
}

int check_dbfiles(){
  if (SD.exists(db_cards)) {

        dbFile = SD.open(db_cards, FILE_WRITE);

        // Sometimes it wont open at first attempt, espessialy after cold start
        // Let's try one more time
        if (!dbFile) {
            dbFile = SD.open(db_cards, FILE_WRITE);
        }

        if (dbFile) {
            Serial.print("Openning current table... ");
            EDB_Status result = db.open(0);
            if (result == EDB_OK) {
                Serial.println("DONE");
            } else {
                Serial.println("ERROR");
                Serial.println("Did not find database in the file " + String(db_cards));
                Serial.print("Creating new table... ");
                db.create(1, TABLE_SIZE, (unsigned int)sizeof(userData));
                Serial.println("DONE");
                return;
            }
        } else {
            Serial.println("Could not open file " + String(db_cards));
            return;
        }
    } 
}

int search_id(unsigned long uidd){ //incomplete
  xFile = SD.open("test1.txt");
  int index=0;
  if (xFile) {
    // read from the file until there's nothing else in it:
    while (xFile.available()) {
      Serial.write(xFile.read());
    }
  }
    // close the file:
    xFile.close();
    return -1;
}

String get_datestring(){
  DateTime now = rtc.now();
  String date = String(now.day())+'/'+String(now.month())+'/'+String(now.year());
  return date;
}

void createRecord(int id, unsigned long uid)
{
    Serial.print("Creating Record... ");
    {
        userData.id = id;
        userData.uid = uid;
        Serial.print("UID to rec ");
        Serial.print(userData.uid,HEX);
        Serial.print(" ID: ");
        Serial.print(userData.id);
        EDB_Status result = db.appendRec(EDB_REC userData);
        if (result != EDB_OK) printError(result);
    }
    Serial.println("...DONE");
}

void printAllRecords(){
    for (int recno = 1; recno <= db.count(); recno++)
    {
        EDB_Status result = db.readRec(recno, EDB_REC userData);
        if (result == EDB_OK)
        {
            Serial.print("Recno: ");
            Serial.print(recno);
            Serial.print(" ID: ");
            Serial.print(userData.id);
            Serial.print(" RFID: ");
            Serial.println(userData.uid,HEX);
        }
        else printError(result);
    }
}

//utility function
void recordLimit()
{
    Serial.print("Record Limit: ");
    Serial.println(db.limit());
}

void printError(EDB_Status err){
    Serial.print("ERROR: ");
    switch (err)
    {
        case EDB_OUT_OF_RANGE:
            Serial.println("Recno out of range");
            break;
        case EDB_TABLE_FULL:
            Serial.println("Table full");
            break;
        case EDB_OK:
        default:
            Serial.println("OK");
            break;
    }
}
  
