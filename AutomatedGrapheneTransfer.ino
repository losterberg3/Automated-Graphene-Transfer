#include <Ezo_uart.h>
#include <SoftwareSerial.h>

// conductivity probe pins
#define rx 2
#define tx 3

// tank pins
#define pump_pin 5
#define solenoid_pin 4
#define switch_pin 6

// open loop timer durations
#define cleaning_duration_min 15
#define etching_duration_min 60
#define draining_duration_min 1.5
#define drying_duration_hour 12
#define probe_frequency_hz 1

// conductivity probe cleansed threshold
#define conductivity_threshold 50

// custom state variable
enum State {
  INITIALIZING,
  DRAINING,
  FILLING,
  ETCHING,
  CLEANING,
  DRYING
};
State state = INITIALIZING; // initialize state variable

// conductivity probe communication
SoftwareSerial myserial(rx, tx);
Ezo_uart probe(myserial, "EC");

// conductivity readings from probe
float conductivity = 0;
float conductivity_check = 0;

// variables to store beginning of each process
unsigned long etching_timer = millis();
unsigned long draining_timer = millis();
unsigned long cleaning_timer = millis();
unsigned long drying_timer = millis();
unsigned long probe_timer = millis();

unsigned long cleaning_duration = 15UL * 60UL * 1000UL;
unsigned long etching_duration = 1UL * 15UL * 1000UL;
unsigned long draining_duration = 10UL * 60UL * 1000UL;
unsigned long drying_duration = 12UL * 60UL * 60UL * 1000UL;
unsigned long probe_duration = 1000.0/probe_frequency_hz;


// user designates open or closed loop mode
char mode;

int cleaning_counter = 0;
int cleaning_counter_threshold = 3;

void setup() {
  // pin setup  
  pinMode(solenoid_pin, OUTPUT); //solenoid pin
  digitalWrite(solenoid_pin, LOW); //make sure solenoid is closed
  
  pinMode(pump_pin, OUTPUT); //pump pin
  digitalWrite(pump_pin, LOW); //make sure pump is off

  pinMode(switch_pin, INPUT); //level switch pin

  // initialize probe timer
  probe_timer = millis();
  
  // communication setup
  Serial.begin(9600);
  myserial.begin(9600);
}

