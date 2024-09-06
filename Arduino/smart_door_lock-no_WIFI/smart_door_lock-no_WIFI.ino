
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>     
#include <SPI.h>       
#include <MFRC522.h> 
#include <ESP32Servo.h> 
#include <WS2812FX.h>
#include <Keypad.h>
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <string.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "Dat"
#define WIFI_PASSWORD "80888088"

// Insert Firebase project API Key
#define API_KEY "AIzaSyBv1JoIg_OF4lb5QOFOBp2yJVyYtNfW6Os"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://rfid-key-default-rtdb.asia-southeast1.firebasedatabase.app/" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

#define LED_COUNT 1
#define LED_RGB 13 
#define BUZZER 5
#define DOOR_BUTTON 4
#define LOCK 15

LiquidCrystal_I2C lcd(0x27,16,2);
Servo myservo;


boolean match = false;   
boolean programMode = false;  
uint8_t successRead;

byte storedCard[4];  
byte readCard[4];
byte masterCard[4]; 
constexpr uint8_t RST_PIN = 27;    
constexpr uint8_t SS_PIN = 14; 

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1','4','7','*'},
  {'2','5','8','0'},
  {'3','6','9','#'},
  {'A','B','C','D'}
};
byte colPins[COLS] = {26, 25, 33, 32}; //connect to the row pinouts of the keypad
byte rowPins[ROWS] = {35, 34 , 12,36 }; //connect to the column pinouts of the keypad

//initialize an instance of class NewKeypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

// Passwords
char password1[] = "*101#";
char password2[] = "ABCD#";
// Biến cờ để theo dõi quá trình nhập mật khẩu
bool isEnteringPassword = false;

// Biến để lưu trữ mật khẩu đã nhập
char enteredPassword[5];
int count = 0;

int pos = 0;
int tickw;
char cardUID[9];
char mastercardUID[9];

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_RGB, NEO_GRB + NEO_KHZ800);
MFRC522 mfrc522(SS_PIN, RST_PIN);


void ShowReaderDetails() {
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 software version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown), probably a Chinese clone?"));//(không xác định), có lẽ là một bản sao Trung Quốc?
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("ALERT: Communication failure, is the MFRC522 module connected correctly?"));//CẢNH BÁO: Lỗi giao tiếp, mô-đun MFRC522 đã được kết nối đúng cách chưa?
    Serial.println(F("SYSTEM ABORTED: Check the connections"));//HỆ THỐNG ĐÃ HỦY BỎ: Kiểm tra kết nối
    // Visualize system is halted
    // digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
    // digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
    // digitalWrite(redLed, LED_ON);   // Turn on red LED
    while (true); // do not go further
  }
}

