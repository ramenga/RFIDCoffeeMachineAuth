#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include <EDB.h>
#include<Fonts/LatoLight.h>

#define C_PIN 13

#define DISP_TIMEOUT 7000

#define ADMIN_CRD_1 49 //Admin Card for viewing Logs
#define ADMIN_CRD_2 50 //ADmin Card 

#define RST_PIN_rc522         48           // Configurable, see typical pin layout 
#define SS_PIN_rc522          53         // Configurable, see typical pin layout 

int counter[100] = {0}; //Now supports a MAX of THIS MUCH of cards

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
unsigned long timepiece;
bool dispState = false;
bool lastDispState = false;
 

/*
 * RTC Segment
 */
#define RTC_ADDR 0x68
DateTime now;
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
char monthsOfYear[12][12] = {"January", "February", "March", "April", "May", "June", "July" , "August","September","October","November","December"};
//Clock Display when Idle
unsigned long clockcounter;
bool clockState = false;
bool lastClockState = false;

/*
 * SD Card Segment
 */
#define RST_PIN_SD            1
#define SS_PIN_SD             41
const char* card_ids = "/cards.txt";  //Read only for this program
const char* user_names = "/users.txt";  // Read only for this program
char* logs;
int users = 0; //To store number of users at starttime 
File zFile;
File xFile;
File usrFile;
File bt1File;
File countFile;
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
    //while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

    //Serial communications for Bluetooth Module
    Serial2.begin(9600);
    
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

  //Determine number of users in USERS.TXT file
  zFile = SD.open(card_ids,FILE_READ);
  if (zFile) {
    //Linear read from file, top to bottom
    //Serial.print("xFiles:");
    while (zFile.available()) {
      String z = zFile.readStringUntil('\n'); //A Line of file
      
      users+=1;
      
    }
  }
  zFile.close();
  Serial.print("No. of Users");
  Serial.println(users);

  //Pins for Output
  pinMode(C_PIN,OUTPUT);
  digitalWrite(C_PIN,LOW);

  
}

