int charCount; // status of input reading
int incomingByte; // for incoming serial data
char command[3]; // the command buffer

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  command[2] = 0;
  charCount = -1;
}

/*
 * read Serial input until ENTER and compose a command from [channel 1-4][0=off or 1=on]
 * return true if valid command in command array
 * to be called in loop()
 */
bool checkCommand(void)
{
  if (charCount < 0)
  {
    Serial.printf("Enter a command [1-4][0-1]:");
    Serial.readStringUntil('\n'); // eliminate rest in input buffer
    charCount = 0;
  }
  if (Serial.available() > 0) {
    incomingByte = Serial.read();
    if ((charCount == 2) && (incomingByte == '\n')) // valid command
    {
      charCount = -1;
      return true;
    }
    else if ((charCount == 0) && ((incomingByte >= '1') && (incomingByte <= '4'))) // valid 1st letter
    {
      command[0] = incomingByte;
      charCount++;
    }
    else if ((charCount == 1) && ((incomingByte >= '0') && (incomingByte <= '1'))) // valid 2nd letter
    {
      command[1] = incomingByte;
      charCount++;
    }
    else  // invalid condition
    {
      charCount = -1;
      Serial.printf("Invalid input!\n\n");
    }
  }
  return false; // in any other case than final valid command
}

void loop() {
  // put your main code here, to run repeatedly:
  if (checkCommand())
    Serial.printf("*%s*\n", command);
}
