// Promicro ripoff ATTENTION elle ne boot pas si connectée directement en usbc au mac
// elles ont beoin de passer par un hub qui va filtrer le courant
// Les promicro sont vu comme des Leonardo
// -----------------------
// EXPLICATION DE GEMINI :
//
// Gemini  dit
// "C'est un grand classique du matériel électronique "low-cost" ou de conception ancienne. 
//Le problème que tu rencontres n'est pas logiciel, mais purement matériel.
//La plupart des cartes ESP32 et Arduino "clones" n'incluent pas deux petites résistances de 5.1 kΩ 
// (appelées résistances de rappel vers le bas ou pull-down) sur les lignes de configuration CC1 et CC2 du port USB-C.
// Le Mac (Source) : En USB-C natif, l'ordinateur attend de détecter ces résistances pour "négocier" l'envoi de courant.
// La Carte (Cible) : Sans ces résistances, le Mac considère que rien n'est branché et refuse d'envoyer du courant.
// Le Hub/Adaptateur : Il agit comme un intermédiaire qui force la sortie du courant, c'est pourquoi tes cartes s'allument quand tu passes par lui."
// -----------------------
// Sur Pro Micro : 17 = RX LED, 30 = TX LED
// sur les ripoff cheap d'Aliexpress il n'y a pas de builtin led
// il faut passer sur les pin Rx/Tx qui ont des leds.
//
const int rxLed = 17; 
const int txLed = 30;

void setup() {
  pinMode(rxLed, OUTPUT);
  pinMode(txLed, OUTPUT);
}

void loop() {
  // On allume RX, on éteint TX (LOW = Allumé sur bcp de clones)
  digitalWrite(rxLed, LOW); 
  digitalWrite(txLed, HIGH);
  delay(500);

  // On inverse
  digitalWrite(rxLed, HIGH);
  digitalWrite(txLed, LOW);
  delay(500);
}