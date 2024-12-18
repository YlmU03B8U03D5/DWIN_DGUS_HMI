#include <SPI.h>
#include <Wire.h>
#include <SC16IS752.h>

// Initialize both bridges
SC16IS752 i2cuart = SC16IS752(SC16IS750_PROTOCOL_I2C, 0x48);      // BT44
SC16IS752 displayUart = SC16IS752(SC16IS750_PROTOCOL_I2C, 0x4D);  // Display

String inputBuffer = "";

// Define the prefix for DWIN display
const byte hexPrefix[] = { 0x5A, 0xA5, 0x04, 0x82, 0x50, 0x00 };
const int prefixLength = 6;

// Variables for averaging
const int AVERAGE_COUNT = 10;
float weightArray[AVERAGE_COUNT];
int weightIndex = 0;
bool arrayFilled = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Start testing");

  // Initialize BT44
  i2cuart.begin(9600, 9600);
  if (i2cuart.ping() != 1) {
    Serial.println("BT44 not found");
    while (1);
  }
  Serial.println("BT44 found");

  // Initialize Display
  displayUart.begin(9600, 115200);
  if (displayUart.ping() != 1) {
    Serial.println("Display not found");
    while (1);
  }
  Serial.println("Display found");

  // Initialize weight array
  for (int i = 0; i < AVERAGE_COUNT; i++) {
    weightArray[i] = 0.0;
  }
}

float calculateAverage() {
  float sum = 0;
  int count = arrayFilled ? AVERAGE_COUNT : weightIndex;
  
  for (int i = 0; i < count; i++) {
    sum += weightArray[i];
  }
  
  return count > 0 ? sum / count : 0;
}

void sendFrameWeight(float weight) {
  byte frame[10] = { 0x5A, 0xA5, 0x05, 0x82, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00 };
  // Copy float bytes into frame in big-endian format
  for (int i = 0; i < sizeof(float); i++) {
    frame[9 - i] = *(((byte*)&weight) + i);
  }
  for (int i = 0; i < 10; i++) {
    displayUart.write(SC16IS752_CHANNEL_B, frame[i]);
    delayMicroseconds(500);
  }
  
  // Convert weight to integer (multiply by 1000 to preserve 3 decimal places)
  uint32_t weightValue = abs(weight * 1000);
  byte valueBytes[8];
  
  // Convert to bytes
  for (int i = 0; i < 8; i++) {
    valueBytes[i] = (weightValue >> ((7-i) * 8)) & 0xFF;
    displayUart.write(SC16IS752_CHANNEL_B, valueBytes[i]);
    delay(1);
  }
}



void loop() {
  if (i2cuart.available(SC16IS752_CHANNEL_B) > 0) {
    char inChar = i2cuart.read(SC16IS752_CHANNEL_B);
    
    if (inChar == '\t') {
      float weight = inputBuffer.toFloat();
      
      // Add to array
      weightArray[weightIndex] = weight;
      weightIndex = (weightIndex + 1) % AVERAGE_COUNT;
      if (weightIndex == 0) arrayFilled = true;

      // Calculate and send average
      float avgWeight = calculateAverage();
      sendFrameWeight(avgWeight);
      
      // Debug output
      Serial.print("Average Weight: ");
      Serial.print(avgWeight, 3);
      Serial.println(" kg");
      
      inputBuffer = "";
    } else {
      inputBuffer += inChar;
    }
  }

  // Call the listen function to handle incoming data from DWIN
  hmi.listen(); // This will read and print incoming data from the DWIN display
}