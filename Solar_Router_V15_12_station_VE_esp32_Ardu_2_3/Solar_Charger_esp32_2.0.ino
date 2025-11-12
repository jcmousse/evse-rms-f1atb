#ifndef ARDUINO_Uno     // MC gestion version ESP32-VE / Arduino Uno
// Ce code pilote la station de charge pour EV à partir des informations transmises par le routeur solaire RMS F1ATB.
// Il est destiné à être intégré au code original du RMS
// Il est conçu pour ESP32 + carte Station_VE instrumentée (voir documentation)
// D'après le code original de Pedro Neves (PowerCircuits) adapté pour ce matériel et pour le code RMS F1ATB

// GNU Affero General Public License (AGPL) / AGPL-3.0-or-later
// Copie, diffusion, modifications, redistributions sous la même licence

// Version 2.0 du 20251017 d'après base Arduino Solar_Charger_2.0.ino
#define version "Solar_Charger_esp32_2.3"

//################ DEBUT declarations specifiques à la station de charge VE ###############
#define CP_OUT 27        // Setting pin 27 as output - Control Pilot signal generator
#define PWM_CHANNEL 0    // Canal PWM (0 à 15)
#define PWM_FREQ 1000    // Fréquence : 1 kHz
#define PWM_RES 8        // Résolution : 8 bits (0-255)
#define CP_IN 36         // Setting pin 36 as input - Control Pilot signal reader
//#define PIN1 21          // top led     // HP EVSE passed into main
//#define PIN2 22          // middle up   // HP EVSE passed into main
//#define PIN3 23          // middle down // HP EVSE passed into main
//#define PIN4 25          // bottom led  // HP EVSE passed into main - now used as alarm led
//#define PIN5 13          // relay       // HP EVSE passed into main
//################ FIN declarations specifiques à la station de charge VE ###############

//################ DEBUT Variables specifiques à la station de charge VE ###############
int counter2 = 0; // Initialize counter2 for LED blinking when charging
int counter3 = 0; // Initialize counter3 for charging overpower delay

// cp pwm values
float cp_pwm = 255; // Initialize the PWM value for the Control Pilot signal generator (255 = flat positive (+12V) signal)
//int cp_pwm_charging; // pseudo pid output - sets the charging current by pwm //var RMS
int cp_pwm_suspend = 255; // pour suspendre la charge
float pwm_c; // pseudo pid output - sets the charging current by pwm

// voltage detection
//int findPeakVoltage();  // Function prototype for finding the peak voltage read by pin 36 and determine the charging state (A, B, C, F)
int peak_voltage = 0;     // Initialize the peak voltage read by pin 36
int current_voltage = 0;  // Initialize the current voltage read by pin 36
int A_voltage = 2800;     // HP EVSE new value for ESP
int B_voltage = 2400;     // HP EVSE new value for ESP
int C_voltage = 2050;     // HP EVSE new value for ESP

// charge driving
int charging = 0; // Initialize the charging state (0 = not charging, 1 = charging)

// ======= Variables du RMS (déplacées dans le code principal) =====================
// Values sent by the solar router : power and state
// int PuissanceI_M; //Power surplus - [W] //var RMS
// int PuissanceS_M; //Power drawn from the grid (consumption) - [W] //var RMS
// bool enab; // state of the solar router relay - enables/disables the charging process //var RMS
// bool enab = digitalRead(gpioEnabVE);
// bool modeAuto = true; // MC gérer le mode auto, Semi-auto ou manuel  //var RMS


// Electrical values
float P_excedent; // Available power W (instant)
float P_excedent_avg; // Available power W (running average)
float I_excedent; // Available current (instant) - charging current target - A

// Initializing averaging available power values (running average)
const int N_SAMPLES = 2; // number of samples in running average
float buffer_s[N_SAMPLES];
int index_s = 0;
int count_s = 0; // Number of stored samples
float somme_s = 0.0;   // HP EVSE added decimal point
float moyenne_s = 0.0; // HP EVSE added decimal point

// === TIMING ===
//unsigned long previousVEMillis = 0; // HP EVSE declaration passed in main 

// === CURRENT CONTROL ===
//float I_charge = 0; //var RMS
//float I_charge_manual = 0;  // MC Gérer le mode manuel et la valeur de I_charge //var RMS

