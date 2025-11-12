// Ce programme pilote la station de charge pour EV à partir des informations transmises par le routeur solaire RMS F1ATB.
// Il est conçu pour l'ensemble Arduino Uno R3 + shield Power Circuits (Pedro Neves)
// Contient le code original de Pedro Neves (PowerCircuits) adapté

// GNU Affero General Public License (AGPL) / AGPL-3.0-or-later
// Copie, diffusion, modifications, redistributions sous la même licence

// Version 1.8.4 du 20250922
// uart pour la réception et pour l'émission
// affichage sur mini page web http://ip-du-rms/status
// ajout correction sur le courant de charge en cas de dépassement
// correction : etat 'S' devient 'B' pour charge suspendue/en attente
// correction consigne pwm pour state 'B' (= cp_pwm_suspend)
// preparation portage esp32 - mise en fonctions - ménage divers
// conversion tempo boucle en millis()
// ajout clignotement 2 leds state b
// ajout condition sur t_delay_boucle

// Version 1.9.1 du 01/10/2025 affichage sur la page web d'accueil et ajout gestion d'un mode auto et manuel
// Version 1.9.2 du 02/10/2025 ajout des modes mode Auto/Manuel/Arrêt
// Version 1.9.3 du 07/10/2025 ajout fonctionnement avec gestion de l'énergie cumulée et de l'énergie de charge maximum
// Version 1.9.3.1 du 13/10/2025 divers nettoyages avant portage vers esp32
// Version 2.0 du 1415/10/2025 gestion du slide de réglage de I_charge_manuel pour le mode manuel et quand dans Action on a la gestion des HC

#define Version "Solar_Charger_2."

#include <Arduino.h>
#include "PWM.h"
#include <stdint.h>

#define CP_OUT 10  // Setting Arduino pin 10 as output - Control Pilot signal generator
#define CP_IN A0   // Setting Arduino pin A0 as input - Control Pilot signal reader

int counter2 = 0;  // Initialize counter2 for LED blinking when charging
int counter3 = 0;  // Initialize counter3 for charging overpower delay

float cp_pwm = 255;    // Initialize the PWM value for the Control Pilot signal generator (255 = flat positive (+12V) signal)
int frequency = 1000;  // Initialize the frequency of the PWM signal (1 kHz)
// Initialize the PWM value for the Control Pilot signal generator when charging
// Example: Set to 10 A -> Duty Cycle = 10/0.6 = 16.67% -> 255 * 0.1667 = 42.5
// Example: Set t0 16 A -> Duty Cycle = 16/0.6 = 26.66% -> 255 * 0.266 = 68.3
//float cp_pwm_charging = 42; // (42 = 10 A)
int cp_pwm_charging;       // pseudo pid output - sets the charging current by pwm
int cp_pwm_suspend = 255;  // pour suspendre la charge
float pwm_c;               // pseudo pid output - sets the charging current by pwm
float duty1 = 0.0;         // géré le % pour affichage pwm

int findPeakVoltage();    // Function prototype for finding the peak voltage read by pin A0 and determine the charging state (A, B, C, F)
int peak_voltage = 0;     // Initialize the peak voltage read by pin A0
int current_voltage = 0;  // Initialize the current voltage read by pin A0

char state = 'A';  // Initialize the charging state (A, B, C, F)
int charging = 0;  // Initialize the charging state (0 = not charging, 1 = charging)

// Values sent by the solar router : power and state
int PuissanceI_M;  //Power surplus - [W]
int PuissanceS_M;  //Power drawn from the grid (consumption) - [W]
bool enab;         // state of the solar router relay - enables/disables the charging process
int enabIntHC;     // état de gestion des HC: 0=pas de charge, 1=HP pas de charge, 2=HC on charge

// Electrical values variables
float P_excedent;      // Available power W (instant)
float P_excedent_avg;  // Available power W (running average)
float I_excedent;      // Available current (instant) - charging current target - A

// Initializing averaging available power values (running average)
const int N_SAMPLES = 2;  // number of samples in running average
float buffer_s[N_SAMPLES];
int index_s = 0;
int count_s = 0;  // Number of stored samples
float somme_s = 0;
float moyenne_s = 0;

// === TIMING ===
unsigned long previousVEMillis = 0;

