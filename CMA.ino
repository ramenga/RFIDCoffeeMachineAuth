/**
*Coffee Machine Authenticator
*Uses multiple components
*Needs bugfix for Nonstored card since it will default to highest count, needs immediate action
*bugfix has beeen implemented but not tested
**/

#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include <EDB.h>
#include<Fonts/LatoLight.h>
#include "rqeLogo.h"

#define C_PIN 13

#define DISP_TIMEOUT 7000

#define ADMIN_CRD_1 49 //Admin Card for viewing Logs
#define ADMIN_CRD_2 50 //Admin Card 2

#define RST_PIN_rc522         48           // Configurable, see typical pin layout 
#define SS_PIN_rc522          53         // Configurable, see typical pin layout 

int counter[200] = {0}; //Now supports a MAX of THIS MUCH of cards. Change according to requirements.

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
unsigned long ssTimeKeeper = 0;
bool dispState = false;
bool lastDispState = false;
//Vars for display animations
byte aVar1 = 0;
byte aVar2 = 0;
byte aVar3 = 0;
bool aBool1 = false;
bool logoScroll = true;
byte scrollMode = 0;
 

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
bool initialClock = true;
const long initialTimeout = 40000;

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

/**
 * Bluetooth Interface
 * Uses serial2 of MEGA, configurable
 */
const byte statePin = 47;
const byte enPin = 45;  

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

  //Bluetooth Pins
  pinMode(enPin,OUTPUT);
  pinMode(statePin,INPUT);
  digitalWrite(enPin,LOW);

  //Pins for Coffee Machine Activate pin
  pinMode(C_PIN,OUTPUT);
  digitalWrite(C_PIN,LOW);

  
}

//Function Declarations
unsigned long getID();  //Get ID of RFID card
String append_zero(int num); //Add zeros before single digit numbers and return as string
String get_datestring(); //Get the date as a string
String get_timestring(); //Get the time as a string
String get_dayofweekstr();  //Get the day of the week as a string
String get_ampm();  //Get AM or PM for 12 Hour format
int search_id(unsigned long uidd);
String get_usrname(int index); //Display corresponding username from the matched index
void disp_swiped(int index); //Display info when swiping card
void log_swiped(unsigned long uidd, int index, bool flag); // Store usage log into SD card
void activate_machine(); //Activate the coffee machine
void disp_clock(); //Display clockface (current) 
bool bluetooth_check(); //Check if BT connection is present
void bluetooth_interface(); //Go into the bluetooth interface
void draw_edges(); //Draw lines on edges of display
void display_bluetooth(bool connection, bool command, bool computing); //Display bluetooth screens in Admin Mode
void display_settime(bool beforeSet); //Setting time through BT interface