/* // Variables deplacées dans le prog principal solar_router
//=====================================
// Réglages de la station de charge // EVSE settings
// Ces valeurs peuvent être modifiées en cas de besoin // can be modified if needed
// Ne modifier qu'en connaissance de cause ! // know what you do ;)
//=====================================
const float I_min_c = 6.0; // courant de charge minimum en A. En principe 6A, évent. 5A (défaut = 6)
const float I_max = 12.0; // courant de charge maximum en A ; 10 < I_max < 32 ; selon installation et protections !!
//const float U_reseau = 235.0; // Tension efficace réelle du réseau en V //var RMS
const float P_charge_min = 1300; // Puissance mini (surplus) en W pour le démarrage de la charge ; 1200 < P_charge_min < 1500 (défaut = 1300)
const float P_charge_max = -500; // Puissance maxi (soutirée) en W à ne pas dépasser ; -500 < P_charge_max < -300 (défaut = -500)
int t_depassement = 15; // temps autorisé de dépassement de la puissance P_charge_max (approx. s)
unsigned long t_delay_boucle = 1000;   // délai d'attente entre le traitement de production/consommation ; 0 <= t_delay_boucle < nnnn avec nnnn en ms, (défaut = 1000 = 1s)

//################ FIN Variables specifiques à la station de charge VE ###############
*/

// ######### Setup functions ###########
// function - setup solar charger
void setup_solar_charger() {
 //initialize buffer for averaging available power values (P_excedent_avg)
  for (int k = 0; k < N_SAMPLES; k++) {
    buffer_s[k] = 0;
  }
  // Set up the PWM output (CP signal value)
  bool success = ledcAttachChannel(CP_OUT, PWM_FREQ, PWM_RES, PWM_CHANNEL); // gpio 27, freq 1000, res. 8, ch.0
  if(success) {
    ledcWrite(CP_OUT, 255); // continous +12V on CP
  }
  // Set up the LED pins as output
  pinMode(PIN1, OUTPUT); // top led
  pinMode(PIN2, OUTPUT); // middle u
  pinMode(PIN3, OUTPUT); // middle d
  pinMode(PIN4, OUTPUT); // bottom led
  // Set the relay pin as output
  pinMode(PIN5, OUTPUT); // relay
  // Initialize relay and LEDs states
  digitalWrite(PIN5, LOW); // Relays open
  digitalWrite(PIN1, HIGH); // Top LED on
  digitalWrite(PIN4, LOW); // Bottom LED off
  // set running timer
  timer_depassement = millis();
}
// ######### END SETUP ###########

// ######### LOOP FUNCTIONS ###########
//****************
// function - Finds the peak voltage read by pin 27
int findPeakVoltage() {
  int i = 0;
  current_voltage = 0;
  peak_voltage = 0;
  while (i < 1000) {
    current_voltage = analogRead(CP_IN); // see if we can use ESP32 specific faster command
    if (current_voltage > peak_voltage) {
      peak_voltage = current_voltage;
    }
    i++;
  }
  return peak_voltage;
}
    //****************
