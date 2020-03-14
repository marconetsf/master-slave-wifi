// Programação responsável por resetar o interruptor
#include <EEPROM.h>
#include <WiFiManager.h>  

WiFiManager wifiManager;
 
void setup()
{   
  Serial.begin(115200);
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);

  wifiManager.resetSettings();
  
  EEPROM.begin(150);//Inicia toda a EEPROM.

  EEPROM.put(120, false);
  
  EEPROM.end();//Fecha a EEPROM.

  Serial.println("dispositivo resetado");
  pisca(5);
}

void loop()
{

}

void pisca(int vezes) {
  for (int d = 0; d <= vezes; d++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
  }
}
