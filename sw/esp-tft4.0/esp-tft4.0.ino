#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()

#include "HeFeng.h"

#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library

#include "GfxUi.h"          // Attached to this sketch
#include "SPIFFS_Support.h" // Attached to this sketch

#define TFT_GREY 0x2104 // Dark grey 16 bit colour


TFT_eSPI tft = TFT_eSPI();   // tft instance

GfxUi ui = GfxUi(&tft);


// timezone config
#define TZ              +8       // (utc+) TZ in hours
#define DST_MN          0      // use 60mn for summer time in some countries

// Setup 
// weather update
const int UPDATE_INTERVAL_SECS = 20 * 60; // Update every 20 minutes  online weather

const String WDAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const String MONTH_NAMES[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

HeFengCurrentData currentWeather;
HeFengForeData foreWeather[3];
HeFeng HeFengClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

// hefeng weather config
const char* HEFENG_KEY="xxxxxxxxxxxxxxxxxxxxx";
const char* HEFENG_LOCATION="101010100";

time_t now;

bool WeatherNowUpdate = false;
bool WeatherForecastUpdate = false;

long timeSinceLastWUpdate = 0;
long timeSinceLastCurrUpdate = 0;

unsigned long targetTime = 0;


ESP8266WebServer server(80);
String HTML_TITLE = "<!DOCTYPE html><html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\"><title>ESP8266网页配网</title>";
String HTML_SCRIPT_ONE = "<script type=\"text/javascript\">function wifi(){var ssid = s.value;var password = p.value;var xmlhttp=new XMLHttpRequest();xmlhttp.open(\"GET\",\"/HandleWifi?ssid=\"+ssid+\"&password=\"+password,true);xmlhttp.send();xmlhttp.onload = function(e){alert(this.responseText);}}</script>";
String HTML_SCRIPT_TWO = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
String HTML_HEAD_BODY_BEGIN = "</head><body>请输入wifi信息进行配网:";
String HTML_FORM_ONE = "<form>WiFi名称：<input id='s' name='s' type=\"text\" placeholder=\"请输入您WiFi的名称\"><br>WiFi密码：<input id='p' name='p' type=\"text\" placeholder=\"请输入您WiFi的密码\"><br><input type=\"button\" value=\"扫描\" onclick=\"window.location.href = '/HandleScanWifi'\"><input type=\"button\" value=\"连接\" onclick=\"wifi()\"></form>";
String HTML_BODY_HTML_END = "</body></html>";


void handleRoot() {
    Serial.println("root page");
    String str = HTML_TITLE + HTML_SCRIPT_ONE + HTML_SCRIPT_TWO + HTML_HEAD_BODY_BEGIN + HTML_FORM_ONE + HTML_BODY_HTML_END;
    server.send(200, "text/html", str);
}


void HandleScanWifi() {
    Serial.println("scan start");

    String HTML_FORM_TABLE_BEGIN = "<table><head><tr><th>序号</th><th>名称</th><th>强度</th></tr></head><body>";
    String HTML_FORM_TABLE_END = "</body></table>";
    String HTML_FORM_TABLE_CON = "";
    String HTML_TABLE;
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
        HTML_TABLE = "NO WIFI !!!";
    }
    else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
            delay(10);
            HTML_FORM_TABLE_CON = HTML_FORM_TABLE_CON + "<tr><td align=\"center\">" + String(i+1) + "</td><td align=\"center\">" + "<a href='#p' onclick='c(this)'>" + WiFi.SSID(i) + "</a>" + "</td><td align=\"center\">" + WiFi.RSSI(i) + "</td></tr>";
        }

        HTML_TABLE = HTML_FORM_TABLE_BEGIN + HTML_FORM_TABLE_CON + HTML_FORM_TABLE_END;
    }
    Serial.println("");

    String scanstr = HTML_TITLE + HTML_SCRIPT_ONE + HTML_SCRIPT_TWO + HTML_HEAD_BODY_BEGIN + HTML_FORM_ONE + HTML_TABLE + HTML_BODY_HTML_END;
    
    server.send(200, "text/html", scanstr);
}


