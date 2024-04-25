#include <ESP8266WiFi.h>
#include <PubSubClient.h>
const char* ssid = "";
const char* password =  "";
const char* mqtt_server = "mqtt.eclipse.org";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

#include "LowPower.h"
#include "EEPROMex.h"
#include "LCD_1602_RUS.h"
#define button 2         // кнопка "проснуться"
#define calibr_button 3  // скрытая кнопка калибровкии сброса
#define disp_power 12    // питание дисплея
#define LEDpin 11        // питание светодиода
#define IRpin 17         // питание фототранзистора
#define IRsens 14        // сигнал фототранзистора
#define coin_amount 5
float coin_value[coin_amount] = {0.5, 1.0, 2.0, 5.0, 10.0};  // стоимость монет
String currency = "RUB"; // валюта (английские буквы!!!)
int stb_time = 10000;    // время бездействия, через которое система уйдёт в сон (миллисекунды)
int coin_signal[coin_amount];    // тут хранится значение сигнала для каждого размера монет
int coin_quantity[coin_amount];  // количество монет
byte empty_signal;               // храним уровень пустого сигнала
unsigned long standby_timer, reset_timer; // таймеры
float summ_money = 0;            // сумма монет в копилке
int sens_signal, last_sens_signal;
boolean coin_flag = false;
boolean recogn_flag, sleep_flag = true;   // флажки


