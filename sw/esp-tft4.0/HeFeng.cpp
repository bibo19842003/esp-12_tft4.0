#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>  
#include <WiFiClientSecureBearSSL.h>  
#include "HeFeng.h"


HeFeng::HeFeng() {

}


void HeFeng::doUpdateCurr(HeFengCurrentData *data, String key,String location) {

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);    
    client->setInsecure();    
    HTTPClient https;  
    String url="https://free-api.heweather.net/s6/weather/now?lang=en&location="+location+"&key="+key;
    Serial.print("[HTTPS] begin...\n");

    if (https.begin(*client, url)) {  // HTTPS    
        // start connection and send HTTP header  
        int httpCode = https.GET();    
        // httpCode will be negative on error  
        if (httpCode > 0) {  
        // HTTP header has been send and Server response header has been handled  
            Serial.printf("[HTTPS] GET... code: %d\n", httpCode);    
     
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {  
                String payload = https.getString();  
                Serial.println(payload);  
                DynamicJsonDocument  jsonBuffer(2048);
                deserializeJson(jsonBuffer, payload);
                JsonObject root = jsonBuffer.as<JsonObject>();
          
                String tmp=root["HeWeather6"][0]["now"]["tmp"];         
                data->tmp=tmp;             
                String fl=root["HeWeather6"][0]["now"]["fl"];         
                data->fl=fl;
                String hum=root["HeWeather6"][0]["now"]["hum"];         
                data->hum=hum;
                String wind_sc=root["HeWeather6"][0]["now"]["wind_sc"];         
                data->wind_sc=wind_sc;
                String wind_dir=root["HeWeather6"][0]["now"]["wind_dir"];         
                data->wind_dir=wind_dir;
                String cond_code=root["HeWeather6"][0]["now"]["cond_code"]; 
                data->cond_code=cond_code;
                String cond_txt=root["HeWeather6"][0]["now"]["cond_txt"]; 
                data->cond_txt=cond_txt;
            }

        } else {  
            Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
            data->tmp="-1";                 
            data->fl="-1";       
            data->hum="-1";      
            data->wind_sc="-1";
            data->wind_dir="-1";            
            data->cond_txt="no network";
            data->cond_code="999";
      }  
  
      https.end();

    } else {  
        Serial.printf("[HTTPS] Unable to connect\n");
        data->tmp="-1";                 
        data->fl="-1";       
        data->hum="-1";      
        data->wind_sc="-1";     
        data->wind_dir="-1";      
        data->cond_txt="no network";
        data->cond_code="999";
    }  

}


void HeFeng::doUpdateFore(HeFengForeData *data, String key,String location) {

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);    
    client->setInsecure();    
    HTTPClient https;  
    String url="https://free-api.heweather.net/s6/weather/forecast?lang=en&location="+location+"&key="+key;
    Serial.print("[HTTPS] begin...\n");

    if (https.begin(*client, url)) {  // HTTPS    
        // start connection and send HTTP header  
        int httpCode = https.GET();    
        // httpCode will be negative on error  
        if (httpCode > 0) {  
            // HTTP header has been send and Server response header has been handled  
            Serial.printf("[HTTPS] GET... code: %d\n", httpCode);    
     
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {  
                String payload = https.getString();  
                Serial.println(payload);  
                DynamicJsonDocument  jsonBuffer(2048);
                deserializeJson(jsonBuffer, payload);
                JsonObject root = jsonBuffer.as<JsonObject>();
                int i;
                for (i=0; i<3; i++){
                    String tmp_min=root["HeWeather6"][0]["daily_forecast"][i]["tmp_min"];
                    data[i].tmp_min=tmp_min;
                    String tmp_max=root["HeWeather6"][0]["daily_forecast"][i]["tmp_max"];         
                    data[i].tmp_max=tmp_max;  
                    String datestr=root["HeWeather6"][0]["daily_forecast"][i]["date"];         
                    data[i].datestr=datestr.substring(5,datestr.length()); 
                    String cond_code_d=root["HeWeather6"][0]["daily_forecast"][i]["cond_code_d"];         
                    data[i].cond_code_d=cond_code_d;
                    String cond_code_n=root["HeWeather6"][0]["daily_forecast"][i]["cond_code_n"];         
                    data[i].cond_code_n=cond_code_n;
                }
            }

        } else {  
            Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());  
            int i;
            for (i=0; i<3; i++){               
                data[i].tmp_min="-1";          
                data[i].tmp_max="-1";  
                data[i].cond_code_d="999";          
                data[i].cond_code_n="999";    
                data[i].datestr="N/A";
            }
        }  
        https.end();

    } else {  
        Serial.printf("[HTTPS] Unable to connect\n");  
        int i;
        for (i=0; i<3; i++){               
            data[i].tmp_min="-1";          
            data[i].tmp_max="-1";
            data[i].cond_code_d="999";          
            data[i].cond_code_n="999";   
            data[i].datestr="N/A";
        }
    }  

}
