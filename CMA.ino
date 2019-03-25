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
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


/*
 * SD Card Segment
 */
#define RST_PIN_SD            1
#define SS_PIN_SD             41
char* card_ids = "/cards.txt"; //Read only for this program
char* user_names = "/users.txt";
char* logs = "/usage_logs.csv";
File myFile;
File xFile;
String buffert;
String uid_str;

/*
 * Arduino Database
 */
/*
#define TABLE_SIZE 8192
#define RECORDS_TO_CREATE 55
char* db_name = "/db/db_cards.db";
File dbFile;
*/


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
    display.display(); //Display default logo

    //RTC Initialize
    if (! rtc.begin()){
      Serial.println("Couldn't find RTC Module");
      while (1);
  }
}

//Function Declarations
unsigned long getID();  //Get ID of RFID card
String get_datestring(); //Get the date as a string
String get_timestring(); //Get the time as a string
String get_dayofweekstr();  //Get the day of the week as a string
int search_id(unsigned long uidd);



void loop() {
  
  
  if(mfrc522.PICC_IsNewCardPresent()) {
  unsigned long uid = getID();
  if(uid != -1){
    
    Serial.print("Card detected, UID: "); 
    Serial.println(uid,HEX);
    Serial.println(get_datestring());
    Serial.println(get_dayofweekstr());
    Serial.println(get_timestring());
    display.clearDisplay();
    display.setTextSize(4);             // Normal 1:1 pixel scale
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(0,0);
    display.println(uid,HEX);
    display.display();
    unsigned long start,stopp;
    Serial.println("Into search fn");
    start = millis();
    int a = search_id(uid);
    stopp = millis();
    Serial.print("Time taken:");
    Serial.println((stopp-start));
    Serial.print("Index:");
    Serial.println(a);
    Serial.println("Out of search fn");
    
    }
  }

}


unsigned long getID(){
  if ( ! mfrc522.PICC_ReadCardSerial()) { 
    return -1;
  }
  unsigned long hex_num=0;
  hex_num += (unsigned long)mfrc522.uid.uidByte[0] << 24;
  hex_num += (unsigned long)mfrc522.uid.uidByte[1] << 16;
  hex_num += (unsigned long)mfrc522.uid.uidByte[2] <<  8;
  hex_num += (unsigned long)mfrc522.uid.uidByte[3];
  mfrc522.PICC_HaltA(); // Stop reading
  return hex_num;
}

int search_id(unsigned long uidd){
  xFile = SD.open(card_ids);
  int index=0;
  if (xFile) {
    //Linear read from file, top to bottom
    //Serial.print("xFiles:");
    
    int x=0;
    while (xFile.available()) {
 
      
      buffert = xFile.readStringUntil('\n'); //A Line of file
      
      uid_str = String(uidd,HEX);
      uid_str.toUpperCase();
      buffert.toUpperCase();
      
      //Serial.println(buffert);
      
      //a is input from file
      //b is input from card(active)
      char a[8];
      //char b[8];
      strcpy(a,buffert.c_str());
      //strcpy(b,uid_str.c_str());
      
      unsigned long A = (unsigned long) strtoul(a,NULL,16);
      
      /*
      Serial.print("buffert:");
      Serial.println(buffert);
      Serial.print("uidstr:");
      Serial.println(uid_str);
      Serial.print("A:");
      Serial.println(A,HEX);
      */
      
      if(A==uidd){
        //Serial.println("MATCHES");
        break;
      }
      index+=1;
      //Serial.print("--------------------------");
    }
    
    
    // close the file:
    xFile.close();
    
    //Return index of the match
    return index;
  }
}

String get_datestring(){
  DateTime now = rtc.now();
  String date = String(now.day())+'/'+String(now.month())+'/'+String(now.year());
  return date;
}

String get_timestring(){
  DateTime now = rtc.now();
  String timestr = String(now.hour())+':'+String(now.minute())+':'+String(now.second());
  return timestr;
}

String get_dayofweekstr(){
  DateTime now = rtc.now();
  String dow = String(daysOfTheWeek[now.dayOfTheWeek()]);
  return dow;
}