LCD_1602_RUS lcd(0x27, 16, 2);
void setup() {
   Serial.begin(9600);                   // открыть порт для связи с ПК для отладки
  delay(500);

  // подтягиваем кнопки
  pinMode(button, INPUT_PULLUP);
  pinMode(calibr_button, INPUT_PULLUP);

  // пины питания как выходы
  pinMode(disp_power, OUTPUT);
  pinMode(LEDpin, OUTPUT);
  pinMode(IRpin, OUTPUT);

  // подать питание на дисплей и датчик
  digitalWrite(disp_power, 1);
  digitalWrite(LEDpin, 1);
  digitalWrite(IRpin, 1);

  // подключить прерывание
  attachInterrupt(0, wake_up, CHANGE);

  empty_signal = analogRead(IRsens);  // считать пустой (опорный) сигнал

  // инициализация дисплея
  lcd.init();
  lcd.backlight();

  if (!digitalRead(calibr_button)) {  // если при запуске нажата кнопка калибровки
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print(L"Сервис");
    delay(500);
    reset_timer = millis();
    while (1) {                                   // бесконечный цикл
      if (millis() - reset_timer > 3000) {        // если кнопка всё ещё удерживается и прошло 3 секунды
        // очистить количество монет
        for (byte i = 0; i < coin_amount; i++) {
          coin_quantity[i] = 0;
          EEPROM.writeInt(20 + i * 2, 0);
        }
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(L"Память очищена");
        delay(100);
      }
      if (digitalRead(calibr_button)) {   // если отпустили кнопку, перейти к калибровке
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(L"Калибровка");
        break;
      }
    }
    while (1) {
      for (byte i = 0; i < coin_amount; i++) {
        lcd.setCursor(0, 1); lcd.print(coin_value[i]);  // отобразить цену монеты, размер которой калибруется
        lcd.setCursor(13, 1); lcd.print(currency);      // отобразить валюту
        last_sens_signal = empty_signal;
        while (1) {
          sens_signal = analogRead(IRsens);                                    // считать датчик
          if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;  // если текущее значение больше предыдущего
          if (sens_signal - empty_signal > 3) coin_flag = true;                // если значение упало почти до "пустого", считать что монета улетела
          if (coin_flag && (abs(sens_signal - empty_signal)) < 2) {            // если монета точно улетела
            coin_signal[i] = last_sens_signal;                                 // записать максимальное значение в память
            EEPROM.writeInt(i * 2, coin_signal[i]);
            coin_flag = false;
            break;
          }
        }
      }
      break;
    }
  }

  // при старте системы считать из памяти сигналы монет для дальнейшей работы, а также их количество в банке
  for (byte i = 0; i < coin_amount; i++) {
    coin_signal[i] = EEPROM.readInt(i * 2);
    coin_quantity[i] = EEPROM.readInt(20 + i * 2);
    summ_money += coin_quantity[i] * coin_value[i];  // сумму посчитать, как произведение цены монеты на количество
  }

   WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.print("Connected to WiFi :");
  Serial.println(WiFi.SSID());
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(MQTTcallback);
  while (!client.connected()) 
  {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP8266"))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
  client.subscribe("esp/test");

  standby_timer = millis();  // обнулить таймер ухода в сон
  
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) 
{
  
}


void loop() {
  if (sleep_flag) {  // если после сна
    delay(500);
    lcd.init();
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(L"Копилка");
    lcd.setCursor(0, 1); lcd.print(summ_money);
    lcd.setCursor(13, 1); lcd.print(currency);
    empty_signal = analogRead(IRsens);
    sleep_flag = false;
  }

  // далее работаем в бесконечном цикле
  last_sens_signal = empty_signal;
  while (1) {
    if (summ_money >= 2500) //Если сумма накоплена
    {
      lcd.setCursor(11, 0); lcd.print(L"накоплено");
      lcd.setCursor(11, 0); lcd.print(L"");
    }
    sens_signal = analogRead(IRsens);  // далее такой же алгоритм, как при калибровке
    if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;
    if (sens_signal - empty_signal > 3) coin_flag = true;
    if (coin_flag && (abs(sens_signal - empty_signal)) < 2) {
      recogn_flag = false;  // флажок ошибки, пока что не используется
      // в общем нашли максимум для пролетевшей монетки, записали в last_sens_signal
      // далее начинаем сравнивать со значениями для монет, хранящимися в памяти
      for (byte i = 0; i < coin_amount; i++) {
        int delta = abs(last_sens_signal - coin_signal[i]);   // вот самое главное! ищем АБСОЛЮТНОЕ (то бишь по модулю)
        // значение разности полученного сигнала с нашими значениями из памяти
        if (delta < 30) {   // и вот тут если эта разность попадает в диапазон, то считаем монетку распознанной
          summ_money += coin_value[i];  // к сумме прибавляем цену монетки
          lcd.setCursor(0, 1); lcd.print(summ_money);
          coin_quantity[i]++;  // для распознанного номера монетки прибавляем количество
          recogn_flag = true;
          break;
        }
      }
      coin_flag = false;
      standby_timer = millis();  // сбросить таймер
      break;
    }

    // Переход в спящий режим
    if (millis() - standby_timer > stb_time) {
      good_night();
      break;
    }

    // Кнопка проснуться
    while (!digitalRead(button)) {
      if (millis() - standby_timer > 500) { //millis() - standby_timer > 500
        lcd.clear();

        // отобразить на дисплее: сверху цены монет (округлено до целых!!!!), снизу их количество
        for (byte i = 0; i < coin_amount; i++) {
          lcd.setCursor(i * 3, 0); lcd.print((int)coin_value[i]);
          lcd.setCursor(i * 3, 1); lcd.print(coin_quantity[i]);
        }
      }
    }
  }
}

// функция сна
void good_night() {
  // записываем в EEPROM новые полученные количества монет по адресам начиная с 20го
  for (byte i = 0; i < coin_amount; i++) {
    EEPROM.updateInt(20 + i * 2, coin_quantity[i]);
  }
  sleep_flag = true;
  // вырубить питание со всех дисплеев и датчиков
  digitalWrite(disp_power, 0);
  digitalWrite(LEDpin, 0);
  digitalWrite(IRpin, 0);
  delay(100);
  
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

// просыпаемся по ПРЕРЫВАНИЮ (эта функция - обработчик прерывания)
void wake_up() {
  // возвращаем питание на дисплей и датчик
  digitalWrite(disp_power, 1);
  digitalWrite(LEDpin, 1);
  digitalWrite(IRpin, 1);
  standby_timer = millis();  // и обнуляем таймер
}
