#ifndef ultrasonic_h
#define ultrasonic_h

void setup_ultrasonic_pins(uint trigPin, uint echoPin);
uint64_t get_pulse_duration_us(uint trigPin, uint echoPin);
float microseconds_to_cm(uint64_t pulse_duration_us);
float microseconds_to_inches(uint64_t pulse_duration_us);
#endif