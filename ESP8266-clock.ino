#define BLINKER_WIFI
#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args)  write(args);
#else
#define printByte(args)  print(args,BYTE);
#endif
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#include <EEPROM.h>
#include <DallasTemperature.h>
#include <MD_DS3231.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <smartconfig.h>
#include <EasyNTPClient.h>
#include <WiFiUdp.h>
#include <BH1750FVI.h>
//#include <Blinker.h>

//#include <BlinkerWidgets.h>



MD_DS3231 rtc;                              //ds3231 (i2c)
LiquidCrystal_I2C lcd(0x27, 20, 4);        //lcd2004 (i2c)
WiFiUDP udp;
EasyNTPClient ntpClient(udp, "cn.ntp.org.cn", 8 * 3600); //ntp

uint8_t ADDRESSPIN = D0;
BH1750FVI::eDeviceAddress_t DEVICEADDRESS = BH1750FVI::k_DevAddress_H;
BH1750FVI::eDeviceMode_t DEVICEMODE = BH1750FVI::k_DevModeContHighRes2;     //BH1750光照(i2c)
BH1750FVI LightSensor(ADDRESSPIN, DEVICEADDRESS, DEVICEMODE);

OneWire oneWire(D3);
DallasTemperature sensors(&oneWire);                            //ds18b20 (1wire,D3)



const char *ssid     = "bu~zhun~ceng~wang(2.4G)";
const char *password = "zlq13834653953";
uint8_t tempC[8] = {0x80, 0x70, 0x88, 0x80, 0x80, 0x88, 0x70, 0x00};

int message = 1;

int prevms = 3000;
bool calibrated = false;

template <class T>
int getArrayLen(T& array) {
  return (sizeof(array) / sizeof(array[0]));
};

void serialPrintlnArray(int array_[],int size_) {
  Serial.print("{");
  unsigned int i;
  for (i = 0; i <size_ ; i++ ) {
    delay(0);
    Serial.print(String(array_[i]));
    delay(0);
    Serial.print(",");
  };
  Serial.println("}");
};

void updateConfigStatus(sc_status status_, void *pdata) {      //esptouch状态显示
  switch (status_) {
    case SC_STATUS_WAIT:
      {
        lcd.setCursor(0, 1);
        lcd.print("                    ");
        lcd.setCursor(0, 1);
        lcd.print("Waiting connection");
      }
    case  SC_STATUS_FIND_CHANNEL:
      {
        lcd.setCursor(0, 1);
        lcd.print("                    ");
        lcd.setCursor(0, 1);
        lcd.print("Finding channel");
      }
    case   SC_STATUS_GETTING_SSID_PSWD :
      {
        lcd.setCursor(0, 1);
        lcd.print("                    ");
        lcd.setCursor(0, 1);
        lcd.print("Receiving data") ;
      }
  };
};

bool autoBackLight = true;
float historyData [2][20]; //={30,30};

short* calculateTend(float Temp[]) {
  short Tend [2];
  uint8_t i = 0;
  uint8_t j = 0;
  float historyAverage[2];

  if (historyData[0][0] == 0) {        //没有数据
    for (i = 0; i <= 1; i++) {
      for (j = 0; j <= 19; j++) {
        historyData[i][j] = Temp[i];   //填满这个数组
      }
    };
  };

  for (i = 0; i <= 1; i++) {         //计算平均数
    float sum=0;
    for (j = 0; j <= 19; j++) {
      sum += historyData[i][j];
    }
    historyAverage[i] = sum / 20;
  };
  Serial.println("average:"+String(historyAverage[0])+" "+String(historyAverage[1]));


  for (i = 0; i <= 1; i++) {
    if ((historyAverage[i] - Temp[i]) <= -0.3) {  //温度上升
      Tend[i] = 1;
    };
    if ((historyAverage[i] - Temp[i]) >= 0.3) {  //温度下降
      Tend[i] = -1;
    };
    if (abs(historyAverage[i] - Temp[i]) < 0.3) {  //温度基本不变
      Tend[i] = 3;
    };
    for (j = 0; j <= 18; j++) {
      historyData[i][j] = historyData[i][j + 1];
    };
    historyData[i][19] = Temp[i];
  };

  return Tend;
};

