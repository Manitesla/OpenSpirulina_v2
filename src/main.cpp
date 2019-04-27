/*
 * Send Sensors Data To HTTP (PHP) Server
 *
 * Autor: Fran Romero https://github.com/yatan
 *        OpenSpirulina http://www.openspirulina.com
 *
 * Based on:
 * 
 * Ethernet web client sketch:
 *   Repeating Web client
 *   http://www.arduino.cc/en/Tutorial/WebClientRepeating
 *
 * DS18B20 sensor de temperatura para líquidos con Arduino:
 *   https://programarfacil.com/blog/arduino-blog/ds18b20-sensor-temperatura-arduino/
 *
 * pH
 *   phMeterSample.ino from: YouYou
 *
 * BH1750: lux sensor to mesure spirulina's biomass concentration.
 *   https://github.com/claws/BH1750
 * 
 * LCD LiquidCrystal_I2C LiquidCrystal Arduino library for the DFRobot I2C LCD displays:
 *   https://github.com/marcoschwartz/LiquidCrystal_I2C
 * 
 * GSM/GPRS A6 modem library:
 *   https://github.com/vshymanskyy/TinyGSM
 * 
 */
#include <Arduino.h>
#include "Configuration.h"                                 // General configuration file

// Third-party libs
#include <SPI.h>
#include <SD.h>
#include <TinyGsmClient.h>
#include <Ethernet2.h>                                     // Shield W5500 with Ethernet2.h will be used in this version of the project
#include "MemoryFree.h"

// OpenSpirulina libs
#include "Load_SD_Config.h"
#include "DateTime_RTC.h"                                  // Class to control RTC date time
#include "LCD_Screen.h"                                    // Class for LCD control
#include "DHT_Sensors.h"                                   // Class for control all DHT sensors
#include "DO_Sensor.h"                                     // Class for Optical Density control
#include "Lux_Sensor.h"                                    // Class for lux sensor control
#include "PH_Sensors.h"                                    // Class for pH sensors control
#include "WP_Temp_Sensors.h"                               // Class for DS18 Waterproof Temperature Sensors control
#include "Current_Sensors.h"                               // Class for current sensors control


/*****************
 * GLOBAL CONTROL
 *****************/
bool DEBUG = DEBUG_DEF_ENABLED;                            // Indicates whether the debug mode on serial monitor is active
bool LCD_enabled = LCD_DEF_ENABLED;                        // Indicates whether the LCD is active
bool RTC_enabled = RTC_DEF_ENABLED;                        // Indicates whether the RTC is active
bool SD_save_enabled = SD_SAVE_DEF_ENABLED;                // Indicates whether the save to SD is enabled

DateTime_RTC dateTimeRTC;                                  // RTC class object (DS3231 clock sensor)

LCD_Screen lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS,
                LCD_CONTRAST, LCD_BACKLIGHT_ENABLED);      // LCD screen

DHT_Sensors dht_sensors;                                   // Handle all DHT sensors
Lux_Sensor lux_sensor;                                     // Lux sensor with BH1750
DO_Sensor  do_sensor;                                      // Sensor 1 [DO] ("Optical Density")

PH_Sensors *pH_sensors;                                    // pH sensors class
WP_Temp_Sensors *wp_t_sensors;                             // DS18B20 Sensors class
Current_Sensors *curr_sensors;                             // Current sensors


File myFile;
char fileName[SD_MAX_FILENAME_SIZE] = "";                  // Name of file to save data readed from sensors



/*****
AUTHENTICATION ARDUINO
*****/
const uint16_t id_arduino  = 21;                           // Define de identity of Arduino
const uint16_t pin_arduino = 12345;

/****
NETWORK SETTINGS 
****/
const char  server[] = "sensors.openspirulina.com";        // Server connect for sending data
const uint16_t  port = 80;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};         // assign a MAC address for the ethernet controller:

/*****
GPRS credentials
*****/
const char apn[]  = "internet";                            // Leave empty, if missing user or pass
const char user[] = "";
const char pass[] = "";

bool conexio_internet = false;

/*
  _   _ _    _ __  __ ____  ______ _____     _____ ______ _   _  _____  ____  _____   _____
 | \ | | |  | |  \/  |  _ \|  ____|  __ \   / ____|  ____| \ | |/ ____|/ __ \|  __ \ / ____|
 |  \| | |  | | \  / | |_) | |__  | |__) | | (___ | |__  |  \| | (___ | |  | | |__) | (___
 | . ` | |  | | |\/| |  _ <|  __| |  _  /   \___ \|  __| | . ` |\___ \| |  | |  _  / \___ \
 | |\  | |__| | |  | | |_) | |____| | \ \   ____) | |____| |\  |____) | |__| | | \ \ ____) |
 |_| \_|\____/|_|  |_|____/|______|_|  \_\ |_____/|______|_| \_|_____/ \____/|_|  \_\_____/
 */
const uint8_t num_PIR = 0;                                 // PIR movement sensor. MAX 3 --> S'ha de traure i enlloc seu posar Current pin 
const uint8_t num_CO2 = 0;                                 // CO2 sensor MAX ?

enum opt_Internet_type {                                   // Valid internet types
	it_none = 0,
	it_Ethernet,
	it_GPRS,
	it_Wifi
};

const opt_Internet_type opt_Internet = it_none;            // None | Ethernet | GPRS Modem | Wifi <-- Why not? Dream on it


/*
  _____ _____ _   _  _____
 |  __ \_   _| \ | |/ ____|
 | |__) || | |  \| | (___
 |  ___/ | | | . ` |\___ \
 | |    _| |_| |\  |____) |
 |_|   |_____|_| \_|_____/
*/

/*   ANALOG PINS  */
const uint8_t pins_co2[num_CO2] = {};                      // CO2 pin (Analog)
const uint8_t pins_pir[num_PIR] = {};                      // PIR Pins  //S'ha de traure.


