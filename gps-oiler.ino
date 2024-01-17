//Warning! Use fast UART speed at least 19200, but 115200 is better. 
//otherwise uart buffer will overfill and it will loose position from time to time.

// defining ACCESS_POINT will activate access point + captive portal
#define ACCESS_POINT

#ifdef ACCESS_POINT
  #include <ESP8266WiFi.h>
  #include <DNSServer.h>
#else
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti WiFiMulti;
#endif


#include <ESP8266WebServer.h>
#include <EEPROM.h>  
#include <SoftwareSerial.h>
#include <TinyGPS.h> 

#define PUMP_PIN D5
SoftwareSerial gpsSerial(D6, D8);  //  RX, TX
TinyGPS gps; // create gps object 


/* Put your SSID & Password */
#ifdef ACCESS_POINT
  const char* ssid = "moto_access_point";  // Enter SSID here
  const char* password = "my_password";  //Enter Password here
  
  const byte DNS_PORT = 53;
  IPAddress apIP(192, 168, 1, 1);
  DNSServer dnsServer;
#else
  const char* ssid = "my_home_or_mobile_AP";  // Enter SSID here
  const char* password = "password";
#endif

#define OIL_RECORD_COUNT 5

String responseHTML = ""
  "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body>"
  "<h1>Hello World!</h1><p>This is a captive portal example. All requests will "
  "be redirected here.</p></body></html>";

void blink(int cnt, int length1, int length2){
  for(int i=0;i<cnt;i++){
    digitalWrite(LED_BUILTIN, LOW);
    delay(length1);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(length2);
  }
}

class Oil_record_t { // its a definition of oiling. There are OIL_RECORD_COUNT = 5 of them. "Usual", "Rain", "Sand" and so on.
  private:
    String name;
    
  public:
    int pump_distance; // one pump impulse per pump_distance meters
    String get_name(){
      return name;
    }
    void set_name(String new_name){
      name = new_name;
    }
    Oil_record_t(){
      pump_distance = 999999;
      name = "";
    }
};

// time of pump impulses in ms.
#define PUMP_ON_TIME 200
#define PUMP_OFF_TIME 200

Oil_record_t oil_records[OIL_RECORD_COUNT];

// default parameters 
int settings_min_speed = 50; // min speed km/h for oiling. If speed is lower, oiling is suspended.
int settings_min_distance = 2000; // min distance from garage to start oiling
int settings_oiling_period_secs_no_gps = 60; 
int settings_oiling_start_secs_no_gps = 300;
float settings_lat = 50.000000; // garage GPS
float settings_lon = 20.000000; // garage GPS

// variables
unsigned long last_oiling_millis_no_gps = 0; 
unsigned long last_gps_present_millis = 0; // to start oiling if no gps signal
float gps_lat = 0;
float gps_lon = 0;
float gps_speed = 0;
float gps_hdop = 0;
unsigned long gps_chars = 0;
unsigned short gps_sentences = 0;
unsigned short gps_failed = 0;
int gps_satellites = TinyGPS::GPS_INVALID_SATELLITES;

int active_row = 3;
int pumps_left = 0;
float distance_since_last_pump;
unsigned long last_pump_millis = 0;
unsigned long next_pump_millis = 0;

void load_settings_from_eeprom(){
  EEPROM.begin(512);
  int addr = 0;
  EEPROM.get(addr,settings_min_speed);
  addr += sizeof(settings_min_speed);
  EEPROM.get(addr,settings_min_distance);
  addr += sizeof(settings_min_distance);
  EEPROM.get(addr,settings_lat);
  addr += sizeof(settings_lat);
  EEPROM.get(addr,settings_lon);
  addr += sizeof(settings_lon);
  EEPROM.get(addr,settings_oiling_period_secs_no_gps);
  addr += sizeof(settings_oiling_period_secs_no_gps);
  for (int i=0;i<OIL_RECORD_COUNT;i++){
    EEPROM.get(addr,oil_records[i].pump_distance);
    addr += sizeof(oil_records[i].pump_distance);
  }
  EEPROM.get(addr,settings_oiling_start_secs_no_gps);
  addr += sizeof(settings_oiling_start_secs_no_gps);
  oil_records[0].pump_distance = 0;
  oil_records[OIL_RECORD_COUNT-1].pump_distance = 999999;
  EEPROM.get(addr,active_row);
  if (active_row >4 | active_row<0)
    active_row = 1;
  addr += sizeof(active_row);
}

