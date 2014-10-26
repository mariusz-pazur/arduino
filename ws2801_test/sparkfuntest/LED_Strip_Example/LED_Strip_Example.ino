int SDI = 2; //Red wire (not the red 5V wire!)
int CKI = 3; //Green wire

#define STRIP_LENGTH 50  //32 LEDs on this strip
long strip_colors[4];

void setup() {
  pinMode(SDI, OUTPUT);
  pinMode(CKI, OUTPUT);
      
   //Pre-fill the color array with known values
  strip_colors[0] = 0xFF0000; //Bright Red
  strip_colors[1] = 0x00FF00; //Bright Green
  strip_colors[2] = 0x0000FF; //Bright Blue
  strip_colors[3] = 0xFFFFFF; //White
}

void loop() {  
  set_color(strip_colors[0]);   
  delay(1000);
  set_color(strip_colors[1]);   
  delay(1000);
  set_color(strip_colors[2]);   
  delay(1000);
  set_color(strip_colors[3]);   
  delay(1000);    
}



//Takes the current strip color array and pushes it out
void set_color (long led_color) {
  //Each LED requires 24 bits of data
  //MSB: R7, R6, R5..., G7, G6..., B7, B6... B0 
  //Once the 24 bits have been delivered, the IC immediately relays these bits to its neighbor
  //Pulling the clock low for 500us or more causes the IC to post the data.

  for(int LED_number = 0 ; LED_number < STRIP_LENGTH ; LED_number++) {
    long this_led_color = led_color; //24 bits of color data

    for(byte color_bit = 23 ; color_bit != 255 ; color_bit--) {
      //Feed color bit 23 first (red data MSB)
      
      digitalWrite(CKI, LOW); //Only change data when clock is low
      
      long mask = 1L << color_bit;
      //The 1'L' forces the 1 to start as a 32 bit number, otherwise it defaults to 16-bit.
      
      if(this_led_color & mask) 
        digitalWrite(SDI, HIGH);
      else
        digitalWrite(SDI, LOW);
  
      digitalWrite(CKI, HIGH); //Data is latched when clock goes high
    }
  }

  //Pull clock low to put strip into reset/post mode
  //digitalWrite(CKI, LOW);
  //delayMicroseconds(5000); //Wait for 500us to go into reset
}