/*
   _____ _      ____  ____          _       __      __     _____   _____
  / ____| |    / __ \|  _ \   /\   | |      \ \    / /\   |  __ \ / ____|
 | |  __| |   | |  | | |_) | /  \  | |       \ \  / /  \  | |__) | (___
 | | |_ | |   | |  | |  _ < / /\ \ | |        \ \/ / /\ \ |  _  / \___ \
 | |__| | |___| |__| | |_) / ____ \| |____     \  / ____ \| | \ \ ____) |
  \_____|______\____/|____/_/    \_\______|     \/_/    \_\_|  \_\_____/
*/

// Array of PIR sensors
int array_pir[num_PIR];

//Time waiting for correct agitation of culture for correct mesuring DO
const unsigned long time_current = 30L * 1000L; //Time un seconds between current ini current end measure.

// Array of CO2 sensors
float array_co2[num_CO2];

// Lux ambient value
float lux;


//TODO: Cambiar tipo de dato de last_send de String a char[9]
//char last_send[9];
String last_send;                                          //Last time sended data
uint16_t loop_count = 0;


// GPRS Modem
#if DUMP_AT_COMMANDS == 1
    #include <StreamDebugger.h>
    StreamDebugger debugger(SERIAL_AT, SERIAL_MON);
    TinyGsm modem(debugger);
#else
    TinyGsm modem(SERIAL_AT);
#endif

// GSM Modem client
TinyGsmClient client(modem);

// Ethernet Network client
EthernetClient eth_client;




/*
  ______ _    _ _   _  _____ _______ _____ ____  _   _  _____  
 |  ____| |  | | \ | |/ ____|__   __|_   _/ __ \| \ | |/ ____| 
 | |__  | |  | |  \| | |       | |    | || |  | |  \| | (___   
 |  __| | |  | | . ` | |       | |    | || |  | | . ` |\___ \  
 | |    | |__| | |\  | |____   | |   _| || |__| | |\  |____) | 
 |_|     \____/|_| \_|\_____|  |_|  |_____\____/|_| \_|_____/  
 */

// Deteccio si hi ha moviment via PIR
bool detect_PIR(int pin) {
    //for(int i=0; i<num_PIR; i++){
    // Si hi ha moviment al pin PIR retorna true
    return (digitalRead(pin) == HIGH);
}

/* Capture light ambient with LDR sensor*/
float capture_LDR(uint8_t s_pin) {
    return analogRead(s_pin);
}

void sort_array(float* arr, const uint8_t n_samples) {
    float tmp_val;

    for (int i=0; i < (n_samples-1); i++) {
        for (int j=0; j < (n_samples-(i+1)); j++) {
            if (arr[j] > arr[j+1]) {
                tmp_val = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = tmp_val;
            }
        }
    }
}

float sort_and_filter_CO2(float* llistat) {
	// Define array without first and last n elements
	float llistat_output;
	// Sort normally
	sort_array(llistat, CO2_SENS_N_SAMP_READ);

	// Descartem els 2 primers i 2 ultims valors
	for (uint8_t i=2; i<(CO2_SENS_N_SAMP_READ-2); i++) {
		llistat_output += llistat[i];
	}
	return llistat_output / (CO2_SENS_N_SAMP_READ - 4);
}

/* Capture CO2 */
void capture_CO2() {
    if (DEBUG) SERIAL_MON.println(F("Capturing CO2"));

    // Take CO2_SENS_N_SAMP_READ samples of Every co2 sensor
    float mostres_co2[CO2_SENS_N_SAMP_READ];

    for (uint8_t j=0; j<CO2_SENS_N_SAMP_READ; j++) {
        // Read voltage
        float adc = analogRead(pins_co2[0]);               // Paso 1, conversión ADC de la lectura del pin analógico
        float voltaje = adc * 5.0 / 1023.0;                // Paso 2, obtener el voltaje
        float co2conc = voltaje * (-3157.89) + 1420.0;

        SERIAL_MON.print(co2conc);
        SERIAL_MON.print(F(" ppm CO2 ["));
        SERIAL_MON.print(j);
        SERIAL_MON.println(F("]"));

        // Save voltatge value
        mostres_co2[j] = co2conc;
        delay(100);
    }
    
    array_co2[0] = sort_and_filter_CO2(mostres_co2);
}

/* Show obteined vales from LCD */
void mostra_LCD() {
    lcd.clear();                        // Clear screen

    // Shows on LCD the average of the two temperatures of  the first pair
    if (wp_t_sensors && wp_t_sensors->get_n_pairs() > 0)
        lcd.add_value_read("T1:", wp_t_sensors->get_result_pair_mean(0));
    
    // Shows on LCD the average of the two temperatures of  the first pair
    if (wp_t_sensors && wp_t_sensors->get_n_pairs() > 1)
        lcd.add_value_read("T2:", wp_t_sensors->get_result_pair_mean(1));
    
    if (pH_sensors && pH_sensors->get_n_sensors() > 0)
        lcd.add_value_read("pH1:", pH_sensors->get_sensor_value(0));
    
    if (pH_sensors && pH_sensors->get_n_sensors() > 1)
        lcd.add_value_read("pH2:", pH_sensors->get_sensor_value(1));

    if (num_CO2 > 0)
        lcd.add_value_read("CO2:", array_co2[0]);

    if (last_send != "") {                                 // Show last send time
        lcd.print_msg(0, 2, "Last: ");
        lcd.print(last_send);
    }
}

/* Capture data in calibration mode */
void pH_calibration() {
    if (DEBUG) SERIAL_MON.println(F("Starting calibration mode..."));
    char buffer_L[6];                                      // String buffer
    
    lcd.clear();                                           // Clear screen
    lcd.home ();                                           // Linea 1
    lcd.print(F("pH Calibration"));

    if (pH_sensors && pH_sensors->get_n_sensors() > 0) {
        lcd.setCursor(0, 1);                               // go to the 2nd line
        lcd.print(F("pH1:"));
        dtostrf(pH_sensors->get_sensor_value(0), 4, 2, buffer_L);
        lcd.print(buffer_L);
    }
    
    if (pH_sensors && pH_sensors->get_n_sensors() > 1) {
        lcd.setCursor(0, 2);                               // go to the 3rd line
        lcd.print(F("pH2:"));
        dtostrf(pH_sensors->get_sensor_value(1), 4, 2, buffer_L);
        lcd.print(buffer_L);
    }
}

