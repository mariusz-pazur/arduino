int led1 = 2;
int led2 = 3;
int led3 = 4;

void setup() 
{                
  pinMode(led1, OUTPUT);     
  pinMode(led2, OUTPUT);     
  pinMode(led3, OUTPUT);  
  Serial.begin(19200);  
}

// the loop routine runs over and over again forever:
void loop() {
  Serial.write("Czesc\r\n");
  digitalWrite(led1, HIGH); 
  digitalWrite(led2, LOW);
  digitalWrite(led3, LOW);
  delay(100);               
  digitalWrite(led1, LOW); 
  digitalWrite(led2, HIGH);
  digitalWrite(led3, LOW);
  delay(100);   
  digitalWrite(led1, LOW); 
  digitalWrite(led2, LOW);
  digitalWrite(led3, HIGH);
  delay(100);
}
