/*
 * 
 * 150 Ohm resistor between MISO of SD Module and RC522 
 */

#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>


#define RST_PIN_rc522         48           // Configurable, see typical pin layout 
#define SS_PIN_rc522          53         // Configurable, see typical pin layout 

MFRC522 mfrc522(SS_PIN_rc522, RST_PIN_rc522);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;
byte readCard[4];
int readSuccessful;

/*
 *Display Segment 
 */
 

/*
 * RTC Segment
 */

/*
 * SD Card Segment
 */
#define RST_PIN_SD            1
#define SS_PIN_SD             41
File myFile;

void setup() {
  // put your setup code here, to run once:
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
    
}

void loop() {
  
  /*
  do{
    readSuccessful = getUID;
  } while (!readSuccessful);

  for(int i = 0; i<mfrc522.uid.size; i++){
    
  }
  */
  
  if(mfrc522.PICC_IsNewCardPresent()) {
  unsigned long uid = getID();
  if(uid != -1){
    Serial.print("Card detected, UID: "); 
    Serial.println(uid,HEX);
    myFile = SD.open("test.txt", FILE_WRITE);
    myFile.println(uid,HEX);
    myFile.close();
    myFile = SD.open("test.txt");
    if (myFile) {
    Serial.println("test.txt:");

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
    }
  }




  

}


unsigned long getID(){
  if ( ! mfrc522.PICC_ReadCardSerial()) { 
    return -1;
  }
  unsigned long hex_num;
  hex_num =  mfrc522.uid.uidByte[0] << 24;
  hex_num += mfrc522.uid.uidByte[1] << 16;
  hex_num += mfrc522.uid.uidByte[2] <<  8;
  hex_num += mfrc522.uid.uidByte[3];
  mfrc522.PICC_HaltA(); // Stop reading
  return hex_num;
}
  