/* Obtain the name of first free file for writting to SD */
void SD_get_next_FileName(char* _fileName) {
    static int fileCount = 0;

    do {
        sprintf(_fileName, "%06d.txt", ++fileCount);
    } while (SD.exists(_fileName));
}

/* Writing title headers to file */
void SD_write_header(const char* _fileName) {
    myFile = SD.open(_fileName, FILE_WRITE);

    // If file not opened okay, exit
    if (!myFile) return;

    if (RTC_enabled) myFile.print(F("DateTime#"));

    // T1_s#T1_b#......#Tn_s#Tn_b#
    uint8_t max_val = (wp_t_sensors)?wp_t_sensors->get_n_pairs():0;
    for (uint8_t i=0; i<max_val; i++) {
        myFile.print(F("Temp"));
        myFile.print(i);
        myFile.print(F("_s#"));

        myFile.print(F("Temp"));
        myFile.print(i);
        myFile.print(F("_b#"));
    }

    // ambient1_temp#ambient1_humetat#
    for (uint8_t i=0; i<dht_sensors.get_num_sensors(); i++) {
        myFile.print(F("ambient_"));
        myFile.print(i);
        myFile.print(F("_temp#"));
        myFile.print(F("ambient_"));  
        myFile.print(i);
        myFile.print(F("_humetat#"));      
    }

    // Lux sensor
    if (lux_sensor.is_init()) myFile.print(F("lux#"));
    
    // pH Sensor
    uint8_t num_max = pH_sensors? pH_sensors->get_n_sensors() : 0;
    for (uint8_t i=0; i<num_max; i++) {
        myFile.print(F("pH_"));
        myFile.print(i);
        myFile.print(F("#"));
    }

    // DO Sensor
    if (do_sensor.is_init()) {
        myFile.print(F("pre_lux#"));                       // pre_lux value
        myFile.print(F("DO_"));                            // Red
        myFile.print("1");
        myFile.print(F("_R#"));                            // Green
        myFile.print(F("DO_"));
        myFile.print("1");
        myFile.print(F("_G#"));
        myFile.print(F("DO_"));                            // Blue
        myFile.print("1");
        myFile.print(F("_B#"));
        myFile.print(F("DO_"));                            // RGB (white)
        myFile.print("1");
        myFile.print(F("_RGB#"));                  
    }

    // CO2 Sensors
    for (uint8_t i=0; i<num_CO2; i++) {
        myFile.print(F("CO2_"));
        myFile.print(i);
        myFile.print(F("#"));
    }

    //TODO: Añadir cabeceras de corriente Ini & End

    myFile.println(F(""));               // End of line
    myFile.close();                      // close the file:
}

/* Writing results to file in SD card */
void SD_save_data(const char* _fileName) {
    String tmp_data = "";                                  // Temporal string to concatenate the information

    if (DEBUG) {
        SERIAL_MON.print("Try to open SD file: ");
        SERIAL_MON.println(_fileName);
    }
    
    myFile = SD.open(_fileName, FILE_WRITE);
    
    if (!myFile) {
        if (DEBUG) SERIAL_MON.println(F("Error opening SD file!"));
        return;    //Exit
    }

    if (RTC_enabled) {                                       //DateTime if have RTC
        tmp_data += dateTimeRTC.getDateTime();
        tmp_data += F("#");
    }

    uint8_t max_val = (wp_t_sensors)?wp_t_sensors->get_n_pairs():0;
    for (uint8_t i=0; i<max_val; i++) {                      // Temperatures del cultiu Tn_s, Tn_b, ...
        tmp_data += wp_t_sensors->get_result_pair(i, 1);
        tmp_data += F("#");
        tmp_data += wp_t_sensors->get_result_pair(i, 2);
        tmp_data += F("#");
    }
    
    // Sensors DHT
	//TODO: Cambiar el volcado de datos por el de la funcion definida ya en la clase DHT_Sensor
    for (uint8_t i=0; i<dht_sensors.get_num_sensors(); i++) {
        tmp_data += dht_sensors.get_Temperature(i);
        tmp_data += F("#");
		tmp_data += dht_sensors.get_Humidity(i);
        tmp_data += F("#");
    }

    // Lux sensor
    if (lux_sensor.is_init()) {
        tmp_data += lux;
        tmp_data += F("#");
    }
    
    // pH Sensor
    //TODO: Implementar bulk_results de PH_Sensors
    uint8_t num_max = pH_sensors? pH_sensors->get_n_sensors() : 0;
    for (uint8_t i=0; i<num_max; i++) {
        tmp_data += pH_sensors->get_sensor_value(i);
        tmp_data += F("#");
    }
    
    // DO Sensor
    //TODO: Implementar bulk_results de DO_Sensor
    if (do_sensor.is_init()) {                                 // If have DO sensor
        tmp_data += do_sensor.get_preLux_value();
        tmp_data += F("#");               
        tmp_data += do_sensor.get_Red_value();                 // R-G-B-RGB (white)
        tmp_data += F("#");
        tmp_data += do_sensor.get_Green_value();
        tmp_data += F("#");
        tmp_data += do_sensor.get_Blue_value();
        tmp_data += F("#");
        tmp_data += do_sensor.get_White_value();
        tmp_data += F("#");
    }
    
    // CO2 Sensors
    for (uint8_t i=0; i<num_CO2; i++) {
        tmp_data += array_co2[i];
        tmp_data += F("#");
    }
    
    // Current sensor init & end
    //TODO: Implementar bulk data de Current_sensors
    num_max = curr_sensors? curr_sensors->get_n_sensors() : 0;
    for (uint8_t i=0; i<num_max; i++) {
        tmp_data += curr_sensors->get_current_value(i);
        tmp_data += F("#");
    }

    if (DEBUG) {
        SERIAL_MON.print(F("Data to write: "));
        SERIAL_MON.println(tmp_data);
    }

    myFile.println(tmp_data);          // Save all obtained data in the file
    myFile.close();                    // And close the file
}

