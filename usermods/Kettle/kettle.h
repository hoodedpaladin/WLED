
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
#define ENABLE_MCP3202 (0)
#define ENABLE_MCP3201 (1)
#define POWER_LED_PIN (6)
#define POWER_BUTTON_PIN (7)
#define NUM_TEMPS (5)
#define VOLTAGE_AVERAGES (32)
class KettleUsermod : public Usermod {

  private:

    // Private class members. You can declare variables and functions only accessible to your usermod here
    bool enabled = true;
    bool initDone = false;
    bool buttonPressed = false;
    bool pleasePressButton = false;
    unsigned long lastTime = 0;
    uint32_t voltageAverage = 0;
    unsigned long voltageLastRead = 0;
    SPIClass spiPort;
    static const char _name[];
    static const char _enabled[];
    static const char _powerled[];
    static const char _voltage[];
    static const char _kettlepresent[];
    static const char _temperature[];
   static const uint16_t voltages[];
   static const uint16_t temperatures[];

#if 0
    // set your config variables to their boot default value (this can also be done in readFromConfig() or a constructor if you prefer)
    bool testBool = false;
    unsigned long testULong = 42424242;
    float testFloat = 42.42;
    String testString = "Forty-Two";

    // These config variables have defaults set inside readFromConfig()
    int testInt;
    long testLong;
    int8_t testPins[2];

    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _enabled[];


    // any private methods should go here (non-inline method should be defined out of class)
    void publishMqtt(const char* state, bool retain = false); // example for publishing MQTT message
#endif


  public:
    KettleUsermod() : spiPort(SPIClass(FSPI)) {}

    // non WLED related methods, may be used for data exchange between usermods (non-inline methods should be defined out of class)
    void setPressed() {
        if (buttonPressed) {
            digitalWrite(POWER_BUTTON_PIN, 0);
            pinMode(POWER_BUTTON_PIN, OUTPUT);
            digitalWrite(POWER_BUTTON_PIN, 0);
        } else {
            pinMode(POWER_BUTTON_PIN, INPUT);
        }
    }

    /**
     * Enable/Disable the usermod
     */
    inline void enable(bool enable) {
        enabled = enable;
        buttonPressed = false;
        setPressed();
        lastTime = millis();
    }

    /**
     * Get usermod enabled/disabled state
     */
    inline bool isEnabled() { return enabled; }

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
      // progress = vdiffcurrent / vdiffrange
      // output = tlow + (progress * tdiffrange)
      // output = tlow + ((vdiffcurrent * tdiffrange) / vdiffrange)
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
      uint16_t voltage = (uint16_t)(voltageAverage / VOLTAGE_AVERAGES);
      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull())
      {
        usermod = root.createNestedObject(FPSTR(_name));
      }
      usermod[FPSTR(_powerled)] = digitalRead(POWER_LED_PIN);
#if ENABLE_MCP3201
      usermod[FPSTR(_voltage)] = voltage;
      usermod[FPSTR(_kettlepresent)] = voltage < 4070;
      usermod[FPSTR(_temperature)] = voltageToTemperature(voltage);
#endif //ENABLE_MCP3201
    }

    void setup() {
      // do your set-up here
      DEBUG_PRINTF("Why hello there\n");

#if ENABLE_MCP3201
      spiPort.begin(36, 37, 35, 34);
      spiPort.setHwCs(1);
#endif //ENABLE_MCP3201
      pinMode(POWER_LED_PIN, INPUT);
      pinMode(POWER_BUTTON_PIN, INPUT);
      buttonPressed = false;
      setPressed();
      initDone = true;
    }


    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() {
      //Serial.println("Connected to WiFi!");
    }

    void loop() {
      unsigned long timeNow = millis();

      if (!enabled || strip.isUpdating()) {
        return;
      }

      if (!buttonPressed && pleasePressButton)
      {
        buttonPressed = 1;
        pleasePressButton = 0;
        setPressed();
        lastTime = timeNow;
        DEBUG_PRINTF("Pressing button\n");
      }
      else if (buttonPressed && (millis() - lastTime) > 100)
      {
        buttonPressed = 0;
        setPressed();
        DEBUG_PRINTF("Releasing button\n");
      }

      if (timeNow - voltageLastRead >= 20)
      {
        updateVoltage();
      }
    }

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
        if(usermod[FPSTR(_enabled)].as<bool>())
        {
          pleasePressButton = 1;
        }
      }
    }
  }