// === CURRENT CONTROL STATE ===
float I_charge = 0;
float I_charge_manual = 0;   // MC Gérer le mode manuel et la valeur de I_charge //RMS
int modeAuto = 0;            // MC gérer le mode 0=Auto, 1=Semi-auto, 2=Manuel et 3=Arrêt //RMS
int EnergieCharge_Wh = 0;    // MC gérer le cumul de charge //RMS
int maxWhInput = 0;          // MC Gérer la charge jusqu'à maxWhInput, si = 0 pas de limite géré //RMS
bool maxWhAtteinte = false;  // MC gérér atteinte de la charge demandé maxWhInput //RMS
int ecran_type = 0;          // MC géré si écran Oled et le type: 0= pas d'écran, 1=SSD1306, 2=SH110x
int Ecran_Type_Preced = 0;   // MC géré changement de type d'écran
bool initEcran = false;      // MC géré l'initialisation d'un écran si modification en cours
//=====================================
// Réglages de la station de charge
// Ces valeurs peuvent être modifiées en cas de besoin
// Ne modifier qu'en connaissance de cause !
//=====================================
float I_min_c = 6.0;                  // courant de charge minimum en A. En principe 6A, 5A permet d'étendre la plage de fonctionnement (défaut = 6)
float I_max = 12.0;                   // courant de charge maximum en A ; 10 < I_max < 32 ; selon installation et protections !!
const float U_reseau = 235.0;         // Tension efficace réelle du réseau en V
const float P_charge_min = 1300;      // Puissance mini (surplus) en W pour le démarrage de la charge ; 1200 < P_charge_min < 1500 (défaut = 1300)
const float P_charge_max = -500;      // Puissance maxi (soutirée) en W à ne pas dépasser ; -500 < P_charge_max < -300 (défaut = -500)
int t_depassement = 15;               // temps autorisé de dépassement de la puissance P_charge_max (approx. s)
unsigned long t_delay_boucle = 1000;  // délai d'attente sur le traitement de puissance disponible ; 0 <= t_delay_boucle < nnnn avec nnnn en ms, (défaut = 1000 = 1s)
//=====================================
// GENERAL SETUP UART disponible sur l'Arduino Uno
void setup() {
  // Set up the Serial port
  Serial.begin(115200);

  //initialize buffer for averaging available power values (P_excedent_avg)
  for (int k = 0; k < N_SAMPLES; k++) {
    buffer_s[k] = 0;
  }

  // Set up the PWM
  InitTimersSafe();
  bool success = SetPinFrequencySafe(CP_OUT, frequency);  // Set the frequency of the PWM signal to 1 kHz
  if (success) {
    pinMode(CP_OUT, OUTPUT);  // Set the CP_OUT pin as output
    pwmWrite(CP_OUT, 255);    // Set the CP_OUT pin to +12V
  }

  // Set up the LED pins as output
  pinMode(4, OUTPUT);  //middle led
  pinMode(6, OUTPUT);  //top led
  pinMode(2, OUTPUT);  //bottom led
  pinMode(3, OUTPUT);  //middle led

  // Set the Relay pin as output
  pinMode(16, OUTPUT);

  // Initialize Relay and LEDs states
  digitalWrite(16, LOW);  // Relays open
  digitalWrite(6, HIGH);  // Top LED on
  digitalWrite(2, LOW);   // Bottom LED off

// Initialize led screen
  if (initEcran == false && ecran_type > 0) {
    InitSSD();  // MC Initialise partie écran OLED
    initEcran = true;
    Ecran_Type_Preced = ecran_type;
  }
}

//=====================================
// MAIN LOOP
void loop() {
  // get the surplus and consumption power values from the solar router (uart) + send values
  getRMSValues();

  // Read the CP voltage peaks
  findPeakVoltage();

  // Set the charging state
  setChargingState();

  // led status display (charging state)
  display_charging_state();

  // led status display (pause state)
  display_pause_state();

  // Main current control
  current_control_main_loop();

  // Running average for available power
  average_available_power();

  // Send values to RMS via uart
  send_ardu_data();

  // Display key values
  display_values();

// Display values on screen
  if (initEcran == true && ecran_type > 0) {
    AfficheOled();  // MC affichage des données sur écran oled
  }
}

//=====================================
// Charge station functions
//=====================================

// function - led status display (charging state)
void display_charging_state() {
  if (charging == 1) {
    digitalWrite(2, HIGH);  // Bottom LED on
    counter2++;
    if (counter2 > 1) {
      counter2 = 0;
      // Blink the middle LEDs when charging
      if (digitalRead(3) == HIGH) {
        digitalWrite(3, LOW);   //middle led
        digitalWrite(4, HIGH);  //middle led
      } else {
        digitalWrite(3, HIGH);  //middle led
        digitalWrite(4, LOW);   //middle led
      }
    }
  }
}