void save_settings_to_eeprom(){
  EEPROM.begin(512);
  int addr = 0;
  EEPROM.put(addr,settings_min_speed);
  addr += sizeof(settings_min_speed);
  EEPROM.put(addr,settings_min_distance);
  addr += sizeof(settings_min_distance);
  EEPROM.put(addr,settings_lat);
  addr += sizeof(settings_lat);
  EEPROM.put(addr,settings_lon);
  addr += sizeof(settings_lon);
  EEPROM.put(addr,settings_oiling_period_secs_no_gps);
  addr += sizeof(settings_oiling_period_secs_no_gps);
  for (int i=0;i<OIL_RECORD_COUNT;i++){
    EEPROM.put(addr,oil_records[i].pump_distance);
    addr += sizeof(oil_records[i].pump_distance);
  }
  EEPROM.put(addr,settings_oiling_start_secs_no_gps);
  addr += sizeof(settings_oiling_start_secs_no_gps);
  EEPROM.put(addr,active_row);
  addr += sizeof(active_row);
  EEPROM.commit();
}

void init_oil_records(){
  oil_records[0].set_name("Pumping");
  oil_records[0].pump_distance = 0;
  oil_records[1].set_name("Usual");
  oil_records[1].pump_distance = 1000;
  oil_records[2].set_name("Sand");
  oil_records[2].pump_distance = 200;
  oil_records[3].set_name("Rain");
  oil_records[3].pump_distance = 500;
  oil_records[4].set_name("Stop");
  oil_records[4].pump_distance = 999999;
}

void init_wifi(){
#ifdef ACCESS_POINT
//  delay(1000);
//  IPAddress local_ip(192,168,1,1);
//  IPAddress gateway(192,168,1,1);
//  IPAddress subnet(255,255,255,0);
//  Serial.print("Configuring access point...");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);
  
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);
    
#else
  delay(1000); 

  WiFiMulti.addAP(ssid, password);
  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");
//  WiFi.begin ( ssid, password );

//  blinking while connecting to wifi
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay ( 500 );
    Serial.print ( "." );
    digitalWrite(LED_BUILTIN, LOW);
    delay(40);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
#endif
}

String get_row(uint8_t index, uint8_t active_index){ // returns html part for one row in the table of oiler settings
  String name;
  String cnt;
  String action;
  if (active_index>=OIL_RECORD_COUNT){
    return "Error!"; 
  }
    name = oil_records[index].get_name();
  if (oil_records[index].pump_distance == 0){
    cnt = "all the time";
  } else if (oil_records[index].pump_distance == 999999){
    cnt = "no pumping";
  } else
    cnt = "<input type=\"text\" size=\"5\" value=\""+ String(oil_records[index].pump_distance) + "\" name=\"v"+ String(index)+ "\"/> ";
  if (index == active_index)
    action = "<b><font color=\"green\">Active</font></b>";
  else
    action = "<button type=\"submit\" name=\"action\" value=\"v" + String(index) + "\">Save & Activate</button>";
  return "<tr><td>" + name + "</td><td>" + cnt + "</td><td>" + action + "</td></tr>";
}

ESP8266WebServer server(80);