void Wipe_code(){
    if (digitalRead(DOOR_BUTTON) == LOW) {  // when button pressed pin should get low, button connected to ground
    //digitalWrite(redLed, LED_ON); // Red Led stays on to inform user we are going to wipe
    Serial.println(F("Format button pressed"));
    Serial.println(F("You have 10 seconds to cancel"));//Bạn có 10 giây để hủy bỏ
    Serial.println(F("This will erase all your records, and there's no way to undo it"));//Điều này sẽ xóa tất cả các bản ghi của bạn, và không có cách nào để hoàn tác
    bool buttonState = monitorWipeButton(10000); // Give user enough time to cancel operation
    if (buttonState == true && digitalRead(DOOR_BUTTON) == LOW) {    // If button still be pressed, wipe EEPROM
      Serial.println(F("EEPROM formatting started"));//Bắt đầu định dạng EEPROM
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
        if (EEPROM.read(x) == 0) {              //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          EEPROM.write(x, 0);       // if not write 0 to clear, it takes 3.3mS
        }
      }
      Serial.println(F("EEPROM formatted successfully"));//EEPROM được định dạng thành công

    }
    else {
      Serial.println(F("Formatacao cancelada")); // Show some feedback that the wipe button did not pressed for 15 seconds
      // digitalWrite(redLed, LED_OFF);
    }
  }

  if (EEPROM.read(1) != 143) {
    Serial.println(F("Master card not set"));//Thẻ Master chưa được đặt
    // lcd.setCursor(0,0);
    // lcd.print("Master card not set!");
    Serial.println(F("Read a chip to set the Master card"));//Đọc một chip để đặt thẻ Master
    lcd.setCursor(0,0);
    lcd.print("Chip read to set");
    lcd.setCursor(0,1);
    lcd.print("The MASTER CARD");
    do {
      successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    lcd.clear();
    Serial.println(F("Master card set"));//Thẻ Master đã được đặt
    lcd.setCursor(0,0);
    lcd.print("UID:");
    for ( uint8_t i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
      masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
      lcd.setCursor(4 + i * 2, 0); 
      if (masterCard[i] < 0x10) {
        lcd.print("0");
      }
      lcd.print(masterCard[i], HEX);
    }
    sprintf(mastercardUID, "%02X%02X%02X%02X", masterCard[0], masterCard[1], masterCard[2], masterCard[3]);
    if (Firebase.RTDB.setString(&fbdo, "rfid/master/cardUID", mastercardUID)) {
      Serial.println("Card UID sent to Firebase successfully");
    } else {
      Serial.println("Failed to send Card UID to Firebase");
      Serial.println("Reason: " + fbdo.errorReason());
    }
    lcd.setCursor(0,1);
    lcd.print("[OK] Master card set");
    digitalWrite(BUZZER, HIGH);
    ws2812fx.setPixelColor(0, PINK);
    ws2812fx.show();
    delay(100);
    digitalWrite(BUZZER, LOW);
    ws2812fx.setPixelColor(0, 0);
    ws2812fx.show();
    delay(50);
    digitalWrite(BUZZER, HIGH);
    ws2812fx.setPixelColor(0, PINK);
    ws2812fx.show();
    delay(100);
    digitalWrite(BUZZER, LOW);
    ws2812fx.setPixelColor(0, 0);
    ws2812fx.show();
    delay(3000);
    lcd.clear();
  }

  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything is ready"));
  Serial.println(F("Waiting for the chips to be read"));
  EEPROM.commit();
}

void intro_connecting_wifi() {
  const char* connectingText[5] = {"    ","    ", ".", "..", "..."};
  lcd.setCursor(0, 0);
  lcd.print("CONNECTING WIFI");
  lcd.setCursor(0, 1);
  lcd.print(WIFI_SSID);
  int ssidLength = strlen(WIFI_SSID);
  int startPosition = min(15, ssidLength);
  lcd.setCursor(startPosition, 1);
  lcd.print(connectingText[tickw]);
  if (tickw == 4){
    tickw = 0;
  }
}

void connect_WIFI(){
  unsigned long previousMillis = 0;
  const unsigned long interval = 250;
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("");
  Serial.print("Connecting to WIFI: ");

  while (WiFi.status() != WL_CONNECTED) { //
    unsigned long currentMillis_connecting = millis();

    if (currentMillis_connecting - previousMillis >= interval) {
      previousMillis = currentMillis_connecting;
      //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
      tickw++;
      intro_connecting_wifi();
      Serial.print(".");
    }

  }

  digitalWrite(LED_BUILTIN, 1);
  Serial.println();

  delay(1000);
  lcd.clear();


}

void config_FIREBASE(){
  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  //if (Firebase.signUp(&config, &auth, "iot.rfidkey@gmail.com", "iotgroup1")){
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}



////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(9600);
  EEPROM.begin(1024);
  pinMode(DOOR_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, 0);
  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);
  myservo.attach(LOCK, 500, 2600); 
  
  ws2812fx.init();
  ws2812fx.setBrightness(30);
  lcd.begin();
  SPI.begin();       
  mfrc522.PCD_Init(); 
  ShowReaderDetails();
  //connect_WIFI();
  //config_FIREBASE();
  Wipe_code();
}




