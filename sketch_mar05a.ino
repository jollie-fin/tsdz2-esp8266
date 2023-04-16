/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP_EEPROM.h>
#include <SoftwareSerial.h> 
#include <stdint.h>
#include "CRC8.h"
#include <cmath>
#include <inttypes.h>

#define TSDZ2_TX D8
#define TSDZ2_RX D7

SoftwareSerial tsdz2(TSDZ2_RX, TSDZ2_TX);

const int analogInPin = A0;  // ESP8266 Analog Pin ADC0 = A0
const int generalIOPin = D0;
const int motorEnablePin = D1;
const int brakePin = D2;

#define MAX_LEVEL 5

#define MS_PER_UNIT 2.04
const uint32_t DIAMETER_INCH_TO_CIRCUMFERENCE_MM = std::round(M_PI * 25.4);
const uint32_t MM_PER_UNIT_TO_KPH_TIMES_1000 = std::round(3.6 / MS_PER_UNIT * 1000.);
const uint32_t US_PER_UNIT = 1000. / MS_PER_UNIT;

#define RL2 100e3
#define RH2 220e3
#define RL1 1e3
#define RH1 22e3

#define PARASITE_RL1 (RL2+RH2)
#define REAL_RL1 ((RL1 * PARASITE_RL1) / (RL1 + PARASITE_RL1))

#define COEFF2 ((RH2+RL2)/RL2)
#define COEFF1 ((RH1+REAL_RL1)/REAL_RL1)

#define MAX_ADC 1023.

const uint32_t COEFF_ADC_TIMES_1000000 = 1e6 / MAX_ADC * COEFF1 * COEFF2;


const uint16_t default_circumference = 1620;
const uint8_t default_max_speed = 25;

uint8_t assist_level = 0;
bool walkassist = false;
uint8_t torque = 0;
uint8_t tara = 0;
bool motor_enable = false;
bool low_voltage = false;
bool is_running = false;
bool is_pas = false;
uint8_t motor_batterylevel = 0;
uint16_t speedsensor = 0;
uint32_t speed_kph_times_1000 = 0;
uint8_t maximum_speed = 25;
bool headlight = false;
uint8_t error_code = 0;

#define MINIMUM_CELL_VOLTAGE 3.7
#define MAXIMUM_CELL_VOLTAGE 4.2
#define NB_CELLS 13

const uint32_t minimum_voltage_times_1000 = 1000. * MINIMUM_CELL_VOLTAGE * NB_CELLS;
const uint32_t maximum_voltage_times_1000 = 1000. * MAXIMUM_CELL_VOLTAGE * NB_CELLS;

uint32_t voltage_times_1000 = 0;
uint32_t initial_voltage_times_1000 = 0;

// Replace with your network credentials
const char* ssid     = "speedomobile";
const char* password = "***REMOVED***";

struct remanent_t{
  uint32_t total_m;
  uint32_t total_s;
  uint16_t circumference;
  uint16_t trip_count;
  uint8_t  max_speed;
};

uint32_t init_m;
uint32_t init_s;

remanent_t remanent;

// Set web server port number to 80
ESP8266WebServer server(80);
IPAddress apIP(10, 10, 10, 10);









/////////////////////////////// HTML //////////////////////////////////

const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE HTML><html>
<head>
  <title>Speedomobile</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
  }
  body {
    margin: 0;
  }
  p {
    margin: 0px 0px 0px 0px;
  }
  button {
    margin: .1rem;
    padding: 15px 20px;
    font-size: 24px;
    text-align: center;
    outline: none;
    color: #fff;
    background-color: #0f8b8d;
    border: none;
    border-radius: 5px;
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
    -webkit-tap-highlight-color: rgba(0,0,0,0);
   }
   /*.button:hover {background-color: #0f8b8d}*/
   button:active {
     background-color: #0f8b8d;
     box-shadow: 2 2px #CDCDCD;
     transform: translateY(2px);
   }
   .counter {
      margin: -.4rem;
     font-size: 5rem;
     color:#8c8c8c;
     font-weight: bold;
   }
   .assist {
      margin: 0;
     font-size: 3rem;
     color:#8c8c8c;
     font-weight: bold;
   }
  </style>
