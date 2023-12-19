#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <SD.h>
#include <ezButton.h>
#include <ezBuzzer.h>

#define SHORT_PRESS_TIME 1000
#define LONG_PRESS_TIME 1000

class MyButton {
public:
  ezButton button;
  unsigned long pressedTime;
  bool isPressing;
  bool isLongDetected;

  // Function pointers for callbacks
  void (*shortPressCallback)() = nullptr;
  void (*longPressCallback)() = nullptr;

  MyButton(int pin) : button(pin), isPressing(false), isLongDetected(false) {}

  void setup(void (*shortCallback)(), void (*longCallback)()) {
    button.setDebounceTime(50);
    shortPressCallback = shortCallback;
    longPressCallback = longCallback;
  }

  void update() {
    button.loop();
    // ... existing button logic ...
    if(button.isPressed()){
      pressedTime = millis();
      isPressing = true;
      isLongDetected = false;
    }
    if(button.isReleased()) {
      isPressing = false;
      long pressDuration = millis() - pressedTime;
      if(pressDuration < SHORT_PRESS_TIME && shortPressCallback != nullptr) {
        shortPressCallback(); // Call the short press function
      }
    }
    if(isPressing && !isLongDetected) {
      long pressDuration = millis() - pressedTime;
      if(pressDuration > LONG_PRESS_TIME && longPressCallback != nullptr) {
        isLongDetected = true;
        longPressCallback(); // Call the long press function
      }
    }
  }
};

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
int displayState = 1;
const int numScreens = 2;

#define ledPin 13
#define pinCS 53
File gpsFile;
bool recordState = false;
int fileNum = 0;
String fileName;
unsigned long previousLedTime = 0;
bool ledState;

MyButton btn1(4);
MyButton btn2(5);
MyButton btn3(6);
MyButton btn4(7);
ezBuzzer buzzer(9);

const int numlatLongs = 11;
float latLongs[numlatLongs][2] = {
  {36.240344, -115.319698}, 
  {36.241099, -115.318955}, 
  {36.241239, -115.318950}, 
  {36.241473, -115.318679}, 
  {36.241694, -115.318673}, 
  {36.242070, -115.318453}, 
  {36.242444, -115.318649}, 
  {36.242260, -115.319588}, 
  {36.241583, -115.319489}, 
  {36.240702, -115.319730}, 
  {36.240350, -115.319743}
};
float maxLat = -90.0;
float minLat = 90.0;
float maxLong = 180.0;
float minLong = -180.0;

float aspectX = 1.0;
float aspectY = 1.0;

TinyGPSPlus gps;
float currentLat = latLongs[0][0];
float currentLong = latLongs[0][1];
int course = 0;
int course2 = 0;
int distanceBetween = 0;
float mph;
int alti;
int sats;
int year;
String month;
String day;
String hour;
String minute;
String second;
bool minSats = false;
int wayPoint = 0;

unsigned long currentTime;
unsigned long previousGPSTime = 0;
unsigned long previousWriteTime = 0;
int serialDelay = 1000;
int writeDelay = 5;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  minmaxLatLong();

  pinMode(ledPin, OUTPUT);
  pinMode(pinCS, OUTPUT);

  btn1.setup(btn1ShortPress, btn1LongPress);
  btn2.setup(btn2ShortPress, btn2LongPress);

  display.begin(SCREEN_ADDRESS, true);
  drawMessage("Initializing");

  if (!SD.begin(pinCS)) {
    Serial.println("Card failed, or not present");
    // drawMessage("Check SD Module and reset");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");

  File root = SD.open("/");
  countFiles(root);
  root.close();

  delay(1000);
}

void loop() {
  btn1.update();
  btn2.update();
  buzzer.loop();

  currentTime = millis();

  if (sats > 4) minSats = true;

  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      getPositionInfo();
      if (currentTime - previousGPSTime > serialDelay) {
        if (minSats) {
          Serial.println(dataOut()); // preview of data being recorded
          if (recordState) {
            if (currentTime - previousWriteTime > writeDelay * 1000) {
              // Serial.println(fileName);
              gpsFile = SD.open(fileName, FILE_WRITE);
              if (gpsFile) {
                gpsFile.println(dataOut());
                gpsFile.close();
              }
              previousWriteTime = currentTime;
            }
          }

        } else {
          Serial.println("Acquiring sats...");
        }
        previousGPSTime = currentTime;
      }
    }
  }

  if (recordState) {
    // blink the ledPin
    if (currentTime - previousLedTime > 500) {
      ledState = !ledState;
      previousLedTime = currentTime;
    }
    digitalWrite(ledPin, ledState);
  } else {
    digitalWrite(ledPin, recordState);
  }

  display.clearDisplay();
  switch(displayState) {
    case 1:
      dataScreen();
      break;
    case 2:
      drawMap();
      break;
  }
  display.display();
}

void drawMessage(String message) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println(message);
  display.display();
}

void drawMap() {
  for (int i = 0; i < numlatLongs - 1; i++) {
    display.drawLine(
      map(longify(latLongs[i][1]), longify(minLong), longify(maxLong), (63*aspectX), 0), 
      map(longify(latLongs[i][0]), longify(minLat), longify(maxLat), 63*aspectY, 0), 
      map(longify(latLongs[i+1][1]), longify(minLong), longify(maxLong), 63*aspectX, 0), 
      map(longify(latLongs[i+1][0]), longify(minLat), longify(maxLat), 63*aspectY, 0), 
      SH110X_WHITE
    );
  }

  display.fillCircle(
    map(longify(currentLong), longify(minLong), longify(maxLong), (63*aspectX), 0), 
    map(longify(currentLat), longify(minLat), longify(maxLat), 63*aspectY, 0), 
    2, 
    SH110X_WHITE
  );

  display.drawCircle(
    map(longify(latLongs[wayPoint][1]), longify(minLong), longify(maxLong), (63*aspectX), 0), 
    map(longify(latLongs[wayPoint][0]), longify(minLat), longify(maxLat), 63*aspectY, 0), 
    3, 
    SH110X_WHITE
  );
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(65,0);
  display.print("Crs :");
  display.print(course);
  display.setCursor(65,15);
  display.print("Crs2:");
  display.print(course2);
  display.setCursor(65,30);
  display.print("Dist:");
  display.print(distanceBetween);
  display.setCursor(65,43);
  display.print("  ");
  display.print(currentLat,5);
  display.setCursor(65,52);
  display.print(currentLong,5);
}

