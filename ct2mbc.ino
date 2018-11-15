#define SECONDS_IN_DAY   86400   // 24 * 60 * 60
#define SECONDS_IN_HOUR  3600    // 60 * 60

#include <M5Stack.h>
#include <WiFiClientSecure.h>
#include <ESP32Time.h>

const int maxcurrent = 10;  //CTクランプの最大アンペア数　【要設定】
String tenant = "z123abcd"; //MBCテナントID　【要設定】
String deviceid = "foo.bar"; //デバイスID　【要設定】
const char *ssid = "WIFISSIDHERE"; //WiFi SSID　【要設定】
const char *password = "WIFIPASSHERE"; //WiFiパスワード　【要設定】
String templete = "USER01"; //テンプレート名　【要設定】
String authkey = "abcdefghijklmnopqrstuvwxyz"; //MBC認証キー　【要設定】
const int sendlimit = 60; //１秒あたりの送信用CTクランプデータの上限数 　※最小設定可能値=1　最大設定可能値=60

const int sensorPin = 36;   //CTクランプ接続ピン
double cmp = 0.0;           //電流値演算用
const char *host = "iot-cloud.motionboard.jp"; //MotionBoardエンドポイント
String mbiot = "/motionboard/rest/tracking/data/upload/public"; //MotionBoard API
const char *boundry = "multipartboundary"; //Multipartリクエスト用境界文字

String jstring = ""; //jsonデータ生成用
int jcnt = 0;

unsigned long previousjson = 0;  //前回送信時のミリ秒格納用変数
unsigned long previousdata = 0;  //前回データ作成時のミリ秒格納用変数

unsigned long time_m1 = 0; 
unsigned long time_m2 = 0;
unsigned long time_spent = 0;
unsigned long time_m3 = 0; 
unsigned long time_m4 = 0;
unsigned long time_spent2 = 0;

//MotionBoard Cloudへの送信（json）
boolean sendMBC(String jdata) {
  //デバッグ用（1回の送信にかかる時間を計測）
  time_m1 = micros();
  
  //WiFiクライアント生成
  WiFiClientSecure client;

  if (client.connect(host, 443)) {
    time_m2 = micros();
    time_spent = time_m2 - time_m1;
    //Serial.print("result: ");
    //Serial.println(time_spent);
  
    // This will send the request to the server
    //Serial.println("Connected to Server");
    //client.setNoDelay(true);
  
    //Serial.println("request starts from here--------------------------------");

    String start_request = "";
    String end_request = "";

    //"deviceid"
    start_request = start_request + "--" + boundry + "\r\n"
                  + "content-disposition: form-data; name=\"id\"" + "\r\n"
                  + "\r\n"
                  + deviceid + "\r\n"

                  //"authkey"
                  + "--" + boundry + "\r\n"
                  + "content-disposition: form-data; name=\"authkey\"" + "\r\n"
                  + "\r\n"
                  + authkey + "\r\n"

                  //"tenant"
                  + "--" + boundry + "\r\n"
                  + "content-disposition: form-data; name=\"tenant\"" + "\r\n"
                  + "\r\n"
                  + tenant + "\r\n"

                  //"uploadFile"
                  + "--" + boundry + "\r\n"
                  + "content-disposition: form-data; name=\"uploadFile\"\r\n"
                  + "\r\n";

    end_request = end_request + "\r\n"
                  + "--" + boundry + "--" + "\r\n";
    
    int contentLength = jdata.length() + start_request.length() + end_request.length();    
    
    String headers = String("POST ") + mbiot + " HTTP/1.1\r\n"
                  + "Host: " + host + "\r\n"
                  + "User-Agent: ESP32/1.0" + "\r\n"
                  + "Accept: */*\r\n"
                  + "Content-Type: multipart/form-data; boundary=" + boundry + "\r\n"
                  + "Content-Length: " + contentLength + "\r\n"
                  + "Connection: close" + "\r\n"
                  + "\r\n"
                  + "\r\n";

    //Serial.print(headers);        
    client.print(headers);
    client.flush();    
  
    //Serial.print(start_request);
    client.print(start_request);
    client.flush();

    Serial.println(jdata);
    client.print(jdata);
    client.flush();

    //Serial.print(end_request);
    client.print(end_request);
    client.flush();

    unsigned long timer = millis();
    while (millis() - timer < 100) {
      if (!client.connected()) {
        client.stop();
      }
    }

    //Serial.println("request ends here--------------------------------");    
  } else {
    Serial.println("connection failed");
    return false;
  } 

  return true;
}