//****************
// function - led status display (pause state)
void display_pause_state() {
  if (charging == 0 && state == 'B') {
    digitalWrite(2, HIGH);  // Bottom LED on
    counter2++;
    if (counter2 > 1) {
      counter2 = 0;
      // Blink the 2 middle LEDs when pause
      if (digitalRead(3) == HIGH) {
        digitalWrite(3, LOW);  //middle led
        digitalWrite(4, LOW);  //middle led
      } else {
        digitalWrite(3, HIGH);  //middle led
        digitalWrite(4, HIGH);  //middle led
      }
    }
  }
}

//****************
// function - Finds the peak voltage read by pin A0
int findPeakVoltage() {
  int i = 0;
  current_voltage = 0;
  peak_voltage = 0;
  while (i < 1000) {
    current_voltage = analogRead(CP_IN);
    if (current_voltage > peak_voltage) {
      peak_voltage = current_voltage;
    }
    i++;
  }
  return peak_voltage;
}

//****************
// function - Running average of the available power value
void average_available_power() {

  // Cancels older value from the sum
  somme_s -= buffer_s[index_s];
  // Stores new value in buffer
  buffer_s[index_s] = P_excedent;
  // Add new value to the sum
  somme_s += P_excedent;
  // Updates circular index
  index_s = (index_s + 1) % N_SAMPLES;
  // Increments counter if the buffer is not full
  if (count_s < N_SAMPLES) {
    count_s++;
  }

  // Computes average value
  moyenne_s = somme_s / count_s;
  P_excedent_avg = moyenne_s;
  delay(1);  // Short delay between readings
}

//****************
// function - Current control main loop
void current_control_main_loop() {

  // === Step 1 : Compute electrical values ===
  P_excedent = PuissanceI_M - PuissanceS_M;  // Available power - signed - W
  I_excedent = P_excedent / U_reseau;        // Available current - A

  // === Step 2 : Current control loop ===
  if (enab == 1) {
    if (P_excedent >= P_charge_min) {
      I_charge = I_excedent;
      counter3 = 0;
      delay(10);
    }
    if (charging == 1) {
      if (P_excedent_avg < P_charge_min && P_excedent_avg >= 230) {
        I_charge = I_charge + 0.3;
        counter3 = 0;
        delay(10);
      } else if (P_excedent_avg < 230 && P_excedent_avg >= 0) {
        I_charge = I_charge + 0.05;
        counter3 = 0;
        delay(10);
      } else if (P_excedent_avg < 0 && P_excedent_avg >= -230) {
        I_charge = I_charge - 0.05;
        counter3 = 0;
        delay(10);
      } else if (P_excedent_avg < -230 && P_excedent_avg >= P_charge_max) {
        I_charge = I_charge - 0.3;
        counter3 = 0;
        delay(10);
      } else if (P_excedent_avg < P_charge_max) {
        I_charge = I_charge - 0.5;
        counter3++;
        if (counter3 >= t_depassement) {
          I_charge = 0;
        }
      }
    }
  } else {
    I_charge = 0;
    if (enabIntHC == 1) {  // Gestion HP/HC active, mais HP
      I_charge = 0;
    } else if (enabIntHC == 2) {  // Je passe en heure creuse
      I_charge = I_charge_manual;
    }
  }

  if (modeAuto == 1) {  // MC force le mode 1=Semi-auto, permet de forcer I_charge à la valeur I_charge_manuel
    if (I_charge < I_charge_manual) I_charge = I_charge_manual;
  }

  if (modeAuto == 2) {  // MC force le mode 2=Manuel
    I_charge = I_charge_manual;
  }

  if (modeAuto == 3) {  // MC force le mode 3=Arrêt
    I_charge = 0;
  }

  if (maxWhInput > 0) {  // MC vérifie si Energie de charge a atteint la valeur maxWhInput, si oui on arrête la charge
    if (EnergieCharge_Wh >= maxWhInput) {
      I_charge = 0;
      maxWhAtteinte = true;
    }
  } else if (maxWhInput == 0) {
    maxWhAtteinte = false;
  }

  // Clamp
  if (I_charge > I_max) {
    I_charge = I_max;
  }
  if (I_charge > 0 && I_charge < I_min_c) {
    I_charge = I_min_c;
  }

  // === step 3 : EVSE PWM computing ===
  if (I_charge == 0) {
    if (peak_voltage < 970) {
      pwmWrite(CP_OUT, (cp_pwm_suspend));  // on suspend la charge
      digitalWrite(16, LOW);               // Relays open
      state = 'B';
      charging = 0;
      counter3 = 0;
    } else if (peak_voltage >= 970) {
      pwmWrite(CP_OUT, 255);  // on suspend la charge
      digitalWrite(16, LOW);  // Relays open
      digitalWrite(2, LOW);   // Bottom LED off
      digitalWrite(3, LOW);   // Middle led off
      digitalWrite(4, LOW);   // Middle led off
      state = 'A';
      charging = 0;
    }
  } else {
    duty1 = (I_charge / 0.6);             // standard EVSE
    float pwm_c = duty1 * 255.0 / 100.0;  // Convert % to 8-bit PWM (0-255)
    cp_pwm_charging = pwm_c;              // convert to pwm_c to int
    pwmWrite(CP_OUT, (cp_pwm_charging));  // write pwm to the output
  }

  // délai de boucle réglable
  unsigned long currentVEMillis = millis();
  if (t_delay_boucle > 0) {
    if (currentVEMillis - previousVEMillis >= t_delay_boucle) {
      previousVEMillis = currentVEMillis;
    }
  }
}

