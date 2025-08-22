String ver = "0.0095";
#include <microDS3231.h>
MicroDS3231 rtc;
#include <UnixTime.h>
UnixTime stamp(3);  // указать GMT (3 для Москвы)


#include "BluetoothSerial.h"
#include <AsyncStream.h>
BluetoothSerial ESP_BT;  //Object for Bluetooth
// указываем обработчик, терминатор (символ конца приёма) и таймаут в мс
// в <> указан размер буфера!
AsyncStream<256> serial(&Serial, '\n');
AsyncStream<256> serialBT(&ESP_BT, '\n');
#include <Wire.h>
#include <RadioLib.h>
#include <SPI.h>
byte new_packet_count = 0;
byte new_packet[112];
byte address_array[4];  // адрес устройства
byte address = 0;
byte address_dest = 255;
byte address_br = 255;
int radio_preset = 0;
int radio_preset_old = 0;
int pw_preset = 9;
int bw_preset = 3;
int sf_preset = 2;
int cr_preset = 0;
int tcxo = 0;

float fRX[] = { 251.900, 252.000, 253.800, 253.700, 257.000, 257.100, 256.850, 258.650, 262.225, 262.325 };
float fTX[] = { 292.900, 293.100, 296.000, 294.700, 297.675, 295.650, 297.850, 299.650, 295.825, 295.925 };

float fRX_east[] = { 251.900, 252.000, 253.800, 253.700, 257.000, 257.100, 256.850, 258.650, 262.225, 262.325 };
float fTX_east[] = { 292.900, 293.100, 296.000, 294.700, 297.675, 295.650, 297.850, 299.650, 295.825, 295.925 };

float fRX_west[] = { 250.900, 252.050, 253.850, 262.175, 267.050, 255.550, 251.950, 257.825, 260.125, 252.400 };
float fTX_west[] = { 308.300, 293.050, 294.850, 295.775, 308.050, 296.550, 292.950, 297.075, 310.125, 309.700 };

float fRX_test[] = { 250.000, 260.000, 270.000, 280.000, 290.000, 300.000, 310.000, 433.000, 434.000, 446.000 };
float fTX_test[] = { 250.000, 260.000, 270.000, 280.000, 290.000, 300.000, 310.000, 433.000, 434.000, 446.000 };

  float allRX[] = { 243.625, 243.625, 243.800, 244.135, 244.275, 245.200, 245.800, 245.850, 245.950, 247.450, 248.750, 248.825,
   249.375, 249.400, 249.450, 249.450, 249.490, 249.530, 249.850, 249.850, 249.890, 249.930, 250.090, 250.900, 251.275, 251.575,
    251.600, 251.850, 251.900, 251.950, 252.000, 252.050, 252.150, 252.200, 252.400, 252.450, 252.500, 252.550, 252.625, 253.550,
     253.600, 253.650, 253.700, 253.750, 253.800, 253.850, 253.850, 253.900, 254.000, 254.730, 254.775, 254.830, 255.250, 255.350,
      255.400, 255.450, 255.550, 255.550, 255.775, 256.450, 256.600, 256.850, 256.900, 256.950, 257.000, 257.050, 257.100, 257.150,
       257.150, 257.200, 257.250, 257.300, 257.350, 257.500, 257.700, 257.775, 257.825, 257.900, 258.150, 258.350, 258.450, 258.500,
        258.550, 258.650, 259.000, 259.050, 259.975, 260.025, 260.075, 260.125, 260.175, 260.375, 260.425, 260.425, 260.475, 260.525,
         260.550, 260.575, 260.625, 260.675, 260.675, 260.725, 260.900, 261.100, 261.100, 261.200, 262.000, 262.040, 262.075, 262.125,
          262.175, 262.175, 262.225, 262.275, 262.275, 262.325, 262.375, 262.425, 263.450, 263.500, 263.575, 263.625, 263.625, 263.675,
           263.725, 263.725, 263.775, 263.825, 263.875, 263.925, 265.250, 265.350, 265.400, 265.450, 265.500, 265.550, 265.675, 265.850,
            266.750, 266.850, 266.900, 266.950, 267.050, 267.050, 267.100, 267.150, 267.200, 267.250, 267.400, 267.875, 267.950, 268.000,
             268.025, 268.050, 268.100, 268.150, 268.200, 268.250, 268.300, 268.350, 268.400, 268.450, 269.700, 269.750, 269.800, 269.850,
              269.950 };
  float allTX[] = { 316.725, 300.400, 298.200, 296.075, 300.250, 312.850, 298.650, 314.230, 299.400, 298.800, 306.900, 294.375, 316.975,
   300.975, 299.000, 312.750, 313.950, 318.280, 316.250, 298.830, 300.500, 308.750, 312.600, 308.300, 296.500, 308.450, 298.225, 292.850,
    292.900, 292.950, 293.100, 293.050, 293.150, 299.150, 309.700, 309.750, 309.800, 309.850, 309.925, 294.550, 295.950, 294.650, 294.700,
     294.750, 296.000, 294.850, 294.850, 307.500, 298.630, 312.550, 310.800, 296.200, 302.425, 296.350, 296.400, 296.450, 296.550, 296.550,
      309.300, 313.850, 305.950, 297.850, 296.100, 297.950, 297.675, 298.050, 295.650, 298.150, 298.150, 308.800, 309.475, 309.725, 307.200,
       311.350, 316.150, 311.375, 297.075, 298.000, 293.200, 299.350, 299.450, 299.500, 299.550, 299.650, 317.925, 317.975, 310.050, 310.225,
        310.275, 310.125, 310.325, 292.975, 297.575, 294.025, 294.075, 294.125, 296.775, 294.175, 294.225, 294.475, 294.275, 294.325, 313.900,
         298.380, 298.700, 294.950, 314.200, 307.075, 306.975, 295.725, 297.025, 295.775, 295.825, 295.875, 300.275, 295.925, 295.975, 296.025,
          311.400, 309.875, 297.175, 297.225, 297.225, 297.275, 297.325, 297.325, 297.375, 297.425, 297.475, 297.525, 306.250, 306.350, 294.425,
           306.450, 302.525, 306.550, 306.675, 306.850, 316.575, 307.850, 297.625, 307.950, 308.050, 308.050, 308.100, 308.150, 308.200, 308.250,
            294.900, 310.375, 310.450, 310.475, 309.025, 310.550, 310.600, 309.150, 296.050, 309.250, 309.300, 309.350, 295.900, 309.450, 309.925,
             310.750, 310.025, 310.850, 310.950 };