void RFID_read(){
  do {
    if(!programMode && !isEnteringPassword){
      lcd.setCursor(3, 0);
      lcd.print("RFID Door");
      lcd.setCursor(1, 1);
      lcd.print("Locking System");
    }

    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0
    checkPassWord();
    //getRTDB();


    // When device is in use if wipe button pressed for 10 seconds initialize Master Card wiping
    if (digitalRead(DOOR_BUTTON) == LOW) { // Check if button is pressed
      // Give some feedback
      Serial.println(F("Format button pressed"));
      Serial.println(F("The Master card will be erased! in 10 seconds"));//Thẻ Master sẽ ancel operation
      bool buttonState = monitorWipeButton(10000); // Give user enough time to c
      if (buttonState == true && digitalRead(DOOR_BUTTON) == LOW) {    // If button still be pressed, wipe EEPROMbị xóa! trong 10 giây
      
        EEPROM.write(1, 0);                  // Reset Magic Number.
        EEPROM.commit();

        //digitalWrite(BUZZER, LOW);

        Serial.println(F("Master card unlinked from the device"));
        Serial.println(F("Press the board reset to reprogram the Master card"));
        digitalWrite(BUZZER, HIGH);
        delay(70);
        digitalWrite(BUZZER, LOW);
        delay(40);
        digitalWrite(BUZZER, HIGH);
        delay(70);
        digitalWrite(BUZZER, LOW); 
        lcd.clear();
        while (1){
          lcd.setCursor(1,0);
          lcd.print("DELETE MASTER");
          lcd.setCursor(0,1);
          lcd.print("Please RESET!...");
        }

      }
      Serial.println(F("Unlinking of the Master card canceled"));//Hủy liên kết thẻ Master
    }
    if (programMode) {
      cycleLeds();              // Program Mode cycles through Red Green Blue waiting to read a new card
    }
    else {
      normalModeOn();     // Normal mode, blue Power LED is on, all others are off
    }
  }
  while (!successRead);   //the program will not go further while you are not getting a successful read
  if (programMode) {
    if ( isMaster(readCard) ) { //When in program mode check First If master card scanned again to exit program mode
      ws2812fx.setPixelColor(0, 0); // Thiết lập màu xanh lam cho LED 3, 4, 5
      ws2812fx.show();
      // if (Firebase.RTDB.setString(&fbdo, "rfid/master/card", "false")) {
      //   Serial.println("Card UID sent to Firebase successfully");
      // } 
      // else {
      //   Serial.println("Failed to send Card UID to Firebase");
      //   Serial.println("Reason: " + fbdo.errorReason());
      // }
      lcd.setCursor(0, 1);
      lcd.print("  Exiting ...   ");
      Serial.println(F("Reading the Master card"));//Đọc thẻ Master
      Serial.println(F("Exiting programming mode"));//Thoát khỏi chế độ lập trình
      Serial.println(F("-----------------------------"));
      digitalWrite(BUZZER, HIGH);
      delay(100);
      digitalWrite(BUZZER, LOW);
      delay(100);
      programMode = false;
      lcd.clear();
      return;
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        Serial.println(F("I know this chip, removing..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Read a chip to add or remove from the EEPROM"));
      }
      else {                    // If scanned card is not known add it
        Serial.println(F("I don't recognize this chip, including..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Read a chip to add or remove from the EEPROM"));
      }
    }
  }
  else {
    if ( isMaster(readCard)) {    // If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("MASTER MODE");
      Serial.println(F("Hello Master - Programming mode initiated"));
      ws2812fx.setPixelColor(0, PINK);
      ws2812fx.show();
      // if (Firebase.RTDB.setString(&fbdo, "rfid/master/card", "true")) {
      //   Serial.println("Card UID sent to Firebase successfully");
      // } 
      // else {
      //   Serial.println("Failed to send Card UID to Firebase");
      //   Serial.println("Reason: " + fbdo.errorReason());
      // }
      uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
      Serial.print(F("Existem "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) in the EEPROM"));//bản ghi trong EEPROM
      Serial.println("");
      Serial.println(F("Read a chip to add or remove from the EEPROM"));//Đọc một chip để thêm hoặc loại bỏ từ EEPROM
      Serial.println(F("Read the Master card again to exit programming mode"));//Đọc thẻ Master lại để thoát khỏi chế độ lập trình
      Serial.println(F("-----------------------------"));
      delay(200);

    }
    else {
      if ( findID(readCard) ) { // If not, see if the card is in the EEPROM
        Serial.println(F("Welcome, you can pass"));
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("UID:");
        for ( uint8_t i = 0; i < 4; i++) {  //
          readCard[i] = mfrc522.uid.uidByte[i];
          lcd.setCursor(4 + i * 2, 0); 
          if (readCard[i] < 0x10) {
            lcd.print("0");
          }
          lcd.print(readCard[i], HEX);
        }

        lcd.setCursor(0, 1);
        lcd.print("[OK] Welcome");
        ws2812fx.setPixelColor(0, GREEN); // Thiết lập màu xanh lam cho LED 3, 4, 5
        ws2812fx.show();
        sprintf(cardUID, "%02X%02X%02X%02X", readCard[0], readCard[1], readCard[2], readCard[3]);
        // if (Firebase.RTDB.setString(&fbdo, "rfid/user/card/pass/cardUID", cardUID)) {
        //   Serial.println("Card UID sent to Firebase successfully");
        // } 
        // else {
        //   Serial.println("Failed to send Card UID to Firebase");
        //   Serial.println("Reason: " + fbdo.errorReason());
        // }
        granted(3000);
        ws2812fx.setPixelColor(0, 0); // Thiết lập màu xanh lam cho LED 3, 4, 5
        ws2812fx.show();
        lcd.clear();
      }
      else {      // If not, show that the ID was not valid
        Serial.println(F("You cannot pass"));
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("UID:");
        for ( uint8_t i = 0; i < 4; i++) {  //
          readCard[i] = mfrc522.uid.uidByte[i];
          lcd.setCursor(4 + i * 2, 0); 
          if (readCard[i] < 0x10) {
            lcd.print("0");
          }
          lcd.print(readCard[i], HEX);
        }
        sprintf(cardUID, "%02X%02X%02X%02X", readCard[0], readCard[1], readCard[2], readCard[3]);
        // if (Firebase.RTDB.setString(&fbdo, "rfid/user/card/error/cardUID", cardUID)) {
        //   Serial.println("Card UID sent to Firebase successfully");
        // } 
        // else {
        //   Serial.println("Failed to send Card UID to Firebase");
        //   Serial.println("Reason: " + fbdo.errorReason());
        // }
        lcd.setCursor(0, 1);
        lcd.print("[X] Cannot pass");
        ws2812fx.setPixelColor(0, RED); // Thiết lập màu xanh lam cho LED 3, 4, 5
        ws2812fx.show();
        denied();
        ws2812fx.setPixelColor(0, 0); // Thiết lập màu xanh lam cho LED 3, 4, 5
        ws2812fx.show();
        lcd.clear();
      }
    }
  }
}