void plotGraph(float data_[2][20],int index) {
  uint8_t i;
  float data[20];
  for(i=0;i<=19;i++){
    data[i]=data_[index][i];
  };
  
  int dataInt10x[20];            //x10再整数化
  for (i = 0; i <= 19; i++) {
    dataInt10x[i] = (int)(round(data[i] * 10));
  };

  int max10x = -2742;
  int min10x = 2550;
  for (i = 0; i <= 19; i++) {  //找到最大,最小的元素
    if (dataInt10x[i] > max10x) {
      max10x = dataInt10x[i];
    };
    if (dataInt10x[i] < min10x) {
      min10x = dataInt10x[i];
    };
  };
  serialPrintlnArray(dataInt10x,20);
  Serial.println("arraysize:"+String(ARRAY_SIZE(dataInt10x)));
  


};


uint8_t elapsedSec = 55;
short tend[2];
void refreshDisplay() {
  uint16_t light = LightSensor.GetLightIntensity();
  float Temp [2] = {rtc.readTempRegister(), sensors.getTempCByIndex(0) - 2.55}; //貌似有误差..?

  if ( elapsedSec == 60) {       //60秒计算一下趋势
    elapsedSec = 0;
    short* Tend;
    //Tend = calculateTend(Temp);
    memcpy(tend, calculateTend(Temp), sizeof(tend));       //不知道怎么写，反正这个能用
    
  };
  if(getKey()==4){
    plotGraph(historyData,1);
  };
  elapsedSec++;
  //最大程度避免刷新闪屏
  String temp_;
  temp_ = String(rtc.yyyy) + "/" + String(rtc.mm) + "/" + String(rtc.dd);
  lcd.setCursor(0, 0);
  lcd.print("                    "); //20个空格
  lcd.setCursor(0, 0);
  lcd.print(temp_);

  temp_ = String(rtc.h) + ":" + String(rtc.m) + ":" + String(rtc.s);
  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(0, 1);
  lcd.print(temp_);

  if (rtc.pm == 0) {
    temp_ = "AM";
  } else {
    temp_ = "PM";
  };
  lcd.setCursor(9, 1);
  lcd.print(temp_);

  temp_ = "In:" + String(Temp[0]) + "C";
  lcd.setCursor(0, 2);
  lcd.print("                    ");
  lcd.setCursor(0, 2);
  lcd.print(temp_);
  switch (tend[0]) {
    case -1: {
        lcd.print(" -");
        break;
      }
    case 1: {
        lcd.print(" +");
        break;
      }
  };

  lcd.setCursor(0, 3);
  lcd.print("                    ");
  lcd.setCursor(0, 3);
  switch (message)
  {
    case 1: //正常显示外部温度
      {
        lcd.print("out:" + String(Temp[1]) + "C");
        switch (tend[1]) {
          case -1: {
              lcd.print(" -");
              break;
            }
          case 1: {
              lcd.print(" +");
              break;
            }
        };
        break;
      }
    case 2: //连接wifi中
      {
        lcd.print("--Wifi connecting--") ;
        break;
      }
    case 3: //时间获取失败
      {
        lcd.print("--Time set failed--");
        ESP.restart();
        break;
      };

  }

  if (light < 7 && autoBackLight) {
    lcd.noBacklight();
  } else {
    lcd.backlight();
  };

};