bool send_data_ethernet(String cadena) {
  if (!conexio_internet) {
    SERIAL_MON.println(F("[Ethernet] Conecting to router..."));
    conexio_internet = Ethernet.begin(mac);

    if (!conexio_internet) {
      SERIAL_MON.println(F("[Ethernet] Conection to router Failed"));
      return false;
    }
  }
  
  eth_client.stop();
  // if there's a successful connection:
  if (eth_client.connect(server, 80)) {
    if (DEBUG)
      SERIAL_MON.println(F("Connecting to openspirulina..."));

    if (DEBUG)
      SERIAL_MON.println(cadena);
    
    // Send string to internet  
    eth_client.println(cadena);
    eth_client.println(F("Host: sensors.openspirulina.com"));
    eth_client.println(F("User-Agent: arduino-ethernet-1"));
    eth_client.println(F("Connection: close"));
    eth_client.println();
    return true;
  }
  else {
    // If you couldn't make a connection:
    SERIAL_MON.println(F("Connection to openspirulina Failed"));
    return false;
  }
}

bool connect_network() {
  if(DEBUG) SERIAL_MON.println(F("Waiting for network..."));

  if (!modem.waitForNetwork()) {
    SERIAL_MON.println(F("Network [fail]"));
    delay(1000);
    return false;
  }
  else {
    if(DEBUG) SERIAL_MON.println(F("Network [OK]"));  
    return true;
  }
}

bool send_data_modem(String cadena, bool step_retry) {
  // Set GSM module baud rate
  SERIAL_AT.begin(9600);

  delay(3000);
  if (DEBUG)
    SERIAL_MON.println(F("Initializing modem..."));
    
  if (!modem.restart()) {
    SERIAL_MON.println(F("Modem init [fail]"));
    delay(1000);
    return false;
  }

  if (DEBUG) {
    SERIAL_MON.println(F("Modem init [OK]"));    
    String modemInfo = modem.getModemInfo();
    SERIAL_MON.print(F("Modem: "));
    SERIAL_MON.println(modemInfo);
  }
  // Unlock your SIM card with a PIN
  //modem.simUnlock("1234");    
  if (connect_network()) {
    // Network OK
    SERIAL_MON.print(F("Connecting to "));
    SERIAL_MON.println(apn);
    if (!modem.gprsConnect(apn, user, pass)) {
      SERIAL_MON.println(F("GRPS [fail]"));
      delay(1000);

      if(step_retry == false) {
        SERIAL_MON.println(F("[Modem] Retrying connection !"));
        send_data_modem(cadena, true);  // Reconnect modem and init again
      }      
      return false;
    }
    SERIAL_MON.println(F("GPRS [OK]"));
  
    IPAddress local = modem.localIP();
    SERIAL_MON.print(F("Local IP: "));
    SERIAL_MON.println(local);
  
    SERIAL_MON.print(F("Connecting to "));
    SERIAL_MON.println(server);
    if (!client.connect(server, port)) {
      SERIAL_MON.println(F("Server [fail]"));
      delay(1000);
      if(step_retry == false) {
        SERIAL_MON.println(F("[Modem] Retrying connection !"));
        send_data_modem(cadena, true);  // Reconnect modem and init again
      }      
      return false;
    }
    SERIAL_MON.println(F("Server [OK]"));
  
      // Make a HTTP GET request:
    client.print(cadena + " HTTP/1.0\r\n");
    client.print(String("Host: ") + server + "\r\n");
    client.print(F("Connection: close\r\n\r\n"));
    /*
      // Wait for data to arrive
      while (client.connected() && !client.available()) {
        delay(100);
        SERIAL_MON.print('.');
      };
      SERIAL_MON.println(F("Received data: "));
      // Skip all headers
      //client.find("\r\n\r\n");
      // Read data
      unsigned long timeout = millis();
      unsigned long bytesReceived = 0;
      while (client.connected() && millis() - timeout < 5000L) {//client.connected() &&
        //while (client.available()) {
        char c = client.read();
        SERIAL_MON.print(c);
        bytesReceived += 1;
        timeout = millis();
        //}
      }
      */
      SERIAL_MON.println();
      client.stop();
      if(DEBUG) SERIAL_MON.println(F("Server disconnected"));
    
      modem.gprsDisconnect();
      if(DEBUG) SERIAL_MON.println(F("GPRS disconnected"));
      /*
      if (DEBUG) {
        SERIAL_MON.println(F("************************"));
        SERIAL_MON.print  (F("Received: "));
        SERIAL_MON.print(bytesReceived);
        SERIAL_MON.println(F(" bytes"));
      }
      */
      return true;
  }
  else {
    SERIAL_MON.println(F("[Modem] Fail !"));
    // Try one more time, if continue fails, continue
    if(step_retry == false) {
      SERIAL_MON.println(F("[Modem] Retrying connection !"));
      send_data_modem(cadena, true);  
    }
    return false; 
  }
  return false;
}