//****************
// function - Drives the charger state
void setChargingState() {
  // Set the charging state based on the peak voltage

  if (peak_voltage > 970) {
    //state A - disconnected
    state = 'A';
    charging = 0;
    digitalWrite(16, LOW);  // Relays open
    //pwmWrite(CP_OUT, 255);  // CP set to fixed +12V
  } else if (peak_voltage > 870) {
    //state B - connected (ready)
    state = 'B';
    charging = 0;
    digitalWrite(16, LOW);  // Relays open
    //pwmWrite(CP_OUT, cp_pwm_suspend); // cp_pwm = 255 (pause)
  } else if (peak_voltage > 780) {
    //state C - charge
    state = 'C';
    charging = 1;
    digitalWrite(16, HIGH);  // Relays closed
    //pwmWrite(CP_OUT, cp_pwm_charging); // Exanple 10 A -> Duty Cycle = 10/0.6 = 16.67% -> 255 * 0.1667 = 42.5
  } else {
    //state F - unknown/error
    state = 'F';  // CP set to fixed -12V
    charging = 0;
    pwmWrite(CP_OUT, 0);
    digitalWrite(16, LOW);  // Relays open
    digitalWrite(2, LOW);   // Bottom LED off
    digitalWrite(3, LOW);   // Middle d LED off
    digitalWrite(4, LOW);   // Middle u LED off
  }
}

//****************
// function - Get data from RMS
void getRMSValues() {
  String data = Serial.readString();
  //Serial.println(String(data)); //debug  - format PuissanceI_M;PuissanceS_M;enab
  int sep1 = data.indexOf(';');
  int sep2 = data.indexOf(';', sep1 + 1);
  int sep3 = data.indexOf(';', sep2 + 1);  // MC mode manuel
  int sep4 = data.indexOf(';', sep3 + 1);  // MC mode manuel
  int sep5 = data.indexOf(';', sep4 + 1);  // MC géré maxWhInput, pour une limite de puissance rechargée, si =0 pas de limite
  int sep6 = data.indexOf(';', sep5 + 1);  // MC géré EnergieCharge_Wh, pour cumul de charge
  int sep7 = data.indexOf(';', sep6 + 1);  // MC géré enabIntHC, pour gestion des HC
  int sep8 = data.indexOf(';', sep7 + 1);  // MC géré I_min_c, charge au minimum accepté
  int sep9 = data.indexOf(';', sep8 + 1);  // MC géré I_max, charge au maximum possible
  int sep10 = data.indexOf(';', sep9 + 1);  // MC géré ecranType, avec 0= pas géré, 1=SSD1306, 2=SH110x

  if (sep1 > 0 && sep2 > sep1 && sep3 > sep2 && sep4 > sep3 && sep5 > sep4 && sep6 >sep5 && sep7 > sep6 && sep8 > sep7 && sep9 > sep8 && sep10 > sep9) {
    PuissanceI_M = data.substring(0, sep1).toInt();
    PuissanceS_M = data.substring(sep1 + 1, sep2).toInt();
    //int boolVal = data.substring(sep2 + 1).toInt();
    int boolVal = data.substring(sep2 + 1, sep3).toInt();        // MC mode manuel
    enab = (boolVal == 1);
    I_charge_manual = data.substring(sep3 + 1, sep4).toFloat();  // MC mode manuel
    boolVal = data.substring(sep4 + 1, sep5).toInt();            // MC mode manuel
    modeAuto = boolVal;                                          // MC mode 0=Auto, 1=Semi-auto, 2=manuel, 3=Arrêt
    maxWhInput = data.substring(sep5 + 1, sep6).toInt();         // MC maxWhInput, pour limite de puissance rechargée, si =0 pas de limite
    EnergieCharge_Wh = data.substring(sep6 + 1, sep7).toInt();   // MC EnergieCharge_Wh, pour cumul de charge
    enabIntHC = data.substring(sep7 + 1, sep8).toInt();          // MC géré HP/HC avec 0=pas géré, 1=actif mais HP pas de charge, 2=Actif mais HC on charge

    I_min_c = data.substring(sep8 + 1, sep9).toFloat();          // MC I_min_c courant minimum
    I_max = data.substring(sep9 + 1, sep10).toFloat();           // MC I_max courant maximum
    ecran_type = data.substring(sep10 + 1).toInt();              // MC géré ecrantype OLED avec 0=pas géré, 1=SSD1306, 2=SH110x
    if (initEcran == true && Ecran_Type_Preced != ecran_type) {
      initEcran = false;
      if (ecran_type == 0) EteindreEcran();                      // MC géré le cas où change dans paramètre borne VE et on met pas d'écran, permet d'arrêter l'écran
    }
    if (initEcran == false && ecran_type > 0) {
      InitSSD();
      initEcran = true;     // Ne le fait qu'une seule fois
      Ecran_Type_Preced = ecran_type;
    }
  }
}