void HandleWifi() {
    String wifis = server.arg("ssid"); //从JavaScript发送的数据中找ssid的值
    String wifip = server.arg("password"); //从JavaScript发送的数据中找password的值
    Serial.println("received:"+wifis);
    server.send(200, "text/html", "连接中..");
    WiFi.begin(wifis,wifip);
}


void handleNotFound() { 
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}


bool autoConfig() {
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    Serial.print("AutoConfig Waiting......");
                tft.fillScreen(TFT_BLACK);
            tft.drawCentreString("Connecting to WiFi", 240, 100, 4);
    int counter = 0;
    for (int i = 0; i < 20; i++)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("AutoConfig Success");
            Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
            Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
            WiFi.printDiag(Serial);
            return true;
        }
        else
        {
            delay(500);
            Serial.print(".");
            for (int k=1;  k<101; k++){
                progressbar(k, 200);
            }
        }
    }
    Serial.println("AutoConfig Faild!" );
    return false;
}


void htmlConfig() {
    WiFi.mode(WIFI_AP_STA);//设置模式为AP+STA
    WiFi.softAP("wifi_clock");

    IPAddress myIP = WiFi.softAPIP();
  
    if (MDNS.begin("clock")) {
        Serial.println("MDNS responder started");
    }
    
    server.on("/", handleRoot);
    server.on("/HandleWifi", HTTP_GET, HandleWifi);
    server.on("/HandleScanWifi", HandleScanWifi);
    server.onNotFound(handleNotFound);//请求失败回调函数
    MDNS.addService("http", "tcp", 80);
    server.begin();//开启服务器
    Serial.println("HTTP server started");
    Serial.println("HTTP server started");
    Serial.println("HTTP server started");

    tft.fillScreen(TFT_BLACK);  
    
    int counter = 0;
    while(1)
    {
        server.handleClient();
        MDNS.update();  
        delay(500);

        //tft.fillScreen(TFT_BLACK);  
        tft.drawCentreString("WIFI AP :   wifi_clock", 240, 100, 4);
        tft.drawCentreString("192.168.4.1", 240, 130, 4);
        tft.drawCentreString("waiting for config wifi", 240, 160, 4);
        
        for (int k=1; k<101; k++){
            progressbar(k, 200);
        }
        
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("HtmlConfig Success");
            Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
            Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
            Serial.println("HTML连接成功");
            break;
        }
    }
       server.close();  
       WiFi.mode(WIFI_STA);
}


void progressbar(int value, int y) {
    int v = map(value, 0, 100, 80, 400); // Map the value to an angle v

    int colour = TFT_GREEN;
 
  // Draw colour blocks every inc degrees
    for (int i = 80; i < 400; i += 8) {
        if (i < v) {
            tft.fillRect(i, y, 8, 40, TFT_GREEN);
        }
        else
        {
           tft.fillRect(i, y, 8, 40, TFT_GREY);
        }
    }
}


void updateData() {
    tft.setTextColor(TFT_WHITE);

    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Updating weather...", 240, 100, 4);
    progressbar(1, 200);
    
    for(int i=0;i<5;i++){
        HeFengClient.doUpdateCurr(&currentWeather, HEFENG_KEY, HEFENG_LOCATION);
        if(currentWeather.cond_txt!="no network"){
            WeatherNowUpdate = true;
            break;
        }
    }

    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Updating forecasts...", 240, 100, 4);
    progressbar(51, 200);
  
    for(int i=0;i<5;i++){
        HeFengClient.doUpdateFore(foreWeather, HEFENG_KEY, HEFENG_LOCATION);
        if(foreWeather[0].datestr!="N/A"){
            WeatherForecastUpdate = true;
            break;
       }
    }
 
    tft.fillScreen(TFT_BLACK);
    progressbar(100, 200);

    tft.fillScreen(TFT_BLACK);

    if (WeatherNowUpdate) {
        tft.drawCentreString("WeatherNow Update OK...", 240, 120, 4);
    } else {
        tft.drawCentreString("WeatherNow Update FALE...!!!", 240, 120, 4);
    }

    if (WeatherForecastUpdate) {
        tft.drawCentreString("WeatherForecast Update OK...", 240, 180, 4);
    } else {
        tft.drawCentreString("WeatherForecast Update FALE...!!!", 240, 180, 4);
    }
   
    delay(1000);
}