bool send_data_server() {
	String cadena = "GET /afegir.php?";
	
	// Append our ID Arduino
	cadena += "idarduino=";
	cadena += id_arduino;

	// Append PIN for that ID
	cadena += "&pin=";
	cadena += pin_arduino;

	// Append temperatures
    uint8_t max_val = (wp_t_sensors)?wp_t_sensors->get_n_pairs():0;
	for (int i=0; i<max_val; i++) {
		cadena += F("&temp");
		cadena += i+1;
		cadena += F("_s=");
		cadena += wp_t_sensors->get_result_pair(i, 1);

		cadena += F("&temp");
		cadena += i+1;
		cadena += F("_b=");
		cadena += wp_t_sensors->get_result_pair(i, 2);
	}

	// Append Ambient temperatures and Humidity
	//TODO: Cambiar el volcado de datos por el de la funcion definida ya en la clase DHT_Sensor
	for (uint8_t i=0; i<dht_sensors.get_num_sensors(); i++) {
		cadena += "&ta";
		cadena += i+1;
		cadena += "=";
		cadena += dht_sensors.get_Temperature(i);
		cadena += "&ha";
		cadena += i+1;
		cadena += "=";
		cadena += dht_sensors.get_Humidity(i);
	}
  
	// Append Lux sensors
	if (lux_sensor.is_init()) {
		cadena += "&lux1=";
		cadena += lux;
	}

	//TODO: Implementar sensor LDR
	/*
	if (ldr_sensor.is_ini()) {
		switch (option_lux) {
			case lux_LDR: cadena += "&ldr1="; break;
			case lux_BH1750: cadena += "&lux1=";
		}
		cadena += lux;
	}
	*/

	// Append pH Sensor
    uint8_t num_max = pH_sensors? pH_sensors->get_n_sensors() : 0;
	for (int i=0; i<num_max; i++) {
		cadena += "&ph";
		cadena += i+1;
		cadena += "=";
		cadena += pH_sensors->get_sensor_value(i);
	}  

	// Append DO Sensor
	if (do_sensor.is_init()) {                             // If have DO sensor
		cadena += "&pre_L=";                               // Previous lux
		cadena += do_sensor.get_preLux_value();            // Pre Lux value
		cadena += "&do1_R=";
		cadena += do_sensor.get_Red_value();               // Red value
		cadena += "&do1_G=";
		cadena += do_sensor.get_Green_value();             // Green value
		cadena += "&do1_B=";
		cadena += do_sensor.get_Blue_value();              // Blue value
		cadena += "&do1_RGB=";
		cadena += do_sensor.get_White_value();             // RGB (white) value
	}

	// Append PIR Sensor
	for (uint8_t i=0; i<num_PIR; i++) {
		cadena += "&pir";
		cadena += i+1;
		cadena += "=";
		if(array_pir[i] == 0)
		cadena += "0";
		else if(array_pir[i] == 1)
		cadena += "1";
  	}  
	
	// Append CO2 Sensors
	for (uint8_t i=0; i<num_CO2; i++) {
		cadena += "&co2";
		cadena += i+1;
		cadena += "=";
		cadena += array_co2[i];
	}

	// Append Current initial & end
    num_max = curr_sensors? curr_sensors->get_n_sensors() : 0;
	for (uint8_t i=0; i<num_max; i++) {
		cadena += "&curr";
		cadena += i+1;
		cadena += "=";
		cadena += curr_sensors->get_current_value(i);
	}

	if (DEBUG) {
		SERIAL_MON.print("Server petition: ");
		SERIAL_MON.println(cadena);
	}

	// Send data to specific hardware
	if (opt_Internet == it_Ethernet) {
		// Add end of GET petition for Ethernet and send
		cadena += " HTTP/1.1";
		return send_data_ethernet(cadena);
	}
	else if (opt_Internet == it_GPRS) {
		// Send petition to GPRS Modem
		return send_data_modem(cadena, false);
	} else {
		return false;
	}
}

void SD_load_DHT_sensors(Load_SD_Config* ini) {
	char buffer[INI_FILE_BUFFER_LEN] = "";
	char tag_sensor[22] = "";
	bool found;
	uint8_t i = 1;

	if (DEBUG) SERIAL_MON.println(F("Loading DHT sensors config.."));
	do {
		sprintf(tag_sensor, "sensor%d.pin", i++);
		found = ini->getValue("sensors:DHT", tag_sensor, buffer, sizeof(buffer));
		if (found) {
			uint8_t pin = atoi(buffer);
			if (DEBUG) {
                SERIAL_MON.print(F("  > Found config: ")); SERIAL_MON.print(tag_sensor);
			    SERIAL_MON.print(F(". Pin = ")); SERIAL_MON.println(pin);
            }
			dht_sensors.add_sensor(pin);
		}
	} while (found);

	// If no configuration found in IniFile..
	if (dht_sensors.get_num_sensors() == 0) {
		if (DEBUG) SERIAL_MON.println(F("No DHT config. found. Loading default.."));
		for (i=0; i<DHT_DEF_NUM_SENSORS; i++) {
			if (DEBUG)  {
                SERIAL_MON.print(F("  > Found config: sensor")); Serial.print(i+1);
			    SERIAL_MON.print(F(". Pin = ")); Serial.println(DHT_DEF_SENSORS[i]);
            }
            dht_sensors.add_sensor(DHT_DEF_SENSORS[i]);
		}
	}
}

void SD_load_Lux_sensor(IniFile* ini) {
	char buffer[INI_FILE_BUFFER_LEN] = "";
	bool found;
	
	if (DEBUG) SERIAL_MON.println(F("Loading Lux sensor config.."));
	
	// Read Lux sensor address
	found = ini->getValue("sensor:lux", "address", buffer, sizeof(buffer));
	if (found) {
		// Convert char[] address to byte value
        uint8_t address = (uint8_t)strtol(buffer, NULL, 16);
		if (DEBUG) { SERIAL_MON.print(F("  > Found config address: 0x")); SERIAL_MON.println(address, HEX); }
		
		// Read Lux Addr pin
		ini->getValue("sensor:lux", "addr_pin", buffer, sizeof(buffer));
		uint8_t addr_pin = atoi(buffer);                   // If addr_pin not found, the value is 0
		if (DEBUG) { SERIAL_MON.print(F("  > Addr pin: ")); SERIAL_MON.println(addr_pin); }

		lux_sensor.begin(address, addr_pin);               // Configure lux sensor with Ini loaded config.

        // Read number of samples to read from sensor
        ini->getValue("sensor:lux", "n_samples", buffer, sizeof(buffer));
        uint8_t n_samples = atoi(buffer);
        if (n_samples != 0) lux_sensor.set_n_samples(n_samples);
	}
    else {
		// Configure lux sensor with default configuration
        if (DEBUG) SERIAL_MON.print(F("No config found. Loading default.."));

		if (LUX_SENS_ACTIVE)
            lux_sensor.begin(LUX_SENS_ADDR, LUX_SENS_ADDR_PIN);
	}
}