//****************
// function --- ENVOI vers ESP32 --- // Send data to RMS
void send_ardu_data() {
  int etatRelais = digitalRead(16);
  int checksum = (int)(I_charge * 100) + state + (int)(duty1 * 100);
  //int checksum = (int)(I_charge * 100) + state + cp_pwm_charging;
  Serial.print("<");
  Serial.print(I_charge, 2);
  Serial.print(";");
  Serial.print(state);
  Serial.print(";");
  Serial.print(duty1, 2);
  Serial.print(";");
  Serial.print(checksum);
  Serial.print(";");
  Serial.print(String(etatRelais));
  Serial.print(";");
  Serial.print(I_charge_manual);
  Serial.print(";");
  Serial.print(String(modeAuto));
  Serial.print(";");
  Serial.print(String(maxWhInput));
  Serial.print(";");
  Serial.print(String(EnergieCharge_Wh));
  Serial.print(";");
  Serial.print(String(enabIntHC));
  Serial.print(";");
  Serial.print(String(I_min_c));
  Serial.print(";");
  Serial.print(String(I_max));
  Serial.print(";");
  Serial.print(String(ecran_type));
  Serial.println(">");
  delay(500);
}

//****************
// Debug
// function - Display key values
void display_values() {
  Serial.print(F("Router enable : "));
  Serial.println(enab);
  Serial.print(F("Power surplus : "));
  Serial.print(PuissanceI_M);
  Serial.print(F("    Power counsumption : "));
  Serial.print(PuissanceS_M);
  Serial.print(F("    P_excedent : "));
  Serial.println(P_excedent);
  Serial.print(F("P_excedent (avg) : "));
  Serial.println(P_excedent_avg);
  Serial.print(F("I_excedent : "));
  Serial.print(I_excedent);
  Serial.print(F("    I_charge : "));
  Serial.print(I_charge);
  Serial.print(F("    PWM : "));
  Serial.println(cp_pwm_charging);
  //Serial.print("PWM Suspendre : "); Serial.println(cp_pwm_suspend);
  //Serial.print("Analog read (A0) : "); Serial.println(analogRead(CP_IN));
  Serial.print(F("Peak voltage : "));
  Serial.print(peak_voltage);
  Serial.print(F("    Charging state : "));
  Serial.print(state);
  Serial.print(F("    Relay state : "));
  Serial.println(digitalRead(16));
  Serial.print(F("I_charge_manuel : "));
  Serial.print(I_charge_manual);  // MC mode auto/manuel
  Serial.print(F("    modeAuto : "));
  Serial.print(String(modeAuto));  // MC mode auto/manuel
  Serial.print(F("  Cumul : "));
  Serial.print(String(EnergieCharge_Wh));  // MC EnergieCharge_Wh
  Serial.print(F("  Limite charge : "));
  Serial.print(String(maxWhInput));  // MC maxWhInput
  Serial.print(F("  HP/HC : "));
  Serial.println(String(enabIntHC));  // MC maxWhInput
  Serial.print(F("Counter3 : "));
  Serial.println(counter3);
  Serial.print(F("I min : "));
  Serial.print(String(I_min_c));  // MC I_min_c
  Serial.print(F("   I max : "));
  Serial.print(String(I_max));  // MC I_max
  Serial.print(F("   Ecran type : "));
  Serial.println(String(ecran_type));  // MC ecran_type
  Serial.println(F("**********************************************"));
}