<title>ESP Web Server</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
</head>
<body>
  <div class="content">
    <div class="card">
      <p></p>
      <p class="counter"><span id="speed">0km/h</span></p>
      <p class="assist"><button id="decrease_assist" onclick="change_assist(-1);">-</button> <span id="assist_level">off</span> <button id="increase_assist" onclick="change_assist(1);">+</button></p>
      <p class="walkassist_control"><button id="walkassist" onmousedown="walk_assist(true);" onmouseup="walk_assist(false);">Walk Assist</button></p>
      <p id="error_code_control" style="display:none;">Error code : <span id="error_code">0</span></p>
      <p class="battery">Battery : <span id="voltage">0V</span> <span id="discharge">0%</span> <span id="remaining">0km</span></p>
      <p class="odometer">Total : <span id="total_km">0km</span> <span id="total_h">0:00</span> <span id="total_average">0km/h</span></p>
      <p class="trip">Trip : <span id="trip_count">#0</span> <span id="trip_km">0m</span> <span id="trip_h">0:00:00</span> <span id="trip_average">0km/h</span></p>
      <p class="settings">Max speed (km/h)<br/><input type="number" class="transmit_value" id="max_speed" name="max speed" min="6" max="50" value="25"></p>
      <p class="settings">Circumference (mm)<br/><input type="number" class="transmit_value" id="circumference" name="circumference" min="1000" max="4000" value="1620"></p>
      <p>Battery<input class="hideshow" id="battery" type="checkbox" checked><br/>
      Walkassist control<input class="hideshow" id="walkassist_control" type="checkbox"><br/>
      Settings<input class="hideshow" id="settings" type="checkbox"><br/>
      Global odometer<input class="hideshow" id="odometer" type="checkbox" checked><br/>
      Trip odometer<input class="hideshow" id="trip" type="checkbox" checked></p>
    </div>
  </div>
<script>

async function transmit_value(elt)
{
    if (elt.tagName == "INPUT") {
        if (elt.type == "checkbox") {
            return await ajax(elt.id, elt.checked);    
        } else {
            return await ajax(elt.id, elt.value);
        }
    } else {
        return await ajax(elt.id, elt.innerText);    
    }
}

async function transmit_value_event(event)
{
    transmit_value(event.target);
}

async function walk_assist(state)
{
    return await ajax("walkassist", state ? "on" : "off");
}

async function change_assist(direction)
{
    const span = document.getElementById("assist_level");
    let current = parseInt(span.innerText);
    if (isNaN(current)) {
        current = -1;
    }
    const next = current + direction;
    if (next < 0) {
        span.innerText = "off";
        await ajax(key="assist_level", value=0);
        await ajax(key="motor_enable", value="off");
    } else if (next > 5) {
        return;
    } else {
        if (span.innerText == "off") {
            await ajax(key="motor_enable", value="on");
        }
        span.innerText = next;
        await transmit_value(span);
    }
}

async function ajax(key=null, value=null)
{
    let path = "ajax";
    if (key) {
        path += "?" + key + "=" + value;
    }
    const response = await fetch(path);
    if (!response.ok) {
        console.log("Invalid response on ajax call : ", {path, response});
        return;
    }
    console.log("answer!");
    console.log(response);
    const json = await response.json();
    console.log("json!");
    document.getElementById("voltage").innerText = parseInt(json["voltage"])/1000 + "V";
    document.getElementById("speed").innerText = parseInt(json["speed"])/1000 + "km/h";
    document.getElementById("error_code").innerText = json["error_code"];
    document.getElementById("error_code_control").style.display = json["error_code"] ? "block" : "none";
    document.getElementById("trip_km").innerText = parseInt(json["current_trip_m"])/1000 + "km";

    const trip_s = parseInt(json["current_trip_s"]);
    const trip_min = (trip_s / 60) % 60;
    const trip_h = (trip_s / 60) / 60;
    document.getElementById("trip_h").innerText = trip_h + ":" + String(trip_min).padStart(2, '0') + ":" + String(trip_s % 60).padStart(2, '0');

    document.getElementById("total_km").innerText = parseInt(json["total_m"])/1000 + "km";

    const total_s = parseInt(json["total_s"]);
    const total_min = (total_s / 60) % 60;
    const total_h = (total_s / 60) / 60;
    document.getElementById("total_h").innerText = total_h + ":" + String(total_min).padStart(2, '0');

    document.getElementById("remaining").innerText = parseInt(json["remaining_m"])/1000 + "km";
    
    if (json["motor_enable"]) {
        document.getElementById("assist_level").innerText = parseInt(json["assist_level"]);
    } else {
        document.getElementById("assist_level").innerText = "off";
    }
    document.getElementById("circumference").innerText = parseInt(json["circumference"]);
    document.getElementById("max_speed").innerText = parseInt(json["max_speed"]);
    document.getElementById("discharge").innerText = parseInt(json["current_charge_percent"]) + "%";
    document.getElementById("trip_count").innerText = "#" + parseInt(json["trip_count"]);
}