void handle_OnConnect() { // processing http request
  Serial.println("Onconnect()");
  bool save = false;
  for (int i = 0; i < server.args(); i++) { // parameters
    save = true;
    String arg_name = server.argName(i);
    int int_value = server.arg(i).toInt();
    float float_value = server.arg(i).toFloat();
    if (arg_name == "v1"){
        oil_records[1].pump_distance = int_value;
    } else if (arg_name == "v2"){
        oil_records[2].pump_distance = int_value;
    } else if (arg_name == "v3"){
        oil_records[3].pump_distance = int_value;
    } else if (arg_name == "min_speed"){
        settings_min_speed = int_value;
    } else if (arg_name == "distance1"){
        settings_min_distance = int_value;
    } else if (arg_name == "lat"){
        settings_lat = float_value;
    } else if (arg_name == "lon"){
        settings_lon = float_value;
    } else if (arg_name == "settings_oiling_period_secs_no_gps"){
        settings_oiling_period_secs_no_gps = int_value;
    } else if (arg_name == "settings_oiling_start_secs_no_gps"){
        settings_oiling_start_secs_no_gps = int_value;
    } else if (arg_name == "action"){
      if (server.arg(i) == "v0"){
        active_row = 0;
        pumps_left = 100;
      } else if (server.arg(i) == "v1"){
        pumps_left = 0;
        active_row = 1;
      } else if (server.arg(i) == "v2"){
        pumps_left = 0;
        active_row = 2;
      } else if (server.arg(i) == "v3"){
        pumps_left = 0;
        active_row = 3;
      } else if (server.arg(i) == "v4"){
        pumps_left = 0;
        active_row = 4;
      }
    }
  }  
  if (save){
    save_settings_to_eeprom();
    server.sendHeader("Location", String("/"), true);
    server.send ( 302, "text/plain", "");
  } else
    server.send(200, "text/html", SendHTML()); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(){ 
  Serial.println("sendHTML()");

  String head = "<html>\
<head>\
<title>Chain oiler</title>\
  <style type=\"text/css\">\
   .btn { \
    font-size: 120%; \
    font-family: Verdana, Arial, Helvetica, sans-serif; \
    color: #336; \
    border-color: #e38d13;\
   }\
  </style>\
</head>\
<body>\n";
  String pump_info = "Pump left:" + String(pumps_left)+"<br>";
  String gps_info = "Satellites: " + String(gps_satellites) + " PREC:"+ String(gps_hdop,3) + " CHARS:" + String(gps_chars) + " SENTENCES:" + String(gps_sentences)+ " CSUM ERR=" + String(gps_failed) + "<BR>\n";
  String gps_pos = "Current GPS position: "+ String(gps_lat, 7) +" "+String(gps_lon, 7)+" . Speed:"+ String(gps_speed) +" Distance to garage is "+
  String(TinyGPS::distance_between(gps_lat, gps_lon, settings_lat, settings_lon)) +" meters. <br>"+
  "Distance from last pump:"+String(distance_since_last_pump)+"<br>"+
  "Last GPS signal received " + String((millis() - last_gps_present_millis) / 1000) +" seconds ago. Last no GPS oiling "+
  String((millis() - last_oiling_millis_no_gps) / 1000)+" seconds ago.<br>";
  String form_start = "<form name=\"test\" method=\"GET\" action=\"/\">\
<table border=\"1\">\
  <tr>\
  <td>Name</td><td>Pump each meters</td><td>Action</td>\
  </tr>";
  String table = "";
  for (uint8_t i=0; i<OIL_RECORD_COUNT; i++){
    table += get_row(i,active_row);
  }
  table += "\n</table><br>";
  
  String min_speed = "Minimum speed for oiling: <input type=\"text\" size=\"5\" value=\""+ String(settings_min_speed) +"\" name=\"min_speed\"/> km/h";
  String dont_oil = "<p><b>Don't oil near within <input type=\"text\" size=\"5\" value=\""+ String(settings_min_distance)+
    "\" name=\"distance1\"/> meters from <input type=\"text\" size=\"8\" value=\""+ String(settings_lat, 7)+
    "\" name=\"lat\" /> <input type=\"text\" size=\"8\" value=\""+ String(settings_lon, 7) +"\" name=\"lon\" /></b></p>";
  String no_gps = "<p>Start oiling after <input type=\"text\" size=\"5\" value=\""+ String(settings_oiling_start_secs_no_gps)+ "\" name=\"settings_oiling_start_secs_no_gps\"/> secs of no GPS signal."+
    " Oil every <input type=\"text\" size=\"5\" value=\""+ String(settings_oiling_period_secs_no_gps)+"\" name=\"settings_oiling_period_secs_no_gps\"/> secs<br>\n";
  String end_html = "  <button type=\"submit\" name=\"action\" value=\"Save\">Save</button>\
</form>\
</body></html>";
  return head + pump_info + gps_info + gps_pos + form_start + table + min_speed + dont_oil + no_gps +
//  "active_row:" + String(active_row) + " next_pump_millis:" + String(next_pump_millis) + " last_pump_millis" + String(last_pump_millis)+
  end_html;
}

void setup() { 
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(10);

  init_wifi();
  Serial.println("Starting web server");
 
  init_oil_records();
  load_settings_from_eeprom();

  blink(3,200,200);

  server.on("/", handle_OnConnect);
  
  server.onNotFound([]() {
//    server.send(200, "text/html", responseHTML);
    server.sendHeader("Location", String("http://moto/"), true);
    server.send ( 302, "text/plain", "");
  });
  
  server.begin();
  Serial.println("HTTP server started");

}

void read_gps_data(){
  float lat = 0,lon = 0; // create variable for latitude and longitude object  

  while(gpsSerial.available()){ // check for gps data 
    uint8_t ch = gpsSerial.read();
//    Serial.print((char)ch);
    if(gps.encode(ch))// encode gps data 
    {  
      unsigned long age;
      gps.f_get_position(&lat,&lon, &age); // get latitude and longitude 
      if (lat != TinyGPS::GPS_INVALID_F_ANGLE & lon != TinyGPS::GPS_INVALID_F_ANGLE & (gps_lat != lat | gps_lon != lon)){
        if (gps_lat != 0 & gps_lon != 0)
          distance_since_last_pump += TinyGPS::distance_between(gps_lat, gps_lon, lat, lon);
        gps_lat = lat;
        gps_lon = lon;
        gps_speed = gps.f_speed_kmph();
        gps_hdop = gps.hdop();
        last_oiling_millis_no_gps = millis();
        last_gps_present_millis = millis();
      }
    }
  }
  gps_satellites = gps.satellites();
  gps.stats(&gps_chars, &gps_sentences, &gps_failed);
}

bool is_pump_available(){ // can pump ?
  if (pumps_left == 0)
    return false;
  if (active_row == 4)
    return false;
  if (active_row == 0)
    return true;

  if (millis() > last_gps_present_millis + settings_oiling_start_secs_no_gps * 1000) //no GPS signal
    return true;

//GPS signal present
  if (gps_speed < settings_min_speed)
    return false;
  if (TinyGPS::distance_between(gps_lat, gps_lon, settings_lat, settings_lon) < settings_min_distance)
    return false;
  return true;
}

void set_pump_pin(){ // function is executed frequently and set pump pin to appropriate level.
  unsigned long curr_time = millis();
  if (next_pump_millis>0 & last_pump_millis != next_pump_millis & next_pump_millis<=curr_time){ // tast to pump is set and we should start
    last_pump_millis = curr_time;
    next_pump_millis = last_pump_millis;
    digitalWrite(PUMP_PIN, HIGH);
  }
  if (next_pump_millis>0 & last_pump_millis == next_pump_millis &  next_pump_millis + PUMP_ON_TIME <= curr_time ){ // tast to pump is set and we should stop
    next_pump_millis = 0;
    digitalWrite(PUMP_PIN, LOW);
  }

  if (next_pump_millis == 0 & last_pump_millis + PUMP_ON_TIME + PUMP_OFF_TIME <=curr_time){
    if (is_pump_available()){
      next_pump_millis = curr_time;
      pumps_left--;
    }
  }
}


void add_pump_if_needed(){ 
  if (millis() > last_gps_present_millis + settings_oiling_start_secs_no_gps * 1000 & millis() > last_oiling_millis_no_gps + settings_oiling_period_secs_no_gps * 1000){
    last_oiling_millis_no_gps = millis();
    pumps_left++;
  }
  
  if (active_row !=0 & active_row != 4 & distance_since_last_pump > oil_records[active_row].pump_distance){
    pumps_left++;
    distance_since_last_pump = distance_since_last_pump - oil_records[active_row].pump_distance;
  }
}

void loop() {
#ifdef ACCESS_POINT
  dnsServer.processNextRequest();
#endif
  server.handleClient();
  read_gps_data();
  set_pump_pin();
  add_pump_if_needed();
}
