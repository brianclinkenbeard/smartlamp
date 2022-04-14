#include <ntp-time.h>
#include <Grove_LCD_RGB_Backlight.h>
#include <MQTT.h>

rgb_lcd lcd;
NtpTime ntp;

const int DELAY = 1000;
const int LIGHT_PIN = A0;
const int SOUND_PIN = A1;
const int POTEN_PIN = A2; // potentiometer (rotary angle sensor)
const int MOTION_PIN = D7;

const int LIGHT_HIGH = 500; // too much light
const int LIGHT_LOW = 100; // not enough light

int light, poten, sound, motion;

bool occupied = false;
long last_occ = 0, last_action = 0;
float kwh = 0.0;

// inelegant
bool turnedoff = true, turnedon = true;

// MQTT
void callback(char* topic, byte* payload, unsigned int length);
byte server[] = { 192,168,0,1 };
MQTT client(server, 1883, callback);

void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    
    Serial.print("callback: ");
    Serial.println(p);

    delay(1000);
}

// for printing time
String hhmmss(unsigned long int now)  //format value as "hh:mm:ss"
{
   String hour = String::format("%02i", Time.hour(now));
   String minute = String::format("%02i",Time.minute(now));
   String second = String::format("%02i",Time.second(now));
   return hour + ":" + minute + ":" + second + " GMT";
};


void setup() {
    Serial.begin(9600);
    
    // ntp server
    ntp.start();
    
    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);
    // lcd.setRGB(0, 255, 0);
    lcd.print("test");
    
    // motion sensor
    pinMode(MOTION_PIN, INPUT);
    
    client.connect("photon");
}

void loop() {
    if (turnedon)
        kwh += 50.0 * (1.0 / (3600000 / DELAY));
        
    // reset power usage at beginning of each day local time
    unsigned long int now = ntp.now();
    if (Time.hour(now) == 16 && Time.minute(now) == 0 && Time.second(now) <= 2)
        kwh = 0.0;

    // print ntp time and power usage
    lcd.clear();
    lcd.print(hhmmss(now));
    lcd.setCursor(0,1);
    lcd.print("KWH: ");
    lcd.print(kwh);
    
    // read potentiometer
    poten = analogRead(POTEN_PIN);
    
    // read sound
    sound = 0;
    for (int i = 0; i < 32; i++)
        sound += analogRead(SOUND_PIN);
    sound >>= 5; // bitshift
    
    if (sound > poten) {
        occupied = true;
        Serial.print("(sound) room occupied at ");
        Serial.println(millis());
        last_occ = millis();
    }
    
    // TODO: read motion
    // if motion > thresh, occupied = true
    motion = digitalRead(MOTION_PIN);
    if (motion == 1) {
        occupied = true;
        Serial.print("(motion) room occupied at ");
        Serial.println(millis());
        last_occ = millis();
    }
    
    if (occupied && millis() > last_occ + 60000) {
        Serial.print("room no longer occupied at ");
        Serial.println(millis());
        occupied = false;
    }
    
    light = 0;
    for (int i = 0; i < 32; i++)
        light += analogRead(LIGHT_PIN);
    light >>= 5; // bitshift
    
    Serial.print("poten: ");
    Serial.print(poten);
    Serial.print("\tsound: ");
    Serial.print(sound);
    Serial.print("\tlight: ");
    Serial.print(light);
    Serial.print("\tmotion: ");
    Serial.println(motion);
    
    if (millis() > last_action + 10000) {
        last_action = millis();
        if (client.isConnected()) {
            // TODO: set lamp to current value
            // client.subscribe("device/currentValue");
            
            client.publish("photon/power", String(kwh));
    
            // NOTE: change "device" to MQTT dimmer ID
            if (turnedoff && occupied && light < LIGHT_LOW) {
                turnedon = true;
                turnedoff = false;
                Serial.println("switching lamp on...");
                client.publish("device/targetValue/set", "ON");
            } else if (turnedon && (!occupied || light > LIGHT_HIGH)) {
                turnedon = false;
                turnedoff = true;
                Serial.println("switching lamp off...");
                client.publish("device/targetValue/set", "OFF");
            }
        } else {
            Serial.println("failed to connect to broker");
        }
    }
    
    delay(DELAY);
}
