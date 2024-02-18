
#pragma once

#include "wled.h"
#include <SPI.h>

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * 
 * This is an example for a v2 usermod.
 * v2 usermods are class inheritance based and can (but don't have to) implement more functions, each of them is shown in this example.
 * Multiple v2 usermods can be added to one compilation easily.
 * 
 * Creating a usermod:
 * This file serves as an example. If you want to create a usermod, it is recommended to use usermod_v2_empty.h from the usermods folder as a template.
 * Please remember to rename the class and file to a descriptive name.
 * You may also use multiple .h and .cpp files.
 * 
 * Using a usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

//class name. Use something descriptive and leave the ": public Usermod" part :)
#define ENABLE_MCP3201 (1)
#define DISABLE_HOLD (0)

#define NUM_BUTTONS  (5)
#define POWER_BUTTON (0)
#define HOLD_BUTTON  (1)
#define BOIL_BUTTON  (2)
#define PLUS_BUTTON  (3)
#define MINUS_BUTTON (4)

#define POWER_LED_PIN    (6)
#define POWER_BUTTON_PIN (7)
#define HOLD_BUTTON_PIN  (2)
#define HOLD_LED_PIN     (1)
#define BOIL_BUTTON_PIN  (21)
#define PLUS_BUTTON_PIN  (5)
#define MINUS_BUTTON_PIN (4)

#define NUM_TEMPS (16)
#define VOLTAGE_AVERAGES (32)

#define ENABLE_HISTORY (1)
#if ENABLE_HISTORY
#define HISTORY_LENGTH (40)
#define HISTORY_STRINGS (1)
#endif //ENABLE_HISTORY

enum KettleState {
  STATE_IDLE,
  STATE_TURNOFF,
  STATE_1_ENSURE_OFF,
  STATE_2_TURN_ON,
  STATE_3_BOIL,
  STATE_4_WAIT_FOR_BOIL,
  STATE_5_ENSURE_OFF,
  STATE_6_TURN_ON,
  STATE_7_ESTABLISH_KNOWN_TEMPERATURE,
  STATE_8_LOWER_TEMP_TO_DESIRED,
  STATE_9_ENACT_HOLD,
  STATE_10_MAINTAIN_HOLD,
  STATE_11_TURN_HOLD_OFF,
};

String getStringFromState(KettleState state)
{
  switch (state)
  {
    case STATE_IDLE:
      return "STATE_IDLE";
    case STATE_TURNOFF:
      return "STATE_TURNOFF";
    case STATE_1_ENSURE_OFF:
      return "STATE_1_ENSURE_OFF";
    case STATE_2_TURN_ON:
      return "STATE_2_TURN_ON";
    case STATE_3_BOIL:
      return "STATE_3_BOIL";
    case STATE_4_WAIT_FOR_BOIL:
      return "STATE_4_WAIT_FOR_BOIL";
    case STATE_5_ENSURE_OFF:
      return "STATE_5_ENSURE_OFF";
    case STATE_6_TURN_ON:
      return "STATE_6_TURN_ON";
    case STATE_7_ESTABLISH_KNOWN_TEMPERATURE:
      return "STATE_7_ESTABLISH_KNOWN_TEMPERATURE";
    case STATE_8_LOWER_TEMP_TO_DESIRED:
      return "STATE_8_LOWER_TEMP_TO_DESIRED";
    case STATE_9_ENACT_HOLD:
      return "STATE_9_ENACT_HOLD";
    case STATE_10_MAINTAIN_HOLD:
      return "STATE_10_MAINTAIN_HOLD";
    case STATE_11_TURN_HOLD_OFF:
      return "STATE_11_TURN_HOLD_OFF";
    default:
      return "unknown state";
  }
}

struct ButtonInfo {
  unsigned long time_pressed;
  unsigned long duration;
  uint8_t pin;
  uint8_t pressed;
  unsigned long lastSeenUnpressed;
};

class KettleUsermod : public Usermod {

  private:

    // Private class members. You can declare variables and functions only accessible to your usermod here
    bool initDone = false;

    ButtonInfo buttonInfo[NUM_BUTTONS];
#if ENABLE_MCP3201
    uint32_t voltageAverage = 0;
    unsigned long voltageLastRead = 0;
#endif //ENABLE_MCP3201
    //buttonsHeld is true while we are holding a button and for 500ms after. It slows down the state machine so we press buttons slowly.
    bool buttonsHeld = 0;
    unsigned long lastButtonReleaseTime = 0;
    KettleState currentState = STATE_IDLE;
    unsigned long timeStateEntered = 0;
    uint16_t currentSetTemperature = 0;
    uint16_t plannedSetTemperature = 0;
    uint16_t goalSetTemperature = 0;
    uint32_t desiredHoldTime = 0;

    SPIClass spiPort;
    static const char _name[];
    static const char _enabled[];
    static const char _powerled[];
    static const char _holdled[];
    static const char _voltage[];
    static const char _kettlepresent[];
    static const char _temperature[];
    static const char _hold[];
    static const char _currentstate[];
    static const char _buttoninfo[];
    static const char _plus[];
    static const char _minus[];
    static const uint16_t voltages[];
    static const uint16_t temperatures[];

  public:
    KettleUsermod() : spiPort(SPIClass(FSPI)) {}

    // Update any buttons. Return true if any are pressed.
    void updatePressed(unsigned long time, bool releaseAll) {
      bool anyPressed = 0;

      for (int i = 0; i < NUM_BUTTONS; i++)
      {
#if DISABLE_HOLD
        if (i == HOLD_BUTTON)
        {
          continue;
        }
#endif //DISABLE_HOLD
        ButtonInfo *pButtonInfo = &buttonInfo[i];
        if (pButtonInfo->pressed)
        {
          if (releaseAll || ((time - pButtonInfo->time_pressed) >= pButtonInfo->duration))
          {
            pButtonInfo->pressed = 0;
            lastButtonReleaseTime = time;
            DEBUG_PRINTF("Releasing button of pin %u\n", pButtonInfo->pin);
            logHistory("Release " + String(i));
            pinMode(pButtonInfo->pin, INPUT);
          } else {
            anyPressed = 1;
            pButtonInfo->lastSeenUnpressed = time;
          }
        }
      }

      if (buttonsHeld && !anyPressed && ((time - lastButtonReleaseTime) >= 500))
      {
        buttonsHeld = 0;
      }
    }

    void pressButton(uint8_t buttonId, unsigned long duration)
    {
#if DISABLE_HOLD
        if (buttonId == HOLD_BUTTON)
        {
          return;
        }
#endif //DISABLE_HOLD
      ButtonInfo *pButtonInfo = &buttonInfo[buttonId];
      DEBUG_PRINTF("Pressing button of pin %u\n", pButtonInfo->pin);
      logHistory("Press " + String(buttonId));
      pButtonInfo->pressed = 1;
      digitalWrite(pButtonInfo->pin, 0);
      pinMode(pButtonInfo->pin, OUTPUT);
      digitalWrite(pButtonInfo->pin, 0);
      pButtonInfo->duration = duration;
      pButtonInfo->time_pressed = millis();
      buttonsHeld = 1;
    }

#if ENABLE_MCP3201
    uint16_t getADCReading()
    {
      u8_t bytes[2] = {0, 0};
      u8_t output[2] = {0, 0};
      spiPort.beginTransaction(SPISettings(500000, SPI_MSBFIRST, SPI_MODE0));
      spiPort.transferBytes(bytes, output, 2);
      spiPort.endTransaction();
      return ((output[0] & 0x1F) << 7) + (output[1] >> 1);
    }

    void updateVoltage()
    {
      uint16_t reading = getADCReading();
      voltageAverage *= (VOLTAGE_AVERAGES - 1);
      voltageAverage /= VOLTAGE_AVERAGES;
      voltageAverage += reading;
      voltageLastRead = millis();
    }
#endif //ENABLE_MCP3201

    uint16_t voltageToTemperature(uint16_t voltage)
    {
      if (voltage <= voltages[0])
      {
        return temperatures[0];
      }
      if (voltage >= voltages[NUM_TEMPS - 1])
      {
        return temperatures[NUM_TEMPS - 1];
      }

      // lerp code
      // progress ranges from 0-1 from low voltage to high voltage, and high temp to low temp
      // progress = vdiff / vdiffrange
      // output = tHigh - (progress * tdiffrange)
      // output = tHigh - ((vdiff * tdiffrange) / vdiffrange)
      int i = 0;
      for (i = 0; i < NUM_TEMPS - 2; i++)
      {
        if (voltage < voltages[i + 1]) break;
      }
      uint32_t num = (voltage - voltages[i]);
      num *= temperatures[i] - temperatures[i+1];
      num /= voltages[i+1] - voltages[i];
      num = temperatures[i] - num;

      return (uint16_t) num;
    }

    void addToJsonInfo(JsonObject& root)
    {
      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull())
      {
        usermod = root.createNestedObject(FPSTR(_name));
      }
      usermod[FPSTR(_powerled)] = digitalRead(POWER_LED_PIN);
      usermod[FPSTR(_holdled)] = digitalRead(HOLD_LED_PIN);
      usermod[FPSTR(_currentstate)] = getStringFromState(currentState);
#if ENABLE_MCP3201
      uint16_t voltage = (uint16_t)(voltageAverage / VOLTAGE_AVERAGES);
      usermod[FPSTR(_voltage)] = voltage;
      usermod[FPSTR(_kettlepresent)] = voltage < 4070;
      usermod[FPSTR(_temperature)] = voltageToTemperature(voltage);
#endif //ENABLE_MCP3201

#if ENABLE_HISTORY
      JsonArray arr = usermod.createNestedArray(FPSTR("history"));
      unsigned int start,length;
      if (numHistoryEntries < HISTORY_LENGTH)
      {
        start = 0;
        length = numHistoryEntries;
      }
      else
      {
        length = HISTORY_LENGTH;
        start = (nextHistoryEntry + HISTORY_LENGTH - 1) % HISTORY_LENGTH;
      }
      for (unsigned int offset = 0; offset < length; offset++)
      {
        unsigned int index = (start + offset) % HISTORY_LENGTH;
        arr.add(history[index]);
        arr.add(historyTimestamp[index]);
      }

      int buttons = 0;
      for (int i = 0; i < NUM_BUTTONS; i++)
      {
        if (buttonInfo[i].pressed == 0)
        {
          if (digitalRead(buttonInfo[i].pin))
          {
            buttons |= (1 << i);
          }
        }
      }
      usermod[FPSTR(_buttoninfo)] = buttons;
#endif // ENABLE_HISTORY
    }

    unsigned int getTemperatureDelay(int change)
    {
      if (change == 1)
        return 400;
      return 2000 + (203 * (change - 1));
    }

    void setup() {
      // do your set-up here
      buttonInfo[POWER_BUTTON].pin = POWER_BUTTON_PIN;
      buttonInfo[HOLD_BUTTON].pin = HOLD_BUTTON_PIN;
      buttonInfo[BOIL_BUTTON].pin = BOIL_BUTTON_PIN;
      buttonInfo[PLUS_BUTTON].pin = PLUS_BUTTON_PIN;
      buttonInfo[MINUS_BUTTON].pin = MINUS_BUTTON_PIN;
      for (int i = 0; i < NUM_BUTTONS; i++) {
        buttonInfo[i].pressed = 0;
      }
      pinMode(POWER_LED_PIN, INPUT);
      pinMode(HOLD_LED_PIN, INPUT);
      updatePressed(millis(), true);


#if ENABLE_MCP3201
      spiPort.begin(36, 37, 35, 34);
      spiPort.setHwCs(1);
#endif //ENABLE_MCP3201

#if ENABLE_HISTORY
      initHistory();
#endif //ENABLE_HISTORY

      initDone = true;
    }

    void setNewState(KettleState newState)
    {
      if (newState == currentState)
        return;

      // Certain transitions might be tricky. Cover those bases.
      if (currentState == STATE_7_ESTABLISH_KNOWN_TEMPERATURE && (newState != STATE_8_LOWER_TEMP_TO_DESIRED))
      {
        currentSetTemperature = 0;
      }
      if (currentState == STATE_8_LOWER_TEMP_TO_DESIRED && (newState != STATE_9_ENACT_HOLD))
      {
        currentSetTemperature = 0;
      }
      logHistory(getStringFromState(currentState) + "->" + getStringFromState(newState));
      currentState = newState;
      timeStateEntered = millis();
    }

    void stateLogic(unsigned long timeNow) {
      uint8_t powerled = digitalRead(POWER_LED_PIN);
      uint8_t holdled = digitalRead(HOLD_LED_PIN);

      if (currentState == STATE_TURNOFF)
      {
        if (!powerled)
        {
          setNewState(STATE_IDLE);
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == STATE_1_ENSURE_OFF)
      {
        if (!powerled)
        {
          setNewState(STATE_2_TURN_ON);
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == STATE_2_TURN_ON)
      {
        if (powerled)
        {
          setNewState(STATE_3_BOIL);
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == STATE_3_BOIL)
      {
        if (goalSetTemperature == 208)
        {
          pressButton(BOIL_BUTTON, 100);
          setNewState(STATE_4_WAIT_FOR_BOIL);
        }
        else
        {
          setNewState(STATE_6_TURN_ON);
        }
      }
      else if (currentState == STATE_4_WAIT_FOR_BOIL)
      {
        if (!powerled)
        {
          setNewState(STATE_5_ENSURE_OFF);
        }
      }
      else if (currentState == STATE_5_ENSURE_OFF)
      {
        if (!powerled)
        {
          setNewState(STATE_6_TURN_ON);
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == STATE_6_TURN_ON)
      {
        if (powerled)
        {
          setNewState(STATE_7_ESTABLISH_KNOWN_TEMPERATURE);
          plannedSetTemperature = 0;
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == STATE_7_ESTABLISH_KNOWN_TEMPERATURE)
      {
        if (plannedSetTemperature != 0)
        {
          // Planned set temperature action completed
          currentSetTemperature = plannedSetTemperature;
          plannedSetTemperature = 0;
        }
        if (currentSetTemperature != 0)
        {
          setNewState(STATE_8_LOWER_TEMP_TO_DESIRED);
        }
        else if (goalSetTemperature >= ((208 + 140) / 2))
        {
          plannedSetTemperature = 208;
          pressButton(PLUS_BUTTON, 18000);
        }
        else
        {
          plannedSetTemperature = 140;
          pressButton(MINUS_BUTTON, 18000);
        }
      }
      else if (currentState == STATE_8_LOWER_TEMP_TO_DESIRED)
      {
        if (plannedSetTemperature != 0)
        {
          // Planned set temperature action completed
          currentSetTemperature = plannedSetTemperature;
          plannedSetTemperature = 0;
        }
        if (currentSetTemperature == goalSetTemperature)
        {
          setNewState(STATE_9_ENACT_HOLD);
        }
        else if (currentSetTemperature > goalSetTemperature)
        {
          pressButton(MINUS_BUTTON, getTemperatureDelay(currentSetTemperature - goalSetTemperature));
          plannedSetTemperature = goalSetTemperature;
        }
        else
        {
          pressButton(PLUS_BUTTON, getTemperatureDelay(goalSetTemperature - currentSetTemperature));
          plannedSetTemperature = goalSetTemperature;
        }
      }
      else if (currentState == STATE_9_ENACT_HOLD)
      {
        if (desiredHoldTime == 0)
        {
          setNewState(STATE_IDLE);
        }
        else if ((timeNow - timeStateEntered) < 2000)
        {
          // do nothing for a bit
        }
        else if (holdled)
        {
          setNewState(STATE_10_MAINTAIN_HOLD);
        }
        else
        {
          pressButton(HOLD_BUTTON, 100);
        }
      }
      else if (currentState == STATE_10_MAINTAIN_HOLD)
      {
        if (!holdled)
        {
          // Hold LED turned off?
          setNewState(STATE_IDLE);
        }
        else if (desiredHoldTime == 1)
        {
          //Do nothing
          setNewState(STATE_IDLE);
        }
        else if ((timeNow - timeStateEntered) / 1000 >= desiredHoldTime)
        {
          setNewState(STATE_11_TURN_HOLD_OFF);
        }
      }
      else if (currentState == STATE_11_TURN_HOLD_OFF)
      {
        if (!holdled)
        {
          setNewState(STATE_IDLE);
        }
        else
        {
          pressButton(HOLD_BUTTON, 100);
        }
      }
    }


    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() {
      //Serial.println("Connected to WiFi!");
    }

    void loop() {
      if (strip.isUpdating()) {
        return;
      }

      unsigned long timeNow = millis();

#if ENABLE_MCP3201
      if (timeNow - voltageLastRead >= 20)
      {
        updateVoltage();
      }
      uint16_t voltage = (uint16_t)(voltageAverage / VOLTAGE_AVERAGES);
#else
      uint16_t voltage = 3000;
#endif //ENABLE_MCP3201
      bool kettlePresent = voltage <= 4070;
      bool releaseAll = false;

      // If the kettle is missing, cease all operation
      if (!kettlePresent)
      {
        setNewState(STATE_IDLE);
        releaseAll = true;
      }

      // Update the pressed buttons
      updatePressed(timeNow, releaseAll);
      // Check for if the user is pressing unpressed buttons
      checkButtonPresses(timeNow);

      if (!buttonsHeld)
      {
        // Also, if we are not pressing buttons then we can run the state machine
        stateLogic(timeNow);
      }
    }

#if ENABLE_HISTORY
  String history[HISTORY_LENGTH];
  unsigned long historyTimestamp[HISTORY_LENGTH];
  unsigned int nextHistoryEntry = 0;
  unsigned int numHistoryEntries = 0;

  void initHistory()
  {
    for (int i = 0; i < HISTORY_LENGTH; i++)
    {
#if HISTORY_STRINGS
      history[i] = "";
#else // HISTORY_STRINGS
      history[i] = 0;
#endif //HISTORY_STRINGS
      historyTimestamp[i] = 0;
    }
    nextHistoryEntry = 0;
    numHistoryEntries = 0;
  }

#if !HISTORY_STRINGS
  void logButtons(int buttons, unsigned long timestamp)
  {
    if ((numHistoryEntries == 0) || (buttons != history[lastHistoryEntry]))
    {
      lastHistoryEntry = (lastHistoryEntry + 1) % HISTORY_LENGTH;
      history[lastHistoryEntry] = buttons;
      historyTimestamp[lastHistoryEntry] = timestamp;
      numHistoryEntries++;
    }
  }
#endif
#if HISTORY_STRINGS
  void logHistory(String message)
  {
    history[nextHistoryEntry] = message;
    historyTimestamp[nextHistoryEntry] = millis();
    nextHistoryEntry = (nextHistoryEntry + 1) % HISTORY_LENGTH;
    numHistoryEntries++;
  }
#endif //HISTORY_STRINGS
#endif //ENABLE_HISTORY

// stole from usermod_v2_klipper_percentage
  void addToJsonState(JsonObject &root)
  {
    JsonObject usermod = root[FPSTR(_name)];
    if (usermod.isNull())
    {
      usermod = root.createNestedObject(FPSTR(_name));
    }
    usermod[FPSTR(_enabled)] = 0;
  }
  void readFromJsonState(JsonObject &root)
  {
    JsonObject usermod = root[FPSTR(_name)];
    if (!usermod.isNull())
    {
      if (usermod[FPSTR(_enabled)].is<bool>())
      {
        bool enabled = usermod[FPSTR(_enabled)].as<bool>();
        if(enabled)
        {
          goalSetTemperature = 208;
          desiredHoldTime = 0;
          if (usermod[FPSTR(_hold)].is<bool>())
          {
            if(usermod[FPSTR(_hold)].as<bool>())
            {
              desiredHoldTime = 1;
            }
          }
          else if (usermod[FPSTR(_hold)].is<unsigned int>())
          {
            desiredHoldTime = usermod[FPSTR(_hold)].as<unsigned int>();
          }
          if (usermod[FPSTR(_temperature)].is<unsigned int>())
            goalSetTemperature = usermod[FPSTR(_temperature)].as<unsigned int>();
          logHistory("Starting: temp=" + String(goalSetTemperature) + " hold=" + String(desiredHoldTime));
          setNewState(STATE_1_ENSURE_OFF);
        }
        else
        {
          logHistory("Turning off");
          setNewState(STATE_TURNOFF);
        }
      }
    }
  }

  void checkButtonPresses(unsigned long timeNow)
  {
    int buttons = 0;

    // Each button has a timer, if voltage is seen high, that timer is refreshed
    // If that timer runs out, the button is pressed
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
      // Only check the buttons that we are not pressing in software
      if (!buttonInfo[i].pressed)
      {
        // If we see the digital signal high, the button is unpressed, so refresh the timer
        if (digitalRead(buttonInfo[i].pin))
        {
          buttonInfo[i].lastSeenUnpressed = timeNow;
          buttons |= (1 << i);
        }
        else
        {
          if ((timeNow - buttonInfo[i].lastSeenUnpressed) > 50)
          {
            // Timer ran out - we have a user button press
            userPressedButton(i);
            buttonInfo[i].lastSeenUnpressed = timeNow;
          }
        }
      }
    }
#if ENABLE_HISTORY && !HISTORY_STRINGS
    logButtons(buttons, timeNow);
#endif //ENABLE_HISTORY && !HISTORY_STRINGS
  }

  void userPressedButton(int button)
  {
    switch(button)
    {
      default:
        // If the user interacts, then we stop what we're doing
        // Also, forget the known set temperature because that might get adjusted
        if ((currentState != STATE_IDLE) || (currentSetTemperature != 0))
        {
          logHistory("Resetting state because of button " + String(button));
        }
        currentSetTemperature = 0;
        setNewState(STATE_IDLE);
        break;
    }
  }

};

const char KettleUsermod::_name[]           PROGMEM = "Kettle";
const char KettleUsermod::_enabled[]        PROGMEM = "enabled";
const char KettleUsermod::_powerled[]        PROGMEM = "powerled";
const char KettleUsermod::_holdled[]        PROGMEM = "holdled";
const char KettleUsermod::_voltage[]        PROGMEM = "voltage";
const char KettleUsermod::_kettlepresent[]        PROGMEM = "kettlepresent";
const char KettleUsermod::_temperature[]        PROGMEM = "temperature";
const char KettleUsermod::_hold[]        PROGMEM = "hold";
const char KettleUsermod::_currentstate[]        PROGMEM = "currentstate";
const char KettleUsermod::_buttoninfo[]        PROGMEM = "buttoninfo";
const char KettleUsermod::_plus[]        PROGMEM = "plus";
const char KettleUsermod::_minus[]        PROGMEM = "minus";
const uint16_t KettleUsermod::voltages[NUM_TEMPS] =     {1663, 1985, 2185, 2334, 2539, 2701, 2800, 3106, 3211, 3331, 3459,3728, 3833, 3895, 3922, 3968};
const uint16_t KettleUsermod::temperatures[NUM_TEMPS] = {2080, 1920, 1810, 1740, 1630, 1540, 1490, 1290, 1240, 1130, 1040, 770,  600,  520,  460,  320};