void dataScreen() {
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);

  if (minSats) {
    display.print("Lat:  ");
    display.println(currentLat, 6);
    display.print("Lon:");
    display.println(currentLong, 6);
    display.print("Ele: ");
    display.println(alti);
    display.print("Sat: ");
    display.print(sats);
    display.print(" Log: ");
    display.print(writeDelay);
    display.println("s");
    display.print("Spd: ");
    display.print(int(mph));
    display.print(" Crs: ");
    display.println(int(course));

  } else {
    display.println("Acquiring sats");
  }

  if (recordState) {
    display.println();
    display.print("Recording: ");
    display.println(fileName);
  }
}

void btn1ShortPress() {
  // Serial.println("Button 1 short press action");
  if (recordState == false) {
    writeDelay += 10;
    if (writeDelay > 35) {
      writeDelay = 5;
    }
    buzzer.beep(50);
  }
  buzzer.beep(50);
}

void btn1LongPress() {
  // Serial.println("Button 1 long press action");
  if (minSats) {
    recordState = !recordState;
    // digitalWrite(ledPin, recordState);
    if (recordState) {
      fileNum++;
      updateFileName();
    }
    buzzer.beep(100);
  }
}

void btn2ShortPress() {
  // Serial.println("Button 1 short press action");
  wayPoint++;
  if (wayPoint > numlatLongs-1) wayPoint = 0;
  buzzer.beep(50);
}

void btn2LongPress() {
  // Serial.println("Button 1 long press action");
  displayState++;
  if (displayState > numScreens) {
    displayState = 1;
  }
  buzzer.beep(100);
}

long longify(float x) {
  return x * 1000000;
}

void minmaxLatLong() {
  /* I had to play around with the signs and the 
  < and > operators to get my map to properly display.
  You may have to do the same if you try this and your map
  is backwards or upside down. */
  for (int i = 0; i < numlatLongs; i++) {
    if (latLongs[i][0] < minLat) minLat = latLongs[i][0];
    if (latLongs[i][0] > maxLat) maxLat = latLongs[i][0];
    if (latLongs[i][1] > minLong) minLong = latLongs[i][1];
    if (latLongs[i][1] < maxLong) maxLong = latLongs[i][1];
  }
  float height = abs(maxLat-minLat);
  float width = abs(maxLong-minLong);
  // constrain and manage the map in the proper direction
  if (height > width) {
    aspectX = width / height;
  } else {
    aspectY = height / width;
  }
}

void countFiles(File dir) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }    
    if (!entry.isDirectory()) {
      // Serial.println(entry.name());
      fileNum++;
    }
    entry.close();
  }
}

void updateFileName() {
  if (fileNum < 10) {
    fileName = "00" + String(fileNum) + ".txt";
  } else if (fileNum < 100) {
    fileName = "0" + String(fileNum) + ".txt";
  } else {
    fileName = String(fileNum) + ".txt";
  }
}

void getPositionInfo(){
  if (gps.location.isValid()) {
		currentLat = gps.location.lat();
		currentLong = gps.location.lng();
    distanceBetween = gps.distanceBetween(
      currentLat, 
      currentLong, 
      latLongs[wayPoint][0], 
      latLongs[wayPoint][1]
    );
    course2 = gps.courseTo(
      currentLat, 
      currentLong, 
      latLongs[wayPoint][0], 
      latLongs[wayPoint][1]
    );
	}
  if (gps.course.isValid()) {
    course = int(gps.course.deg());
	}
  if (gps.satellites.isValid()) {
    sats = gps.satellites.value();
	}

	if (gps.altitude.isValid()) {
    alti = gps.altitude.feet();
	}

  if (gps.date.isValid()) {
    year = gps.date.year();
    month = leadingZero(gps.date.month());
    day = leadingZero(gps.date.day());
	}

  if (gps.time.isValid()) {
    hour = leadingZero(gps.time.hour());
    minute = leadingZero(gps.time.minute());
    second = leadingZero(gps.time.second());
	}

  if (gps.speed.isValid()) {
    mph = gps.speed.mph();
	}
}

String leadingZero(int x) {
  String zero = "0";
  if (x < 10) {
    return zero + String(x);
  }
  return String(x);
}

String processTime() {
  String timeOut;
  timeOut = String(hour);
  timeOut += ":";
  timeOut += String(minute);
  timeOut += ":";
  timeOut += String(second);
  return timeOut;
}

String processDate() {
  String dateOut;
  dateOut = String(year);
  dateOut += "-";
  dateOut += String(month);
  dateOut += "-";
  dateOut += String(day);
  return dateOut;
}

String dataOut() {
  String output = "lat>";
  output += String(currentLat, 6);
  output += ",lon>";
  output += String(currentLong, 6);
  output += ",sats>";
  output += String(sats);
  output += ",alti>";
  output += String(alti);
  output += ",date>";
  output += processDate();
  output += ",time>";
  output += processTime();
  output += ",course>";
  output += String(course);
  output += ",speed>";
  output += String(mph);
  return output;
}
