//kod przygotowany przez użytkownika Miglandz z forum forbot.pl (https://forbot.pl/forum/profile/15182-miglandz/)
//integracja z MQTT i niektóre komenatrze uzytkownik jasiek_ z forum forbot.pl (https://forbot.pl/forum/profile/20709-jasiek_/)

#include <PubSubClient.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "ESP32_FTPClient.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "FS.h"                // SD Card ESP32
#include "SD.h"                // SD Card - działa z mailem
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory
// define the number of bytes you want to access
#define EEPROM_SIZE 2

#include "ESP32_MailClient.h"

#include "driver/adc.h"
#include <esp_bt.h>

#include <OneWire.h>
#include <DallasTemperature.h>

const int oneWireBus = 3;
int wuc = 1;// po reboocie 1 WakeUpCase

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

#include <WiFiClient.h>

char ftp_server[] = "192.168.1.XX";
char ftp_user[]   = "XX";
char ftp_pass[]   = "XXX";

char clientRSSI[50];

// you can pass a FTP timeout and debbug mode on the last 2 arguments
ESP32_FTPClient ftp (ftp_server, ftp_user, ftp_pass, 5000, 2);

//unsigned long lastTemp = 0;
//long opoznienie = 0;
const char *ssid =     "XXXX";         // Put your SSID here
const char *password = "XXXX";     // Put your PASSWORD here
const char* mqttServer = "XXX.XXX.X.XX"; //wpisz ip brokera MQTT (jeśli używane w home-assitant - ten sam numer ip co home-assitanta 
const int mqttPort = 1883; //wpisz port brokera MQTT (domyślnie 1883)
const char* mqttUser = "XX"; //wpisz nazwę użytkownika brokera MQTT
const char* mqttPassword = "XXX"; //wpisz hasło brokera MQTT

WiFiClient wifiClient;

PubSubClient client(wifiClient);


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  1800       /* Time ESP32 will go to sleep (in seconds) --  polgodziny */


RTC_DATA_ATTR int bootCount = 0;

#define PART_BOUNDARY "123456789000000000000987654321"

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

int pictureNumber = 0;
int pictureNumber2 = 0;

//int se = 0;
int w = 0;