int8_t Pwr[] = { -5, -2, 1, 4, 7, 10, 13, 16, 19, 22 };
float BW[] = { 7.81, 10.42, 15.63, 20.83, 31.25 };
int8_t SF[] = { 5, 6, 7, 8, 9, 10, 11, 12 };
int8_t CR[] = { 5, 6, 7, 8 };
int beacon_time = 60000;
byte new_beacon_count = 0;
// Using VSPI and a SX1262 module
//SPIClass *loraSPI = new SPIClass(VSPI);
//                      NSS DIO1 NRST BUSY
SX1262 radio = new Module(5, 26, 27, 25);
String str;
byte by;
// Флаг для инициализации принятого пакета
volatile bool receivedFlag = false;
uint32_t beacon1 = 0;  // переменная таймера маяка
bool beacon_on = 0;
int delay_time = 500;
bool eho_on = 1;       //эхо включен
bool boosted_on = 0;   //усиление вкл
bool online_on = 0;    //онлайн вкл
uint32_t online1 = 0;  // переменная онлайн
int online_time = 300000;
const int TCXO_Pin = 15;
int TCXO_State = 0;

void setup() {
  pinMode(TCXO_Pin, INPUT_PULLUP);
  Serial.begin(115200);
  // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));
  TCXO_State = digitalRead(TCXO_Pin);
  if (TCXO_State == LOW) { tcxo = 2.4; Serial.print(F("tcxo enable"));} else {tcxo = 0;}
  int state = radio.begin(fRX[radio_preset], BW[bw_preset], SF[sf_preset], CR[cr_preset], 0x18, Pwr[pw_preset], 10, tcxo, false);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true)
      ;
  }


  uint32_t id = 0;
  for (int i = 0; i < 17; i = i + 8) { id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i; }  // адрес устройства
  address_array[3] = id & 0xFF;
  address_array[2] = (id >> 8) & 0xFF;
  address_array[1] = (id >> 16) & 0xFF;
  address_array[0] = (id >> 24) & 0xFF;
  address = address_array[3] ^ address_array[2];
  new_packet[3] = address;
  Serial.println("My address: " + String(address));
  Serial.println("Broadcast address: 255");





  // set the function that will be called
  // when new packet is received
  radio.setDio1Action(setFlag);
  // start listening for LoRa packets
  Serial.print(F("Starting to Rx ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true)
      ;
  }


  // вывод времени готовой строкой String
  Serial.println(rtc.getTimeString());
  // вывод даты готовой строкой String
  Serial.println(rtc.getDateString());

  //при необходимости режим прослушивания можно отключить,
  // вызвав любой из следующих методов:
  //
  // radio.standby()
  // radio.sleep()
  // radio.transmit();
  // radio.receive();
  // radio.readData();
  // radio.scanChannel();
  //-------------

  Help();

  ESP_BT.begin("SC" + String(address));  // Дописать + adress
  ESP_BT.setPin("1234");
}



// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // мы получили пакет, устанавливаем флаг
  receivedFlag = true;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++