void autoSetTime() {
  rtc.control(DS3231_12H, DS3231_ON);

  unsigned long Time = ntpClient.getUnixTime();

  //暴力把unix时间转换为年月日...
  unsigned long Y2KTime = (Time - 946684800) / 86400;//从2000年开始的天数
  unsigned long YTime;                              //从今年开始的天数
  unsigned int Year;
  unsigned int Month = 0;
  unsigned long Day;
  //日期

  if (Y2KTime % 146097 <= 36525)
  {
    Year = 2000 + Y2KTime / 146097 * 400 + Y2KTime % 146097 / 1461 * 4 + (Y2KTime % 146097 % 1461 - 1) / 365;
    YTime = (Y2KTime % 146097 % 1461 - 1) % 365 + 1;
  }
  else
  {
    Year = 2000 + Y2KTime / 146097 * 400 + (Y2KTime % 146097 - 1) / 36524 * 100 + ((Y2KTime % 146097 - 1) % 36524 + 1) / 1461 * 4 + (((Y2KTime % 146097 - 1) % 36524 + 1) % 1461 - 1) / 365;
    YTime = (((Y2KTime % 146097 - 1) % 36524 + 1) % 1461 - 1) % 365 + 1;
  }
  Day = YTime;
  unsigned char f = 1; //循环标志

  while (f)
  {
    switch (Month)
    {
      case 0:
        if (Day < 31)
          f = 0;
        else
          Day -= 31;
        break;
      case 1:
        if (Day < 29)
          f = 0;
        else
        {
          if (LY(Year))
          {
            Day -= 29;
          }
          else
          {
            Day -= 28;
          }
        }
        break;
      case 2:
        if (Day < 31)
          f = 0;
        else
          Day -= 31;
        break;
      case 3:
        if (Day < 30)
          f = 0;
        else
          Day -= 30;
        break;
      case 4:
        if (Day < 31)
          f = 0;
        else
          Day -= 31;
        break;
      case 5:
        if (Day < 30)
          f = 0;
        else
          Day -= 30;
        break;
      case 6:
        if (Day < 31)
          f = 0;
        else
          Day -= 31;
        break;
      case 7:
        if (Day < 31)
          f = 0;
        else
          Day -= 31;
        break;
      case 8:
        if (Day < 30)
          f = 0;
        else
          Day -= 30;
        break;
      case 9:
        if (Day < 31)
          f = 0;
        else
          Day -= 31;
        break;
      case 10:
        if (Day < 30)
          f = 0;
        else
          Day -= 30;
        break;
      case 11:
        if (Day < 31)
          f = 0;
        else
          Day -= 31;
        break;
    }
    Month += 1;
  }
  Day += 1;
  if (Year >= 2030) { //获取错误!
    message = 3;
    goto end_;
  };
  rtc.yyyy = Year;
  rtc.mm = Month;
  rtc.dd = Day;


  //星期
  /*
    switch (Y2KTime % 7) //2000年1月1日是星期六
    {
    case 0: display.drawBitmap(112, 48, weekData6, 16, 16, WHITE); break;
    case 1: display.drawBitmap(112, 48, weekData7, 16, 16, WHITE); break;
    case 2: display.drawBitmap(112, 48, weekData1, 16, 16, WHITE); break;
    case 3: display.drawBitmap(112, 48, weekData2, 16, 16, WHITE); break;
    case 4: display.drawBitmap(112, 48, weekData3, 16, 16, WHITE); break;
    case 5: display.drawBitmap(112, 48, weekData4, 16, 16, WHITE); break;
    case 6: display.drawBitmap(112, 48, weekData5, 16, 16, WHITE); break;
    }*/


  if (((Time  % 86400L) / 3600) > 12) {
    rtc.h = ((Time  % 86400L) / 3600);
    rtc.pm = 1;
  } else {
    rtc.h = ((Time  % 86400L) / 3600);
    rtc.pm = 0;
  };
  Serial.println(((Time  % 86400L) / 3600));
  //if ( ((Time % 3600) / 60) < 10 ) {
  // // In the first 10 minutes of each hour, we'll want a leading '0'
  //  Serial.print('0');
  // display.print('0');
  //}

  // Serial.print((Time  % 3600) / 60); // print the minute (3600 equals secs per minute)
  //display.print((Time  % 3600) / 60);
  rtc.m = (Time  % 3600) / 60;
  // Serial.print(':');
  /// if ( (Time % 60) < 10 ) {
  //   // In the first 10 seconds of each minute, we'll want a leading '0'
  //   Serial.print('0');
  //}
  //Serial.println(Time % 60); // print the second
  rtc.s = Time % 60;
  rtc.writeTime();
end_: ;
};