/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted ( uint16_t setDelay) {
  digitalWrite(BUZZER, HIGH);
  delay(70);
  digitalWrite(BUZZER, LOW);
  delay(40);
  digitalWrite(BUZZER, HIGH);
  delay(70);
  digitalWrite(BUZZER, LOW); 



  for (pos = 5; pos <= 179; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(2);                       // waits 15 ms for the servo to reach the position
  }
  delay(setDelay);
  for (pos = 179; pos >= 5; pos -= 1) { // goes from 180 degrees to 0 degrees
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(2);                       // waits 15 ms for the servo to reach the position
  }
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(BUZZER, HIGH);
  delay(300);
  digitalWrite(BUZZER, LOW);
  delay(300);
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Chip UID read:"));//UID của chip đã đọc:
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
unsigned long previousMilliscycleLeds = 0;
const long buzzerOnIntervalcycleLeds = 1000;
const long buzzerOffIntervalcycleLeds = 50;

void cycleLeds() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMilliscycleLeds >= buzzerOnIntervalcycleLeds) {
    digitalWrite(BUZZER, HIGH);
    previousMilliscycleLeds = currentMillis;
  } else if (currentMillis - previousMilliscycleLeds >= buzzerOffIntervalcycleLeds) {
    digitalWrite(BUZZER, LOW);
  }
}


