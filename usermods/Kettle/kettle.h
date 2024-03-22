
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

#define USE_HEATER_BUTTON (0)

#define NUM_BUTTONS  (6 + USE_HEATER_BUTTON)
#define POWER_BUTTON (0)
#define HOLD_BUTTON  (1)
#define MINUS_BUTTON (2)
#define PLUS_BUTTON  (3)
#define BOIL_BUTTON  (4)
#define PRESET_BUTTON (5)
#if USE_HEATER_BUTTON
#define HEATER_BUTTON (6)
#endif

#define POWER_LED_PIN    (6)
#define POWER_BUTTON_PIN (7)
#define HOLD_BUTTON_PIN  (2)
#define HOLD_LED_PIN     (1)
#define BOIL_BUTTON_PIN  (21)
#define PLUS_BUTTON_PIN  (5)
#define MINUS_BUTTON_PIN (4)
#define PRESET_BUTTON_PIN (8)
#define HEATER_BUTTON_PIN (9)

#define NUM_TEMPS (16)
#define VOLTAGE_AVERAGES (32)

#define ENABLE_HISTORY (1)
#if ENABLE_HISTORY
#define HISTORY_LENGTH (80)
#define HISTORY_STRINGS (1)
String history[HISTORY_LENGTH];
unsigned long historyTimestamp[HISTORY_LENGTH];
unsigned int nextHistoryEntry = 0;
unsigned int numHistoryEntries = 0;
unsigned int consumedHistoryEntries;
#endif //ENABLE_HISTORY
#define TIMESTAMP_LENGTH (40)
unsigned long timestamps[TIMESTAMP_LENGTH];
unsigned int nextTimestamp = 0;
unsigned long last_time;

#define THREADSAFE_ENTER noInterrupts()
#define THREADSAFE_EXIT interrupts()

enum KettleState {
  S_IDLE,
  S_TURNOFF,
  S_1_OFF,
  S_2_ON,
  S_3_WAIT,
  S_4_OFF,
  S_5_ON,
  S_6_ESTAB,
  S_7_ADJ,
  S_8_HOLD,
  S_9_MAINTAIN,
  S_10_OFF,
  S_11_WAIT,
};

String getStringFromState(KettleState state)
{
  switch (state)
  {
    case S_IDLE:
      return "S_IDLE";
    case S_TURNOFF:
      return "S_TURNOFF";
    case S_1_OFF:
      return "S_1_OFF";
    case S_2_ON:
      return "S_2_ON";
    case S_3_WAIT:
      return "S_3_WAIT";
    case S_4_OFF:
      return "S_4_OFF";
    case S_5_ON:
      return "S_5_ON";
    case S_6_ESTAB:
      return "S_6_ESTAB";
    case S_7_ADJ:
      return "S_7_ADJ";
    case S_8_HOLD:
      return "S_8_HOLD";
    case S_9_MAINTAIN:
      return "S_9_MAINTAIN";
    case S_10_OFF:
      return "S_10_OFF";
    case S_11_WAIT:
      return "S_11_WAIT";
    default:
      return "unknown state";
  }
}

class KettleUsermod;

struct InterruptInfo {
  KettleUsermod *pKettle;
  unsigned int buttonNum;
  bool attached;
  bool interrupt_seen;
};
void interruptHandler(void *pVoid);

struct ButtonInfo {
  unsigned long time_pressed;
  unsigned long duration;
  uint8_t pin;
  uint8_t pressed;
  uint8_t index;
  uint8_t monitor;
  unsigned long lastSeenUnpressed;
  InterruptInfo interruptInfo;
};

class KettleUsermod : public Usermod {

  private:

    // Private class members. You can declare variables and functions only accessible to your usermod here
    bool initDone = false;