unsigned char LY(unsigned int y)//判断是否为闰年
{
  if (y % 400 == 0)
    return 1;
  if (y % 100 == 0)
    return 0;
  if (y % 4 == 0)
    return 1;
}

/*
  void switchAutoBackLight(const String & state)
  {
    BLINKER_LOG2("get switch state: ", state);

    if (state == BLINKER_CMD_ON) {
        autoBackLight = true;
        BUILTIN_SWITCH.print("on");
    }
    else {
        autoBackLight = false;
        BUILTIN_SWITCH.print("off");
    }
  };
*/

#define key4 D5
#define key3 D6
#define key2 D7
#define key1 D8

unsigned short getKey() {
  if (digitalRead(key1) == HIGH) {
    return 1;
  };
  if (digitalRead(key2) == HIGH) {
    return 2;
  };
  if (digitalRead(key3) == HIGH) {
    return 3;
  };
  if (digitalRead(key4) == HIGH) {
    return 4;
  };
  return 0;

};
void setup() {
  Serial.begin(115200);
  lcd.init();                      // 初始化...
  LightSensor.begin();
  Wire.begin();
  sensors.begin();
  lcd.backlight();
  pinMode(key1, INPUT);             //界面切换的四个按键，高电平=按下
  pinMode(key2, INPUT);
  pinMode(key3, INPUT);
  pinMode(key4, INPUT);

  uint8_t L8[8] = {0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f};    //8段竖条，反正内存够~
  uint8_t L7[8] = {0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f};
  uint8_t L6[8] = {0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f};
  uint8_t L5[8] = {0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f};
  uint8_t L4[8] = {0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f};
  uint8_t L3[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f};
  uint8_t L2[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f};
  uint8_t L1[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f};
  lcd.createChar(8, L8);
  lcd.createChar(7, L7);
  lcd.createChar(6, L6);
  lcd.createChar(5, L5);
  lcd.createChar(4, L4);
  lcd.createChar(3, L3);
  lcd.createChar(2, L2);
  lcd.createChar(1, L1);


  EEPROM.begin(32); //32byte eeprom,保存esptouch之后的wifi

  if (EEPROM.read(0) != 1) {      //第一次启动
    lcd.setCursor(0, 0);
    lcd.print("DS3231 clock");
    delay(500);
    WiFi.mode(WIFI_STA);
    //smartconfig_start(updateConfigStatus, 1);     //不知为何不能用
    WiFi.beginSmartConfig();
    while (1) {
      delay(500);
      lcd.setCursor(0, 0);
      lcd.print("Smartconfig...");
      if (WiFi.smartConfigDone()) {
        lcd.setCursor(0, 1);
        lcd.print("Attempt to connect");
        unsigned short timeout = 0;
        while (WiFi.status() != WL_CONNECTED) {    //尝试连接
          delay(100);
          ++timeout;
          if (timeout > 1200) {
            lcd.setCursor(0, 2);
            lcd.print("Smartconfig failed");
            ESP.restart();
          };
        };
        lcd.setCursor(0, 2);
        lcd.print("Smartconfig success");
        EEPROM.write(0, 1);               //初始化过了
        EEPROM.commit();
        break;
      };
    };
  };
};

void loop() {
  // Blinker.run();

  if (millis() - prevms > 960) { //refresh every second
    prevms = millis();
    message = 1;
    rtc.readTime();

    if (WiFi.status() != WL_CONNECTED) {
      message = 2;
    };

    if (WiFi.status() == WL_CONNECTED && calibrated == false) {
      autoSetTime();
      calibrated = true;
    };

    refreshDisplay();
    sensors.requestTemperatures();

  }
};
