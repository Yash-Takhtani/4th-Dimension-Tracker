#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <string.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

const char* ssid     = "Optimus Prime";
const char* password = "jaibhole";

#define TFT_SCLK 9
#define TFT_MOSI 10
#define TFT_RST -1
#define TFT_DC 6
#define TFT_CS 7

class MyST7789 : public Adafruit_ST7789 {
public:
  MyST7789(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst)
    : Adafruit_ST7789(cs, dc, mosi, sclk, rst) {}

    void setOffsets(uint8_t col, uint8_t row) {
      _colstart = _colstart2 = col;
      _rowstart = _rowstart2 = row;
    }
  };
  
  MyST7789 tft = MyST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
  
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

const int button1 = 2;
const int button2 = 3;
const int button3 = 4;
const int button4 = 5;
const int button5 = 21;
const int button6 = 8;
const int buzzer = 20;

Preferences prefs;

String months[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
String days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
String daysShort[7] = {"S", "M", "T", "W", "T", "F", "S"};
int day = -1;

String last_time = "";
int last_date = 0;
int date = 32;
String today = "";

String getTime(){
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  
  String period = (h<12) ? "AM":"PM";
  if (h != 12) h = h%12;
  h = (h == 0)?12:h; 
  char buf[9];
  sprintf(buf, "%02d:%02d %s", h, m, period.c_str());
  return String(buf);
}

void showtimedaydate(struct tm *t){
  last_date = date;
  date = t->tm_mday;
  int month = t->tm_mon + 1;
  String mon = months[month-1];
  day = t->tm_wday;
  today = days[day];
  int year = t->tm_year + 1900;
  
  tft.setTextColor(tft.color565(255,255,255), 0x000);
  tft.setTextSize(2);
  tft.setCursor(5,55);
  tft.print(today);
  tft.setCursor(150,55);
  tft.print(String(date) + " " + mon + " " + String(year));
  
  tft.setCursor(0,29);
  tft.setTextSize(3);
  
  String time = getTime();
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(time, 0, 0, &x1, &y1, &w, &h);
  int x = (284 - w) / 2;
  
  tft.setCursor(x,29);
  tft.print(time);
}

// alarm config
int alarmringed[3] = {0,0,0};
struct alarmtime {
  int hour;
  int min;
  char p[3];
  int days[7];
  char strtime[3][3];
};

alarmtime alarmprofiles[3] = {
  {12,0,"AM",{0,0,0,0,0,0,0},{"12", "00", "AM"}},
  {12,0,"AM",{0,0,0,0,0,0,0},{"12", "00", "AM"}},
  {12,0,"AM",{0,0,0,0,0,0,0},{"12", "00", "AM"}}
};

unsigned long snoozering;
int selectedprofileindex = 0;
#define SAP (alarmprofiles[selectedprofileindex]) // Sealected Alarm Profile

int alarmsettingpos = 0;
int alarmstate = 0;
// 0 - idle
// 1 - ringing

alarmtime alarmBLE;

int mode = 0;
int prevmode = -1;
// 0 - home
// 1 - stopwatch
// 2 - timer
// 3 - alarm
// 4 - timer ringing
// 5 - alarm ringing

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789000"
#define CHARACTERISTIC_UUID "12345678-1234-1234-1234-123456789001"

void resetalarmscreen(){
  tft.fillScreen(0x000);
  tft.setTextColor(tft.color565(255,90,90), 0x000);
  tft.setTextSize(1);
  tft.setCursor(8,10);
  tft.println("Alarm");
  tft.setTextSize(2);
  tft.setCursor(264,10);
  tft.println(selectedprofileindex+1);
  
  tft.setTextColor(tft.color565(255,255,255), 0x000);
  tft.setTextSize(3);
  tft.setCursor(70,15);
  tft.println(
      String(SAP.strtime[0]) + ":" +
      SAP.strtime[1] + " " +
      SAP.p
    );

    tft.setTextSize(2);
  for (int i=0;i<7;i++){
    tft.setCursor(93 + i*14,48);
    if (SAP.days[i]){
      tft.setTextColor(tft.color565(255,255,0), 0x000);
    }
    tft.print(daysShort[i]);
    tft.setTextColor(tft.color565(255,255,255), 0x000);
  }
}

class AlarmCallbacks : public NimBLECharacteristicCallbacks {

    void onWrite(NimBLECharacteristic *pCharacteristic,
                 NimBLEConnInfo &connInfo) override {

        Serial.println("Alarm data recieved!");
        std::string received = pCharacteristic->getValue();
        Serial.println(received.c_str());
        JsonDocument doc;
        DeserializationError error =
            deserializeJson(doc, received);
        if (error) {
            Serial.print("JSON ERROR: ");
            Serial.println(error.c_str());
            return;
        }

        alarmBLE.hour = doc["hour"] | 0;
        alarmBLE.min = doc["min"] | 0;

        strlcpy(alarmBLE.p, doc["p"] | "",
                sizeof(alarmBLE.p));

        for (int i = 0; i < 7; i++) {
            alarmBLE.days[i] = doc["days"][i] | 0;
        }

        for (int i = 0; i < 3; i++) {
            strlcpy(alarmBLE.strtime[i],
                    doc["strtime"][i] | "",
                    sizeof(alarmBLE.strtime[i]));
                  }
        SAP = alarmBLE;
        prefs.putBytes("alarmprofiles", &alarmprofiles, sizeof(alarmprofiles));
        delay(200); // to prevent unwanted snooze ringing.
        if (mode == 3) resetalarmscreen();
    }
};

int duty = 350;

void setup() {
  ledcAttach(buzzer, 2000, 10);
  pinMode(button1,INPUT_PULLUP);
  pinMode(button2,INPUT_PULLUP);
  pinMode(button3,INPUT_PULLUP);
  pinMode(button4,INPUT_PULLUP);
  pinMode(button5,INPUT_PULLUP);
  pinMode(button6,INPUT_PULLUP);
  Serial.begin(115200);
  tft.init(76, 284);
  tft.setOffsets(82, 18);
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(tft.color565(255,255,255), 0x000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  prefs.begin("data",false); 
  if (prefs.getBytesLength("alarmprofiles") == sizeof(alarmprofiles)) {
    prefs.getBytes("alarmprofiles",&alarmprofiles,sizeof(alarmprofiles));
  }
  
  selectedprofileindex = prefs.getInt("selectedprofileindex", 0);
  
  NimBLEDevice::init("4th Dimension Tracker");
  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service =
    server->createService(SERVICE_UUID);
  NimBLECharacteristic *characteristic =
    service->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE
    );
  characteristic->setCallbacks(new AlarmCallbacks());
  service->start();
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setName("4th Dimension Tracker");
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();
  Serial.println("BLE Alarm Server Started");

  // Volume
  duty = prefs.getInt("duty",350);
  
  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  String time = getTime();
  time_t now = timeClient.getEpochTime();
  struct tm *t = gmtime(&now);
  showtimedaydate(t);
}

// ringtone
const int melody[] = {
  1600, 2000, 2400, 2000,
  1600, 2000, 2400, 2000,
  1800, 2200, 2600, 2200,
  1800, 2200, 2600, 2200
};

const int noteDuration[] = {
  180, 180, 260, 180,
  180, 180, 260, 300,
  180, 180, 260, 180,
  180, 180, 260, 300
};

const int melodyLength = sizeof(melody) / sizeof(melody[0]);

int currentNote = 0;
unsigned long noteStart = 0;
unsigned long nextNoteTime = 0;

bool musicPlaying = false;
bool noteOn = false;

void startMusic() {
  musicPlaying = true;
  currentNote = 0;
  noteOn = false;
  nextNoteTime = millis();
}

void stopMusic() {
  musicPlaying = false;
  noteOn = false;
  ledcWrite(buzzer, 0);
}

void updateMusic() {
  if (!musicPlaying) return;

  unsigned long now = millis();

  if (noteOn) {
    if (now - noteStart >= (unsigned long)noteDuration[currentNote - 1]) {
      ledcWrite(buzzer, 0);
      noteOn = false;
      nextNoteTime = now + 35;
    }
    return;
  }

  if ((long)(now - nextNoteTime) >= 0) {
    if (currentNote >= melodyLength) {
      currentNote = 0;
    }

    ledcWriteTone(buzzer, melody[currentNote]);
    ledcWrite(buzzer, duty);

    noteStart = now;
    noteOn = true;
    currentNote++;
  }
}

void beep(int frequency,int duration){
  if (musicPlaying) return;
  ledcWriteTone(buzzer, frequency);
  ledcWrite(buzzer, duty);
  delay(duration);
  ledcWrite(buzzer, 0);
}

int prev_b1 = 1;
int prev_b2 = 1;
int prev_b3 = 1;
int prev_b4 = 1;
int prev_b5 = 1;
int prev_b6 = 1;

bool b1pressed = false;
bool b2pressed = false;
bool b3pressed = false;
bool b4pressed = false;
bool b5pressed = false;
bool b6pressed = false;

int b1 = 0;
int b2 = 0;
int b3 = 0;
int b4 = 0;
int b5 = 0;
int b6 = 0;

// stopwatch
int timeelapsed = 0;
unsigned long swtime;
int swstate = 0;
// 0 reset 
// 1 running
// 2 pause

// timer
int timerh;
int timerm;
int timers;
unsigned long timert; // the time when timer will ring
int timersettingpos;
int timerstate;
// 0 reset 
// 1 running
// 2 pause
int timerdat[3] = {00,00,00};
long timeleft;
bool ringtimer;

void loop() {
  b1 = digitalRead(button1);
  b2 = digitalRead(button2);
  b3 = digitalRead(button3);
  b4 = digitalRead(button4);
  b5 = digitalRead(button5);
  b6 = digitalRead(button6);

  b1pressed = ((prev_b1 == 0) && (b1 == 1));
  b2pressed = ((prev_b2 == 0) && (b2 == 1));
  b3pressed = ((prev_b3 == 0) && (b3 == 1));
  b4pressed = ((prev_b4 == 0) && (b4 == 1));
  b5pressed = ((prev_b5 == 0) && (b5 == 1));
  b6pressed = ((prev_b6 == 0) && (b6 == 1));

  if ((b3pressed||b4pressed||b5pressed||b6pressed) && (mode != 0)) beep(1500,50);

  if (b2pressed){
    if ((mode != 5) && (mode != 4)){
      beep(2000,50);
      mode += 1;
      if (mode > 3) mode = 0;
    }
  }
  
  timeClient.update();
  String time = getTime();
  time_t now = timeClient.getEpochTime();
  struct tm *t = gmtime(&now);
  day = t->tm_wday;

  //alarm ringing
  if (day >= 0 && day < 7 && mode != 5) {
    for (int i = 0; i < 3; i++) {
      int alarmHour24 = alarmprofiles[i].hour % 12;

      if (strcmp(alarmprofiles[i].p, "PM") == 0) {
        alarmHour24 += 12;
      }

      bool matches =
        alarmprofiles[i].days[day] &&
        t->tm_hour == alarmHour24 &&
        t->tm_min == alarmprofiles[i].min;

      if (matches || ((millis() >= snoozering) && (millis() <= snoozering+500))) {
        if ((!alarmringed[i] && mode != 3)){
          alarmringed[i] = 1;
          alarmstate = 1;
          mode = 5;
          
          startMusic();
          
          tft.fillScreen(0x0000);
          tft.setTextSize(2);
          tft.setTextColor(tft.color565(255, 0, 0), 0x000);
          tft.setCursor(96, 9);
          tft.println("Wake Up!");
          tft.setTextColor(tft.color565(255, 255, 255), 0x000);
          showtimedaydate(t);
          break;
        }
      }else{
        alarmringed[i] = 0;
      }
    }
  }
  
  if (mode == 0){ // home screen
    if (prevmode != 0){
      tft.fillScreen(0x000);
      tft.drawRect(91,9,102,12,tft.color565(255,255,255));
      tft.fillRect(92,10,(duty/5),10,tft.color565(255,255,255));
      showtimedaydate(t);
    }
    if (time != last_time) showtimedaydate(t);
    last_time = time;
    if (b3pressed){ // v+
      if (duty <= 475) duty += 25;
      prefs.putInt("duty",duty);
      beep(2000,50);
      tft.fillRect(92,10,(duty/5),10,tft.color565(255,255,255));
    }
    if (b4pressed){ // v-
      if (duty >= 25) duty -= 25;
      prefs.putInt("duty",duty);
      beep(2000,50);
      tft.fillRect(92 + (duty/5),10,100-(duty/5),10,0x000);
    }
  }
  
  else if (mode == 1){ // stop watch
    if (prevmode != 1){
      tft.fillScreen(0x000);
      tft.setTextColor(tft.color565(255,255,0), 0x000);
      tft.setTextSize(1);
      tft.setCursor(8,10);
      tft.println("Stopwatch");
      tft.setTextColor(tft.color565(200,200,200), 0x000);
      tft.setTextSize(3);
      tft.setCursor(70,25);
      tft.println("00:00:00");
    }
    
    if (b3pressed){
      tft.setTextColor(tft.color565(200,200,200), 0x000);
      tft.setTextSize(3);
      tft.setCursor(70,25);
      tft.println("00:00:00");
      swstate = 0;
      timeelapsed = 0;
    }else if (b1pressed){
      beep(2000,50);
      if (swstate != 1){
        swstate = 1;
        swtime = millis() - (timeelapsed*1000);
      }else{
        swstate = 2;
      }
    }

    if (swstate == 1){
      timeelapsed = (millis()-swtime)/1000;
      char buf[9];
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
      timeelapsed / 3600,
      (timeelapsed % 3600) / 60,
      timeelapsed % 60);
      tft.setTextColor(tft.color565(255,255,255), 0x000);
      tft.setCursor(70,25);
      tft.println(buf);
    }
  }
  
  else if (mode == 2){ // Timer
    if (prevmode != 2){
      tft.fillScreen(0x000);
      tft.setTextColor(tft.color565(0, 140, 255), 0x000);
      tft.setTextSize(1);
      tft.setCursor(8,10);
      tft.println("Timer");
      tft.setTextColor(tft.color565(255,255,255), 0x000);
      tft.setTextSize(3);
      tft.setCursor(70,25);
      char buf[9];
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
      timerh,
      timerm,
      timers);
      tft.println(buf);
    }
  
    if (timerstate == 0){
      timerdat[0] = timerh;
      timerdat[1] = timerm;
      timerdat[2] = timers; 
      tft.setCursor(70+(36*timersettingpos) + 18*(timersettingpos),25);
      tft.setTextColor(tft.color565(0,255,0), 0x000);
      tft.print(((timerdat[timersettingpos] < 10)? "0":"") + String(timerdat[timersettingpos]));
    }else if (timerstate == 1){
      if(timert <= millis()){
        mode = 4;
        ringtimer = true;
        timerstate = 0;
        timeleft = 0;
      }else{
        timeleft = (timert-millis())/1000;
      }

      char buf[9];
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
      timeleft/3600,
      (timeleft%3600)/60,
      timeleft%60);
      tft.setCursor(70,25);
      tft.setTextColor(tft.color565(255,255,255), 0x000);
      tft.println(buf);
    }
    
    if (b1pressed){
      if (timerstate != 1){
        timerstate = 1;
        timert = millis()+(timeleft*1000);
      }else{
        timerstate = 2;
      }
    }

    if (b3pressed){
      tft.setCursor(70+(36*timersettingpos) + 18*(timersettingpos),25);
      tft.setTextColor(tft.color565(255,255,255), 0x000);
      tft.print(((timerdat[timersettingpos] < 10)? "0":"") + String(timerdat[timersettingpos]));
      timersettingpos++;
      if (timersettingpos > 2) timersettingpos = 0;
    }
    if (b4pressed){
      timerstate = 0;
      timeleft = 0;
      timert = 0;
      timersettingpos = 0;
      timerh = 0;
      timerm = 0;
      timers = 0;
      timerdat[0] = 0;
      timerdat[1] = 0;
      timerdat[2] = 0;
      tft.setTextColor(tft.color565(255,255,255), 0x000);
      tft.setTextSize(3);
      tft.setCursor(70,25);
      tft.println("00:00:00");
    }
    if (b5pressed && (timerstate == 0)){
      if (timersettingpos == 0){
        timerh = (timerh % 99) + 1;
        timerdat[0] = timerh;
        timeleft = timeleft + 3600;
      }else if (timersettingpos == 1){
        timerm = (timerm + 1) % 60;
        timerdat[1] = timerm;
        timeleft = timeleft + 60;
      }else if (timersettingpos == 2){
        timers = (timers + 1) % 60;
        timerdat[2] = timers;
        timeleft = timeleft + 1;
      }
    }
    if (b6pressed && (timerstate == 0)){
      if ((timersettingpos == 0) && (timerh % 99) > 0){
        timeleft = timeleft - 3600;
        timerh = (timerh % 99) - 1;
        timerdat[0] = timerh;
      }else if ((timersettingpos == 1) && (timerm % 60) > 0){
        timerm = (timerm - 1) % 60;
        timeleft = timeleft - 60;
        timerdat[1] = timerm;
      }else if ((timersettingpos == 2) && (timers % 60) > 0){
        timers = (timers - 1) % 60;
        timerdat[2] = timers;
        timeleft = timeleft - 1;
      }
    }
  }
  
  else if (mode == 3){ // alarm
    if (prevmode != 3){ // show initial config
      resetalarmscreen();
    }
    if (b6pressed){
      selectedprofileindex += 1;
      if (selectedprofileindex > 2){
        selectedprofileindex = 0;
      }
      prefs.putInt("selectedprofileindex", selectedprofileindex);
      resetalarmscreen();
    }
    if (alarmsettingpos < 3){ // assign colour to selected
      tft.setTextSize(3);
      tft.setCursor(70+(36*alarmsettingpos) + 18*(alarmsettingpos),15);
      tft.setTextColor(tft.color565(0,255,0), 0x000);
      tft.print(SAP.strtime[alarmsettingpos]);
      tft.setTextColor(tft.color565(255,255,255), 0x000);
    }else{
      tft.setTextSize(2);
      tft.setCursor(93+(14*(alarmsettingpos-3)),48);
      tft.setTextColor(tft.color565(0,255,0), 0x000);
      if (SAP.days[alarmsettingpos-3]){
        tft.setTextColor(tft.color565(0,200,0), 0x000);
      } else{
        tft.setTextColor(tft.color565(0,255,0), 0x000);
      }
      tft.print(daysShort[alarmsettingpos-3]);
    }

    if (b3pressed){ // move btw settings
      if(alarmsettingpos < 3){
        tft.setTextSize(3);
        tft.setCursor(70+(36*alarmsettingpos) + 18*(alarmsettingpos),15);
        tft.setTextColor(tft.color565(255,255,255), 0x000);
        tft.print(SAP.strtime[alarmsettingpos]);
      }else{
        tft.setTextSize(2);
        tft.setCursor(93+(14*(alarmsettingpos-3)),48);
        if (SAP.days[alarmsettingpos-3]){
          tft.setTextColor(tft.color565(255,255,0), 0x000);
        }else{
          tft.setTextColor(tft.color565(255,255,255), 0x000);
        }
        tft.print(daysShort[alarmsettingpos-3] + " ");
      }
      alarmsettingpos++;
      if (alarmsettingpos > 9) alarmsettingpos = 0;
    }

    if(b4pressed){ // increase selected
      if (alarmsettingpos == 0){
        SAP.hour = (SAP.hour % 12) + 1;
        snprintf(SAP.strtime[0], 3,
         "%02d", SAP.hour);
      }else if (alarmsettingpos == 1){
        SAP.min = (SAP.min + 1) % 60;
        snprintf(SAP.strtime[1], 3,
         "%02d", SAP.min);
      }else if (alarmsettingpos == 2){
        if (String(SAP.p) == "AM")
            strcpy(SAP.p, "PM");
        else
            strcpy(SAP.p, "AM");

        strcpy(SAP.strtime[2],
              SAP.p);
      }else{
        SAP.days[alarmsettingpos-3] = !(SAP.days[alarmsettingpos-3]);
      }
      prefs.putBytes("alarmprofiles", &alarmprofiles, sizeof(alarmprofiles));
    }
    
    if(b5pressed){ // decrease selected
      if (alarmsettingpos == 0) {
          SAP.hour =
              (SAP.hour == 1)
              ? 12 : SAP.hour - 1;

          snprintf(SAP.strtime[0], 3,
                  "%02d", SAP.hour);

      } else if (alarmsettingpos == 1) {
          SAP.min =
              (SAP.min == 0)
              ? 59 : SAP.min - 1;

          snprintf(SAP.strtime[1], 3,
                  "%02d", SAP.min);

      } else if (alarmsettingpos == 2) {
          if (String(SAP.p) == "AM")
              strcpy(SAP.p, "PM");
          else
              strcpy(SAP.p, "AM");

          strcpy(SAP.strtime[2],
                SAP.p);

      } else {
          SAP.days[alarmsettingpos - 3] =
              !SAP.days[alarmsettingpos - 3];
      }
      prefs.putBytes("alarmprofiles", &alarmprofiles, sizeof(alarmprofiles));
    }
  }else if (mode == 4){ // timer ring
    if (ringtimer){
      ringtimer = false;
      tft.fillScreen(0x000);
      tft.setTextColor(tft.color565(255,255,255), 0x000);
      tft.setTextSize(2);
      tft.setCursor(94,15);
      tft.println("Time's up!");
      tft.setTextSize(3);
      tft.setCursor(70,40);
      tft.println(((timerh < 10)? "0":"") + String(timerh) + ":" + ((timerm < 10)? "0":"") + String(timerm) + ":" + ((timers < 10)? "0":"") + String(timers));
      startMusic();
    }
    if (b1pressed){
      stopMusic();
      tft.fillScreen(0x000);
      mode = 0;
    }
    updateMusic();
  }else if (mode == 5){ // alarm ring
    updateMusic();
    if (alarmstate == 1){
      if (b1pressed){ // dismiss
        stopMusic();
        alarmstate = 0;
        mode = 0;
        tft.fillRect(90,8,110,17,0x0000);
        tft.drawRect(91,9,102,12,tft.color565(255,255,255));
        tft.fillRect(92,10,(duty/5),10,tft.color565(255,255,255));
      }else if (b4pressed){ // snooze
        stopMusic();
        alarmstate = 0;
        mode = 0;
        snoozering = millis()+300000; 
        tft.fillRect(90,8,110,17,0x0000);
        tft.drawRect(91,9,102,12,tft.color565(255,255,255));
        tft.fillRect(92,10,(duty/5),10,tft.color565(255,255,255));
      }
    }
    if ((time != last_time)) showtimedaydate(t);
    last_time = time;
  }

  prev_b1 = b1;
  prev_b2 = b2;
  prev_b3 = b3;
  prev_b4 = b4;
  prev_b5 = b5;
  prev_b6 = b6;
  prevmode = mode;
}