// function - Current control main loop
void current_control_main_loop() {

  // === Step 1 : Compute electrical values ===
  P_excedent = PuissanceI_M - PuissanceS_M;  // Available power - signed - W
  I_excedent = P_excedent / U_reseau; // Available current - A
  
  // === Step 2 : Current control loop ===
  if (enab == 1){  
    // if (P_excedent >= P_charge_min) { // problem if a lot of excedents?
    //     I_charge = I_excedent;
    //     //counter3 = 0; // HP EVSE replaced with running timer below
    //     timer_depassement = millis();
    //     delay(10); }
    // if (charging == 1) {    
    //   if (P_excedent_avg < P_charge_min && P_excedent_avg >= 230) {
    if (I_charge == 0 && P_excedent >= P_charge_min) { // problem if a lot of excedents? // ajouter if (I_charge == 0){}
        I_charge = I_excedent;
        //counter3 = 0; // HP EVSE replaced with running timer below
        timer_depassement = millis();
        delay(10); }
    if (charging == 1) {    
      if  (P_excedent_avg >= 230) { // problem if a lot of excedents? // remove P_excedent_avg < P_charge_min ?
        I_charge = I_charge +0.3;
        //counter3 = 0; // HP EVSE replaced with running timer below
        timer_depassement = millis();
        delay(10); }
      else if (P_excedent_avg < 230 && P_excedent_avg >= 0) {
        I_charge = I_charge +0.05;
        //counter3 = 0; // HP EVSE replaced with running timer below
        timer_depassement = millis();
        delay(10); }
      else if (P_excedent_avg < 0 && P_excedent_avg >= -230) {
        I_charge = I_charge -0.05;
        //counter3 = 0; // HP EVSE replaced with running timer below
        timer_depassement = millis();
        delay(10); }
      else if (P_excedent_avg < -230 && P_excedent_avg >= P_charge_max) {
        I_charge = I_charge -0.3;
        //counter3 = 0; // HP EVSE replaced with running timer below
        timer_depassement = millis();
        delay(10); }
      else if (P_excedent_avg < P_charge_max) {
        I_charge = I_charge -0.5;
        // HP EVSE replaced by running timer
        //timercounter3++;
        //if (counter3 >= t_depassement) {
        //I_charge = 0;
        //}  
        if (millis() - timer_depassement > (t_depassement * 1000)) I_charge = 0;
      }
    }
  } else {
    I_charge = 0;
    if (enabIntHC == 1) {  // Gestion HP/HC active, mais HP
      I_charge = 0;
    } 
    else if (enabIntHC == 2) {  // Je passe en heure creuse
      I_charge = I_charge_manual;
    }
  }

  if (modeAuto == 1) {  // MC force le mode Semi-auto, permet de forcer I_charge à la valeur I_charge_manuel
    if (I_charge < I_charge_manual) I_charge = I_charge_manual;
  }

  if (modeAuto == 2) {  // MC force le mode manuel
    I_charge = I_charge_manual;
  }

  if (modeAuto == 3) {  // MC force le mode arrêt
    I_charge = 0;
  }

  if (maxWhInput > 0) {  // MC vérifie si Energie de charge a atteint la valeur maxWhInput, si oui on arrête la charge
    if (EnergieCharge_Wh >= maxWhInput) {
      I_charge = 0;
      maxWhAtteinte = true;
    }
  } 
  else if (maxWhInput == 0) {
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
  if (I_charge <= 0) { // HP EVSE changed to <= from ==, prevents negative values
    I_charge = 0;
    if (peak_voltage < A_voltage) {
    ledcWrite(CP_OUT, (cp_pwm_suspend)); // on suspend la charge
    duty1 = 100.0; // HP EVSE for display
    //digitalWrite(PIN5, LOW);              // Relays open // HP EVSE to be driven by vehicle
    //state = 'B'; // HP EVSE to be driven by vehicle
    //charging = 0;
    //counter3 = 0; // HP EVSE replaced by running timer
    }
    else if (peak_voltage >= A_voltage) {
    //ledcWrite(CP_OUT, 5); // on suspend la charge
    duty1 = 100.0;              // HP EVSE for display
    digitalWrite(PIN5, LOW);    // Relays open
    digitalWrite(PIN4, LOW);    // Bottom LED off
    digitalWrite(PIN2, LOW);    // Middle led off
    digitalWrite(PIN3, LOW);    // Middle led off
    state = 'A';
    charging = 0;
    }
  }
  else {
    duty1 = (I_charge / 0.6);        // standard EVSE // HP EVSE declared duty1 in main as used in display
    float pwm_c = duty1 * 255.0 / 100.0;   // Convert % to 8-bit PWM (0-255)
    cp_pwm_charging = pwm_c;              // convert pwm_c to int
    ledcWrite(CP_OUT, cp_pwm_charging);  // write pwm to the output //AR
  }
  // HP EVSE control loop delay moved to main loop 
  // adjustable processing loop delay     
  //unsigned long currentVEMillis = millis();
  //if (t_delay_boucle > 0) {
  // if (currentVEMillis - previousVEMillis >= t_delay_boucle) {
  // previousVEMillis = currentVEMillis;
  // }
} 


 //****************
// function - Drives the charger state
void setChargingState() {
  // Set the charging state based on the peak voltage
  
  if (peak_voltage > A_voltage) {
    //state A - disconnected
    state = 'A';
    charging = 0;
    I_charge = 0; 
    duty1 = 100.0;              // HP EVSE for display
    ledcWrite(CP_OUT, 255);     // +12V;  // CP set to fixed +12V //AR // HP EVSE - reinstated, necessary otherwise stays at previous value
    digitalWrite(PIN5, LOW);    // Relays open
    digitalWrite(PIN4, LOW);    // Bottom LED off
    digitalWrite(PIN2, LOW);    // Middle u led off
    digitalWrite(PIN3, LOW);    // Middle d led off
  }
    
  //if (I_charge > 0 ) {    // pour éviter en mode suspendu de changer cet état, mettre off le relais et état des leds // HP EVSE removed this
    else if (peak_voltage > B_voltage) { // HP EVSE added else
      //state B - connected (ready)
      state = 'B';
      charging = 0;
      //ledcWrite(CP_OUT, cp_pwm_suspend); // cp_pwm = 255 (pause) //AR
      digitalWrite(PIN5, LOW); // Relays open
      digitalWrite(PIN4, LOW); // HP EVSE reset alarm led
    }
    else if (peak_voltage > C_voltage) {
      //state C - charge
      state = 'C';
      charging = 1;
      //ledcWrite(CP_OUT, cp_pwm_charging); // Exanple 10 A -> Duty Cycle = 10/0.6 = 16.67% -> 255 * 0.1667 = 42.5 //AR
      digitalWrite(PIN5, HIGH); // Relays closed
      digitalWrite(PIN4, LOW); // HP EVSE reset alarm led
    }
    else {
      //state F - unknown/error
      state = 'F'; // CP set to fixed -12V
      charging = 0;
      ledcWrite(CP_OUT, 0);       //AR // HP EVSE only possibility to get out of this state is to reset
      digitalWrite(PIN5, LOW);    // Relays open
      digitalWrite(PIN4, HIGH);   // Bottom LED off // HP EVSE set alarm
      digitalWrite(PIN3, LOW);    // Middle d LED off
      digitalWrite(PIN2, LOW);    // Middle u LED off
    }
  //}
}


//****************
// function - led status display (charging state)
void display_charging_state() {
  if (charging == 1) {
      digitalWrite(PIN3, blinkState);   // HP EVSE use blinking signal
      digitalWrite(PIN2, !blinkState);  // HP EVSE alternating
  }
}
 
//****************
// function - led status display (pause state)
void display_pause_state() {
  if (charging == 0 && state == 'B') {  // HP EVSE changed to B from b
    digitalWrite(PIN3, blinkState);     // HP EVSE use blinking signal
    digitalWrite(PIN2, blinkState);     // HP EVSE at the same time    
  }
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
// function - Calcul de l'énergie délivrée // MC pour gérer le cumul de recharge
void calc_deliv_energy () {
  if (state == 'C') { // HP EVSE change to C from c
    unsigned long nowVe = millis();
    float deltaT = (nowVe - lastUpdateVe) / 1000.0;
    lastUpdateVe = nowVe;
    EnergieCharge_Wh += (I_charge * U_reseau * deltaT) / 3600.0;
  }
}  
 
//****************
// Debug
// function - Display key values
void display_values() {
  Serial.print(F("Router enable : "));  Serial.println(enab);
  Serial.print(F("Mode Auto  : "));  Serial.println(modeAuto);
  Serial.print(F("HC / HP enable : "));  Serial.println(enabIntHC);
  Serial.print(F("Power surplus : "));  Serial.print(PuissanceI_M);
  Serial.print(F("    Power counsumption : "));  Serial.print(PuissanceS_M);
  Serial.print(F("    P_excedent : "));  Serial.println(P_excedent);
  Serial.print(F("P_excedent (avg) : "));  Serial.println(P_excedent_avg);
  Serial.print(F("I_excedent : "));  Serial.print(I_excedent);
  Serial.print(F("    I_charge : "));  Serial.print(I_charge);
  Serial.print(F("    PWM : "));  Serial.println(cp_pwm_charging);
  //Serial.print("PWM Suspendre : "); Serial.println(cp_pwm_suspend);
  //Serial.print("Analog read : "); Serial.println(analogRead(CP_IN));
  Serial.print(F("Peak voltage : "));  Serial.print(peak_voltage);
  Serial.print(F("    Charging state : "));  Serial.print(state);
  Serial.print(F("    Relay state : "));  Serial.println(digitalRead(PIN5));
  Serial.print(F("I_charge_manuel : "));  Serial.print(I_charge_manual);    // MC mode auto/manuel
  Serial.print("   Timer: "); Serial.println (millis() - timer_depassement);
  Serial.print(F("Defauts F : "));  Serial.println(Nb_defaut);
  Serial.println(F("**********************************************"));
  
}

// ######### END LOOP ###########
#endif  // MC ARDUINO_Uno