//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  // digitalWrite(blueLed, LED_ON);  // Blue LED ON and ready to read card
  // digitalWrite(redLed, LED_OFF);  // Make sure Red LED is off
  // digitalWrite(greenLed, LED_OFF);  // Make sure Green LED is off
  // digitalWrite(relay, LOW);    // Make sure Door is Locked
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    EEPROM.commit();
    successWrite();
    Serial.println(F("ID successfully added to the EEPROM")); //ID đã được thêm vào EEPROM thành công
  }
  else {
    failedWrite(a );
    Serial.println(F("Error! There is something wrong with the chip ID or a problem in the EEPROM."));//Lỗi! Có vấn đề gì đó với ID chip hoặc có vấn đề ở EEPROM
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite(a);      // If not
    Serial.println(F("Error! There is something wrong with the chip ID or a problem in the EEPROM."));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    EEPROM.commit();
    successDelete();
    Serial.println(F("ID removido da EEPROM com sucesso"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != 0 )      // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM


void successWrite() {
  lcd.setCursor(0, 1);
  lcd.print("Add UID:");
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    lcd.setCursor(8 + i * 2, 1); 
    if (readCard[i] < 0x10) {
      lcd.print("0");
    }
    lcd.print(readCard[i], HEX);
  }
  sprintf(cardUID, "%02X%02X%02X%02X", readCard[0], readCard[1], readCard[2], readCard[3]);
  // if (Firebase.RTDB.setString(&fbdo, "rfid/master/add/card/cardUID", cardUID)) {
  //   Serial.println("Card UID sent to Firebase successfully");
  // } 
  // else {
  //   Serial.println("Failed to send Card UID to Firebase");
  //   Serial.println("Reason: " + fbdo.errorReason());
  // }
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
  delay(50);
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
  delay(50);
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);

}




///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite(byte a[]) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UID:");
  // Chuyển đổi mảng byte thành chuỗi ký tự
  char charArray[5];
  sprintf(charArray, "%02X%02X%02X%02X", a[0], a[1], a[2], a[3]);
  lcd.setCursor(4, 0);
  lcd.print(charArray);
  lcd.setCursor(1, 3);
  lcd.print("Failed...");
  // digitalWrite(BUZZER, HIGH);
  // delay(300);
  // digitalWrite(BUZZER, LOW);
  // delay(300);
  delay(1000);
  lcd.clear();
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM

void successDelete() {
  lcd.setCursor(2, 3);
  lcd.print("Remove...");

  lcd.setCursor(0, 1);
  lcd.print("Del UID:");
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    lcd.setCursor(8 + i * 2, 1); 
    if (readCard[i] < 0x10) {
      lcd.print("0");
    }
    lcd.print(readCard[i], HEX);
    sprintf(cardUID, "%02X%02X%02X%02X", readCard[0], readCard[1], readCard[2], readCard[3]);
    // if (Firebase.RTDB.setString(&fbdo, "rfid/master/delete/card/cardUID", cardUID)) {
    //   Serial.println("Card UID sent to Firebase successfully");
    // } 
    // else {
    //   Serial.println("Failed to send Card UID to Firebase");
    //   Serial.println("Reason: " + fbdo.errorReason());
    // }
  }


  digitalWrite(BUZZER, HIGH);
  delay(1000);
  digitalWrite(BUZZER, LOW);
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

bool monitorWipeButton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval)  {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(DOOR_BUTTON) != LOW)
        return false;
    }
  }
  return true;
}


void loop() {
 
  RFID_read();
}