//Function Declarations
unsigned long getID();  //Get ID of RFID card
String get_datestring(); //Get the date as a string
String get_timestring(); //Get the time as a string
String get_dayofweekstr();  //Get the day of the week as a string
int search_id(unsigned long uidd);
String get_usrname(int index); //Display corresponding username from the matched index
void disp_swiped(int index); //Display info when swiping card
void log_swiped(unsigned long uidd, int index, bool flag); // Store usage log into SD card
void activate_machine(); //Activate the coffee machine
void disp_clock(); //Display clockface (current) 
void bluetooth_interface(); //Go into the bluetooth interface




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
    display.setTextSize(4);             
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(0,0);
    //display.println(uid,HEX);
    //display.display();
    unsigned long start,stopp;
    Serial.println("Into search fn");
    start = millis();
    int a = search_id(uid);
    
    if(a==50){ 
      bluetooth_interface(); //Admin Mode
      goto exits;
    }
    
    //display.println(a);
    //display.display();
    stopp = millis();
    Serial.print("Time taken:");
    Serial.println((stopp-start));
    Serial.print("Index:");
    Serial.println(a);
    Serial.println("Out of search fn");
    disp_swiped(a); //Display swiped card info
    dispState = true;
    log_swiped(uid,a,false);  // Log the swiped card to file
    activate_machine(); // Activate the Coffee Machine

    
  }
  }
  exits:

  if (dispState != lastDispState){
      timepiece = millis();
      lastDispState = dispState;
      Serial.print("Timepiece:");
      Serial.println(timepiece);
    }
  if (((millis() - timepiece) > DISP_TIMEOUT) && (dispState == true)  ) {
    //Turn off display after a certain timeout, when the card is swiped
    /*
    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        ledState = !ledState;
      }
    */

      
      display.clearDisplay();
      display.display();
      Serial.println("ClearDisp");
      lastDispState = false;
      dispState = false;
      
      //Now clock is safe to be displayed again
      clockState = true;
      display.setFont();
    }

    if ( clockState == true ){
      //Now you can update the clock
      
      disp_clock();
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

      char a[8];
      
      strcpy(a,buffert.c_str());
            
      unsigned long A = (unsigned long) strtoul(a,NULL,16);
          
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

void disp_swiped(int index){  ///SWIPED DISPLAY
  clockState = false; //Disable displaying of clock
  String usr_name = get_usrname(index);
  /*
  usrFile = SD.open(user_names);
  
  if (usrFile) {  //If the file exists
    //Linear read from file, top to bottom till 
    
    int i=0;
    for(i=0 ; usrFile.available() && i<=index ; i++) {
      usr_name = usrFile.readStringUntil('\n');
    }
  */
    display.clearDisplay();

    display.drawLine(0, 0, 128, 0, WHITE);//Top
    display.drawLine(0, 0, 0, 16, WHITE); //Left
    //display.drawLine(0, 63, 58, 63, WHITE); //Bottom
    //display.drawLine(68, 63, 127, 63, WHITE); //Bottom
    display.drawLine(127, 0, 127, 16, WHITE); //Right
    display.setFont(&Open_Sans_Light_15);
    display.setTextSize(0);
    display.setCursor(8,14);
    
    display.println("Welcome !!");
    display.setTextSize(1);
    display.print(usr_name);
    display.display();
}

String get_usrname(int index){
  usrFile = SD.open(user_names);
  String usr_name;
  if (usrFile) {  //If the file exists
    //Linear read from file, top to bottom till match is found
    
    int i=0;
    for(i=0 ; usrFile.available() && i<=index ; i++) {
      usr_name = usrFile.readStringUntil('\n');
    }
  usrFile.close();
  }
  usr_name.trim();
  return usr_name;
  
}

String get_datestring(){
  DateTime now = rtc.now();
  String date = String(now.day())+'/'+String(now.month())+'/'+String(now.year());
  return date;
}

String get_timestring(){
  DateTime now = rtc.now();
  String ampm =" AM";
  if(now.hour()/12 >= 1)
    ampm = " PM";
  String timestr = String(now.hour()%12)+':'+String(now.minute())+':'+String(now.second()+ampm);
  return timestr;
}
 
String get_dayofweekstr(){
  DateTime now = rtc.now();
  String dow = String(daysOfTheWeek[now.dayOfTheWeek()]);
  return dow;
}

void log_swiped(unsigned long uidd, int index, bool flag){
  DateTime now = rtc.now();
  String flg;
  if(flag){ // Flag for ACK feature in future
    flg = "1";
  }
  else{
    flg = "0";
  }
  
  String logfilename_rd;
  String logfilename_ur;
  if(now.month()/10 < 1){
      logfilename_rd = "R" + flg + String(now.year()) +"0"+ String(now.month())+".csv";
      logfilename_ur = "U" + flg + String(now.year()) +"0"+ String(now.month())+".csv";
  }
  else{
      logfilename_rd = "R" + flg + String(now.year()) + String(now.month())+".csv";
      logfilename_ur = "U" + flg + String(now.year()) + String(now.month())+".csv";
  }
  String uid_ = String(uidd,HEX);
  uid_.toUpperCase();
  //For Human Readable CSV
  File logFile_rd = SD.open(logfilename_rd, FILE_WRITE);
  String entry_rd = uid_ + ","+get_usrname(index)+","+get_datestring()+","+get_timestring();
  Serial.println(entry_rd);


  logFile_rd.println(entry_rd);
  logFile_rd.close();

  //For Machine Read Optimised CSV
  File logFile_ur = SD.open(logfilename_ur, FILE_WRITE);
  //String entry_ur = String(uidd,HEX)+","+get_usrname(index)+","+get_datestring()+","+get_timestring();
  //String(now.day())+'/'+String(now.month())+'/'+String(now.year());
  //String(now.hour())+':'+String(now.minute())+':'+String(now.second());
  logFile_ur.print(uid_);
  logFile_ur.print(",");
  logFile_ur.print(get_usrname(index));
  logFile_ur.print(",");
  logFile_ur.print(now.year());
  logFile_ur.print(",");
  logFile_ur.print(now.month());
  logFile_ur.print(",");
  logFile_ur.print(now.day());
  logFile_ur.print(",");
  logFile_ur.print(now.hour());
  logFile_ur.print(",");
  logFile_ur.print(now.minute());
  logFile_ur.print(",");
  logFile_ur.println(now.second());
  
  //logFile_ur.println(entry_ur);
  logFile_ur.close();
}

void activate_machine(){
  digitalWrite(C_PIN, HIGH);
  delay(1000);
  digitalWrite(C_PIN, LOW);
}

void disp_clock(){
  display.clearDisplay();
  
  //Draw Lines on edges
  display.drawLine(0, 0, 128, 0, WHITE);
  display.drawLine(0, 0, 0, 64, WHITE);
  display.drawLine(0, 63, 127, 63, WHITE);
  display.drawLine(127, 0, 127, 63, WHITE);
  
  //Draw DoW
  display.setCursor(3,5);
  display.setTextSize(0);   
  display.print("Today is ");
  display.print(get_dayofweekstr());
  display.setCursor(3,25);
  display.setTextSize(2);
  display.print(get_timestring());
  display.display();
}

void bluetooth_interface(){
  //Some graphics
  display.clearDisplay();
  Serial2.println("KNI Coffee Machine 2000");
  Serial2.println("Please state the nature of the Coffee Emergency");
  
  while(!Serial2.available()){
    //Wait till serial data is present
  }
  
  String ektar = Serial2.readString();
  //String portra = "201904"; //Internal testing purposes till next line
  //ektar = portra;
  
  String line;
  ektar.trim(); //Remove white spaces or \n
  if ( !SD.exists("U0"+ektar+".csv")){
    Serial2.println("DATA_DONT_EXIST");
    goto ends;
  }
  File bt1File = SD.open("U0"+ektar+".csv",FILE_READ);
  
  /*
  String countFileName = "L"+ektar;
  if (SD.exists(countFileName)){ //New file to be created every instance
    SD.remove(countFileName);
  }
  File countFile = SD.open(countFileName,FILE_WRITE);
  */
  
  //Reset counter variable
  memset(counter, 0, sizeof(counter));
  
  while (bt1File.available()) { //Counting of logs for each ID
      line = bt1File.readStringUntil('\n');
      unsigned long value;
      for (int i = 0; i < line.length(); i++) {
         if (line.substring(i, i+1) == ",") {
              //Convert the first delimited to ULong
              char a[8];
              strcpy(a,line.substring(0, i).c_str());
              value = (unsigned long) strtoul(a,NULL,16);
      
              //Serial.println(line.substring(0, i));
              //Serial.println(value,HEX);
              int index_ = search_id(value);
              counter[index_] += 1;
              //Serial.println(index_);
              //secondVal = line.substring(i+1)
              break;
         }
      }
      
      //Serial.println(line);
      
  }
  bt1File.close();

  String countFileName = "L"+ektar+".csv";
  if (SD.exists(countFileName)){ //New file to be created every instance, deleting old one
    SD.remove(countFileName);
  }
  countFile = SD.open(countFileName,FILE_WRITE);

  
  for(int z=0; z<users ; z++){
    //Serial.print(z);
    //Serial.print(":");
    //Serial.println(counter[z]);
    String entry_z = String(z) + "," +get_usrname(z) +"," + counter[z];
    //Serial.println(entry_z);
    countFile.println(entry_z);
  }
  countFile.close();
  countFile = SD.open(countFileName,FILE_READ);
  if (countFile) {
    //Linear read from file, top to bottom
    //Serial.print("xFiles:");
    Serial2.println("READY_OK");
    while (countFile.available()) {
      String z = countFile.readStringUntil('\n'); //A Line of file
      Serial2.println(z);
    }
  }
  countFile.close();
  ends: 
  Serial.println("BT mOde Exit"); 
  // close the file:
  
}
