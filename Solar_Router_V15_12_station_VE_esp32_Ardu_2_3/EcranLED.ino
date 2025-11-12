#include "EcranLED.h"
LGFXoled oled;
LGFX_Sprite sprite (&oled);     // HP EVSE, ESP_ARD MC 

void Init_LED_OLED(void) {
  //LEDs et OLRD
  //LEDgroupe = 0;   //0:pas de LED,1à 9 LEDs, 10 et 11 écran OLED SSD1306 , 12 et  13 OLED SH1106
  if (LEDgroupe > 0 && LEDgroupe < 10) {  //Simples LEDs
    pinMode(LEDyellow[LEDgroupe], OUTPUT);
    pinMode(LEDgreen[LEDgroupe], OUTPUT);
  }
  if (LEDgroupe >= 10) {
    bool SD = true;                                                     //SSD1306
    if (LEDgroupe > 11) SD = false;                                     //SH1106
    oled.LGFXoled_init(LEDyellow[LEDgroupe], LEDgreen[LEDgroupe], SD);  //SDA , SCL et SSD1306 ou SH1106
    // put your setup code here, to run once:
    oled.begin();
    oled.setRotation(2);
    oled.setBrightness(255);
    oled.setTextFont(1);
    oled.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.createSprite(128, 64);    // ESP_ARD MC HP EVSE create a full screen sprite even if it will not be used
    sprite.setColorDepth(1);         // HP EVSE 1-bit monochrome OLED sprite
    if(ecran_type == 0){             // HP EVSE there is no screen selected for the EVSE - use the RMS screen as normal
      PrintCentreO("F1ATB", -1, 0, 2);
      oled.setScrollRect(0, 16, oled.width(), oled.height() - 16); 
      oled.setTextScroll(true); 
      delay(100);
    }
    else {                           //MC - HP EVSE - there is an EVSE screen - show BORNE VE message instead of RMS standard
      String iVersion = String(Version);
      String iCh ="RMS v" + iVersion.substring(0, iVersion.indexOf("_"));
      PrintCentreO("F1ATB", -1, 0, 2);
      //PrintCentreO("RMS" , -1, 18, 2);
      PrintCentreO(iCh , -1, 18, 1);
      PrintCentreO("BORNE VE", -1, 36, 2);
      #ifdef ARDUINO_Uno
        iCh = "Aduino Uno v";
      #else
        iCh = "ESP32 v";
      #endif  // MC ARDUINO_Uno
      iCh += iVersion.substring(iVersion.lastIndexOf("_")+1);
      PrintCentreO(iCh, -1, 54, 1);
    }   // ESP_ARD MC ARDUINO_Uno
  }
}

//****************
//* Gestion LEDs *
//****************
void Gestion_LEDs() {

  int retard_min = 100;
  int retardI;
  cptLEDyellow++;
  if ((WiFi.status() != WL_CONNECTED && ESP32_Type < 10) || (EthernetBug > 0 && ESP32_Type >= 10)) {  // Attente connexion au Wifi ou ethernet
    if (WiFi.getMode() == WIFI_STA) {                                                                 // en  Station mode
      cptLEDyellow = (cptLEDyellow + 6) % 10;
      cptLEDgreen = cptLEDyellow;
    } else {  //AP Mode
      cptLEDyellow = cptLEDyellow % 10;
      cptLEDgreen = (cptLEDyellow + 5) % 10;
    }
  } else {
    for (int i = 0; i < NbActions; i++) {
      retardI = Retard[i];
      retard_min = min(retard_min, retardI);
    }
    if (retard_min < 100) {
      cptLEDgreen = int((cptLEDgreen + 1 + 8 / (1 + retard_min / 10))) % 10;
    } else {
      cptLEDgreen = 10;
    }
  }


  if (LEDgroupe > 0 && LEDgroupe < 10) {
    int L = 0, H = 1;  //LED classique
    if (LEDgroupe == 2) {
      L = 1;
      H = 0;
    }
    if (cptLEDyellow > 5) {
      digitalWrite(LEDyellow[LEDgroupe], L);
    } else {
      digitalWrite(LEDyellow[LEDgroupe], H);
    }
    if (cptLEDgreen > 5) {
      digitalWrite(LEDgreen[LEDgroupe], L);
    } else {
      digitalWrite(LEDgreen[LEDgroupe], H);
    }
  }
  if (LEDgroupe >= 10) {

    if (cptLEDyellow == 5) {  //New puissance dispo
      if(ecran_type == 0){    // HP EVSE there is no screen selected for the EVSE - use the RMS screen as normal
        oled.fillRect(0, 0, oled.width(), 16, TFT_BLACK); // Delete previous message
        int Puissance = PuissanceS_M - PuissanceI_M;
        PrintCentreO(String(Puissance) + "W", -1, 0, 2);
        int teta = 360 * Puissance / 5000;
        if (Puissance >= 0) {
          oled.fillArc(8, 8, 0, 7, -90, teta - 90, TFT_WHITE);
        } else {
          oled.fillArc(8, 8, 0, 7, teta - 90, -90, TFT_WHITE);
        }
        PrintDroiteO(String(int(100 -retard_min )) + "%", oled.width(), 8, 1);
      }
      else {
        MakeOLED();           // HP EVSE - there is an EVSE screen, put the EVSE info instead
      } 
    }
    if (cptLEDyellow > 200) {
      if (ecran_type == 0){                     // HP EVSE if there is no screen selected for the EVSE - use the RMS screen as normal
        oled.fillRect(0, 0, oled.width(), 16, TFT_BLACK);
        PrintCentreO("Puissance Inconnue", -1, 0, 2);
      }
      else{                                     // HP EVSE - EVSE screen present
        oled.drawString("P: Inconnue", 2, 0);   // HP EVSE - case not tested
      }
    }
  }
}