bool secondPasswordEntered = false;
char wrongPassword[6] = "";
void checkPassWord(){
  char customKey = customKeypad.getKey();
  if (customKey){
    if (!isEnteringPassword) {
      isEnteringPassword = true;
      count = 0;
      memset(enteredPassword, 0, sizeof(enteredPassword));
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PASSWORD:");
    }

    lcd.setCursor(count + 9, 0);
    lcd.print(customKey);
    Serial.print(customKey);

    enteredPassword[count++] = customKey;

    if (count == 5) {
      enteredPassword[count] = '\0';
      if (strncmp(enteredPassword, password1, 5) == 0) {
        Serial.println(" - OK1");
        lcd.setCursor(0, 1);
        lcd.print("[OK] Welcome");
        ws2812fx.setPixelColor(0, GREEN);
        ws2812fx.show();
        // if (Firebase.RTDB.setString(&fbdo, "rfid/user/keypad/pass/anonymous", "true")) {
        //   Serial.println("Card UID sent to Firebase successfully");
        // } 
        //else {
        //   Serial.println("Failed to send Card UID to Firebase");
        //   Serial.println("Reason: " + fbdo.errorReason());
        // }
        granted(3000);
        ws2812fx.setPixelColor(0, 0);
        ws2812fx.show();
        delay(1000);
        lcd.clear();
      }
      else if (strncmp(enteredPassword, password2, 5) == 0) {
        Serial.println(" - OK2");
        if (!secondPasswordEntered) {
          programMode = true;
          secondPasswordEntered = true;
          lcd.clear();
          lcd.setCursor(2, 0);
          lcd.print("MASTER MODE");
          Serial.println(F("Hello Master - Programming mode initiated"));
          ws2812fx.setPixelColor(0, PINK);
          ws2812fx.show();
          // if (Firebase.RTDB.setString(&fbdo, "rfid/master/password", "true")) {
          //   Serial.println("Card UID sent to Firebase successfully");
          // }
          //  else {
          //   Serial.println("Failed to send Card UID to Firebase");
          //   Serial.println("Reason: " + fbdo.errorReason());
          // }
          uint8_t count = EEPROM.read(0);
          Serial.print(F("Existem "));
          Serial.print(count);
          Serial.print(F(" record(s) in the EEPROM"));
          Serial.println("");
          Serial.println(F("Read a chip to add or remove from the EEPROM"));//Đọc một chip để thêm hoặc loại bỏ từ EEPROM
          Serial.println(F("Read the Master card again to exit programming mode"));//Đọc thẻ Master lại để thoát khỏi chế độ lập trình
          Serial.println(F("-----------------------------"));
        } else {
          // Nếu đã nhập mật khẩu lần thứ hai, đặt lại các biến và chế độ
          programMode = false;
          secondPasswordEntered = false;
          lcd.clear();
          lcd.setCursor(2, 0);
          lcd.print("MASTER MODE");
          lcd.setCursor(0, 1);
          lcd.print("  Exiting ...   ");
          ws2812fx.setPixelColor(0, 0); // Thiết lập màu xanh lam cho LED 3, 4, 5
          ws2812fx.show();
          // if (Firebase.RTDB.setString(&fbdo, "rfid/master/password", "false")) {
          //   Serial.println("Card UID sent to Firebase successfully");
          // } 
          // else {
          //   Serial.println("Failed to send Card UID to Firebase");
          //   Serial.println("Reason: " + fbdo.errorReason());
          // }
          delay(1000); // Chờ 1 giây trước khi xóa màn hình LCD
          lcd.clear();
        }
      }
      // Trường hợp không khớp với cả hai mật khẩu
      else {
        Serial.println(" - ERROR");
        lcd.setCursor(0, 1);
        lcd.print("[X] Wrong ...");
        ws2812fx.setPixelColor(0, RED); // Thiết lập màu xanh lam cho LED 3, 4, 5
        ws2812fx.show();
        strncpy(wrongPassword, enteredPassword, sizeof(wrongPassword) - 1);
        wrongPassword[sizeof(wrongPassword) - 1] = '\0';
        // if (Firebase.RTDB.setString(&fbdo, "rfid/user/keypad/wrong/anonymous", wrongPassword)) {
        // Serial.println("Card UID sent to Firebase successfully"); 
        // } 
        //else {
        //   Serial.println("Failed to send Card UID to Firebase");
        //   Serial.println("Reason: " + fbdo.errorReason());
        // }
        denied();
        ws2812fx.setPixelColor(0, 0); // Thiết lập màu xanh lam cho LED 3, 4, 5
        ws2812fx.show();
        delay(1000); // Chờ 1 giây trước khi xóa màn hình LCD
        lcd.clear();
      }
      isEnteringPassword = false;
      count = 0;
      memset(enteredPassword, 0, sizeof(enteredPassword));
    }
  }
}

