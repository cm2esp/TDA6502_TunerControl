/*
 * Controlling an i2c TV Tuner based on Philips TDA6502
 * This sketch uses the debug pinout of the tuner present
 * at the Cuban/Chinese Haier 21" Analog TV which on this
 * project is used as an RF Frontend and downconverter for
 * an RTL-SDR dongle or other receiver providing pre-amp,
 * filtering and overcoming coax loss by the lower frequency
 * IF output. LNA power and AGC (sort of variable gain
 * amplifier) can be commanded via the Arduino board. A prompt
 * like serial interface is provided to interact with the
 * tuner, sending instructions or query status.
 * 
 * Version: 1.0.0
 * Date: 23/12/2020
 * Author: Raydel Abreu, CM2ESP
 */

#include <Wire.h>
const bool DEBUG_FLAG = false;
const int SerialTimeOut = 1000;
const byte TUNE_RETRIES = 5;
const float IF_FREQ = 45;
const float F_STEP = 50.0;
const byte ADB = 0x60;
const byte LNA_PIN = 10; //LNA is ON OFF switch
const byte AGC_PIN = 11;  //AGC needs a PWM DAC to control gain
//Default Init Status is 145 MHz VHF High
byte DIVA = B00001110;
byte DIVB = B11011000;
//byte CNTR = B11001110; //FSTEP = 62.5k
byte CNTR = B11001000; //FSTEP = 50k
byte BBSW = B0010;
byte gain = 127;
bool PLL_Lock = false;
bool LNA_Status = false;
unsigned int multWord = 3800;

void serialRead()
{
  if (Serial.available() > 0)
  {
    float freq = 0;
    String inBuff = Serial.readStringUntil('\n');
    //Verify if is LNA toggle command

    if (inBuff[0] == 'a' || inBuff[0] == 'A')
    {
      LNA_Status = !LNA_Status;
      Serial.print("LNA Power is: ");
      if (LNA_Status) Serial.println("ON"); else Serial.println("OFF");
      if (LNA_Status) digitalWrite(LNA_PIN, HIGH); else digitalWrite(LNA_PIN, LOW);
    }
    else
    {
      //Look for AGC gain control command
      if (inBuff[0] == 'g' || inBuff[0] == 'G')
      {
        gain = inBuff.substring(1).toInt();
        analogWrite(AGC_PIN, gain);
        Serial.print("Set AGC to: ");
        Serial.println(gain);
      }
      else
      {
        //Status query
        if (inBuff[0] == '?')
        {
          //Frequency
          Serial.println();
          Serial.println(".= Status Query =.");
          Serial.print("Tuned to: ");
          Serial.println((float)((multWord * F_STEP / 1000.0) - IF_FREQ));
          //PLL Lock
          Serial.print("PLL Lock: ");
          if (PLL_Lock) Serial.println("YES"); else Serial.println("NO");
          //LNA Status
          Serial.print("LNA Status: ");
          if (LNA_Status) Serial.println("ON"); else Serial.println("OFF");
          //AGC Gain
          Serial.print("AGC Gain: ");
          Serial.println(gain);
          Serial.println("System Ready.");
          Serial.println();
        }
        else
        {
          //Look for frequency input instead
          freq = inBuff.toFloat();
        }
      }
    }

    if (freq > 50 && freq < 830)
    {
      freq += IF_FREQ;
      freq = freq * 1000;
      multWord = (unsigned int)(freq / F_STEP);

      //Verify tuning, multiplier steps might not match exactly
      float real_tuned = (float)((multWord * F_STEP / 1000.0) - IF_FREQ);
      Serial.print("Tuning to: ");
      Serial.println(real_tuned);

      DIVB = multWord & 0xff;
      DIVA = (multWord >> 8);
      if (DEBUG_FLAG)
      {
        Serial.print("Tuning Multiplier: ");
        Serial.println(multWord);
        Serial.println(DIVA, BIN);
        Serial.println(DIVB, BIN);
      }
      //Adjust Band Switch
      if (real_tuned <= 130) BBSW = B0001; //VHF Low
      if (real_tuned > 130 && real_tuned <= 300) BBSW = B0010; //VHF High
      if (real_tuned > 300) BBSW = B0100; //UHF
      //Tune now
      Tune();
    }
    Serial.print("> ");
  }
}

void AskTuner()
{
  if (DEBUG_FLAG)
  {
    Serial.print("Interrogation sent to: ");
    Serial.println(ADB, HEX);
  }

  Wire.requestFrom(ADB, 1);    // request 1 byte

  while (Wire.available()) { // device may send less than requested
    byte c = Wire.read(); // receive a byte as character

    if (DEBUG_FLAG)
    {
      Serial.print("Received: ");
      Serial.println(c, BIN);        // print the character
    }

    if (c == 88)PLL_Lock = true; else PLL_Lock = false;

    Serial.print("PLL LOCK: ");
    if (PLL_Lock) Serial.println("YES"); else Serial.println("NO");
    Serial.println("");
  }
}

void SendTuner()
{
  Serial.println("Sending tune request...");

  Wire.beginTransmission(ADB); // transmit to device #
  delay(10);
  Wire.write(DIVA);        // sends one bytes
  delay(10);
  Wire.write(DIVB);        // sends one bytes
  delay(10);
  Wire.write(CNTR);        // sends one bytes
  delay(10);
  Wire.write(BBSW);        // sends one bytes
  delay(10);
  Wire.endTransmission();    // stop transmitting

  Serial.println("Done...");
}

void Tune()
{
  for (int i = 0; i < TUNE_RETRIES; i++)
  {
    SendTuner();
    delay(100);
    AskTuner();
    if (PLL_Lock) break;
  }
}

void setup() {
  Wire.begin(); // join i2c bus (address optional for master)

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.setTimeout(SerialTimeOut);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Serial Initialized...");

  //Led On to know it is ready
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  //At init, turn LNA OFF
  pinMode(LNA_PIN, OUTPUT);
  digitalWrite(LNA_PIN, LOW);

  //At Init set AGC Voltage to 2.5V
  analogWrite(AGC_PIN, gain);

  //Send Init Status
  Tune();
}

void loop()
{
  serialRead();
  delay(1);
}