void loop() {
  
  if (state == INITIALIZING) {
    digitalWrite(pump_pin, LOW);
    digitalWrite(solenoid_pin, LOW);
    Serial.println("Press 'c' to start conductivity feedback process or 't' to start the time based process...");
    mode = '\0'; // Clear user input variable

    while(true) {
      if (Serial.available()) {
        mode = Serial.read(); // Read the input character
        if (mode == 'c') {
          while (Serial.available()) Serial.read(); // <-- Clear buffer
          Serial.println("Conductivity feedback process selected. Fill tank with APS and press enter to begin etching process. To go back, type 'b'...");
          while (Serial.available() == 0) {
            delay(50); //wait for a user input
          }
          String input = Serial.readStringUntil('\n');
          if (input != "b") {
            //state = ETCHING;
            etching_timer = millis();
            //break;
            //Serial.println("Time (ms) | Conductivity (uS/cm) | State");
            Serial.println("Conductivity feedback currently not available, please select 't'...");
          } else {
            Serial.println("Press 'c' to start conductivity feedback process or 't' to start the time based process...");
            mode = '\0'; // Clear user input variable
          }
        } else if (mode == 't') {
          while (Serial.available()) Serial.read(); // <-- Clear buffer
          Serial.println("Time based process selected. Fill tank with APS and press enter to begin etching process. To go back, type 'b'...");
          while (Serial.available() == 0) {
            delay(50); //wait for user input
          }
          String input = Serial.readStringUntil('\n');
          if (input != "b") {
            etching_timer = millis();
            state = ETCHING;
            Serial.println("Time (ms) | Conductivity (uS/cm) | State");
            break;
          } else {
            Serial.println("Press 'c' to start conductivity feedback process or 't' to start the time based process...");
            mode = '\0'; // Clear user input variable
          }
        } else {
          Serial.println("Unknown Input: Press 'c' to start conductivity feedback process or 't' to start the time based process...");
        }
      }
      delay(50);
    }
  }

  if (state == ETCHING) {
    //During etching, pump and solenoid off
    digitalWrite(pump_pin, LOW);
    digitalWrite(solenoid_pin, LOW);

    if (mode == 't') {
      // Wait 1 hour, reset timer, then start draining
      if (millis() > (etching_timer + etching_duration)) {
        Serial.println("Etching timer complete, please press enter to continue ...");
        while(true) {
          if (Serial.available()) {
            char input = Serial.read();
            if (true) {
              Serial.println("Starting Drain");
              break;
            }
          }
          delay(50);
        }
        draining_timer = millis();
        state = DRAINING;
      }
    }
  }

  if (state == DRAINING) {

    digitalWrite(pump_pin, LOW);
    digitalWrite(solenoid_pin, HIGH);

    if (millis() > (draining_timer + draining_duration)) {
      state = FILLING;
    } 

  }

  if (state == FILLING) {
    digitalWrite(pump_pin, HIGH);
    digitalWrite(solenoid_pin, LOW);

    if (digitalRead(switch_pin) == HIGH) {
      digitalWrite(pump_pin, LOW);
      cleaning_timer = millis();
      state = CLEANING;
    }
  }

  if (state == CLEANING) {
    digitalWrite(pump_pin, LOW);
    digitalWrite(solenoid_pin, LOW);

    if (millis() > (cleaning_timer + cleaning_duration)) {
      
      if (cleaning_counter >= cleaning_counter_threshold) { //CHANGE EVENTUALLLY
        state = DRYING;
        draining_timer = millis();
        drying_timer = millis();
        Serial.print("Graphene Cleaned! Final Conductivity Value: ");
        Serial.println(conductivity, 2);
        Serial.println("Drying...");
      } else {
        cleaning_counter++;
        state = DRAINING;
        draining_timer = millis();
      }
    }
  }

  if (state == DRYING) {
    // first drain tank
    if (millis() < (draining_timer + draining_duration)) {
      digitalWrite(pump_pin, LOW);
      digitalWrite(solenoid_pin, HIGH);
    } else if (millis() > (drying_timer + drying_duration)) {
      Serial.println("Done Drying! Substrates ready to be removed");
      state = INITIALIZING;
    } else {
      digitalWrite(pump_pin, LOW);
      digitalWrite(solenoid_pin, LOW);
    }

    // while (millis() < draining_timer + draining_duration) {
    //   digitalWrite(pump_pin, LOW);
    //   digitalWrite(solenoid_pin, HIGH);
    //   delay(50);
    // }
    // digitalWrite(pump_pin, LOW);
    // digitalWrite(solenoid_pin, LOW);
    // if (millis() > drying_timer + drying_duration) {
    //   Serial.println("Done Drying! Substrates ready to be removed");
    //   state = INITIALIZING;
    // }
  }
  
  // log conductivity, dont bother when drying
  if (millis() > (probe_timer + probe_duration)) {
    probe_timer = millis();
    if (probe.send_read() && state != DRYING) {
      while (myserial.available()) myserial.read();  // Clear any excess data
      conductivity_check = probe.get_reading();
      if (conductivity_check > 1.0) {
        conductivity = conductivity_check;
      }

      Serial.print(millis());
      Serial.print(", ");
      Serial.print(conductivity, 2);
      Serial.print(", ");
      switch (state) {
        case INITIALIZING: Serial.println("INITIALIZING"); break;
        case DRAINING: Serial.println("DRAINING"); break;
        case FILLING: Serial.println("FILLING"); break;
        case ETCHING: Serial.println("ETCHING"); break;
        case CLEANING: Serial.println("CLEANING"); break;
        case DRYING: Serial.println("DRYING"); break;
        default: Serial.println("UNKNOWN");
      }
    }
  }
  
  
  delay(50);

}