void drawtempunit(int x, int y) {

    tft.drawString("o", x, y, 2);

    tft.drawString("C", x+10, y+5, 4);
}


void drawCurrentWeather() {

    ui.drawJpeg("/icon/color-128/" + currentWeather.cond_code + ".jpeg", 5, 25);

    tft.setTextColor(TFT_YELLOW);
    
    tft.drawRightString(currentWeather.tmp, 245, 10, 8);

    drawtempunit(250, 10);

    tft.setTextColor(TFT_WHITE);
    
    tft.drawString(currentWeather.cond_txt, 145, 90, 4);
    
    tft.drawString("Wind: " + currentWeather.wind_dir + " / " + currentWeather.wind_sc, 145, 115, 4);

    tft.drawString(currentWeather.fl+"C / "+currentWeather.hum+"%", 145, 140, 4);
}


void drawForecast() {
    drawForecastDetails(0, 180, 0);
    drawForecastDetails(160, 180, 1);
    drawForecastDetails(320, 180, 2);
}


void drawForecastDetails(int x, int y, int dayIndex) {
    tft.setTextColor(TFT_WHITE);

    tft.drawCentreString(foreWeather[dayIndex].datestr, x + 80, y, 4);

    ui.drawJpeg("/icon/color-64/" + foreWeather[dayIndex].cond_code_d + ".jpeg", x + 16, y + 25);
    ui.drawJpeg("/icon/color-64/" + foreWeather[dayIndex].cond_code_n + ".jpeg", x + 80, y + 25);
    
    String temp=foreWeather[dayIndex].tmp_min + "C | "+foreWeather[dayIndex].tmp_max +"C";
    tft.drawCentreString(temp, x + 80, y + 105, 4);
}


void drawtime() {
    now = time(nullptr);
    struct tm* timeInfo;
    timeInfo = localtime(&now);
    char datebuff[16];
    char timebuff[16];
    String date = WDAY_NAMES[timeInfo->tm_wday];
    sprintf_P(datebuff, PSTR("%04d-%02d-%02d %s"), timeInfo->tm_year + 1900, timeInfo->tm_mon+1, timeInfo->tm_mday);
   // sprintf_P(timebuff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
   sprintf_P(timebuff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
   
    tft.setTextColor(TFT_WHITE);
    
    tft.drawCentreString(String(datebuff), 390, 130, 4);
    
    tft.drawCentreString(WDAY_NAMES[timeInfo->tm_wday].c_str(), 390, 90, 4);
    
    tft.setTextColor(TFT_YELLOW);

    tft.drawCentreString(String(timebuff), 380, 20, 7);
}



void setup() {

    Serial.begin(115200);
    Serial.println();

    pinMode(button_wifi, INPUT); // 按钮输入
  
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    SPIFFS.begin();
    listFiles();

    targetTime = millis() + 1000;

    bool wifiConfig = autoConfig();
    if(wifiConfig == false) {
        htmlConfig();//HTML配网
    }

    configTime(TZ_SEC, DST_SEC, "pool.ntp.org","0.cn.pool.ntp.org","1.cn.pool.ntp.org");
    
    // update weather data
    updateData();   

}







void loop() {

    // 20 minutes update
    if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS)) {
        updateData();
        timeSinceLastWUpdate = millis();
    }

    if (targetTime < millis()) {
        targetTime = millis() + 10000;

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE); // Background is not defined so it is transparent

        // display data and time
        drawtime();

        // display current weather
        drawCurrentWeather();

        // DISPLAY 3 DAYS WEATHER
        drawForecast();
    }


}