void loop() {
  String tmp;
  bool bt = 0;


  if (online_on == 1) { Online(); }
  if (beacon_on == 1) { Beacon(); }

  if (serialBT.available()) {
    Serial.println(serialBT.buf);
    memcpy(serial.buf, serialBT.buf, 256);
    bt = 1;
  }                              // если данные с BT ком порта получены
  if (serial.available() or bt)  // если данные с ком порта или bt получены
  {
    if (serial.buf[0] == '~')  // если ~ значит управляющая команда
    {
      radio.readData(tmp);
      if (serial.buf[1] == 's') { SetPreset(); }        // Смена канала
      if (serial.buf[1] == 'p') { SetPower(); }         // мощность -5 -2 1 4 7 10 13 16 19 22
      if (serial.buf[1] == 'b') { SetBW(); }            // Bandwidth 7.81  10.42  15.63  20.83  31.25
      if (serial.buf[1] == 't') { SatPing(); }          // Пинговалка
      if (serial.buf[1] == 'h') { Help(); }             // Хелп
      if (serial.buf[1] == 'm') { Beacon_on(); }        // Маяк m1-вкл m0-выкл
      if (serial.buf[1] == 'e') { Eho_on(); }           // Эхо m1-вкл m0-выкл
      if (serial.buf[1] == 'f') { SpreadingFactor(); }  // SpreadingFactor от 5 до 12
      if (serial.buf[1] == 'r') { CodingRate(); }       // CodingRate от 5 до 8
      if (serial.buf[1] == 'a') { Auto_tune(); }        // Авто настройка
      if (serial.buf[1] == 'x') {
        radio.reset(false);
        ESP.restart();
      }                                                // Перезагрузка
      if (serial.buf[1] == 'i') { info(); }            // Инфо
      if (serial.buf[1] == 'g') { BoostedGain_on(); }  // Boosted Gain g1-вкл g0-выкл
      if (serial.buf[1] == 'u') { Find(); }            // Поиск адресата
      if (serial.buf[1] == 'o') { Online_on(); }       // Онлайн o1-вкл o0-выкл
      if (serial.buf[1] == '0') { MassPing(); }        // Пинг всего
      if (serial.buf[1] == 'c') { Frequency_change();} // смена банка частот
    }

    if (serial.buf[0] == '@') { SendMsg_BR(); }  // если @ значит нужно отправить широковещательное текстовое сообщение
    if (serial.buf[0] == '#') { SendMsg_AD(); }  // если # значит нужно отправить текстовое сообщение адресату
    RX_start();
  }


  // Проверяем установлен ли флаг приема - сообщение принято
  if (receivedFlag) { received_msg(); }
}
// Функции ========================================
void SatPing() {
  radio.setFrequency(fTX[radio_preset]);  // устанавливаем частоту передачи
  byte ping[5];
  byte rx_ping[5];
  bool err_ping = 1;
  ping[1] = (radio.randomByte());
  ping[2] = (radio.randomByte());  // ID пакета
  ping[0] = ping[1] ^ ping[2];     // идентификатор
  ping[3] = address;
  ping[4] = 0;
  str = "RX:" + String(fRX[radio_preset], 3) + '/' + "TX:" + String(fTX[radio_preset], 3);
  Println(str);
  radio.transmit(ping, 5);
  uint32_t time1 = micros();
  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
  int state = radio.receive(rx_ping, 5);
  uint32_t time2 = micros();
  err_ping = memcmp(ping, rx_ping, 5);
  if (state == RADIOLIB_ERR_NONE || err_ping == 0) {
    // пакет был успешно принят
    uint32_t time3 = time2 - time1;
    str = "Ping>OK RSSI:";
    Print(str);
    str = radio.getRSSI();
    Print(str);
    str = " dBm/SNR:";
    Print(str);
    str = radio.getSNR();
    Print(str);
    str = " dB: ";
    Println(str);

    uint32_t len = ((((time3 * 0.000001) * 299792458 / 2) / 1000));
    str = "distance:~";
    Print(str);
    str = len;
    Print(str);
    str = "km   time:";
    Print(str);
    str = time3 * 0.001;
    Print(str);
    str = "ms";
    Println(str);

  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
    str = F("CRC error!");
    Println(str);
  } else {
    // some other error occurred
    str = F("failed, code ");
    Print(str);
    str = state;
    Println(str);
  }
  receivedFlag = false;
  delay(150);
  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
  //radio.startReceive()
}
void SetBW() {
  switch (serial.buf[2]) {
    case '0':
      bw_preset = 0;
      radio.setBandwidth(BW[bw_preset]);
      str = "Bandwidth set 7.81";
      Println(str);
      break;
    case '1':
      bw_preset = 1;
      radio.setBandwidth(BW[bw_preset]);
      str = "Bandwidth set 10.42";
      Println(str);
      break;
    case '2':
      bw_preset = 2;
      radio.setBandwidth(BW[bw_preset]);
      str = "Bandwidth set 15.63";
      Println(str);
      break;
    case '3':
      bw_preset = 3;
      radio.setBandwidth(BW[bw_preset]);
      str = "Bandwidth set 20.83";
      Println(str);
      break;
    case '4':
      bw_preset = 4;
      radio.setBandwidth(BW[bw_preset]);
      str = "Bandwidth set 31.25";
      Println(str);
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
}
void SetPreset() {
  switch (serial.buf[2]) {
    case '0':
      radio_preset = 0;
      break;
    case '1':
      radio_preset = 1;
      break;
    case '2':
      radio_preset = 2;
      break;
    case '3':
      radio_preset = 3;
      break;
    case '4':
      radio_preset = 4;
      break;
    case '5':
      radio_preset = 5;
      break;
    case '6':
      radio_preset = 6;
      break;
    case '7':
      radio_preset = 7;
      break;
    case '8':
      radio_preset = 8;
      break;
    case '9':
      radio_preset = 9;
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
  str = ("Set channel " + String(radio_preset) + "- RX:" + String(fRX[radio_preset], 3) + '/' + "TX:" + String(fTX[radio_preset], 3));
  Println(str);
}

void SetPower() {
  switch (serial.buf[2]) {
    case '0':
      pw_preset = 0;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set -5";
      Println(str);
      break;
    case '1':
      pw_preset = 1;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set -2";
      Println(str);
      break;
    case '2':
      pw_preset = 2;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set 1";
      Println(str);
      break;
    case '3':
      pw_preset = 3;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set 4";
      Println(str);
      break;
    case '4':
      pw_preset = 4;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set 7";
      Println(str);
      break;
    case '5':
      pw_preset = 5;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set 10";
      Println(str);
      break;
    case '6':
      pw_preset = 6;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set 13";
      Println(str);
      break;
    case '7':
      pw_preset = 7;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set 16";
      Println(str);
      break;
    case '8':
      pw_preset = 8;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set 19";
      Println(str);
      break;
    case '9':
      pw_preset = 9;
      radio.setOutputPower(Pwr[bw_preset]);
      str = "Power set 22";
      Println(str);
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
}
void Help() {
  str = ("Мой адрес: " + String(address));
  Print(str);
  str = (" / BC адрес: 255");
  Println(str);
  str = ("developed by KiT:) ver-" + ver);
  Println(str);
  str = " @ - передать всем сообщение";
  Println(str);
  str = " #023 - передать адресату-023 сообщение";
  Println(str);
  str = "~x - команды настройки";
  Println(str);
  str = "   s - смена канала 0...9";
  Println(str);
  str = "   p - мощность -5 -2 1 4 7 10 13 16 19 22";
  Println(str);
  str = "   b - Bandwidth 7.81  10.42  15.63  20.83  31.25";
  Println(str);
  str = "   r - CodingRate 5 6 7 8";
  Println(str);
  str = "   f - SpreadingFactor 5 6 7 8 9 10 11 12";
  Println(str);
  str = "   t - Пинговалка";
  Println(str);
  str = "   a - Автонастройка";
  Println(str);
  str = "   h - Хелп";
  Println(str);
  str = "   m0 - маяк выкл. m1 - маяк вкл. (раз в 60 сек.)";
  Println(str);
  str = "   e0 - Эхо e1-вкл e0-выкл";
  Println(str);
  str = "   i - Инфо";
  Println(str);
  str = "   g - Boosted Gain g1-вкл g0-выкл";
  Println(str);
  str = "   o0 - Находится онлайн o1-вкл o0-выкл";
  Println(str);
  str = "   uxxx - Поиск клиента с адресом ххх";
  Println(str);
  str = "   cx - банка частот. ce-восток, cw-запад, ct-тест";
  Println(str);

}





void Beacon() {
  if (millis() - beacon1 >= beacon_time) {  // ищем разницу
    beacon1 = millis();                     // сброс таймера
    byte beacon_array[15]{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 66, 69, 65, 67, 79, 78 };
    beacon_array[1] = (radio.randomByte());
    beacon_array[2] = (radio.randomByte());               // ID пакета
    beacon_array[0] = beacon_array[1] ^ beacon_array[2];  // идентификатор
    beacon_array[3] = address;
    beacon_array[4] = address_br;
    if (new_beacon_count <= 254) {
      beacon_array[8] = new_beacon_count;
      new_beacon_count++;
    }  // Локальный счетчик сообщений 0...255
    else {
      new_packet[8] = new_beacon_count;
      new_beacon_count = 0;
    }
    //beacon_array[9]=66;beacon_array[10]=69;beacon_array[11]=65;beacon_array[12]=67;beacon_array[13]=79;beacon_array[14]=78;beacon_array[15]=13;
    radio.setFrequency(fTX[radio_preset]);  // устанавливаем частоту передачи
    radio.transmit(beacon_array, 15);
    radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
    receivedFlag = false;
    // возвращаем модуль на прослушку
    radio.startReceive();
  }
}
void Beacon_on() {
  switch (serial.buf[2]) {
    case '0':
      str = "Beacon_off";
      Println(str);
      beacon_on = 0;
      break;
    case '1':
      str = "Beacon_on";
      Println(str);
      beacon_on = 1;
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
}
void SpreadingFactor() {
  switch (serial.buf[2]) {
    case '0':
      sf_preset = 0;
      radio.setSpreadingFactor(SF[sf_preset]);
      str = "SpreadingFactor set 5";
      Println(str);
      break;
    case '1':
      sf_preset = 1;
      radio.setSpreadingFactor(SF[sf_preset]);
      str = "SpreadingFactor set 6";
      Println(str);
      break;
    case '2':
      sf_preset = 2;
      radio.setSpreadingFactor(SF[sf_preset]);
      str = "SpreadingFactor set 7";
      Println(str);
      break;
    case '3':
      sf_preset = 3;
      radio.setSpreadingFactor(SF[sf_preset]);
      str = "SpreadingFactor set 8";
      Println(str);
      break;
    case '4':
      sf_preset = 4;
      radio.setSpreadingFactor(SF[sf_preset]);
      str = "SpreadingFactor set 9";
      Println(str);
      break;
    case '5':
      sf_preset = 5;
      radio.setSpreadingFactor(SF[sf_preset]);
      str = "SpreadingFactor set 10";
      Println(str);
      break;
    case '6':
      sf_preset = 6;
      radio.setSpreadingFactor(SF[sf_preset]);
      str = "SpreadingFactor set 11";
      Println(str);
      break;
    case '7':
      sf_preset = 7;
      radio.setSpreadingFactor(SF[sf_preset]);
      str = "SpreadingFactor set 12";
      Println(str);
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
}

void CodingRate() {
  switch (serial.buf[2]) {
    case '0':
      cr_preset = 0;
      radio.setCodingRate(CR[cr_preset]);
      str = "CodingRate set 5";
      Println(str);
      break;
    case '1':
      cr_preset = 1;
      radio.setCodingRate(CR[cr_preset]);
      str = "CodingRate set 6";
      Println(str);
      break;
    case '2':
      cr_preset = 2;
      radio.setCodingRate(CR[cr_preset]);
      str = "CodingRate set 7";
      Println(str);
      break;
    case '3':
      cr_preset = 3;
      radio.setCodingRate(CR[cr_preset]);
      str = "CodingRate set 8";
      Println(str);
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
}


void Println(String str) {
  Serial.println(str);
  ESP_BT.println(str);
}
void Print(String str) {
  Serial.print(str);
  ESP_BT.print(str);
}
void Print_hex(byte by) {
  Serial.print(by, HEX);
  ESP_BT.print(by, HEX);
}


void RX_start() {
  receivedFlag = false;                   // Сбрасываем флаг
  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
  radio.startReceive();
}
void received_msg() {

  receivedFlag = false;  // Сбрасываем флаг
  byte rx_array[109];
  memset(rx_array, 0, 109);
  bool msg_addr_ok = 0;
  bool msg_id_ok = 0;
  int state = radio.readData(rx_array, 0);
  byte addr_source = rx_array[3];
  byte x = rx_array[1] ^ rx_array[2];
  if (rx_array[0] == x) {
    msg_id_ok = 1;
  } else {
    msg_id_ok = 0;
  }                      // проверяем ID сообщения
  byte y = rx_array[4];  // адрес получателя
  if (y == address_br || y == address) {
    msg_addr_ok = 1;
  } else {
    msg_addr_ok = 0;
  }  // проверяем адрес сообщения наш или BC

  if (state == RADIOLIB_ERR_NONE && msg_addr_ok == 1 && msg_id_ok == 1) {
    // пакет успешно принят И адрес назначения наш или BC И ID- ok
    //for (int i = 0; i < sizeof(rx_array); i++) {Serial.print(rx_array[i],HEX);Serial.print (" ");}
    if (y == address) {
      str = "RX# ";
      Print(str);
    } else {
      str = "RX: ";
      Print(str);
    }
    str = rtc.getTimeString();
    Print(str);
    str = " ";
    Print(str);
    str = radio.getRSSI();
    Print(str);
    str = "dBm/";
    Print(str);
    str = radio.getSNR();
    Print(str);
    str = "dB ID:0x";
    Print(str);
    by = rx_array[1];
    Print_hex(by);
    by = rx_array[2];
    Print_hex(by);
    str = " Adr:";
    Print(str);
    str = rx_array[3];
    Print(str);
    str = " №:";
    Print(str);
    str = rx_array[8];
    Println(str);
    byte delivered = rx_array[5];
    if (delivered == 2) {
      str = " Msg delivered";
      Print(str);
    }



    byte tmp_array[100];
    String win_buf;
    String utf_buf;
    for (int k = 9; k < sizeof(rx_array); k++) { tmp_array[k - 9] = rx_array[k]; }
    win_buf = String((char *)tmp_array);
    utf_buf = win_utf8(win_buf);  // конвертируем строку в utf8
    str = ("    ");
    Print(str);
    str = (utf_buf);
    Println(str);


    byte addr_eho = rx_array[3];
    if (eho_on == 1 && delivered == 1) { eho(addr_eho); }  // если 5 байт==1 то отправляем подтверждение



  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // пакет был принят, но поврежден
    str = F("CRC error!");
    Println(str);

  } else {
    // другая ошибка  (6 таймаут)
    if (y == 0) {
      str = "RX: ";
      Print(str);
      str = rtc.getTimeString();
      Print(str);
      str = " ";
      Print(str);
      str = radio.getRSSI();
      Print(str);
      str = "dBm/";
      Print(str);
      str = radio.getSNR();
      Print(str);
      str = ("dB Ping Adr:");
      Print(str);
      str = (rx_array[3]);
      Println(str);
    } else if (msg_addr_ok == 0 && msg_id_ok == 1) {
      str = F("RX: addr msg");
      Println(str);
    } else {
      str = F("failed, code ");
      Print(str);
      str = state;
      Println(str);
    }
  }

  // возвращаем модуль на прослушку
  RX_start();
}

void Auto_tune() {
  str = ("Wait");
  Println(str);
  float tune_max = -1000;  //1.175E-38; /* первоначально будем сравнивать с минимальным для float значением */
  int tune_max_index = 0;
  float auto_tune_arr[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  // массив для авто тюн

  byte tune[5];
  byte rx_tune[5];
  bool err_tune = 1;
  tune[1] = (radio.randomByte());
  tune[2] = (radio.randomByte());  // ID пакета
  tune[0] = tune[1] ^ tune[2];     // идентификатор
  tune[3] = address;
  tune[4] = 0;
  radio_preset_old = radio_preset;
  for (radio_preset = 0; radio_preset < 10; radio_preset++) {
    str = ("Test channel " + String(radio_preset) + "-> ");
    Print(str);
    radio.setFrequency(fTX[radio_preset]);  // устанавливаем частоту передачи
    radio.transmit(tune, 5);
    radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
    int state = radio.receive(rx_tune, 5);
    err_tune = memcmp(tune, rx_tune, 5);
    if (state == RADIOLIB_ERR_NONE && err_tune == 0) {
      str = radio.getSNR();
      Println(str);
      float myFloat = str.toFloat();
      auto_tune_arr[radio_preset] = myFloat;
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      str = F("CRC error!");
      Println(str);
      auto_tune_arr[radio_preset] = -1000;
    } else {
      str = F("failed, code ");
      Print(str);
      str = state;
      Println(str);
      auto_tune_arr[radio_preset] = -1000;
    }
  }
  for (int i = 0; i < 10; i++) { tune_max = max(tune_max, auto_tune_arr[i]); }
  for (int a = 0; a < 10; a++) {
    if (auto_tune_arr[a] == tune_max) { tune_max_index = a; }
  }
  if (tune_max == -1000) {
    str = "Error tune";
    Println(str);
    radio_preset = radio_preset_old;
  } else {
    radio_preset = tune_max_index;  //переключаем на лучший пресет
    str = ("Set channel " + String(radio_preset) + "- RX:" + String(fRX[radio_preset], 3) + '/' + "TX:" + String(fTX[radio_preset], 3));
    Println(str);
  }
  receivedFlag = false;
  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
}


void SendMsg_BR() {

  new_packet[1] = (radio.randomByte());
  new_packet[2] = (radio.randomByte());           // ID пакета
  new_packet[0] = new_packet[1] ^ new_packet[2];  // идентификатор
  new_packet[3] = address;
  new_packet[4] = address_br;
  new_packet[5] = 0;
  new_packet[6] = 0;
  new_packet[7] = 0;  // резерв
  if (new_packet_count <= 254) {
    new_packet[8] = new_packet_count;
    new_packet_count++;
  }  // Локальный счетчик сообщений 0...255
  else {
    new_packet[8] = new_packet_count;
    new_packet_count = 0;
  }


  String win_buf;
  byte win_array[101];
  memset(win_array, 0, 101);
  win_buf = utf8_win(serial.buf);                                      // конвертируем строку в win -String win_buf
  win_buf.getBytes(win_array, 101);                                    // переносим строку в массив byte
  for (int i = 9; i < 109; i++) { new_packet[i] = win_array[i - 8]; }  // копируем в массив нового пакета


  //    for (int i = 0; i < sizeof(new_packet); i++) {Serial.print(new_packet[i],HEX);Serial.print (" ");}
  //Serial.println (" ");


  radio.setFrequency(fTX[radio_preset]);  // устанавливаем частоту передачи
  radio.transmit(new_packet, 9 + win_buf.length());

  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
  delay(delay_time);
  receivedFlag = false;
}
void SendMsg_AD() {
  String addr = "000";
  int addr_int;
  addr[0] = serial.buf[1];
  addr[1] = serial.buf[2];
  addr[2] = serial.buf[3];
  addr_int = addr.toInt();
  address_dest = lowByte(addr_int);
  new_packet[1] = (radio.randomByte());
  new_packet[2] = (radio.randomByte());           // ID пакета
  new_packet[0] = new_packet[1] ^ new_packet[2];  // идентификатор
  new_packet[3] = address;
  new_packet[4] = address_dest;
  new_packet[6] = 0;
  new_packet[7] = 0;  // резерв
  if (eho_on == 1) {
    new_packet[5] = 1;
  } else {
    new_packet[5] = 0;
  }  // Требуется подтверждение приема- эхо
  if (new_packet_count <= 254) {
    new_packet[8] = new_packet_count;
    new_packet_count++;
  }  // Локальный счетчик сообщений 0...255
  else {
    new_packet[8] = new_packet_count;
    new_packet_count = 0;
  }


  String win_buf;
  byte win_array[104];
  memset(win_array, 0, 104);
  win_buf = utf8_win(serial.buf);                                      // конвертируем строку в win -String win_buf
  win_buf.getBytes(win_array, 104);                                    // переносим строку в массив byte
  for (int i = 9; i < 109; i++) { new_packet[i] = win_array[i - 5]; }  // копируем в массив нового пакета

  //for (int i = 0; i < sizeof(new_packet); i++) {Serial.print(new_packet[i],HEX);Serial.print (" ");}
  //Serial.println (" ");


  radio.setFrequency(fTX[radio_preset]);  // устанавливаем частоту передачи
  radio.transmit(new_packet, 9 + win_buf.length());

  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
  delay(delay_time);
  receivedFlag = false;
}



//UTF-8 в Windows-1251
String utf8_win(String source) {
  int i, k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length();
  i = 0;

  while (i < k) {
    n = source[i];
    i++;

    if (n >= 0xC0) {
      switch (n) {
        case 0xD0:
          {
            n = source[i];
            i++;
            if (n == 0x81) {
              n = 0xA8;
              break;
            }
            if (n >= 0x90 && n <= 0xBF) n = n + 0x30;
            break;
          }
        case 0xD1:
          {
            n = source[i];
            i++;
            if (n == 0x91) {
              n = 0xB8;
              break;
            }
            if (n >= 0x80 && n <= 0x8F) n = n + 0x70;
            break;
          }
      }
    }
    m[0] = n;
    target = target + String(m);
  }
  return target;
}

//Windows-1251 в UTF-8
String win_utf8(String source) {
  int i, k;
  String target;
  unsigned char n;
  char m[2] = { '0', '0' };

  k = source.length();
  i = 0;

  while (i < k) {
    n = source[i];
    i++;
    if (n >= 192 && n <= 239) {
      m[0] = 208;
      m[1] = n - 48;
      target += String(m);
    }
    if (n >= 240 && n <= 255) {
      m[0] = 209;
      m[1] = n - 112;
      target += String(m);
    }
    if (n == 168) {
      m[0] = 208;
      m[1] = 129;
      target += String(m);
    }
    if (n == 184) {
      m[0] = 209;
      m[1] = 145;
      target += String(m);
    }
    if (n >= 32 && n <= 127) { target += char(n); }
  }
  return target;
}

void eho(byte addr) {

  String tmp;
  tmp += "eho:";
  tmp += radio.getRSSI();
  tmp += "/";
  tmp += radio.getSNR();
  byte eho[30];
  byte tmp_buf[20];
  eho[1] = (radio.randomByte());
  eho[2] = (radio.randomByte());  // ID пакета
  eho[0] = eho[1] ^ eho[2];       // идентификатор
  eho[3] = address;
  eho[4] = addr;
  eho[5] = 2;
  eho[6] = 0;
  eho[7] = 0;  // eho[5]=2 подтверждение ,резерв
  if (new_packet_count <= 254) {
    eho[8] = new_packet_count;
    new_packet_count++;
  }  // Локальный счетчик сообщений 0...255
  else {
    eho[8] = new_packet_count;
    new_packet_count = 0;
  }
  tmp.getBytes(tmp_buf, (tmp.length() + 1));                                  // переносим строку в массив byte
  for (int i = 9; i < (tmp.length() + 10); i++) { eho[i] = tmp_buf[i - 9]; }  // копируем в массив нового пакета

  str = "TX: eho";
  Println(str);
  radio.setFrequency(fTX[radio_preset]);  // устанавливаем частоту передачи
  radio.transmit(eho, (tmp.length() + 9));
  //for (int i = 0; i < sizeof(tmp_buf); i++) {Serial.print(tmp_buf[i],HEX);Serial.print (" ");} Serial.println (" ");
  //Serial.println (tmp);  Serial.println (tmp.length());
  //for (int i = 0; i < sizeof(eho); i++) {Serial.print(eho[i],HEX);Serial.print (" ");} Serial.println (" ");
  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
  delay(delay_time);
}

void Eho_on() {
  switch (serial.buf[2]) {
    case '0':
      str = "Eho_off";
      Println(str);
      eho_on = 0;
      break;
    case '1':
      str = "Eho_on";
      Println(str);
      eho_on = 1;
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
}
void info() {
  str = ("ver-" + ver);
  Println(str);
  str = ("My address: " + String(address));
  Print(str);
  str = (" / BC address: 255");
  Println(str);
  str = (rtc.getTimeString());
  Print(str);
  str = (" / " + (rtc.getDateString()));
  Println(str);
  str = ("Channel " + String(radio_preset) + "- RX:" + String(fRX[radio_preset], 3) + '/' + "TX:" + String(fTX[radio_preset], 3));
  Println(str);
  str = ("SF-" + String(SF[sf_preset]));
  Print(str);
  str = (" CR-" + String(CR[cr_preset]));
  Print(str);
  str = (" BW-" + String(BW[bw_preset]));
  Print(str);
  str = (" PW-" + String(Pwr[pw_preset]));
  Println(str);
  str = ("Beacon-" + String(beacon_on));
  Println(str);
  str = ("Echo-" + String(eho_on));
  Println(str);
  str = ("Online-" + String(online_on));
  Println(str);
  str = ("Boosted-" + String(boosted_on));
  Println(str);
}





void BoostedGain_on() {
  int state;
  switch (serial.buf[2]) {
    case '0':
      str = "Boosted Gain- off";
      Println(str);
      boosted_on = 0;
      state = radio.setRxBoostedGainMode(boosted_on, true);
      break;
    case '1':
      str = "Boosted Gain- on";
      Println(str);
      boosted_on = 1;
      state = radio.setRxBoostedGainMode(boosted_on, true);
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("OK");
  } else {
    Serial.print("failed, code ");
    Serial.println(state);
  }
}

int Find_TX_RX() {
  byte new_packet_find[13]{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 104, 117, 110, 116 };
  String addr_find = "000";
  int addr_find_int;
  addr_find[0] = serial.buf[2];
  addr_find[1] = serial.buf[3];
  addr_find[2] = serial.buf[4];
  addr_find_int = addr_find.toInt();
  byte address_find = lowByte(addr_find_int);
  //Serial.println (adress_find,HEX);
  new_packet_find[1] = (radio.randomByte());
  new_packet_find[2] = (radio.randomByte());                     // ID пакета
  new_packet_find[0] = new_packet_find[1] ^ new_packet_find[2];  // идентификатор
  new_packet_find[3] = address;
  new_packet_find[4] = address_find;
  new_packet_find[6] = 0;
  new_packet_find[7] = 0;  // резерв
  if (eho_on == 1) {
    new_packet_find[5] = 1;
  } else {
    new_packet_find[5] = 0;
  }  // Требуется подтверждение приема- эхо
  if (new_packet_count <= 254) {
    new_packet_find[8] = new_packet_count;
    new_packet_count++;
  }  // Локальный счетчик сообщений 0...255
  else {
    new_packet_find[8] = new_packet_count;
    new_packet_count = 0;
  }
  radio.setFrequency(fTX[radio_preset]);  // устанавливаем частоту передачи
  radio.transmit(new_packet_find, 13);
  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема



  delay(270);
  int find_time = 2000;
  byte find_eho[109];
  //int state = radio.receive(find_eho, 109);
  uint32_t find1 = millis();  // переменная онлайн
  RX_start();

  while (millis() - find1 <= find_time) {
    if (receivedFlag) break;
  }
  //for (find1 = millis(); millis() - find1 <= find_time;) {if (receivedFlag) break;}
  receivedFlag = false;  // Сбрасываем флаг
  int state = radio.readData(find_eho, 109);
  int find_state = 0;
  bool msg_addr_ok = 0;
  bool msg_id_ok = 0;
  byte addr_source = find_eho[3];
  byte x = find_eho[1] ^ find_eho[2];
  if (find_eho[0] == x) {
    msg_id_ok = 1;
  } else {
    msg_id_ok = 0;
  }                      // проверяем ID сообщения
  byte y = find_eho[4];  // адрес получателя
  if (y == address) {
    msg_addr_ok = 1;
  } else {
    msg_addr_ok = 0;
  }  // проверяем адрес сообщения наш
  if (state == RADIOLIB_ERR_NONE && msg_addr_ok == 1 && msg_id_ok == 1) {
    // пакет успешно принят И адрес назначения наш И ID- ok
    find_state = 1;
    //Serial.println ("ok");
  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    // timeout occurred while waiting for a packet
    find_state = 2;
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
    find_state = 3;
  } else {
    // some other error occurred
    find_state = 4;
  }

  return (find_state);
}


void Find() {
  int state = 0;
  for (int i = 0; i < 10; i++) {
    radio_preset = i;
    str = ("Сhannel " + String(radio_preset) + "- RX:" + String(fRX[radio_preset], 3) + '/' + "TX:" + String(fTX[radio_preset], 3));
    Println(str);
    state = Find_TX_RX();
    if (state == 1) break;
  }
  if (state == 1) {
    str = ("Found OK");
    Println(str);
  } else {
    str = ("Not found");
    Println(str);
  }
}

void Online_on() {
  switch (serial.buf[2]) {
    case '0':
      str = "Online_off";
      Println(str);
      online_on = 0;
      break;
    case '1':
      str = "Online_on";
      Println(str);
      online_on = 1;
      break;
    default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
}


int ch_ping() {
  int ch = 0;
  radio.setFrequency(fTX[radio_preset]);  // устанавливаем частоту передачи
  byte ping[5];
  byte rx_ping[5];
  bool err_ping = 1;
  ping[1] = (radio.randomByte());
  ping[2] = (radio.randomByte());  // ID пакета
  ping[0] = ping[1] ^ ping[2];     // идентификатор
  ping[3] = address;
  ping[4] = 0;
  radio.transmit(ping, 5);

  radio.setFrequency(fRX[radio_preset]);  // устанавливаем частоту приема
  int state = radio.receive(rx_ping, 5);
  err_ping = memcmp(ping, rx_ping, 5);
  if (state == RADIOLIB_ERR_NONE || err_ping == 0) {
    // пакет был успешно принят
    ch = 1;
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
  } else {
    // some other error occurred
  }
  receivedFlag = false;
  return (ch);
}

void Online() {
  if (millis() - online1 >= online_time) {  // ищем разницу
    online1 = millis();                     // сброс таймера
    str = "Checking channel";
    Println(str);
    int check = 0;
    check = ch_ping();
    if (check == 1) {
      str = "Channel is stable";
      Println(str);
    } else {
      Auto_tune();
    }
  }
  RX_start();
}


void MassPing() {
  int counter = 0;
  radio_preset_old = radio_preset;  //сохраняем текущий пресет
  str = (rtc.getTimeString());
  Print(str);
  str = (" / " + (rtc.getDateString()));
  Println(str);
  for (radio_preset = 0; radio_preset < 167; radio_preset++) {

    str = "RX:" + String(allRX[radio_preset], 3) + '/' + "TX:" + String(allTX[radio_preset], 3);
    Print(str);
    int check = 0;
    check = all_ping();
    if (check == 1) {
      str = " OK  RSSI:" + String(radio.getRSSI()) + " dBm/SNR:" + String(radio.getSNR()) + " dB: ";
      Println(str);
      counter++;
    } else {
      str = " Bad";
      Println(str);
    }
    delay(150);
  }

  radio_preset = radio_preset_old;  //возвращаем пресет на место
  str = "Found: " + String(counter);
  Println(str);
}

int all_ping() {
  int ch = 0;
  radio.setFrequency(allTX[radio_preset]);  // устанавливаем частоту передачи
  byte ping[5];
  byte rx_ping[5];
  bool err_ping = 1;
  ping[1] = (radio.randomByte());
  ping[2] = (radio.randomByte());  // ID пакета
  ping[0] = ping[1] ^ ping[2];     // идентификатор
  ping[3] = address;
  ping[4] = 0;
  radio.transmit(ping, 5);

  radio.setFrequency(allRX[radio_preset]);  // устанавливаем частоту приема
  int state = radio.receive(rx_ping, 5);
  err_ping = memcmp(ping, rx_ping, 5);
  if (state == RADIOLIB_ERR_NONE || err_ping == 0) {
    // пакет был успешно принят
    ch = 1;
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
  } else {
    // some other error occurred
  }
  receivedFlag = false;
  return (ch);
}

void Frequency_change() {
  switch (serial.buf[2]) {
    case 'e':
      str = "East bank on";
      Println(str);
      memcpy(fRX, fRX_east, sizeof(fRX));
      memcpy(fTX, fTX_east, sizeof(fTX));
      str="";
      for (int i = 0; i < 10; i++) {str+=String(fRX[i],3);str+=" ";}
      Println(str);
      str="";
      for (int i = 0; i < 10; i++) {str+=String(fTX[i],3);str+=" ";}
      Println(str);
      break;
      case 'w':
      str = "West bank on";
      Println(str);
      memcpy(fRX, fRX_west, sizeof(fRX));
      memcpy(fTX, fTX_west, sizeof(fTX));
      str="";
      for (int i = 0; i < 10; i++) {str+=String(fRX[i],3);str+=" ";}
      Println(str);
      str="";
      for (int i = 0; i < 10; i++) {str+=String(fTX[i],3);str+=" ";}
      Println(str);
      break;
      case 't':
      str = "Test bank on";
      Println(str);
      memcpy(fRX, fRX_test, sizeof(fRX));
      memcpy(fTX, fTX_test, sizeof(fTX));
      str="";
      for (int i = 0; i < 10; i++) {str+=String(fRX[i],3);str+=" ";}
      Println(str);
      str="";
      for (int i = 0; i < 10; i++) {str+=String(fTX[i],3);str+=" ";}
      Println(str);
      default:  // выполнить, если значение не совпадает ни с одним из case
      break;
  }
}
/*
    uint8_t Seconds= rtc.getSeconds();           // получить секунды
    uint8_t Minutes= rtc.getMinutes();           // получить минуты
    uint8_t Hours= rtc.getHours();               // получить часы
    uint8_t Date= rtc.getDate();                 // получить число
    uint8_t Month= rtc.getMonth();               // получить месяц
    uint16_t Year= rtc.getYear();                // получить год
    stamp.setDateTime(Year, Month, Date, Hours, Minutes, Seconds);   // 20 мая 2021, 7:04:15
    uint32_t unix = stamp.getUnix(); //получить unix время
    new_packet[4] = ((unix >> 0) & 0xFF);  // 00 00 00 FF
    new_packet[5] = ((unix >> 8) & 0xFF); // 00 00 FF 00
    new_packet[6] = ((unix >> 16) & 0xFF);// 00 FF 00 00
    new_packet[7] = ((unix >> 24) & 0xFF);// FF 00 00 00

      uint32_t rx_time= ((uint32_t)rx_array[7] << 24) + ((uint32_t)rx_array[6] << 16) + ((uint32_t)rx_array[5] << 8) + (uint32_t)rx_array[4]; // собираем число из 4-х байтов
      Serial.println(rx_time);
      stamp.getDateTime(rx_time);

     for (int i = 0; i < sizeof(new_packet); i++) {Serial.print(new_packet[i]);Serial.print (" ");}
   */