function hideshow_event(event)
{
    hideshow(event.target);
}

function hideshow(elt)
{
    Array.from(document.getElementsByClassName(elt.id)).map(to_hideshow => to_hideshow.style.display = elt.checked ? "block" : "none");
}

Array.from(document.getElementsByClassName("hideshow")).map(checkbox =>
{
    checkbox.addEventListener("change", hideshow_event);
    hideshow(checkbox);
});

Array.from(document.getElementsByClassName("transmit_value")).map(elt =>
{
    elt.addEventListener("change", transmit_value_event);
});

window.setTimeout(async function update() {
  await ajax();
  window.setTimeout(update, 100);
}, 100);

</script>
</body>
</html>
)rawliteral";








/////////////////////////////// Declaration //////////////////////////////////
void reset_eeprom();
void read_from_eeprom();
void write_to_eeprom();

void switch_uart_to_tsdz2();
void switch_uart_to_usb();
void switch_uart(bool new_usb_uart);

template <typename T>
void super_println(T &&arg);


void handleRoot();
void answerClient();
void handleAjax();

void send_to_motor();
void receive_from_motor();









/////////////////////////////// Setup ////////////////////////////////////////

void setup() { 
  delay(1000);
  Serial.begin(115200);
  tsdz2.begin(9600);
  EEPROM.begin(sizeof(remanent_t));
  read_from_eeprom();

  init_m = remanent.total_m;
  init_s = remanent.total_s;
  super_println("trip_count");
  super_println(remanent.trip_count);
  remanent.trip_count++;

  pinMode(motorEnablePin, OUTPUT);
  pinMode(brakePin, INPUT_PULLUP);

  // Connect to Wi-Fi network with SSID and password
  super_println("");
  super_println("Connecting to ");
  super_println(ssid);
  super_println(password);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  
  super_println("AP IP address: ");
  
  super_println(myIP);
  
  server.on("/", handleRoot);
  server.on("/ajax", handleAjax);
  
  server.begin();
}






////////////////////////////// loop /////////////////////////////////

void loop(){
  server.handleClient();

  static unsigned long last_sent = 0;
  static unsigned long last_adc = 0;
  static bool once = false;

  if (!once) {
    switch_uart_to_tsdz2();
    once = true;
  }

  unsigned long current_ms = millis();

  if (current_ms - last_sent > 60) { // 15 per second
    last_sent = current_ms;
    send_to_motor();
  }

  if (current_ms - last_adc > 200) { // 5 per second
    last_adc = current_ms;
    voltage_times_1000 = (uint64_t) analogRead(analogInPin) * COEFF_ADC_TIMES_1000000 / 1000;
    if (!initial_voltage_times_1000) {
      initial_voltage_times_1000 = voltage_times_1000;
    }
  }

  receive_from_motor();
}





////////////////////////////// Server /////////////////////////////////



void handleRoot()
{
  server.send_P(200, "text/html", index_html);
}

void addDataJson(bool &first, char *&index, char *index_end, const char *key, int32_t val)
{
  if (first)
    index += snprintf(index, index_end - index, "\"%s\":%" PRId32, key, val);
  else
    index += snprintf(index, index_end - index, ",\"%s\":%" PRId32, key, val);
  first = false;
}

void addStringJson(char *&index, char *index_end, const char *payload)
{
  index += snprintf(index, index_end - index, "%s", payload);
}