    ButtonInfo buttonInfo[NUM_BUTTONS];
#if ENABLE_MCP3201
    uint32_t voltageAverage = 0;
    unsigned long voltageLastRead = 0;
    uint16_t voltage;
    uint16_t temperature;
    bool kettlePresent = 0;
#endif //ENABLE_MCP3201
    //buttonsHeld is true while we are holding a button and for 500ms after. It slows down the state machine so we press buttons slowly.
    bool buttonsHeld = 0;
    unsigned long lastButtonReleaseTime = 0;
    KettleState currentState = S_IDLE;
    unsigned long timeStateEntered = 0;
    uint16_t currentSetTemperature = 0;
    uint16_t plannedSetTemperature = 0;
    uint16_t goalSetTemperature = 0;
    uint32_t desiredHoldTime = 0;
    uint32_t desiredWaitTime = 0;
    String current_command = "";

    // Keep track of temperature history
    #define TEMPERATURE_HISTORY_LEN (20)
    unsigned long lastTimeNotHeating = 0;
    unsigned long lastTimeTemperatureLogged = 0;
    uint16_t temperature_history[TEMPERATURE_HISTORY_LEN];
    int fill_estimate = -1;

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
    static const char _press[];
    static const char _heating[];
    static const char _fill_estimate[];
    static const char _wait[];
    static const char _timestamps[];
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
            //DEBUG_PRINTF("Releasing button of pin %u\n", pButtonInfo->pin);
            logHistory("Release " + String(i));
            //pinMode(pButtonInfo->pin, INPUT_PULLUP);
            pinMode(pButtonInfo->pin, INPUT);
          } else {
            anyPressed = 1;
            pButtonInfo->timeUnpressed = 0;
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
      //DEBUG_PRINTF("Pressing button of pin %u\n", pButtonInfo->pin);
      logHistory("Press " + String(buttonId));
      pButtonInfo->pressed = 1;
      uint8_t output_level = (pButtonInfo->monitor) ? 0 : 1;
      digitalWrite(pButtonInfo->pin, output_level);
      pinMode(pButtonInfo->pin, OUTPUT);
      digitalWrite(pButtonInfo->pin, output_level);
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
      voltage = voltageAverage / VOLTAGE_AVERAGES;
      temperature = voltageToTemperature(voltage);
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
      usermod[FPSTR(_heating)] = digitalRead(HEATER_BUTTON_PIN);
      usermod[FPSTR(_currentstate)] = current_command;
#if ENABLE_MCP3201
      usermod[FPSTR(_voltage)] = voltage;
      usermod[FPSTR(_kettlepresent)] = voltage < 4000;
      usermod[FPSTR(_temperature)] = temperature;
#endif //ENABLE_MCP3201

#if ENABLE_HISTORY
      JsonArray arr = usermod.createNestedArray(FPSTR("history"));
      unsigned int start,length;
      THREADSAFE_ENTER;
      if (numHistoryEntries < HISTORY_LENGTH)
      {
        start = 0;
        length = numHistoryEntries;
      }
      else
      {
        length = HISTORY_LENGTH;
        start = nextHistoryEntry % HISTORY_LENGTH;
      }
      for (unsigned int offset = 0; offset < length; offset++)
      {
        unsigned int index = (start + offset) % HISTORY_LENGTH;
        arr.add(history[index]);
        arr.add(historyTimestamp[index]);
      }

      arr = usermod.createNestedArray(FPSTR(_timestamps));
      for (unsigned int i = 0; i < TIMESTAMP_LENGTH; i++) {
        arr.add(timestamps[i]);
      }
      THREADSAFE_EXIT;

      int buttons = 0;
      for (int i = 0; i < NUM_BUTTONS; i++)
      {
        if ((buttonInfo[i].pressed == 0) && buttonInfo[i].monitor)
        {
          if (digitalRead(buttonInfo[i].pin))
          {
            buttons |= (1 << i);
          }
        }
      }
      usermod[FPSTR(_buttoninfo)] = buttons;
      if (fill_estimate > 0) {
        // Round down to 50 mL
        usermod[FPSTR(_fill_estimate)] = fill_estimate - (fill_estimate % 50);
      } else {
        usermod[FPSTR(_fill_estimate)] = fill_estimate;
      }
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
      buttonInfo[PRESET_BUTTON].pin = PRESET_BUTTON_PIN;
#if USE_HEATER_BUTTON
      buttonInfo[HEATER_BUTTON].pin = HEATER_BUTTON_PIN;
#endif //USE_HEATER_BUTTON
      for (int i = 0; i < NUM_BUTTONS; i++) {
        buttonInfo[i].pressed = 0;
        buttonInfo[i].interruptInfo.pKettle = this;
        buttonInfo[i].interruptInfo.buttonNum = i;
        buttonInfo[i].interruptInfo.attached = 1;
        buttonInfo[i].index = i;
#if USE_HEATER_BUTTON
        buttonInfo[i].monitor = (i == HEATER_BUTTON) ? 0 : 1;
#else
        buttonInfo[i].monitor = 1;
#endif // USE_HEATER_BUTTON
        if (buttonInfo[i].monitor) {
          attachInterruptArg(buttonInfo[i].pin, interruptHandler, &buttonInfo[i], RISING);
        }
      }
      pinMode(POWER_LED_PIN, INPUT);
      pinMode(HOLD_LED_PIN, INPUT);
#if !USE_HEATOR_BUTTON
      pinMode(HEATER_BUTTON_PIN, INPUT);
#endif
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

    void setNewState(KettleState newState, String reason)
    {
      if (newState == S_IDLE) {
        // Clear the user command now
        current_command = "";
      }
      if (newState == currentState)
        return;

      // Always release buttons at state transitions
      updatePressed(millis(), true);

      // Certain transitions might be tricky. Cover those bases.
      if (currentState == S_6_ESTAB && (newState != S_7_ADJ))
      {
        currentSetTemperature = 0;
      }
      if (currentState == S_7_ADJ && (newState != S_8_HOLD))
      {
        currentSetTemperature = 0;
      }
      if (reason == "")
      {
        logHistory(getStringFromState(currentState) + "->" + getStringFromState(newState));
      }
      else
      {
        logHistory(getStringFromState(currentState) + "->" + getStringFromState(newState) + " (" + reason + ")");
      }
      currentState = newState;
      timeStateEntered = millis();
    }

    void stateLogic(unsigned long timeNow) {
      uint8_t powerled = digitalRead(POWER_LED_PIN);
      uint8_t holdled = digitalRead(HOLD_LED_PIN);

      if (currentState == S_TURNOFF)
      {
        if (!powerled && !holdled)
        {
          setNewState(S_IDLE, "");
        }
        else if ((timeNow - timeStateEntered) > 20000)
        {
          setNewState(S_IDLE, "failed to turn off");
        }
        else if (holdled)
        {
          pressButton(HOLD_BUTTON, 100);
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == S_1_OFF)
      {
        if (!powerled && !holdled)
        {
          setNewState(S_2_ON, "");
        }
        else if ((timeNow - timeStateEntered) > 20000)
        {
          setNewState(S_IDLE, "failed to turn off");
        }
        else if (holdled)
        {
          pressButton(HOLD_BUTTON, 100);
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == S_2_ON)
      {
        if (powerled)
        {
          setNewState(S_3_WAIT, "");
        }
        else if ((timeNow - timeStateEntered) > 20000)
        {
          setNewState(S_IDLE, "failed to turn on");
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == S_3_WAIT)
      {
        if (!powerled)
        {
          setNewState(S_4_OFF, "Power finished");
        }
        else if ((temperature / 10) >= goalSetTemperature)
        {
          setNewState(S_4_OFF, "Temp reached");
        }
      }
      else if (currentState == S_4_OFF)
      {
        if (!powerled)
        {
          setNewState(S_5_ON, "");
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == S_5_ON)
      {
        if (powerled)
        {
          setNewState(S_6_ESTAB, "");
          plannedSetTemperature = 0;
        }
        else
        {
          pressButton(POWER_BUTTON, 100);
        }
      }
      else if (currentState == S_6_ESTAB)
      {
        if (plannedSetTemperature != 0)
        {
          // Planned set temperature action completed
          currentSetTemperature = plannedSetTemperature;
          plannedSetTemperature = 0;
        }
        if (currentSetTemperature != 0)
        {
          setNewState(S_7_ADJ, "");
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
      else if (currentState == S_7_ADJ)
      {
        if (plannedSetTemperature != 0)
        {
          // Planned set temperature action completed
          currentSetTemperature = plannedSetTemperature;
          plannedSetTemperature = 0;
        }
        if (currentSetTemperature == goalSetTemperature)
        {
          setNewState(S_8_HOLD, "");
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
      else if (currentState == S_8_HOLD)
      {
        if (desiredHoldTime == 0)
        {
          setNewState(S_IDLE, "No hold");
        }
        else if ((timeNow - timeStateEntered) < 2000)
        {
          // do nothing for a bit
        }
        else if (holdled)
        {
          // If we want a specific hold duration, we have to wait for after it's finished.
          // If we just want the generic hold, we don't have to wait.
          if ((desiredHoldTime == 0) || !powerled)
          {
            setNewState(S_9_MAINTAIN, "");
          }
        }
        else
        {
          pressButton(HOLD_BUTTON, 100);
        }
      }
      else if (currentState == S_9_MAINTAIN)
      {
        if (!holdled)
        {
          // Hold LED turned off?
          setNewState(S_IDLE, "Hold LED disappeared");
        }
        else if (desiredHoldTime == 1)
        {
          //Do nothing
          setNewState(S_IDLE, "No hold duration specified");
        }
        else if ((timeNow - timeStateEntered) / 1000 >= desiredHoldTime)
        {
          setNewState(S_10_OFF, "Duration over");
        }
      }
      else if (currentState == S_10_OFF)
      {
        if (!holdled)
        {
          setNewState(S_IDLE,"");
        }
        else
        {
          pressButton(HOLD_BUTTON, 100);
        }
      }
      else if (currentState == S_11_WAIT)
      {
        unsigned long alreadyWaited = (timeNow - timeStateEntered) / 1000;
        makeCurrentCommand(alreadyWaited);
        if (alreadyWaited >= desiredWaitTime) {
          setNewState(S_1_OFF, "");
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

String currentlyPrinting;
unsigned int currentlyPrintingOffset;
    void doHistoryToSerial()
    {
#ifdef WLED_DEBUG
      int availableForWrite = DEBUGOUT.availableForWrite();
#if ENABLE_HISTORY
      while (availableForWrite > 100) {
        if (currentlyPrintingOffset < currentlyPrinting.length()) {
          DEBUGOUT.write(currentlyPrinting.charAt(currentlyPrintingOffset++));
          availableForWrite--;
          continue;
        }
        if ((consumedHistoryEntries + HISTORY_LENGTH) < numHistoryEntries) {
          consumedHistoryEntries = numHistoryEntries - HISTORY_LENGTH;
          continue;
        }
        else if (consumedHistoryEntries < numHistoryEntries) {
          currentlyPrinting = history[consumedHistoryEntries % HISTORY_LENGTH] + "\n";
          currentlyPrintingOffset = 0;
          //DEBUG_PRINTLN(history[consumedHistoryEntries % HISTORY_LENGTH]);
          consumedHistoryEntries++;
          continue;
        } else {
          break;
        }
      }
#endif // ENABLE_HISTORY
#endif //ifdef WLED_DEBUG
    }

    unsigned int maxUartSpace = 0;
    void loop() {
      if (strip.isUpdating()) {
        return;
      }

      unsigned long timeNow = millis();

      // Debugging: what timestamps did we enter this loop at?
      if ((timeNow - last_time) > 10) {
        timestamps[nextTimestamp] = timeNow;
        nextTimestamp = (nextTimestamp + 1) % TIMESTAMP_LENGTH;
        timestamps[nextTimestamp] = (timeNow - last_time);
        nextTimestamp = (nextTimestamp + 1) % TIMESTAMP_LENGTH;
      }
      last_time = timeNow;

#if ENABLE_MCP3201
      if (timeNow - voltageLastRead >= 20)
      {
        THREADSAFE_ENTER;
        updateVoltage();
        THREADSAFE_ENTER;
      }
#else
      uint16_t voltage = 3000;
#endif //ENABLE_MCP3201
      THREADSAFE_EXIT;
      kettlePresent = voltage <= 4000;
      bool releaseAll = false;

      // If the kettle is missing, cease all operation
      if (!kettlePresent && currentState != S_11_WAIT)
      {
        setNewState(S_IDLE, "Kettle missing");
        releaseAll = true;
      }
      THREADSAFE_EXIT;

      // Update the pressed buttons
      THREADSAFE_ENTER;
      updatePressed(timeNow, releaseAll);
      THREADSAFE_EXIT;

      // Check for if the user is pressing unpressed buttons
      THREADSAFE_ENTER;
      checkButtonPresses(timeNow);
      THREADSAFE_EXIT;

      THREADSAFE_ENTER;
      if (!buttonsHeld)
      {
        // Also, if we are not pressing buttons then we can run the state machine
        stateLogic(timeNow);
      }
      THREADSAFE_EXIT;

      THREADSAFE_ENTER;
      temperatureTracking(timeNow);
      THREADSAFE_EXIT;

      THREADSAFE_ENTER;
      doHistoryToSerial();
      THREADSAFE_EXIT;
  }

#if ENABLE_HISTORY

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
    consumedHistoryEntries = 0;
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
    THREADSAFE_ENTER;
    JsonObject usermod = root[FPSTR(_name)];
    if (usermod.isNull())
    {
      usermod = root.createNestedObject(FPSTR(_name));
    }
    usermod[FPSTR(_enabled)] = 0;
    THREADSAFE_EXIT;
  }
  void readFromJsonState(JsonObject &root)
  {
    THREADSAFE_ENTER;
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
          desiredWaitTime = 0;
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
          if (usermod[FPSTR(_wait)].is<unsigned int>())
            desiredWaitTime = usermod[FPSTR(_wait)].as<unsigned int>();
          String message = "Starting: temp=" + String(goalSetTemperature);
          if (desiredHoldTime != 0) {
            message += " hold=" + String(desiredHoldTime);
          }
          if (desiredWaitTime != 0) {
            message += " wait=" + String(desiredWaitTime);
          }

          // Build current_command string for the UI
          makeCurrentCommand(0);
          if (desiredWaitTime == 0) {
            setNewState(S_1_OFF, message);
          } else {
            setNewState(S_11_WAIT, message);
          }
        }
        else
        {
          setNewState(S_TURNOFF, "turning off");
        }
      }
      else if (usermod[FPSTR(_press)].is<unsigned int>())
      {
        unsigned int buttonId = usermod[FPSTR(_press)].as<unsigned int>();
        if (buttonId < NUM_BUTTONS)
        {
          unsigned int duration = 100;
          if (usermod[FPSTR("duration")].is<unsigned int>())
          {
            duration = usermod[FPSTR("duration")].as<unsigned int>();
          }
          pressButton(buttonId, duration);
        } else {
          *((int *)0) = 1;
        }
      }
    }
    THREADSAFE_EXIT;
  }

  unsigned long timeSinceLastCheckButton = 0;
  void checkButtonPresses(unsigned long timeNow)
  {
    int buttons = 0;

    // Only count a max of 5ms per check, to try to fix glitches
    // The interrupt should be working, but shrug
    unsigned long duration = timeNow - timeSinceLastCheckButton;
    if (duration > 5) {
      duration = 5;
    }
    timeSinceLastCheckButton = timeNow;

    // Each button has a timer, if voltage is seen high, that timer is refreshed
    // If that timer runs out, the button is pressed
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
      ButtonInfo *pButtonInfo = &buttonInfo[i];
      // Only check the buttons that we are not pressing in software
      if (!pButtonInfo->pressed && pButtonInfo->monitor)
      {
        // If we see the digital signal high, the button is unpressed, so refresh the timer
        if (pButtonInfo->interruptInfo.interrupt_seen || digitalRead(pButtonInfo->pin))
        {
          pButtonInfo->interruptInfo.interrupt_seen = 0;
          pButtonInfo->timeUnpressed = 0;
          buttons |= (1 << i);
        }
        else
        {
          if (duration > 0) {
            pButtonInfo->timeUnpressed += duration;
            if (pButtonInfo->timeUnpressed > 50)
            {
              // Timer ran out - we have a user button press
              userPressedButton(i);
              pButtonInfo->timeUnpressed = 0;
            }
            else if (pButtonInfo->timeUnpressed >= 10)
            {
              // Let's start checking on it with an interrupt
              if (!pButtonInfo->interruptInfo.attached)
              {
                pButtonInfo->interruptInfo.attached = 1;
                attachInterruptArg(pButtonInfo->pin, interruptHandler, &buttonInfo[i], RISING);
                //logHistory("Interrupt on " + String(i));
              }
            }
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
    if ((button == PLUS_BUTTON) || (button == MINUS_BUTTON) || (button == PRESET_BUTTON))
    {
      if (currentSetTemperature != 0)
      {
        logHistory("Resetting known temp because of button " + String(button));
        currentSetTemperature = 0;
      }
    }
    // If the user interacts, then we stop what we're doing
    // Also, forget the known set temperature because that might get adjusted
    if (currentState != S_IDLE)
    {
      logHistory("Resetting state because of button " + String(button));
      setNewState(S_IDLE, "button " + String(button));

      // log timestamps
      for (int i = 0; i < TIMESTAMP_LENGTH; i += 10) {
        String message = "";
        for (int j = 0; (j < 10) && (i+j < TIMESTAMP_LENGTH); j++) {
          message += String(timestamps[i+j]);
          message += ",";
        }
        logHistory(message);
      }
    }
  }

  //void buttonIsUnpressed(unsigned int buttonNum)
  //{
  //  THREADSAFE_ENTER;
  //  if (buttonNum >= NUM_BUTTONS) return;
  //  //logHistory("Interrupt off " + String(buttonNum));
  //  buttonInfo[buttonNum].lastSeenUnpressed = millis();
  //  detachInterrupt(buttonInfo[buttonNum].pin);
  //  buttonInfo[buttonNum].interruptInfo.attached = 0;
  //  THREADSAFE_EXIT;
  //}

  // Estimate that we add 3746 degrees F per mL per 10 seconds
  void makeCapacityEstimate(unsigned int ago) {
    if (ago >= TEMPERATURE_HISTORY_LEN) {
      ago = TEMPERATURE_HISTORY_LEN - 1;
    }

    unsigned int difference = temperature_history[0] - temperature_history[ago];
    if (difference > 2500) {
      fill_estimate = -1;
      return;
    }

    unsigned int capacity = (37460 * ago) / difference;
    logHistory("Estimate: " + String(capacity) + " from " + String(ago) + "0 ago difference " + String(difference));

    if (capacity > 1500) {
      fill_estimate = -1;
      return;
    }
    difference = (fill_estimate > capacity) ? (fill_estimate - capacity) : (capacity - fill_estimate);
    if (difference > 50) {
      fill_estimate = capacity;
    }
  }

  void temperatureTracking(unsigned long timeNow) {
    // Check for constant duration of heating
#if !USE_HEATER_BUTTON
    if (!kettlePresent) {
      fill_estimate = -1;
    }

    if (!kettlePresent || !digitalRead(HEATER_BUTTON_PIN)) {
      if ((timeNow - lastTimeNotHeating) >= 10000) {
        logHistory("Stopped heating after " + String(timeNow - lastTimeNotHeating));
      }
      lastTimeNotHeating = timeNow;
    }

    // Log temperature always
    if ((timeNow - lastTimeTemperatureLogged) >= 10000) {
      lastTimeTemperatureLogged = timeNow;
      for (int i = TEMPERATURE_HISTORY_LEN - 1; i > 0; i--) {
        temperature_history[i] = temperature_history[i-1];
      }
      temperature_history[0] = temperature;

      // If the temperature has gone down, someone added water
      if ((temperature_history[1] > temperature_history[0]) && ((temperature_history[1] - temperature_history[0]) > 50)) {
        fill_estimate = -1;
        lastTimeNotHeating = timeNow;
      }

      //if ((timeNow - lastTimeNotHeating) >= 60000) {
      //  logHistory("Heat: " + String(temperature_history[5]) + "->" + String(temperature_history[0]));
      //}
      if ((timeNow - lastTimeNotHeating) >= 20000) {
        unsigned long ago = (timeNow - lastTimeNotHeating - 20000) / 10000;
        if (ago < 1) {
          // 10 seconds less estimate time @ 20 seconds
          // Just for a quicker estimate
          ago = 1;
        }

        makeCapacityEstimate(ago);
      }
    }
#endif //!USE_HEATER_BUTTON
  }

  void makeCurrentCommand(unsigned long already_waited) {
    unsigned long waitTime = (desiredWaitTime > already_waited) ? desiredWaitTime - already_waited : 0;
    String message;
    if (waitTime != 0)
    {
      message = "Wait for ";
      if (waitTime > 3600) {
        message += String(waitTime / 3600) + " hours ";
        waitTime = waitTime % 3600;
      }
      message += String(waitTime / 60);
      uint32_t seconds = waitTime % 60;
      if (seconds >= 6)
      {
        message += "." + String(seconds / 6);
      }
      message += " mins, then b";
    }
    else
    {
      message = "B";
    }

    message += "oil to " + String(goalSetTemperature);
    if (desiredHoldTime > 0)
    {
      message += " and hold";
    }

    current_command = message;
  }
};

void interruptHandler(void *pVoid)
{
  ButtonInfo *pButtonInfo = (ButtonInfo *)pVoid;
  pButtonInfo->interruptInfo.interrupt_seen = 1;
  detachInterrupt(pButtonInfo->pin);
  pButtonInfo->interruptInfo.attached = 0;
  //pButtonInfo->interruptInfo.pKettle->logHistory("Interrupt off " + String(pButtonInfo->index));
}

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
const char KettleUsermod::_press[]        PROGMEM = "press";
const char KettleUsermod::_heating[]        PROGMEM = "heating";
const char KettleUsermod::_fill_estimate[]        PROGMEM = "fill_estimate";
const char KettleUsermod::_wait[]        PROGMEM = "wait";
const char KettleUsermod::_timestamps[]        PROGMEM = "timestamps";
const uint16_t KettleUsermod::voltages[NUM_TEMPS] =     {1663, 1985, 2185, 2334, 2539, 2701, 2800, 3106, 3211, 3331, 3459,3728, 3833, 3895, 3922, 3968};
const uint16_t KettleUsermod::temperatures[NUM_TEMPS] = {2080, 1920, 1810, 1740, 1630, 1540, 1490, 1290, 1240, 1130, 1040, 770,  600,  520,  460,  320};