unsigned long sendDataPrevMillis = 0;
void getRTDB(){
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 10000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    if (Firebase.RTDB.getString(&fbdo, "rfid/master/add/card/cardUID")) {
      if (fbdo.dataType() == "string") {
        String value = fbdo.stringData();
        Serial.print("Add card Web:");
        Serial.println(value);
        byte cardToAdd[4];
        for (int i = 0; i < 4; i++) {
          cardToAdd[i] = strtol(value.substring(i*2, i*2+2).c_str(), NULL, 16);
        }
        writeID(cardToAdd); // Ghi thẻ vào EEPROM
      }
    } 
    // else {
    //     Serial.println(fbdo.errorReason());
    // }
    if (Firebase.RTDB.getString(&fbdo, "rfid/master/delete/card/cardUID")) {
      if (fbdo.dataType() == "string") {
        String value = fbdo.stringData();
        Serial.print("Delete Card Web:");
        Serial.println(value);
        byte cardToDelete[4];
        for (int i = 0; i < 4; i++) {
          cardToDelete[i] = strtol(value.substring(i*2, i*2+2).c_str(), NULL, 16);
        }
        deleteID(cardToDelete); // Xóa thẻ khỏi EEPROM
      }
    }
    //  else {
    //     Serial.println(fbdo.errorReason());
    // }
  }
}


// void getRTDB(){

//   if (Firebase.ready() && signupOK) {
//     if (Firebase.RTDB.getString(&fbdo, "rfid/master/add/card/cardUID") && fbdo.dataAvailable()) {
//       String value = fbdo.stringData();
//       Serial.print("Add card Web:");
//       Serial.println(value);
      
//       // Chèn mã thẻ vào hệ thống
//       // Chuyển đổi chuỗi thành mảng byte để ghi vào EEPROM
//       byte cardToAdd[4];
//       for (int i = 0; i < 4; i++) {
//         cardToAdd[i] = strtol(value.substring(i*2, i*2+2).c_str(), NULL, 16);
//       }
//       writeID(cardToAdd); // Ghi thẻ vào EEPROM
//     } 

//     if (Firebase.RTDB.getString(&fbdo, "rfid/master/delete/card/cardUID") && fbdo.dataAvailable()) {
//       String value = fbdo.stringData();
//       Serial.print("Delete Card Web:");
//       Serial.println(value);
      
//       // Xóa mã thẻ khỏi hệ thống
//       // Chuyển đổi chuỗi thành mảng byte để so sánh với các thẻ trong EEPROM
//       byte cardToDelete[4];
//       for (int i = 0; i < 4; i++) {
//         cardToDelete[i] = strtol(value.substring(i*2, i*2+2).c_str(), NULL, 16);
//       }
//       deleteID(cardToDelete); // Xóa thẻ khỏi EEPROM
//     }
//   }
// }

// void streamCallback(FirebaseStream data)
// {
//   Serial.printf("sream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
//                 data.streamPath().c_str(),
//                 data.dataPath().c_str(),
//                 data.dataType().c_str(),
//                 data.eventType().c_str());
//   printResult(data); // see addons/RTDBHelper.h
//   Serial.println();

//   // This is the size of stream payload received (current and max value)
//   // Max payload size is the payload size under the stream path since the stream connected
//   // and read once and will not update until stream reconnection takes place.
//   // This max value will be zero as no payload received in case of ESP8266 which
//   // BearSSL reserved Rx buffer size is less than the actual stream payload.
//   Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());

//   // Due to limited of stack memory, do not perform any task that used large memory here especially starting connect to server.
//   // Just set this flag and check it status later.
//   dataChanged = true;
// }

// void streamTimeoutCallback(bool timeout)
// {
//   if (timeout)
//     Serial.println("stream timed out, resuming...\n");

//   if (!stream.httpConnected())
//     Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
// }