void answerClient()
{
  static char json[512];
  char *index = json;
  char *index_end = json + sizeof(json);
  bool first = true;
  uint32_t current_trip_m = remanent.total_m - init_m;
  uint32_t current_trip_s = remanent.total_s - init_s;
  int32_t remaining_m = (int64_t) initial_voltage_times_1000 == voltage_times_1000 ? -1 : (voltage_times_1000 - minimum_voltage_times_1000) * (current_trip_m) / (initial_voltage_times_1000 - voltage_times_1000);

  int32_t num = (int32_t)(voltage_times_1000 - minimum_voltage_times_1000) * 100;
  int32_t denom = maximum_voltage_times_1000 - minimum_voltage_times_1000;
  int32_t current_charge_percent = num/denom;
  
  addStringJson(index, index_end, "{");
  addDataJson(first, index, index_end, "voltage", voltage_times_1000);
  addDataJson(first, index, index_end, "speed", speed_kph_times_1000);
  addDataJson(first, index, index_end, "error_code", error_code);
  addDataJson(first, index, index_end, "torque", torque - tara);
  addDataJson(first, index, index_end, "current_trip_m", current_trip_m);
  addDataJson(first, index, index_end, "current_trip_s", current_trip_s);
  addDataJson(first, index, index_end, "total_m", remanent.total_m);
  addDataJson(first, index, index_end, "total_s", remanent.total_s);
  addDataJson(first, index, index_end, "remaining_m", remaining_m);
  addDataJson(first, index, index_end, "assist_level", assist_level);
  addDataJson(first, index, index_end, "circumference", remanent.circumference);
  addDataJson(first, index, index_end, "current_charge_percent", current_charge_percent);
  addDataJson(first, index, index_end, "pas", is_pas);
  addDataJson(first, index, index_end, "running", is_running);
  addDataJson(first, index, index_end, "low_voltage", low_voltage);
  addDataJson(first, index, index_end, "max_speed", remanent.max_speed);
  addDataJson(first, index, index_end, "trip_count", remanent.trip_count); 
  addDataJson(first, index, index_end, "motor_enable", motor_enable);
  addStringJson(index, index_end, "}");

  if (index >= index_end) {
    super_println("outofmemory");
    server.send(404, "text/json", "\"oom\"");
    return;
  }
  server.send(200, "text/json", json, index - json);
}

void handleAjax()
{
  for (int i = 0; i < server.args(); i++) {
    const String &arg_name = server.argName(i);
    const String &arg_value = server.arg(i);

    super_println(arg_name);
    super_println(arg_value);

    if (arg_name == "circumference") {
      remanent.circumference = arg_value.toInt();
    } else if (arg_name == "assist_level") {
      assist_level = arg_value.toInt();
    } else if (arg_name == "walkassist") {
      walkassist = (arg_value == "true" || arg_value == "on");
    } else if (arg_name == "motor_enable") {
      bool new_motor_enable = (arg_value == "true" || arg_value == "on");

      if (!new_motor_enable && motor_enable) {
        write_to_eeprom();
      }

      digitalWrite(motorEnablePin, new_motor_enable ? HIGH : LOW);
      motor_enable = new_motor_enable;
    } else if (arg_name == "max_speed") {
      remanent.max_speed = arg_value.toInt();
      write_to_eeprom();
    } else if (arg_name == "reset_eeprom") {
      reset_eeprom();
      write_to_eeprom();
    }
  
  }
  answerClient();
}



////////////////////////////// TSDZ2 /////////////////////////////////

uint8_t control_flag()
{
  uint8_t retval = 0;
 
  if (headlight)
    retval |= 1<<0;

  if (walkassist) {
    retval |= 1<<5;
  } else {
    switch (assist_level)
    {
      case 0:
        retval |= 1<<4;
        break;
      case 1:
        retval |= 1<<7;
        break;
      case 2:
        retval |= 1<<6;
        break;
      case 3:
        retval |= 1<<1;
        break;
      case 4:
        retval |= 1<<2;
        break;
      default:
        retval |= 1<<3;
        break;
    }
  }
  return retval;
}


void send_to_motor()
{
  if (motor_enable) {
    switch_uart_to_tsdz2();
  } else {
    return;
  }
  uint8_t to_send[7] = {0x59, control_flag(), 0, static_cast<uint8_t>(remanent.circumference / DIAMETER_INCH_TO_CIRCUMFERENCE_MM), 0, maximum_speed, 0xFF};
  CRC8 crc8;
  crc8.add(to_send, sizeof(to_send) - 1);
  to_send[sizeof(to_send) - 1] = crc8.getCRC();

  tsdz2.write(to_send, sizeof(to_send));  

#if 0
  static char buffer[128];
  char *ptr = buffer;
  char *end_ptr = buffer + sizeof(buffer);
  ptr += snprintf(ptr, end_ptr - ptr, "Sending to TSDZ2 : ");
  for (size_t i = 0; i < sizeof(to_send); i++)
  {
    ptr += snprintf(ptr, end_ptr - ptr, "0x%02x ", (int)to_send[i]);
  }
  super_println(buffer);
#endif
}