void SD_load_DO_sensor(IniFile* ini) {
   	char buffer[INI_FILE_BUFFER_LEN] = ""; 
	bool found;
    
    if (DEBUG) SERIAL_MON.println(F("Loading DO sensor config.."));

    // Read DO sensor address (hexadecimal format)
	found = ini->getValue("sensor:DO", "address", buffer, sizeof(buffer));
    if (found) {
        uint8_t address, led_R_pin, led_G_pin, led_B_pin;

        address = (uint8_t)strtol(buffer, NULL, 16);    // Convert hex char[] to byte value
		if (DEBUG) { SERIAL_MON.print(F("  > Found config address: 0x")); SERIAL_MON.println(address, HEX); }

        // Read red LED pin
        if (!ini->getValue("sensor:DO", "led_R_pin", buffer, sizeof(buffer), led_R_pin))
            led_R_pin = DO_SENS_R_LED_PIN;

        // Read green LED pin
        if (!ini->getValue("sensor:DO", "led_G_pin", buffer, sizeof(buffer), led_G_pin))
            led_G_pin = DO_SENS_G_LED_PIN;
        
        // Read blue LED pin
        if (!ini->getValue("sensor:DO", "led_B_pin", buffer, sizeof(buffer), led_B_pin))
            led_B_pin = DO_SENS_B_LED_PIN;

        // Configure DO sensor with load parametern from IniFile
        do_sensor.begin(address, led_R_pin, led_G_pin, led_B_pin);
    }
    else if (DO_SENS_ACTIVE) {
        // Configure DO sensor with default configuration
        if (DEBUG) SERIAL_MON.print(F("No config found. Loading default.."));
        do_sensor.begin(DO_SENS_ADDR, DO_SENS_R_LED_PIN, DO_SENS_G_LED_PIN, DO_SENS_B_LED_PIN);
    }
}

void SD_load_pH_sensors(IniFile* ini) {
	char buffer[INI_FILE_BUFFER_LEN] = "";
	char tag_sensor[22] = "";
	bool found;
	uint8_t i = 1;

	if (DEBUG) SERIAL_MON.println(F("Loading pH sensors config.."));
	do {
		sprintf(tag_sensor, "sensor%d.pin", i++);
		found = ini->getValue("sensors:pH", tag_sensor, buffer, sizeof(buffer));
		if (found) {
			uint8_t pin = atoi(buffer);
			Serial.print(F("  > Found config: ")); Serial.print(tag_sensor);
			Serial.print(F(". Pin = ")); Serial.println(pin);

            if (!pH_sensors) pH_sensors = new PH_Sensors();     //If the object has not been initialized yet, we do it now
            pH_sensors->add_sensor(pin);
		}
	} while (found);

	// If no configuration found in IniFile..
	if (!pH_sensors && PH_DEF_NUM_SENSORS > 0) {
		if (DEBUG) SERIAL_MON.println(F("No pH config. found. Loading default.."));
		for (i=0; i<PH_DEF_NUM_SENSORS; i++) {
            if (DEBUG) {
                SERIAL_MON.print(F("  > Found config: sensor")); SERIAL_MON.print(i+1);
			    SERIAL_MON.print(F(". Pin = ")); SERIAL_MON.println(PH_DEF_PIN_SENSORS[i]);
            }
            pH_sensors->add_sensor(PH_DEF_PIN_SENSORS[i]);
		}
	}
}

/* Converts a string to bytes array like a device address */
bool convert_str_to_addr(char* str, uint8_t* addr, uint8_t max_len) {
    char *pch;
    uint8_t len = 0;
    
    pch = strtok(str, ",");
    while (pch != NULL && len < max_len) {
        addr[len] = (uint8_t)strtol(pch, NULL, 16);
        len++;
        pch = strtok(NULL, ",");
    }

    return (len <= max_len);
}

void SD_load_WP_Temp_sensors(IniFile* ini) {
	char buffer[INI_FILE_BUFFER_LEN] = "";
	char tag_sensor[20] = "";
    uint8_t i=1, addr_s[8], addr_b[8];
	uint16_t one_wire_pin;

    if (DEBUG) SERIAL_MON.println(F("Loading WP temperature sensors config.."));
    
    bool found = ini->getValue("sensors:wp_temp", "one_wire_pin", buffer, sizeof(buffer),
                               one_wire_pin);                         // Load One Wire config

	while (found) {
        // Load surface sensor address
		sprintf(tag_sensor, "addr_t%d_s", i);
		found = ini->getValue("sensors:wp_temp", tag_sensor, buffer, sizeof(buffer));
        if (!found || !convert_str_to_addr(buffer, addr_s, 8)) break; // If can't find the sensor or the address is not correct, exit

        // Load background sensor address
        sprintf(tag_sensor, "addr_t%d_b", i);
        found = ini->getValue("sensors:wp_temp", tag_sensor, buffer, sizeof(buffer));
        if (!found || !convert_str_to_addr(buffer, addr_b, 8)) break; // If can't find the sensor or the address is not correct, exit

        if (DEBUG) {
            SERIAL_MON.print(F("  > Found config: sensor")); SERIAL_MON.print(i);
            SERIAL_MON.println(F(" pair"));
        }
        
        if (!wp_t_sensors) wp_t_sensors = new WP_Temp_Sensors(one_wire_pin);  //If the obj has not been initialized yet, we do it now
        wp_t_sensors->add_sensors_pair(addr_s, addr_b);

        i++;
	}

    // Load default configuration
    if (!wp_t_sensors && WP_T_DEF_NUM_PAIRS > 0) {
        if (DEBUG) SERIAL_MON.println(F("No pH config found. Loading default.."));
        
        wp_t_sensors = new WP_Temp_Sensors(WP_T_ONE_WIRE_PIN);
        for (i=0; i<WP_T_DEF_NUM_PAIRS; i++) {
            if (DEBUG) {
                SERIAL_MON.print(F("  > Found config: sensor")); SERIAL_MON.print(i);
                SERIAL_MON.println(F(" pair"));
            }
            wp_t_sensors->add_sensors_pair(WP_T_DEF_SENST_PAIRS[i][0], WP_T_DEF_SENST_PAIRS[i][1]);   
        }
    }
}