#if 0
    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root)
    {
      // if "u" object does not exist yet wee need to create it
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      //this code adds "u":{"ExampleUsermod":[20," lux"]} to the info object
      //int reading = 20;
      //JsonArray lightArr = user.createNestedArray(FPSTR(_name))); //name
      //lightArr.add(reading); //value
      //lightArr.add(F(" lux")); //unit

      // if you are implementing a sensor usermod, you may publish sensor data
      //JsonObject sensor = root[F("sensor")];
      //if (sensor.isNull()) sensor = root.createNestedObject(F("sensor"));
      //temp = sensor.createNestedArray(F("light"));
      //temp.add(reading);
      //temp.add(F("lux"));
    }


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root)
    {
      if (!initDone || !enabled) return;  // prevent crash on boot applyPreset()

      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));

      //usermod["user0"] = userVar0;
    }




    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     * 
     * CAUTION: serializeConfig() will initiate a filesystem write operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the loop, never in network callbacks!
     * 
     * addToConfig() will make your settings editable through the Usermod Settings page automatically.
     *
     * Usermod Settings Overview:
     * - Numeric values are treated as floats in the browser.
     *   - If the numeric value entered into the browser contains a decimal point, it will be parsed as a C float
     *     before being returned to the Usermod.  The float data type has only 6-7 decimal digits of precision, and
     *     doubles are not supported, numbers will be rounded to the nearest float value when being parsed.
     *     The range accepted by the input field is +/- 1.175494351e-38 to +/- 3.402823466e+38.
     *   - If the numeric value entered into the browser doesn't contain a decimal point, it will be parsed as a
     *     C int32_t (range: -2147483648 to 2147483647) before being returned to the usermod.
     *     Overflows or underflows are truncated to the max/min value for an int32_t, and again truncated to the type
     *     used in the Usermod when reading the value from ArduinoJson.
     * - Pin values can be treated differently from an integer value by using the key name "pin"
     *   - "pin" can contain a single or array of integer values
     *   - On the Usermod Settings page there is simple checking for pin conflicts and warnings for special pins
     *     - Red color indicates a conflict.  Yellow color indicates a pin with a warning (e.g. an input-only pin)
     *   - Tip: use int8_t to store the pin value in the Usermod, so a -1 value (pin not set) can be used
     *
     * See usermod_v2_auto_save.h for an example that saves Flash space by reusing ArduinoJson key name strings
     * 
     * If you need a dedicated settings page with custom layout for your Usermod, that takes a lot more work.  
     * You will have to add the setting to the HTML, xml.cpp and set.cpp manually.
     * See the WLED Soundreactive fork (code and wiki) for reference.  https://github.com/atuline/WLED
     * 
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      //save these vars persistently whenever settings are saved
      top["great"] = userVar0;
      top["testBool"] = testBool;
      top["testInt"] = testInt;
      top["testLong"] = testLong;
      top["testULong"] = testULong;
      top["testFloat"] = testFloat;
      top["testString"] = testString;
      JsonArray pinArray = top.createNestedArray("pin");
      pinArray.add(testPins[0]);
      pinArray.add(testPins[1]); 
    }


    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     * 
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     * 
     * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
     * 
     * getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
     * The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
     * 
     * This function is guaranteed to be called on boot, but could also be called every time settings are updated
     */
    bool readFromConfig(JsonObject& root)
    {
      // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
      // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top["great"], userVar0);
      configComplete &= getJsonValue(top["testBool"], testBool);
      configComplete &= getJsonValue(top["testULong"], testULong);
      configComplete &= getJsonValue(top["testFloat"], testFloat);
      configComplete &= getJsonValue(top["testString"], testString);

      // A 3-argument getJsonValue() assigns the 3rd argument as a default value if the Json value is missing
      configComplete &= getJsonValue(top["testInt"], testInt, 42);  
      configComplete &= getJsonValue(top["testLong"], testLong, -42424242);

      // "pin" fields have special handling in settings page (or some_pin as well)
      configComplete &= getJsonValue(top["pin"][0], testPins[0], -1);
      configComplete &= getJsonValue(top["pin"][1], testPins[1], -1);

      return configComplete;
    }


    /*
     * appendConfigData() is called when user enters usermod settings page
     * it may add additional metadata for certain entry fields (adding drop down is possible)
     * be careful not to add too much as oappend() buffer is limited to 3k
     */
    void appendConfigData()
    {
      oappend(SET_F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F(":great")); oappend(SET_F("',1,'<i>(this is a great config value)</i>');"));
      oappend(SET_F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F(":testString")); oappend(SET_F("',1,'enter any string you want');"));
      oappend(SET_F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F("','testInt');"));
      oappend(SET_F("addOption(dd,'Nothing',0);"));
      oappend(SET_F("addOption(dd,'Everything',42);"));
    }


    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw()
    {
      //strip.setPixelColor(0, RGBW32(0,0,0,0)) // set the first pixel to black
    }


    /**
     * handleButton() can be used to override default button behaviour. Returning true
     * will prevent button working in a default way.
     * Replicating button.cpp
     */
    bool handleButton(uint8_t b) {
      yield();
      // ignore certain button types as they may have other consequences
      if (!enabled
       || buttonType[b] == BTN_TYPE_NONE
       || buttonType[b] == BTN_TYPE_RESERVED
       || buttonType[b] == BTN_TYPE_PIR_SENSOR
       || buttonType[b] == BTN_TYPE_ANALOG
       || buttonType[b] == BTN_TYPE_ANALOG_INVERTED) {
        return false;
      }

      bool handled = false;
      // do your button handling here
      return handled;
    }
  

