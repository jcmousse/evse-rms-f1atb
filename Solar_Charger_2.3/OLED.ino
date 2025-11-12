// Utiliser un afficheur OLED sh110x avec un Arduino Uno
// https://tutoduino.fr/
// https://riton-duino.blogspot.com/2019/11/afficheur-oled-ssd1306-comparons.html
// Copyleft 2020
// Librairie pour l'afficheur OLED
// https://github.com/greiman/SSD1306Ascii

#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

#define I2C_ADDRESS 0x3C
SSD1306AsciiWire oled;
unsigned long previousBlinkMillis = 0;
bool blinkState = false;
const unsigned long blinkInterval = 500;  // clignote toutes les 500 ms

void printCentered(const char *s, int y);
void printValue(int value, int x, int y);

void InitSSD() {
  Wire.begin();		
  if (ecran_type == 1) {
    oled.begin(&Adafruit128x64, 0x3C);  // SSD1306
  } else if (ecran_type == 2) {
    oled.begin(&SH1106_128x64, I2C_ADDRESS);  // SH110x
  }
  oled.clear();

  // Titre fixe
  oled.setFont(System5x7);
  if (ecran_type == 2) {
    printCentered("Chargeur VE RMS",0);
    printCentered("---------------------",1);
  } else if (ecran_type == 1) {
    printCentered("Chargeur VE RMS",2);
    printCentered("---------------------",3);
  }
}
void EteindreEcran() {
  oled.clear();
  oled.displayRemap(0); // s’assure que tout est noir
  oled.ssd1306WriteCmd(0xAE);
  //Serial.println(F("Ecran eteint"));
}
void Clignote(const char *p) {
  if (blinkState) {
    oled.print(p);
  } else {
    // Efface exactement la même longueur
    for (uint8_t i = 0; i < strlen(p); i++) oled.print(" ");
  }
}
void AfficheOled(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousBlinkMillis >= blinkInterval) {
    previousBlinkMillis = currentMillis;
    blinkState = !blinkState;
  }
  if (ecran_type == 2) {      // #ifdef _EcranSH110x
    // --- Ligne Production ---
    oled.setFont(System5x7);
    oled.setCursor(0,2);
    oled.clearToEOL();
    oled.print("Prod/Conso:");
    printValue(PuissanceI_M-PuissanceS_M, 100, 2);
    oled.print(" W");
    oled.setCursor(0,3);
    oled.clearToEOL();
    oled.print("Charge:");
    if (maxWhInput > 0) {
      printValue(EnergieCharge_Wh, 75, 3);
      oled.print("Wh-");
      printValue(maxWhInput, 122, 3);
      oled.print("W");
    } else {
      printValue(EnergieCharge_Wh, 100, 3);
      oled.print(" Wh");
    }

    // --- Intensité en GRAND au centre ---
    //oled.setFont(TimesNewRoman16); // police 16px
    //oled.setFont(TimesNewRoman16_bold);
    oled.setFont(Arial_bold_14);
    oled.setCursor(0,5);           // position centrale
    oled.clearToEOL();
    oled.print("I Charge: ");
    oled.print(I_charge,1);
    oled.print("A");
    oled.setCursor(110,5);
    if (modeAuto == 0) {          // Pour Afficher l'info Auto/Semi-auto/Manuel/Arrêt
      if (enabIntHC == 1) {           // Pas en heure creuse
        oled.setCursor(112,5);
        oled.print("Hp");             // Mode Auto, régulation suivant production
      } else if (enabIntHC == 2) {    // Je passe en heure creuse
        oled.setCursor(112,5);
        if (I_charge == 0) {
          oled.print("Hc");           // Mode Auto, régulation suivant production
        } else {
          Clignote("Hc");
        }
      } else {
        if (I_charge == 0) {
          oled.print("~");          // Mode Auto, régulation suivant production
        } else {
          Clignote("~");
        }
      }
    } else if (modeAuto == 1) {
      //oled.print(">");            // Mode Semi-auto, définir la valeur I minimum et variable si production plus
      Clignote(">~");               // Mode Semi-auto, définir la valeur I minimum et variable si production plus
    } else if (modeAuto == 2) {
      //oled.print("=");            // Mode Manuel, définir la valeur I
      Clignote("=");                // Mode Manuel, définir la valeur I
    } else {
      oled.print("x");              // Mode Arrêt
    }
    if (maxWhAtteinte == true){     // Charge a atteint la maximum demandé
      oled.setCursor(113,5);
      oled.print("fin");
    }
    // --- PWM en bas ---
    oled.setFont(System5x7);
    oled.setCursor(0,7);
    oled.clearToEOL();
    oled.print("PWM: ");
    oled.print(duty1);
    oled.print(" %");

    // --- État de la borne ---
    oled.setCursor(90,7); // à droite du I
    //oled.clearToEOL();
    oled.print("Etat:");

    oled.print(state);
  } else if (ecran_type == 1) {     //#ifdef _EcranSSD1306
    // --- Intensité en GRAND au centre ---
    //oled.setFont(TimesNewRoman16); // police 16px
    //oled.setFont(TimesNewRoman16_bold);
    oled.setFont(Arial_bold_14);
    oled.setCursor(0,0);           // position centrale
    oled.clearToEOL();
    oled.print("I Charge: ");
    oled.print(I_charge,1);
    oled.print("A");
    oled.setCursor(110,0);
    if (modeAuto == 0) {              // Pour Afficher l'info Auto/Semi-auto/Manuel/Arrêt
      if (enabIntHC == 1) {           // Pas en heure creuse
        oled.setCursor(112,0);
        oled.print("Hp");             // Mode Auto, régulation suivant production
      } else if (enabIntHC == 2) {    // Je passe en heure creuse
        oled.setCursor(112,0);
        if (I_charge == 0) {
          oled.print("Hc");             // Mode Auto, régulation suivant production
        } else {
          Clignote("Hc");
        }
      } else {
        if (I_charge == 0) {
          oled.print("~");          // Mode Auto, régulation suivant production
        } else {
          Clignote("~");
        }
      }
    } else if (modeAuto == 1) {
      //oled.print(">");            // Mode Semi-auto, définir la valeur I minimum et variable si production plus
      Clignote(">~");               // Mode Semi-auto, définir la valeur I minimum et variable si production plus
    } else if (modeAuto == 2) {
      //oled.print("=");            // Mode Manuel, définir la valeur I
      Clignote("=");                // Mode Manuel, définir la valeur I
    } else {
      oled.print("x");              // Mode Arrêt
    }
    if (maxWhAtteinte == true){     // Charge a atteint la maximum demandé
      oled.setCursor(113,0);
      oled.print("fin");
    }

  // --- Ligne Production ---
    oled.setFont(System5x7);
    oled.setCursor(0,4);
    oled.clearToEOL();
    oled.print("Prod/Conso:");
    printValue(PuissanceI_M-PuissanceS_M, 100, 4);
    oled.print(" W");
    oled.setCursor(0,5);
    oled.clearToEOL();
    oled.print("Charge:");
    if (maxWhInput > 0) {
      printValue(EnergieCharge_Wh, 76, 5);
      oled.print("Wh-");
      printValue(maxWhInput, 122, 5);
      oled.print("W");
    } else {
      printValue(EnergieCharge_Wh, 100, 5);
      oled.print(" Wh");
    }
    // --- PWM en bas ---
    oled.setCursor(0,7);
    oled.clearToEOL();
    oled.print("PWM: ");
    oled.setFont(Arial_bold_14);
    oled.setCursor(35,6);
    oled.print(duty1);
    oled.print(" %");

      // --- État de la borne ---
    oled.setFont(System5x7);
    oled.setCursor(86,7); // à droite du I
    //oled.clearToEOL();
    oled.print("Etat:");
    oled.setFont(Arial_bold_14);
    oled.setCursor(118,6);
    oled.print(state);
  }
}

void printRight(const char *s, int y)
{
  size_t w;

  w = oled.strWidth(s);
  oled.setCursor(oled.displayWidth() - w, y);
  oled.print(s);
}
void printValue(int value, int x, int y)
{
  char s[10];
  size_t w;

  sprintf(s, "%d", value);
  w = oled.strWidth(s);
  oled.setCursor(x - w, y);
  oled.print(s);
}
void printCentered(const char *s, int y)
{
  size_t w;

  w = oled.strWidth(s);
  oled.setCursor((oled.displayWidth() - w) / 2, y);
  oled.print(s);
}