void setup() {
  //シリアルを開始する
  while(!Serial) {
    Serial.begin(115200);
  }
  
  //スピーカーをオフにする（ノイズ対策）
  dacWrite(25, 0);
  
  M5.begin();
  M5.Lcd.fillRect(0, 0, 320, 240, TFT_WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_RED);
  M5.Lcd.drawString("WIFI", 0, 0, 4);
  M5.update();

  //WiFi接続
  int cnt = 0;
  Serial.println("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (cnt > 10){
      Serial.println("giveup. restart m5stack...");
      ESP.restart();
    }
    cnt++;
  }
  Serial.println("\nConnected.");

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_GREEN);
  M5.Lcd.drawString("WIFI", 0, 0, 4);
  M5.Lcd.fillRect(0, 80, 320, 240, WHITE);
  M5.Lcd.setTextColor(TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString("0.00", 20, 80, 6);
  M5.Lcd.setTextSize(4);
  M5.Lcd.drawString("A", 220, 80, 4);
  M5.update();
  
  ESP32Time.begin(); //NTPサーバーから時間取得（１日１回）  
  String ymd = calc_ymd();
  while (ymd == "1970/1/1 0:0") {
    ESP32Time.set_time();
    delay(500);
  } 

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_BLUE);
  M5.Lcd.drawString(String(ymd), 100, 0, 4);
  M5.update();

  previousjson = millis();
}

//Jsonデータ生成（ArduinoJsonライブラリだとMBCの求めるフォーマットが生成できないため、手動生成。）
void createJson(String current, String unix_millis) {
  //json文字列を生成（jcnt=0：先頭文字列）
  if (jcnt == 0) {
    jstring = "{\"template\": \"" + templete + "\", \"status\": [{\"time\": \"" + unix_millis + "\", \"enabled\": \"true\", \"values\": [{ \"name\": \"ecurrent\", \"type\":\"3\", \"value\":" + current + "}]}";  
  } else {
    jstring = jstring + ",{\"time\": \"" + unix_millis + "\", \"enabled\": \"true\", \"values\": [{ \"name\": \"ecurrent\", \"type\":\"3\", \"value\":" + current + "}]}";
  }
}