#ifndef WLED_DISABLE_MQTT
    /**
     * handling of MQTT message
     * topic only contains stripped topic (part after /wled/MAC)
     */
    bool onMqttMessage(char* topic, char* payload) {
      // check if we received a command
      //if (strlen(topic) == 8 && strncmp_P(topic, PSTR("/command"), 8) == 0) {
      //  String action = payload;
      //  if (action == "on") {
      //    enabled = true;
      //    return true;
      //  } else if (action == "off") {
      //    enabled = false;
      //    return true;
      //  } else if (action == "toggle") {
      //    enabled = !enabled;
      //    return true;
      //  }
      //}
      return false;
    }

    /**
     * onMqttConnect() is called when MQTT connection is established
     */
    void onMqttConnect(bool sessionPresent) {
      // do any MQTT related initialisation here
      //publishMqtt("I am alive!");
    }
#endif


    /**
     * onStateChanged() is used to detect WLED state change
     * @mode parameter is CALL_MODE_... parameter used for notifications
     */
    void onStateChange(uint8_t mode) {
      // do something if WLED state changed (color, brightness, effect, preset, etc)
    }


    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_EXAMPLE;
    }

   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
#endif
};


// add more strings here to reduce flash memory usage
//const char KettleUsermod::_name[]    PROGMEM = "ExampleUsermod";
#if 0
const char MyExampleUsermod::_enabled[] PROGMEM = "enabled";


// implementation of non-inline member methods

void MyExampleUsermod::publishMqtt(const char* state, bool retain)
{
#ifndef WLED_DISABLE_MQTT
  //Check if MQTT Connected, otherwise it will crash the 8266
  if (WLED_MQTT_CONNECTED) {
    char subuf[64];
    strcpy(subuf, mqttDeviceTopic);
    strcat_P(subuf, PSTR("/example"));
    mqtt->publish(subuf, 0, retain, state);
  }
#endif
}
#endif

const char KettleUsermod::_name[]           PROGMEM = "Kettle";
const char KettleUsermod::_enabled[]        PROGMEM = "enabled";
const char KettleUsermod::_powerled[]        PROGMEM = "powerled";
const char KettleUsermod::_voltage[]        PROGMEM = "voltage";
const char KettleUsermod::_kettlepresent[]        PROGMEM = "kettlepresent";
const char KettleUsermod::_temperature[]        PROGMEM = "temperature";
const uint16_t KettleUsermod::voltages[NUM_TEMPS] = {1694, 2513, 2701, 3460, 3849};
const uint16_t KettleUsermod::temperatures[NUM_TEMPS] = {2080, 1610, 1540, 1000, 590};