// Définir le numéro de la broche à laquelle la LED est connectée
const int ledPin = 9; // Broche de la LED
const int analogPinA0 = A0; // Broche d'entrée analogique pour l'intervalle
const int analogPinA1 = A1; // Broche d'entrée analogique pour le potentiomètre

// Variables pour stocker les valeurs de longueur et de fréquence d'impulsion
int onTime = 100; // Longueur d'impulsion par défaut
int interval = 1000; // Intervalle par défaut

void setup() {
  // Initialiser la broche de la LED comme une sortie
  pinMode(ledPin, OUTPUT);
  
  // Initialiser la communication série
  Serial.begin(9600); // Vitesse de communication série
}

void loop() {
  // Lire la valeur de l'entrée analogique A0 (0 à 1023)
  int sensorValueA0 = analogRead(analogPinA0);
  
  // Mapper la valeur lue de A0 (0-1023) à un intervalle de temps (1500-10 ms)
  interval = map(sensorValueA0, 0, 1023, 1500, 10); // Inverser l'intervalle

  // Lire la valeur du potentiomètre sur A1 (0 à 1023)
  int potValue = analogRead(analogPinA1);
  
  // Mapper la valeur du potentiomètre à un temps d'allumage (10-800 ms)
  onTime = map(potValue, 0, 1023, 10, 800); // Mapper à la plage de temps

  // Mapper la valeur de A0 à une sortie PWM (0-255)
  int pwmValue = map(sensorValueA0, 0, 1023, 0, 255); // Mapper à la plage PWM

  // Écrire la valeur PWM sur la broche D9
  analogWrite(ledPin, pwmValue);
  
  // Allumer la LED
  digitalWrite(ledPin, HIGH);
  // Maintenir la LED allumée pendant le temps déterminé par le potentiomètre
  delay(onTime);
  
  // Éteindre la LED
  digitalWrite(ledPin, LOW);
  // Attendre l'intervalle déterminé par la lecture analogique A0
  delay(interval);

  // Calculer la fréquence d'impulsion (en Hz)
  float pulseFrequency = 1000.0 / (onTime + interval); // Fréquence en Hz

  // Afficher la longueur d'impulsion et la fréquence d'impulsion
  Serial.print("Longueur d'impulsion: ");
  Serial.print(onTime);
  Serial.print(" ms, Fréquence d'impulsion: ");
  Serial.print(pulseFrequency);
  Serial.println(" Hz");
}