void receive_from_motor()
{
  static unsigned long last_reception = 0xFFFFFFFF;
  static unsigned long um_accumulate = 0;
  static unsigned long ms_accumulate = 0;

  if (motor_enable) {
    switch_uart_to_tsdz2();
  } else {
    return;
  }

  if (tsdz2.available() < 9 || tsdz2.read() != 0x43) {
    return;
  }

  uint8_t buffer[8];
  tsdz2.readBytes(buffer, sizeof(buffer));
  CRC8 crc8;
  crc8.add(buffer, sizeof(buffer) - 1);
  if (crc8.getCRC() != buffer[sizeof(buffer) - 1]) {
    error_code = 0xFF;
    return;
  }
 
  unsigned long current_time = millis();
  bool shall_reset = false;
  if (last_reception == 0xFFFFFFFF) {
    shall_reset = true;
  }
  unsigned long elapsed = current_time - last_reception;
  if (elapsed > 500) {
    shall_reset = true;
  }
  if (shall_reset) {
    elapsed = 0;  
  }

  motor_batterylevel = buffer[0];
  uint8_t motor_status = buffer[1];
  low_voltage = motor_status & (1 << 0);
  is_running = motor_status & (1 << 2);
  is_pas = motor_status & (1 << 3);
  tara = buffer[2];
  torque = buffer[3];
  error_code = buffer[4];
  speedsensor = (uint16_t) buffer[5] | (buffer[6] << 8);

  if (shall_reset) {
    speed_kph_times_1000 = 0;
  } else {
    speed_kph_times_1000 = (uint32_t) remanent.circumference * MM_PER_UNIT_TO_KPH_TIMES_1000 / speedsensor;

    if (motor_enable) {
      um_accumulate += (uint32_t) remanent.circumference * elapsed * US_PER_UNIT / speedsensor;
      remanent.total_m += um_accumulate / 1000000;
      um_accumulate %= 1000000;
      ms_accumulate += elapsed;
      remanent.total_s += ms_accumulate / 1000;
      ms_accumulate %= 1000;
    }
  }
}






////////////////////////////// UART management /////////////////////////////////
#if 0
bool usb_uart = true;

void switch_uart_to_tsdz2()
{
  if (usb_uart) {
//    Serial.println("Switching to tsdz2, will be silent for a while");
    Serial.flush();
    Serial.end();
    Serial.swap();
    Serial.begin(9600);
    usb_uart = false;
  }
}

void switch_uart_to_usb()
{
  if (!usb_uart) {
    Serial.flush();
    Serial.end();
    Serial.swap();
    Serial.begin(115200);
//    Serial.println("Switching to usb, I'm back");
    usb_uart = true;
  }
}

void switch_uart(bool new_usb_uart)
{
  if (new_usb_uart) {
    switch_uart_to_usb();
  } else {
    switch_uart_to_tsdz2();
  }
}

template <typename T>
void super_println(T &&arg)
{
  bool backup = usb_uart;
  switch_uart_to_usb();
  Serial.println(std::forward<T>(arg));
  switch_uart(backup);
}

#else


template <typename T>
void super_println(T &&arg)
{
  Serial.println(std::forward<T>(arg));
}

void switch_uart_to_tsdz2()
{

}

void switch_uart_to_usb()
{

}

#endif

////////////////////////////// EEPROM /////////////////////////////////
void reset_eeprom()
{
  init_m -= remanent.total_m;
  init_s -= remanent.total_s;
  remanent.total_m = 0;
  remanent.total_s = 0;
  remanent.circumference = default_circumference;
  remanent.trip_count = 1;
  remanent.max_speed = default_max_speed;
  
  write_to_eeprom();
}

void read_from_eeprom()
{
  EEPROM.get(0, remanent);
}

void write_to_eeprom()
{
  EEPROM.put(0, remanent);
  EEPROM.commit();
}