void setup() {

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  btStop();
  adc_power_off();
  esp_bt_controller_disable();

  //  opoznienie = 10000 / (1 << (12 - 12));
  //  lastTemp = millis();

  IPAddress ip(192, 168, 1, 22); // ustaw IP foto-pułapki
  IPAddress gateway(192, 168, 1, 222); //ustaw brmaę domyślną (IP routera)
  IPAddress subnet(255, 255, 255, 0);
  IPAddress primaryDNS(8, 8, 8, 8); // optional



  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet, primaryDNS);
  WiFi.setAutoConnect ( true );     // Make sure auto connect is set for next boot;
  WiFi.setAutoReconnect ( true );
  

  Serial.begin(115200);
  Serial.println("Start");
  //Serial.setDebugOutput(true);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // wlaczamy leda
  pinMode(4, INPUT);
  digitalWrite(4, LOW);
  rtc_gpio_hold_dis(GPIO_NUM_4);

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;  //10-63 lower number means higher quality 
    config.fb_count = 1;
    Serial.println("psram");
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("no psram");
  }

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  // pin 13 do budzenia
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1);

  // i ustawiamy wake up
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    //return;
    go_to_sleep();
  }
  else if (wuc != 1) // przy reboocie nie robimy zdjęcia
  {
    Serial.println("Camera init ok");
    camera_fb_t * fb = NULL;

    // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
    delay(100);
    // po 200ms wylaczamy aby nie zarlo baterii a w terenie bylo wiadomo ze dziala.
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_en(GPIO_NUM_4);


   

           sensor_t * s = esp_camera_sensor_get();

          s->set_brightness(s, 0);     // -2 to 2
          s->set_contrast(s, 0);       // -2 to 2
          s->set_saturation(s, 0);     // -2 to 2
          s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
          s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
          s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
          s->set_wb_mode(s, 1);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
          s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
          s->set_aec2(s, 0);           // 0 = disable , 1 = enable
          s->set_ae_level(s, 0);       // -2 to 2
          s->set_aec_value(s, 300);    // 0 to 1200
          s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
          s->set_agc_gain(s, 0);       // 0 to 30
          s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
          s->set_bpc(s, 0);            // 0 = disable , 1 = enable
          s->set_wpc(s, 1);            // 0 = disable , 1 = enable // likwiduje bad piksele białe
          s->set_raw_gma(s, 0);        // 0 = disable , 1 = enable // nie wiadomo co to robi domyslnie 1
          s->set_lenc(s, 1);           // 0 = disable , 1 = enable
          s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
          s->set_vflip(s, 0);          // 0 = disable , 1 = enable
          s->set_dcw(s, 1);            // 0 = disable , 1 = enable
          s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
          s->set_framesize(s, FRAMESIZE_SXGA);     // ustawiamy jeszcze raz jakość 
          
    Serial.println("Starting SD Card");
    MailClient.sdBegin(14, 2, 15, 13);

    if (!SD.begin()) {
      Serial.println("SD Card Mount Failed");
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("No SD Card attached");
    }


    // initialize EEPROM with predefined size
    EEPROM.begin(EEPROM_SIZE);
    pictureNumber = EEPROM.read(0) + 1;
    pictureNumber2 = EEPROM.read(1);

    // czekamy zeby obraz był ok
    delay(2200);
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      //return;
      go_to_sleep();
    }

    // Path where new picture will be saved in SD Card
    String path = "/IMG" + String(pictureNumber2) + "_" + String(pictureNumber) + ".jpg";
    String path2 = "IMG" + String(pictureNumber2) + "_" + String(pictureNumber) + "_" + String(wuc) + "_0.jpg"; // osobno dla FTP
    fs::FS &fs = SD;
    //fs::FS &fs = SD_MMC;
    Serial.printf("Picture file name: %s\n", path.c_str());

    File file = fs.open(path.c_str(), FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.printf("Saved file to path: %s\n", path.c_str());
      if (pictureNumber >= 255)
      {
        pictureNumber = 0;
        pictureNumber2++;
        if (pictureNumber2 > 40)
        {
          pictureNumber2 = 0;
        }
      }

    }
    EEPROM.write(0, pictureNumber);
    EEPROM.write(1, pictureNumber2);
    EEPROM.commit();
    file.close();



    while (WiFi.status() != WL_CONNECTED) {
      if (w < 20)
      {
        delay(100);
        Serial.print(".");
        w ++;
      }
      else
      {
        go_to_sleep();
      }

    }

    if ( WiFi.status() == WL_CONNECTED)
    {
      w = 0;
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("Hassio motion ON");

      if (wuc > 1)
      {
        send_trap_sw(1);
      }
      pinMode(33, OUTPUT); // czerwony led jak wysyłamy dane
      const char *f_name = path2.c_str();
      ftp.OpenConnection();
      ftp.InitFile("Type I");
      ftp.ChangeWorkDir("/files"); // change it to reflect your directory
      ftp.NewFile(f_name); //file name
      ftp.WriteData(fb->buf, fb->len);
      ftp.CloseFile();
      if (wuc > 1) // jak ruch to seria fotek
      {
        esp_camera_fb_return(fb);
        // lecimy i wysyłamy więcej zdjęć - to do przerobienia aby działąło z jednym połączeniem
        for (int i = 1; i <= 2; i++)
        {
          Serial.println("Wysyłam foto " + String(i));
          String path2 = "IMG" + String(pictureNumber2) + "_" + String(pictureNumber) + "_" + String(wuc) + "_" + String(i) + ".jpg"; // osobno dla FTP
          camera_fb_t * fb = NULL;
          delay(500);
          // Take Picture with Camera
          fb = esp_camera_fb_get();
          const char *f_name = path2.c_str();
          ftp.InitFile("Type I");
          ftp.NewFile(f_name); //file name
          ftp.WriteData(fb->buf, fb->len);
          ftp.CloseFile();

          esp_camera_fb_return(fb);
        }

      }
      ftp.CloseConnection();
    }
  }


  while (WiFi.status() != WL_CONNECTED)
  {
    if (w < 20)
    {
      delay(100);
      Serial.print(".");
      w ++;
    }
    else
    {
      break;
    }

  }
  // temp wysyłamy zawsze jak sie obudzi i jest połączenie z wifi
  if ( WiFi.status() == WL_CONNECTED)
  {
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);
    //delay(200);
    // Send Temperatura
    Serial.print("Temperatura: ");
    Serial.print(temperatureC);
    Serial.println("ºC");
    if (temperatureC < 85 and temperatureC > -130)
    {
      send_trap_temp(temperatureC);
    }
    else
    {
      sensors.requestTemperatures();
      delay(100);
      float temperatureC = sensors.getTempCByIndex(0);
      Serial.print("Temperatura: ");
      Serial.print(temperatureC);
      Serial.println("ºC");
      if (temperatureC < 85 and temperatureC > -130)
      {
        send_trap_temp(temperatureC);
      }
    }
  }


  //if (wuc > 1)
  //{
  Serial.println("Going to sleep now - FAST");
  Serial.flush();
  esp_deep_sleep_start();

}