void loop() {  
  

  if(initialClock && millis()>initialTimeout){
    initialClock = false;
    clockState = true;
  }
  
  if(mfrc522.PICC_IsNewCardPresent()) {
  unsigned long uid = getID();
  if(uid != -1){
    
    Serial.print("Card detected, UID: "); 
    Serial.println(uid,HEX);
    Serial.println(get_datestring());
    Serial.println(get_dayofweekstr());
    Serial.println(get_timestring());
    
    unsigned long start,stopp;
    Serial.println("Into search fn");
    start = millis();
    int a = search_id(uid);
    
    if(a == ADMIN_CRD_1 || a == ADMIN_CRD_2){ 
      bluetooth_interface(); //Admin Mode
      Serial.println("BT interface Exit");
      goto exits;
    }
    
    if(a == -1){
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
  //Serial.println("EXITS");

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
      ssTimeKeeper+=1;
      initialClock = false;
      if(ssTimeKeeper%200<130){
        //Something
        disp_clock();
        logoScroll = true;
      }else{
        display_scrollinglogo();
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
  bool matched_any = false;
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
        matched_any = true;
        break;
      }
      index+=1;
      //Serial.print("--------------------------");
    }
    
    
    // close the file:
    xFile.close();
    
    //Return index of the match
    if (matched_any){
        return index;
    }
    else
        return -1;
  }
}

void disp_swiped(int index){  ///SWIPED DISPLAY
  clockState = false; //Disable displaying of clock
  String usr_name = get_usrname(index);

    display.clearDisplay();
    display.stopscroll();            
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(0,0);

    display.drawLine(0, 0, 128, 0, WHITE);//Top Line
    display.drawLine(0, 0, 0, 6, WHITE); //Top Left
    display.drawLine(127, 0, 127, 6, WHITE); //Top Right
    display.drawLine(0, 63, 127, 63, WHITE);//Bottom Line
    display.drawLine(127, 57, 127, 63, WHITE);//Bottom Right
    display.drawLine(0, 57, 0, 63, WHITE);//Bottom Left
    display.setFont(&Open_Sans_Light_15);
    display.setTextSize(0);
    display.setCursor(20,14);
    
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
  String date = append_zero(now.day())+'-'+String(monthsOfYear[now.month()-1])+'-'+String(now.year());
  //String date = append_zero(now.day())+'-'+String(now.month())+'-'+String(now.year());
  return date;
}

String get_timestring(){
  DateTime now = rtc.now();
  String timestr = String(now.hour()%12)+':'+append_zero(now.minute())+':'+append_zero(now.second());
  return timestr;
}
 
String get_dayofweekstr(){
  DateTime now = rtc.now();
  String dow = String(daysOfTheWeek[now.dayOfTheWeek()]);
  return dow;
}

String get_ampm(){
  DateTime now = rtc.now();
  String ampm ="AM";
  if(now.hour()/12 >= 1)
    ampm = "PM";
  return ampm;
}

String append_zero(int num){
  if(num<10){
    return "0"+String(num);
  }
  else
    return String(num);
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
  display.stopscroll();
  display.clearDisplay();
  display.setTextColor(WHITE); 
  //Draw Lines on edges
  draw_edges();
  
  //Draw Watch (not wall clock) Face
  //Display Date and DoW
  display.setFont();
  display.setCursor(3,5);
  display.setTextSize(0);   
  display.print("Today is ");
  display.print(get_dayofweekstr());
  display.setCursor(9,50);
  display.print(get_datestring());
  
  //Display Time
  display.setCursor(5,25);
  //display.setFont(&Open_Sans_Light_15);
  display.setTextSize(2);
  display.print(get_timestring());
  display.setTextSize(0);
  display.setFont(&Open_Sans_Light_15);
  display.setCursor(102,38);
  display.print(get_ampm());
  display.display();

}

bool bluetooth_check(){
  if(digitalRead(statePin)){
    return true;
  }
  return false;
}

void bluetooth_interface(){
  //Some graphics
  display.stopscroll();
  display.clearDisplay();
  
  Serial2.println("KNI Coffee Machine 2000");
  Serial2.println("Please state the nature of the Coffee Emergency");

  //Put some connection checking condition here
  beginn:
  while(!bluetooth_check()){
    //Wait till bluetooth connection is available
    Serial.println("BT not available");
    display_bluetooth(true, false, false);
  }
  Serial.println("BT check PASS");
  
  Serial2.println("READY_OK");
  
  while(!Serial2.available()){
    //Wait till serial data is present
    display_bluetooth(false, true, false);
    if(!bluetooth_check()){
      goto beginn;
    }
  }
  
  String ektar = Serial2.readString();
  ektar.trim(); //Remove white spaces or '\n'
  //String portra = "201904"; //Internal testing purposes till next line
  //ektar = portra;
  
  if(ektar.equals("SETTIME")){ //Time setting cases
    Serial.println("Setting the time");
    Serial2.println("Enter date & time in DDMMYYYYHHMM format");
    //Setting time interface here
    display_settime(true);
    while(!Serial2.available()){
      //Wait till serial data is present
    }
    String ektar = Serial2.readString();
    ektar.trim();
    char dates[8];
    char times[4];
    Serial.println("Substring:"+ektar.substring(8, 12));
    strcpy(dates,ektar.substring(0, 8).c_str());
    strcpy(times,ektar.substring(8, 12).c_str());
    unsigned long values = (unsigned long) strtoul(dates,NULL,10);
    unsigned long values_1 = (unsigned long) strtoul(times,NULL,10);
    int date = values/1000000;
    values = values%1000000;
    int months = values/10000;
    int years = values%10000;
    int hours = values_1/100;
    int minutes = values_1%100;
    Serial.println("date:"+String(date));
    Serial.println("months:"+String(months));
    Serial.println("year:"+String(years));
    Serial.println("hour:"+String(hours));
    Serial.println("min:"+String(minutes));
    //Set the noew time into RTC module
    rtc.adjust(DateTime(years, months, date, hours, minutes, 0));
    display_settime(false);
    goto ends;
    
  }
  String line;
  
  if ( !SD.exists("U0"+ektar+".csv")){
    Serial2.println("DATA_DONT_EXIST");
    goto ends;
  }
  display_bluetooth(false, false, true);
  File bt1File = SD.open("U0"+ektar+".csv",FILE_READ);
    
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
  if (SD.exists(countFileName)){ //New file to be created every instance
    SD.remove(countFileName);
  }
  countFile = SD.open(countFileName,FILE_WRITE);

  
  for(int z=0; z<users ; z++){
    //Serial.print(z);
    //Serial.print(":");
    //Serial.println(counter[z]);
    String entry_z = String(z) + "," + get_usrname(z) +"," + counter[z];
    //Serial.println(entry_z);
    countFile.println(entry_z);
  }
  countFile.close();
  countFile = SD.open(countFileName,FILE_READ);
  if (countFile) {
    //Linear read from file, top to bottom
    //Serial.print("xFiles:");
    Serial2.print("USAGELOGS,");
    Serial2.println(users);
    while (countFile.available()) {
      String z = countFile.readStringUntil('\n'); //A Line of file
      Serial2.println(z);
    }
  }
  countFile.close();
  
  ends:
  Serial.println("BT mOde Exit");
  display.clearDisplay(); 
  Serial.println("BT mOde Exit");
  display.display();
  Serial.println("BT mOde Exit");
  
}

void draw_edges(){
  display.drawLine(0, 0, 128, 0, WHITE);
  display.drawLine(0, 0, 0, 64, WHITE);
  display.drawLine(0, 63, 127, 63, WHITE);
  display.drawLine(127, 0, 127, 63, WHITE);
}

void display_bluetooth(bool connection, bool command, bool computing){
  //Use aVar1,aVar2,aVar3... for animations
  display.setTextColor(WHITE); 
  display.stopscroll();
  
  if(connection){ 
    //When BT connection is not established
    display.clearDisplay();
    draw_edges();
    display.setFont(&Open_Sans_Light_15);
    display.setCursor(5,14);
    display.setTextSize(0); 
    display.print("Bluetooth not ");
    display.setCursor(5,28);
    display.print("connected.");
    display.setCursor(5,47);
    display.print("WAITING .....");
    display.display();
    aVar1++;
    if(aBool1==false){
      display.startscrolldiagright(0x00, 0x07);
      aBool1=true;
    }
    
  }

  if(command){
    //Serial command not yet received, awaiting orders 
    display.clearDisplay();
    display.stopscroll();
    aBool1=false;
    draw_edges();
    display.drawLine(22, 0, 0, 14, WHITE);
    display.drawLine(106, 0, 128, 14, WHITE);
    display.setFont(&Open_Sans_Light_15);
    display.setCursor(24,14);
    display.setTextSize(0); 
    display.print("Bluetooth");
    display.setCursor(24,28);
    display.print("connected.");
    display.setCursor(3,47);
    display.print("Await Command");
    
    if(aVar1>=128){
      aVar1=0;
    }
    if(aVar2>=120){
      aVar2=0;
    }
    if(aVar3>=120){
      aVar3=0;
    }
    aVar1++;
    aVar2+=3;
    aVar3+=5;
    display.drawLine(aVar1, 60, aVar1+5, 60, WHITE);
    display.drawLine(aVar2, 58, aVar2+5, 58, WHITE);
    display.drawLine(aVar3, 56, aVar3+7, 56, WHITE);
    
    display.display();
  }

  if(computing){
    //Currently computing answers
    display.clearDisplay();
    display.stopscroll();
    aBool1=false;
    draw_edges();
    display.setFont(&Open_Sans_Light_15);
    display.setCursor(5,14);
    display.setTextSize(0); 
    display.print("Computing .....");
    display.display();
  }
}

void display_settime(bool beforeSet){
  //Screens for Date/Time setting through BT interface
  display.setTextColor(WHITE); 
  
  if(beforeSet){ 
    display.clearDisplay();
    display.stopscroll();
    
    draw_edges();
    display.drawLine(20, 0, 0, 14, WHITE);
    display.drawLine(108, 0, 128, 14, WHITE);
    display.setFont(&Open_Sans_Light_15);
    display.setCursor(18,14);
    display.setTextSize(0); 
    display.print("Set the Time");
    display.setCursor(24,28);
    display.print("in Format:");
    display.setFont();
    display.setTextSize(2);
    display.setCursor(3,31);
    display.print("DDMMYYYY");
    display.setCursor(3,47);
    display.print("HHMM");
    display.display();
    
  }else{
    //Display set time for a brief period
    display.clearDisplay();
    //Draw Lines on edges
    draw_edges();
  
    
    //Display NEW Date
    display.setFont();
    display.setCursor(3,5);
    display.setTextSize(0);   
    display.print("The Date/Time is set to ");
    display.setCursor(9,50);
    display.print(get_datestring());
  
    //Display NEW Time
    display.setCursor(5,25);
    //display.setFont(&Open_Sans_Light_15);
    display.setTextSize(2);
    display.print(get_timestring());
    display.setTextSize(0);
    display.setFont(&Open_Sans_Light_15);
    display.setCursor(102,38);
    display.print(get_ampm());
    display.display();
    delay(10000);
  }

}

void display_scrollinglogo(){
  if(logoScroll){
    display.clearDisplay();
    draw_edges();
    display.drawBitmap(0, 0,  rqe_128_64, 128, 64, 1);
    
    display.display();
    logoScroll = false;
    switch(scrollMode){
      case 0:
        display.startscrolldiagleft(0x00, 0x07);
        scrollMode=1;
        break;
      case 1:
        display.startscrolldiagright(0x07, 0x00);
        scrollMode=2;
        break;
      case 2:
        display.startscrollleft(0x00, 0x0F);
        scrollMode=3;
        break;
      case 3:
        display.startscrollright(0x00, 0x0F);
        scrollMode=0;
        break;
    }
  }
    //delay(20000);
    //display.stopscroll();
    //display.clearDisplay();
}