bool extract_str_params_Current_sensor(char *str, uint8_t &pin, Current_Sensors::Current_Model_t &model, uint16_t &var) {
    char *pch;

    pch = strtok(str, ",");                                // Get the first piace
    if (pch == NULL) return false;                         // Check piece, if it's not correct, exit
    pin = (uint8_t)strtol(pch, NULL, 10);                  // Obtain the pin value
    
    pch = strtok(NULL, ",");                               // Get the second piace

    while (isspace(*pch)) ++pch;                           // Skip possible white spaces
    char *tmp = pch;                                       // Remove trailing white space
    while (*tmp != ',' && *tmp != '\0')
        if (*tmp == ' '|| *tmp == '\t') *tmp++ = '\0'; else tmp++;

    if (strcmp(pch, "acs712") == 0)                        // Select sensor model
        model = Current_Sensors::Current_Model_t::ACS712;
    else if (strcmp(pch, "sct013") == 0)
        model = Current_Sensors::Current_Model_t::SCT013;
    else
        return false;
    
    pch = strtok(NULL, ",");                               // Get the third piace
    if (pch == NULL) return false;
    var = (uint16_t)strtol(pch, NULL, 10);                 // Obtain the pin value

    return true;
}

//TODO: Validar nuevo metodo de carga de sensores de corriente
void SD_load_Current_sensors(IniFile* ini) {
	char buffer[INI_FILE_BUFFER_LEN] = "";
	char tag_sensor[16] = "";
    bool sens_cfg;
    uint8_t i = 1;
    uint8_t pin;
    Current_Sensors::Current_Model_t s_model;
    uint16_t var;

    if (DEBUG) SERIAL_MON.println(F("Loading Current sensors config.."));
    do {
        // Try to read pin
		sprintf(tag_sensor, "sensor%d", i++);
		sens_cfg = ini->getValue("sensors:current", tag_sensor, buffer, sizeof(buffer), pin);

		if (sens_cfg && extract_str_params_Current_sensor(buffer, pin, s_model, var)) {
			if (DEBUG) {
                SERIAL_MON.print(F("  > Found config: ")); Serial.print(tag_sensor);
			    SERIAL_MON.print(F(". Pin = ")); Serial.println(pin);
            }

            if (!curr_sensors) curr_sensors = new Current_Sensors();      //If the object has not been initialized yet, we do it now
            curr_sensors->add_sensor(pin, s_model, var);
        }
    } while (sens_cfg);

    // Load default configuration
    if (!curr_sensors && CURR_SENS_DEF_NUM > 0) {
        if (DEBUG) SERIAL_MON.println(F("No current config found. Loading default.."));
        curr_sensors = new Current_Sensors();

        for (uint8_t i=0; i<CURR_SENS_DEF_NUM; i++) {
            curr_sensors->add_sensor(CURR_SENS_DEF_PINS[i],
                                    (Current_Sensors::Current_Model_t) CURR_SENS_DEF_MODELS[i],
                                    CURR_SENS_DEF_VAR[i]);
            if (DEBUG) {
                SERIAL_MON.print(F("  > Found config: "));
			    SERIAL_MON.print(F(". Pin = ")); Serial.println(CURR_SENS_DEF_PINS[i]);
            }
        }
    }
}


/*
   _____ ______ _______ _    _ _____
  / ____|  ____|__   __| |  | |  __ \
 | (___ | |__     | |  | |  | | |__) |
  \___ \|  __|    | |  | |  | |  ___/
  ____) | |____   | |  | |__| | |
 |_____/|______|  |_|   \____/|_|
*/
void setup() {
    pinMode(PH_CALIBRATION_SWITCH_PIN, INPUT);                            //Configure the input pin for the calibration switch

    Wire.begin();                                                         //Initialize the I2C bus (BH1750 library doesn't do this automatically)

	// Initialize serial if DEBUG default value is true
	if (DEBUG) SERIAL_MON.begin(SERIAL_BAUD);
	while (DEBUG && !SERIAL_MON) { ; }                                    // wait for serial port to connect. Needed for native USB port only
    
	// Always init SD card because we need to read init configuration file.
	if (SD.begin(SD_CARD_SS_PIN)) {
		if (DEBUG) SERIAL_MON.println(F("Initialization SD done."));

        // Read initial config from file
        Load_SD_Config ini(INI_FILE_NAME);                                // IniFile configuration

        // Try to open and check config file
        if (ini.open_config()) {
            ini.load_bool("debug", "enabled", DEBUG);                     // Load if debug mode configuration
            ini.load_bool("LCD", "enabled", LCD_enabled);                 // Load if LCD is enabled
            ini.load_bool("RTC", "enabled", RTC_enabled);                 // Load if RTC is enabled
            ini.load_bool("SD_card", "save_on_sd", SD_save_enabled);      // Load if SD save is enabled

			SD_load_DHT_sensors(&ini);                                    // Initialize DHT sensors configuration
			SD_load_Lux_sensor(&ini);                                     // Initialize Lux light sensor
            SD_load_DO_sensor(&ini);                                      // Initialize DO sensor
            SD_load_pH_sensors(&ini);                                     // Initialize pH sensors
            SD_load_WP_Temp_sensors(&ini);                                // Initialize DS18B20 waterproof temperature sensors
            SD_load_Current_sensors(&ini);                                // Initialize current sensors
        }
	}
	else {
		if (DEBUG) SERIAL_MON.println(F("Initialization SD failed!"));
	}

    // If DEBUG is active and Serial not initialized, then start this
    if (!DEBUG_DEF_ENABLED && DEBUG) SERIAL_MON.begin(SERIAL_BAUD);
        else if (DEBUG_DEF_ENABLED && !DEBUG) SERIAL_MON.end();

	// If save in SD card option is enabled
	if (SD_save_enabled) {
		SD_get_next_FileName(fileName);                                   // Obtain the next file name to write
		SD_write_header(fileName);                                        // Write File headers
	}
    
    // Inicialitza LCD en cas que n'hi haigui
	if (LCD_enabled) {
		if (DEBUG) SERIAL_MON.println(F("Initialization LCD.."));

		lcd.init();
		lcd.show_init_msg(LCD_INIT_MSG_L1, LCD_INIT_MSG_L2,
						  LCD_INIT_MSG_L3, LCD_INIT_MSG_L4, 5000);
    }

	// Inicialitza RTC en cas de disposar
	if (RTC_enabled) {
		if (DEBUG) SERIAL_MON.print(F("Init RTC Clock.. "));
		
		// Try to initialize the RTC module
		if (dateTimeRTC.begin()) {
			if (DEBUG) SERIAL_MON.println(dateTimeRTC.getDateTime());     // RTC works. Print current time
		}
		else {
			if (DEBUG) SERIAL_MON.println(F("No clock working"));         // RTC does not work
		}
	}

	// Initialize Ethernet shield
	if (opt_Internet == it_Ethernet) {
		// give the ethernet module time to boot up:
		delay(2000);
		if (DEBUG) SERIAL_MON.println(F("Starting Ethernet Module"));
	
		// start the Ethernet connection using a fixed IP address and DNS server:
		// Ethernet.begin(mac, ip, myDns, gateway, subnet);
		// DHCP IP ( For automatic IP )
		conexio_internet = Ethernet.begin(mac);

		if (!conexio_internet) SERIAL_MON.println(F("[Ethernet] Fail obtain IP"));
		
		// print the Ethernet board/shield's IP address:
		if (DEBUG) {
			SERIAL_MON.print(F("My IP address: "));
			SERIAL_MON.println(Ethernet.localIP());
		}
	}
	// Initialize GPRS Modem
	else if(opt_Internet == it_GPRS) {
		// Not implemented jet
	}
}