void PrintScroll(String m) {
  TelnetPrintln( m);
  if (LEDgroupe >= 10 && ecran_type == 0) { // ESP_ARD MC HP EVSE there is no EVSE screen - scroll RMS message otherwise do nothing
    oled.setTextSize(1);
    oled.setTextScroll(true);
    oled.println(Ascii(m));
    oled.setTextScroll(false);
  }
  if (ESP32_Type == 4 && NumPage == 3) {  //Ecran LCD présent
    lcd.setTextSize(1);
    lcd.setTextScroll(true);
    lcd.println(Ascii(m));
    lcd.setTextScroll(false);
  }
}
void PrintCentreO(String S, int X, int Y, float Sz) {
  if (X < 0) X = oled.width() / 2;
  oled.setTextSize(Sz);
  int W = oled.textWidth(S);
  oled.drawString(S, X - W / 2, Y);
}
void PrintDroiteO(String S, int X, int Y, float Sz) {
  oled.setTextSize(Sz);
  int W = oled.textWidth(S);
  oled.drawString(S, X - W - 4, Y);
}
// ESP_ARD MC ARDUINO_Uno et ESP32
void Clignote(String p, int x, int y) {
  size_t w = sprite.textWidth(p);
  if (blinkScreen == true) {
    sprite.fillRect(x-1, y-1, w, 9, TFT_BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.drawString(p, x, y); 
  }
  else {
    sprite.fillRect(x-1, y-1, w, 9, TFT_BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    //sprite.drawString(p, x, y);
  }
  blinkScreen = !blinkScreen;  
}
void MakeOLED(){  // HP EVSE new procedure fro EVSE OLED screen
  //                 HP EVSE build the screen in background as sprite and then load up in one go
  sprite.fillScreen(TFT_BLACK); // clear screen
  sprite.setTextColor(TFT_WHITE);  
  sprite.setTextSize(1);
  sprite.drawFastHLine (0, 9, 128, TFT_WHITE);    // separation horizontal line top
  sprite.drawFastHLine (0, 30, 104, TFT_WHITE);   // separation horizontal line middle
  sprite.drawFastVLine (104, 10, 54, TFT_WHITE);  // vertical separation line
  sprite.drawFastVLine (47, 53, 11, TFT_WHITE);   // vertical line Etat box
  sprite.drawFastHLine (0, 53, 47, TFT_WHITE);    // horizontal line Etat box
  //int Puissance = PuissanceS_M - PuissanceI_M;
  int Puissance = PuissanceI_M - PuissanceS_M;    // MC Valeur inversée si positif production et négatif je consomme
  String ToPrint;
  sprite.drawString("P:" + String(Puissance) + "W", 2, 0);  // show power
  ToPrint = "PWM:" + String(duty1,1) + "%";
  sprite.drawString(ToPrint, 127-ToPrint.length()*6, 0);    // show pwm
  sprite.drawString("Charge:", 0, 17);                      // show charge 
  sprite.setTextSize(2);                                    // show value in size 2
#ifndef ARDUINO_Uno                  // MC pour géré version ESP32 & Arduino uno
  RelaisChargeOn = digitalRead(PIN5);
#endif  // MC ARDUINO_Uno
  //ToPrint = (RelaisChargeOn? String(I_charge,1) + "A" : "0.0A"); // MC pour afficher suivant état du relais, ESP32 & Arduino Uno //if relay not closed value = 0A
  ToPrint = String(I_charge,1) + "A"; // MC pour afficher suivant état du relais, ESP32 & Arduino Uno //if relay not closed value = 0A
  sprite.drawString(ToPrint, 103- ToPrint.length()*12, 13);
  sprite.setTextSize(1);
  sprite.drawString("Energie Wh Limite", 0, 35);            // show energy and limit
  ToPrint = String(EnergieCharge_Wh);
  sprite.drawString(ToPrint, 42-ToPrint.length()*6, 44);
  ToPrint = (maxWhInput == 0? "-" : String(maxWhInput));
  sprite.drawString(ToPrint, 102-ToPrint.length()*6, 44);
  String iMode = (modeAuto == 0) ? "AUT" : (modeAuto == 1) ? "SMI" : (modeAuto == 2 ? "MAN" : "ARR"); //MC
  if (modeAuto == 3) {                              // MC
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.drawString(iMode, 109, 13);
  } else {
    sprite.fillRect(108, 12, 19, 9, TFT_WHITE);
    sprite.setTextColor(TFT_BLACK, TFT_WHITE);
    sprite.drawString(iMode, 109, 13);
  }
  /*if (modeAuto == 0) {  // AUT display
    sprite.fillRect(108, 12, 19, 9, TFT_WHITE);
    sprite.setTextColor(TFT_BLACK, TFT_WHITE);
    sprite.drawString("AUT", 109, 13);
  }
  else{
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.drawString("AUT", 109, 13);
  }*/
  //if (enab == 1 && modeAuto == 0) {  // SOL display (control in routeur mode)
  if (enabIntHC == 0) {                                   // Pas HP/HC à gérer
    if (enab == 1 && (modeAuto == 0 || modeAuto == 1)) {  // MC SOL display (control in routeur mode)
      //Clignote("SOL", 109, 41);
      sprite.fillRect(108, 26, 19, 9, TFT_WHITE);
      sprite.setTextColor(TFT_BLACK, TFT_WHITE);
      sprite.drawString("SOL", 109, 27);  
    } 
    else {
      sprite.setTextColor(TFT_WHITE, TFT_BLACK);
      sprite.drawString("SOL", 109, 27);  
    }
  } else {
    ToPrint = (enabIntHC == 1 ? "HP " : "HC ");             // MC EnabIntHC: 0=Pas HP/HC 1=HP, 2=HC
    if (modeAuto == 0 || modeAuto == 1) {                   // MC SOL display (control in routeur mode)
      sprite.fillRect(108, 26, 19, 9, TFT_WHITE);
      sprite.setTextColor(TFT_BLACK, TFT_WHITE);
      sprite.drawString(ToPrint, 109, 27);  
    } 
    else {
      sprite.setTextColor(TFT_WHITE, TFT_BLACK);
      sprite.drawString(ToPrint, 109, 27);  
    }
  }
  if (modeAuto == 2 || (modeAuto == 0 && enabIntHC == 2 && enab == 0)) {  // MC ON display, manual mode or auto on, EnabIntHC: 0=Pas HP/HC 1=HP, 2=HC
    //Clignote("ON ", 109, 41);
    sprite.fillRect(108, 40, 19, 9, TFT_WHITE);
    sprite.setTextColor(TFT_BLACK, TFT_WHITE);
    sprite.drawString("ON ", 109, 41);   
  }
  else {
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.drawString("ON ", 109, 41);
  }
  if (modeAuto == 3 || (enabIntHC == 1 && modeAuto == 0 && enab ==0)) {  // MC OFF display, manual mode or auto off
    sprite.fillRect(108, 54, 19, 9, TFT_WHITE);
    sprite.setTextColor(TFT_BLACK, TFT_WHITE);
    sprite.drawString("OFF", 109, 55);
  }
  else{
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.drawString("OFF", 109, 55);
  }
  if (maxWhAtteinte == true){   // Charged at the limit
    sprite.fillRect(82, 55, 19, 9, TFT_WHITE);
    sprite.setTextColor(TFT_BLACK, TFT_WHITE);
    sprite.drawString("FIN", 83, 56);
  }
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.drawString("Etat: " + String(state), 2, 57); // State
  ToPrint = (modeAuto == 0) ? "~~  " : (modeAuto == 1) ? ">=" + String((int)I_charge_manual) + "~" : (modeAuto == 2 ? "=   " : "x   ");     // MC affichage caractère suivant modeAuto
  sprite.setTextSize(1);
  // MC pour géré la version Arduino Uno
  //if (RelaisChargeOn && blinkState){ // MC pour géré version ESP32 & Arduino uno  HP EVSE blink charging sign if relay closed
  if (RelaisChargeOn){ // MC pour géré version ESP32 & Arduino uno  HP EVSE blink charging sign if relay closed
    Clignote(ToPrint, 56, 57);
    //sprite.drawString(ToPrint, 56, 57);                                                            // MC affichage caractère suivant le modeAuto
  } else {
    if (enabIntHC == 1) {
      sprite.drawString("....", 56, 57);
    } else {
      sprite.drawString(ToPrint, 56, 57);   
    }                                                          // MC affichage caractère suivant le modeAuto
  }
  sprite.pushSprite (0,0); // push the design to the screen
}
// ESP_ARD MC ARDUINO_Uno et ESP32