void loop() {
  //WiFi接続チェック
  if (!checkConnection(ssid, password)){
    Serial.println("WiFi-reconnection failed. restart m5stack...");
    ESP.restart();
  }

  String ymd = calc_ymd();
  String unix_millis = calc_unix_millis();

  //time_m3 = micros();

  int val = analogRead(sensorPin); //CTクランプからセンサー生データ読み込み
  double current = maxcurrent / ( 5.0 / ( val * 0.001));   //電流値を求める

  //同一データ値だとMBに無視されてしまうので意図的に値を変更
  if (current == 0 && jcnt % 2 == 1) { 
    current = 0.01;
  } else if (current == 0 && jcnt % 2 == 0) {
    current = 0.02;
  }

  //50回溜まったら一度MBCに送る（MBCの受付上限数）
  //または前回送信時より2秒経ったら一度送る
  if (jcnt >= 50 && jstring != "") {
    jstring = jstring + "]}";
    boolean res = sendMBC(jstring);
    jstring = "";
    jcnt = 0;
    previousjson = millis();
  } else if (millis() - previousjson > 2000 && jstring != "") {
    jstring = jstring + "]}";
    boolean res = sendMBC(jstring);
    jstring = "";
    jcnt = 0;
    previousjson = millis();
  } else if (millis() - previousdata > 1000 / sendlimit) {
    previousdata = millis();
    createJson(String(current), unix_millis);
    jcnt++;
  }
  
  if (current == 0.01 || current == 0.02) {current = 0;}

  //前回送信時と同じ値だった場合、M5Stackの液晶への描画は行わない。描画するとちらつくため。
  if (current != 0) {
    M5.Lcd.fillRect(0, 80, 210, 240, WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString((String)current, 20, 80, 6);
    M5.Lcd.setTextSize(4);
    M5.Lcd.drawString("A", 220, 80, 4);
    M5.Lcd.fillRect(100, 0, 320, 40, WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.drawString(String(ymd), 100, 0, 4);
  } else if (cmp != current) {
    M5.Lcd.fillRect(0, 80, 210, 240, WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.drawString((String)current, 20, 80, 6);
    M5.Lcd.setTextSize(4);
    M5.Lcd.drawString("A", 220, 80, 4);
    M5.Lcd.fillRect(100, 0, 320, 40, WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.drawString(String(ymd), 100, 0, 4);
  }

  if (M5.BtnA.wasPressed()) {
    ESP32Time.set_time();  
  }  
  
  cmp = current;
  M5.update();
  //delay(200);

  //time_m4 = micros();
  //time_spent2 = time_m4 - time_m3;
}

//WiFi接続を定期的に確認する
boolean checkConnection(const char* ssid, const char* password) {
  if (WiFi.status() != WL_CONNECTED) {
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.drawString("WIFI", 0, 0, 4);
    M5.update();
    int count = 0;
    Serial.print("Waiting for Wi-Fi connection");
    //M5.Lcd.print("Waiting for Wi-Fi connection");
    while ( count < 5 ) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        //M5.Lcd.println();
        Serial.println("wifi re-connected!");
        //M5.Lcd.println("Connected!");
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.drawString("WIFI", 0, 0, 4);
        M5.update();
        return true;
      }
      delay(10000);
      Serial.print(".");
      //M5.Lcd.print(".");
      count++;
    }
    Serial.println("Timed out.");
    //M5.Lcd.println("Timed out.");
    return false;
  } else {
    return true;
  }
}

String calc_ymd(){
  //現在時刻をNTPサーバーから取得
  time_t t = time(NULL);
  struct tm *t_st;
  t_st = localtime(&t);
  String ymd = String(1900 + t_st->tm_year) + "/" + String(t_st->tm_mon + 1) + "/" + String(t_st->tm_mday) + " " + String(t_st->tm_hour) + ":" + String(t_st->tm_min);
  Serial.println(ymd);

  return ymd;
}

String calc_unix_millis(){
  //現在時刻をNTPサーバーから取得
  time_t t = time(NULL);
  //Serial.print("t: " + t);
  struct tm *t_st;
  t_st = localtime(&t);

  //UNIXタイムの計算
  unsigned long unix_seconds;
  //Serial.print("unixtime: ");
  int year = 1900 + t_st->tm_year;
  int month = 1 + t_st->tm_mon;
  int day = t_st->tm_mday;
  int hour = t_st->tm_hour;
  int minute = t_st->tm_min;
  int second = t_st->tm_sec;
  unix_seconds = calc_unix_seconds(year, byte(month), byte(day), byte(hour), byte(minute), byte(second));
  String unix_millis;
  if (jcnt < 10) {
    unix_millis = String(unix_seconds) + "00" + String(jcnt);
  } else if (jcnt < 100) {
    unix_millis = String(unix_seconds) + "0" + String(jcnt);
  } else {
    unix_millis = String(unix_seconds) + String(jcnt);
  }
  
  return unix_millis;
}

//unixtime計算用（うるう年）
boolean is_leapyear(int year) {
  if ((year % 400) == 0 || ((year % 4) == 0 && (year % 100) != 0)) {
    return true;
  } else {
    return false;
  }
}

//unixtime計算用
unsigned long calc_0_days(int year, byte month, byte day) {
  unsigned long days;
  int daysinmonth_ruiseki[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
   
  year--; //当年は含まず 
  days = (unsigned long)year * 365;
  days += year / 4;  //閏年の日数を足しこむ
  days -= year/100;  //閏年で無い日数を差し引く
  days += year/400;  //差し引きすぎた日数を足しこむ
  days += (unsigned long)daysinmonth_ruiseki[month-1];
  if (is_leapyear( year ) &&  3 <= month) {
    day++;
  }
  days += (unsigned long)day;
   
  return days;
}

//unixtime計算用
unsigned long calc_unix_days(int year, byte month, byte day) {
  unsigned long days; 
  return calc_0_days(year, month, day) - calc_0_days(1970, 1, 1);
}

//unixtime計算用
unsigned long calc_unix_seconds(int year, byte month, byte day, byte hour, byte minutes, byte second) {
  unsigned long days;
  unsigned long seconds;
 
  days = calc_unix_days(year, month, day);
  seconds = days * SECONDS_IN_DAY;
  seconds += (unsigned long)hour * SECONDS_IN_HOUR;
  seconds += (unsigned long)minutes * 60;
  seconds += (unsigned long)second;
  seconds += -9 * SECONDS_IN_HOUR; // JPN(GMT+9) Japan Time
 
  return seconds;
}