/*
  _      ____   ____  _____
 | |    / __ \ / __ \|  __ \
 | |   | |  | | |  | | |__) |
 | |   | |  | | |  | |  ___/
 | |___| |__| | |__| | |
 |______\____/ \____/|_|
*/
void loop() {
    // If pin calibration ph switch is HIGH
    while (digitalRead(PH_CALIBRATION_SWITCH_PIN) == HIGH) {
        if (DEBUG) SERIAL_MON.println(F("Calibration switch active!"));
        pH_calibration();
        delay(10000);
    }

    if (DEBUG) {
        SERIAL_MON.print(F("\nFreeMem: ")); SERIAL_MON.print(freeMemory());
        SERIAL_MON.print(F(" - loop: ")); SERIAL_MON.println(++loop_count);
		SERIAL_MON.println(F("Getting data.."));
    }

	if (LCD_enabled)
		lcd.print_msg_val(0, 3, "Getting data.. %d", (int32_t)loop_count);

    if (curr_sensors) {
       if (DEBUG) SERIAL_MON.println(F("Capture current.."));
       curr_sensors->capture_all_currents();
    }

    // Si tenim sondes de temperatura
    if (wp_t_sensors) {
		if (DEBUG) SERIAL_MON.println(F("Capture WP temperatures.."));
		wp_t_sensors->store_all_results();
	}
    
	// Capture PH for each pH Sensor
    if (pH_sensors) {
        if (DEBUG) SERIAL_MON.println(F("Capture pH sensors.. "));
        pH_sensors->store_all_results();
    }

    // Capture PH for each pH Sensor
	if (dht_sensors.get_num_sensors() > 0) {
		if (DEBUG) SERIAL_MON.println(F("Capture DHT sensor(s).."));
		dht_sensors.capture_all_DHT();
	}

    if (lux_sensor.is_init()) {
		if (DEBUG) SERIAL_MON.println(F("Capture lux sensor.."));
		lux = lux_sensor.capture_lux();
	}

    // Capture status of PIR Sensors
    for (uint8_t i=0; i<num_PIR; i++) {
        if (detect_PIR(pins_pir[i]) == true)
            array_pir[i] = 1;
        else
            array_pir[i] = 0;
    }

    //Capture DO values (Red, Green, Blue, and White)
    if (do_sensor.is_init()) {
		if (DEBUG) SERIAL_MON.println(F("Capture DO sensor.."));
        do_sensor.capture_DO(); 
        delay(500);
    }
    
    // Capture CO2 concentration
    if (num_CO2 > 0) {
		if (DEBUG) SERIAL_MON.println(F("Capture CO2 sensor.."));
		capture_CO2();
	}
    
    // END of capturing values
    if (LCD_enabled) mostra_LCD();
    
	if (opt_Internet != it_none) {
		// Si s'envia correctament actualitzar last_send
		if  (send_data_server()) {
			delay(200);
			last_send = dateTimeRTC.getTime();
            delay(100);
            last_send += " OK";
            if (LCD_enabled) mostra_LCD();
        }
        else {
            delay(200);
            last_send = dateTimeRTC.getTime();
            delay(100);
            last_send += " FAIL";

            if (LCD_enabled) mostra_LCD();
        }
    }

    // Save data to SD card
    if (SD_save_enabled) SD_save_data(fileName);


	/********************************************
	***  Waiting time until the next reading  ***
	*********************************************/

	/* If have RTC clock make timer delay if not internal delay */
    if (RTC_enabled) {
		uint32_t time_next_loop = dateTimeRTC.inc_unixtime(DELAY_SECS_NEXT_READ);    // Set next timer loop for actual time + delay time

        if (DEBUG) {
            SERIAL_MON.print(F("Waiting for "));
            SERIAL_MON.print( dateTimeRTC.unix_time_remain(time_next_loop) );
            SERIAL_MON.println(F(" seconds"));
        }
        
        int32_t time_remain;
        do {
            time_remain = dateTimeRTC.unix_time_remain(time_next_loop);

            if (LCD_enabled) lcd.print_msg_val(0, 3, "Next read.: %ds ", time_remain);
            if (DEBUG) SERIAL_MON.print(F("."));
            if (digitalRead(PH_CALIBRATION_SWITCH_PIN) == HIGH) {
                if (DEBUG) SERIAL_MON.println(F("Calibration switch activated!"));
                break;
            }

            delay(1000);
        } while (time_remain > 0);
    }

  /* If RTC is not available */
  else {
    // Waiting for 10 minutes
    for (uint16_t j=0; j<DELAY_SECS_NEXT_READ; j++) {
      if (digitalRead(PH_CALIBRATION_SWITCH_PIN) == HIGH) {
        if (DEBUG) SERIAL_MON.println(F("Calibration switch activated!"));
        break;
      }

      if (DEBUG) SERIAL_MON.print(F("."));
      delay(1000);
    }
  }
  SERIAL_MON.flush();
}
