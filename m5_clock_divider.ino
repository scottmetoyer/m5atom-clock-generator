#include "M5Atom.h"
#include <MIDI.h>

extern const unsigned char AtomImageData[375 + 2];

int currentClockMultiplier = 0;
float accX = 0, accY = 0, accZ = 0;
bool IMU6886Flag = false;
long lastUpdate;
long taps[8];
int tapIndex;
long tapTimer = 0;
bool isTapping = false;

// Clock calculation variables
long tempo = 0;
float interval = 0.0;
bool pulseOn = false;
long pulseCounter = 0; // MS counter for pulses

struct HairlessMidiSettings : public midi::DefaultSettings
{
   static const bool UseRunningStatus = false;
   static const long BaudRate = 115200;
};
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial, MIDI, HairlessMidiSettings);

const unsigned char image_pulse[77]=
{
/* width  005 */ 0x05,
/* height 005 */ 0x05,
/* Line   000 */ 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, // 
/* Line   001 */ 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, // 
/* Line   002 */ 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, // 
/* Line   003 */ 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, // 
/* Line   004 */ 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, 0xaa,0xaa,0xaa, // 
};

const unsigned char image_off[77]=
{
/* width  005 */ 0x05,
/* height 005 */ 0x05,
/* Line   000 */ 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, // 
/* Line   001 */ 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, // 
/* Line   002 */ 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, // 
/* Line   003 */ 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, // 
/* Line   004 */ 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, // 
};

void storeTap() {
  // This is our first tap
  if (!isTapping) {
    // Initialize the tap count variables
    tapTimer = millis();
    isTapping = true;
    tapIndex = 0;
    return;
  }

  long now = millis();
  taps[tapIndex] = now - tapTimer;
  tapTimer = now;
  
  tapIndex++;
  if (tapIndex == 8) {
    tapIndex = 7;
  }

  calculateIntervals();
}

void calculateIntervals() {
  long total = 0;

  for (int i = 0; i < tapIndex; i++) {
    total += taps[i];
  }

  // Get the tap average
  tempo = total / tapIndex;
  
  // Calculate 24 PPQN
  interval = tempo/24.0;  
}

void setup()
{
    M5.begin(true, false, true);
    if (M5.IMU.Init() != 0)
        IMU6886Flag = false;
    else
        IMU6886Flag = true;
    delay(10);
    M5.dis.displaybuff((uint8_t *)image_off);

    MIDI.begin(); 
    MIDI.turnThruOff();
    Serial.begin(38400);  

    lastUpdate = millis();
}

void loop()
{
    if (M5.Btn.wasPressed())
    {
      storeTap();
    }

    // Check accelerometer data
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    int clockModifier = constrain(map(accX * 1000, -1000, 1000, -32, 32), -32, 32);
   
    // Check if it's time to send a pulse
    long now = millis();

    // Turn off the tap flag if it's been more than 10 seconds without a tap
    if (isTapping && now - tapTimer > 4000) {
      isTapping = false;
    }
    
    int calculatedInterval = interval;

    if (clockModifier > 0) {
      calculatedInterval = interval * clockModifier;
    } else if (clockModifier < 0) {
      calculatedInterval = interval / (clockModifier * -1);
    }
    
    if(now - lastUpdate >= calculatedInterval) {
      lastUpdate = now;

      if (interval > 0) {
        MIDI.sendClock();
        
        pulseCounter += 1;
        if (pulseCounter == 12) {
           pulseCounter = 0;
          if (pulseOn) {
            pulseOn = false;
            M5.dis.displaybuff((uint8_t *)image_off);
          } else {
            pulseOn = true;
            M5.dis.displaybuff((uint8_t *)image_pulse);
          }
        }
      }
    }
    
    M5.update();
    MIDI.read();
}