// ## Wysyłanie pojedyńczej temp z czujnika DS18B20
void send_trap_temp (float temp)
{
  client.setServer(mqttServer, mqttPort);
  while (!client.connected()) {
Serial.println("Connecting to MQTT...");
 
if (client.connect("ESP32Client", mqttUser, mqttPassword )){
 
Serial.println("connected");
 
}else {
 
Serial.print("failed with state ");
Serial.print(client.state());
delay(2000);
 
}

  }
String temperatura;
temperatura=temp;  
client.publish("cam_trap/temp", (char*)temperatura.c_str()); //ustaw dowolny temat rozpoznowalny przez home-assitant
}
// wysylanie danych do przelacznika
void send_trap_sw (byte stan)
{
  client.setServer(mqttServer, mqttPort);
  while (!client.connected()) {
Serial.println("Connecting to MQTT...");
 
if (client.connect("ESP32Client", mqttUser, mqttPassword )){
 
Serial.println("connected");
 
}else {
 
Serial.print("failed with state ");
Serial.print(client.state());
delay(2000);
 
}

  }
    if (stan == 1)
    {
      client.publish("cam_trap/pir","ON"); //ustaw dowolny temat rozpoznowalny przez home-assitant - pir sensor
      String str = String(WiFi.RSSI());//dodatkowo przy okazji publikacji PIR, publikuje siłę syganłu
      str.toCharArray(clientRSSI, str.length()+1); //
      client.publish("cam_trap/signal", clientRSSI); //ustaw dowolny temat rozpoznowalny przez home-assitant - siła sygnału
    }
    else if (stan == 0)
    {
      client.publish("cam_trap/pir","OFF"); //ustaw dowolny temat rozpoznowalny przez home-assitant
    }  

}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 :
      Serial.println("Wakeup caused by external signal using RTC_IO");
      wuc = 2;
      break;
    case ESP_SLEEP_WAKEUP_EXT1 :
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      wuc = 2;
      break;
    case ESP_SLEEP_WAKEUP_TIMER :
      Serial.println("Wakeup caused by timer");
      wuc = 0;
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void go_to_sleep()
{
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  rtc_gpio_hold_en(GPIO_NUM_4);
  Serial.flush();
  sleep(100);
  esp_deep_sleep_start();
}

void loop() {

  
}
