// This is a rewrite of the LMNC sequencer code by Sam Battle
// Modifications to handle hardware changes, and
// for preferred coding style

// CC0 1.0 Rich Holmes 2020

#define DBG 0

// Hardware configuration

#define STEP1 2 // D2
#define STEP2 3 // D3
#define STEP3 4 // D4
#define STEP4 5 // D5
#define STEP5 6 // D6
#define STEP6 7 // D7
#define STEP7 8 // D8
#define STEP8 9 // D9
#define RESET 10 // D10
#define ZERO 11 // D11
#define FORW 12 // D12
#define BACK 13 // D13
#define ROTARY 14 // A0; read as analog
#define BUTTON1 10 // D10; same as RESET (same action)
#define BUTTON2 15 // A1; read as digital
#define BUTTON3 16 // A2; read as digital
#define BUTTON4 17 // A3; read as digital
#define BUTTON5 18 // A4; read as digital
#define BUTTON6 19 // A5; read as digital
#define BUTTON7 A6 // A6; must be read as analog
#define BUTTON8 A7 // A7; must be read as analog

#define GATEOFFTIME 1

int steps[] = {STEP1, STEP2, STEP3, STEP4, STEP5, STEP6, STEP7, STEP8};
int stepset = 0;
int buttons[] = {BUTTON1, BUTTON2, BUTTON3, BUTTON4, BUTTON5, BUTTON6, BUTTON7, BUTTON8};

unsigned int valRot = 0;

unsigned int valForw = 0;
unsigned int valZero = 0;
unsigned int valReset = 0;
unsigned int valBack = 0;
unsigned int old_valForw = 0;
unsigned int old_valZero = 0;
unsigned int old_valReset = 0;
unsigned int old_valBack = 0;

unsigned int valButton[] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned int old_valButton[] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned int dbc_valButton[] = {0, 0, 0, 0, 0, 0, 0, 0};

bool changed = false;

unsigned int seq_length = 8;

// stepOn is the step to turn on next, or 0 to turn off all steps

unsigned int stepOn = 0;
unsigned int old_stepOn = 0;

bool doNewGate = false;  // true if a new gate is to be generated
bool stepForward = true; // true if a forward or backward step is to be taken

// pattern control, patterns as suggested by CTorp

#define PAT_SINGLE 1  // single (1-2-3-4-5-6-7-8-1-2-3-4-5-6-7-8-)
#define PAT_INCROT 2  // inclusive rotate (1-2-3-4-5-6-7-8-8-7-6-5-4-3-2-1-)
#define PAT_EXCROT 3  // exclusive rotate (1-2-3-4-5-6-7-8-7-6-5-4-3-2-)
#define PAT_DOUBLE 4  // double (1-1-2-2-3-3-4-4-5-5-6-6-7-7-8-8-)
#define PAT_RANDOM 5  // random

unsigned int pattern = PAT_SINGLE;
unsigned int pat_dir = 1; // 1 when going forward within pattern, -1 when going backward
bool pat_first = true;  // true for first of pair, false for second

// debouncing
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 5;
unsigned new_debounce_state = 0;
unsigned old_debounce_state = 0;
unsigned delta = 9999;

#if DBG==1
unsigned int old_seq_length = 8;
unsigned int old_pattern = PAT_SINGLE;
#endif

void setup ()
{
#if DBG==1
  Serial.begin(9600);  // for Arduino Uno / Pro Mini
#endif
  pinMode (STEP1, OUTPUT);
  pinMode (STEP2, OUTPUT);
  pinMode (STEP3, OUTPUT);
  pinMode (STEP4, OUTPUT);
  pinMode (STEP5, OUTPUT);
  pinMode (STEP6, OUTPUT);
  pinMode (STEP7, OUTPUT);
  pinMode (STEP8, OUTPUT);
  pinMode (FORW, INPUT);
  pinMode (BACK, INPUT);
  pinMode (RESET, INPUT);
  pinMode (ZERO, INPUT);
  pinMode (BUTTON1, INPUT);
  pinMode (BUTTON2, INPUT);
  pinMode (BUTTON3, INPUT);
  pinMode (BUTTON4, INPUT);
  pinMode (BUTTON5, INPUT);
  pinMode (BUTTON6, INPUT);  // BUTTON7, BUTTON8 and ROTARY will be read as analog
}

void loop ()
{  
// Check the rotary switch
    
  // Switch selects sequence length (# of stages used) and pattern
  //   
  // Position   Length  Pattern
  //  1          2       Single
  //  2          3       Single
  //  3          4       Single
  //  4          5       Single
  //  5          6       Single
  //  6          7       Single
  //  7          8       Single
  //  8          8       Inclusive rotation
  //  9          8       Exclusive rotation
  // 10          8       Double
  // 11          8       Random

  valRot = analogRead (ROTARY);
  pattern = PAT_SINGLE;
  seq_length = 8;
  if (valRot > 973) pattern = PAT_RANDOM;
  else if (valRot > 870) pattern = PAT_DOUBLE;
  else if (valRot > 768) pattern = PAT_EXCROT;
  else if (valRot > 666) pattern = PAT_INCROT;
  else if (valRot > 563) seq_length = 8;
  else if (valRot > 461) seq_length = 7;
  else if (valRot > 358) seq_length = 6;
  else if (valRot > 256) seq_length = 5;
  else if (valRot > 154) seq_length = 4;
  else if (valRot > 51) seq_length = 3;
  else seq_length = 2; // we don't allow seq_length == 1, come on, be reasonable

#if DBG==1
  if (old_seq_length != seq_length)
    {
      Serial.print("seq_length ");
      Serial.println(seq_length);
    }
  if (old_pattern != pattern)
    {
      Serial.print("pattern ");
      Serial.println(pattern);
    }
  old_seq_length = seq_length;
  old_pattern = pattern;
#endif

  // Next read all buttons and toggles and check for a change

  new_debounce_state = 0;
  valForw = digitalRead (FORW);
  new_debounce_state = 2*new_debounce_state + valForw;
  valBack = digitalRead (BACK);
  new_debounce_state = 2*new_debounce_state + valBack;
  valZero = digitalRead (ZERO);
  new_debounce_state = 2*new_debounce_state + valZero;
  valReset = digitalRead (RESET);
  new_debounce_state = 2*new_debounce_state + valReset;
  for (int ib = 2; ib <= 6; ++ib)
    {
      valButton[ib-1] = digitalRead (buttons[ib-1]);
      new_debounce_state = 2*new_debounce_state + valButton[ib-1];
    }
  valButton[6] = analogRead (buttons[6]);
  valButton[7] = analogRead (buttons[7]);
  new_debounce_state = 2*new_debounce_state + (valButton[7] > 1000 ? 1 : 0);

  // If changed note the time
  if (new_debounce_state != old_debounce_state)
  {
#if DBG==1
    Serial.print("delta ");
    Serial.println(millis()-lastDebounceTime);
    lastDebounceTime = millis();
#endif
  }  
  

  old_debounce_state = new_debounce_state;
  
  // Proceed only if no state change for a while
  delta = millis() - lastDebounceTime;
  if (delta < debounceDelay)
    return;

  // Switches are stable so we proceed.

  doNewGate = false;

  // step forward
  if ((valForw == HIGH) && (old_valForw == LOW))
    {
      doNewGate = true;
      stepForward = true;
#if DBG==1
	  Serial.println("forward ");
#endif
    }
  old_valForw = valForw;
  
  // step backward
  if ((valBack == HIGH) && (old_valBack == LOW))   
    {
      doNewGate = true;
      stepForward = false;
#if DBG==1
	  Serial.println("backward ");
#endif
    }
  old_valBack = valBack;

  // Execute patterns for either forward or backward step
  // Note that the following code allows for any pattern to be used with any
  // sequence length, though the current hardware and the code to read it only
  // permit PAT_SIMPLE with sequence length 2 through 8, or other patterns
  // with sequence length 8.
  
  if (doNewGate)
    if (pattern == PAT_RANDOM)
      stepOn = random (1, seq_length+1);
    else if (stepOn == 0)
      {
	stepOn = 1;
	pat_first = true;
      }
    else if (pattern == PAT_SINGLE)
      if (stepForward)
	{
	  stepOn = stepOn + 1;
	  if (stepOn > seq_length) stepOn = 1;
	}
      else
	{
	  stepOn = stepOn - 1;
	  if (stepOn < 1) stepOn = seq_length;
	}
    else if (pattern == PAT_INCROT)
      if (pat_dir == 1 && stepOn == seq_length)
	pat_dir = -1;
      else if (pat_dir == -1 && stepOn == 1)
	pat_dir = 1;
      else
	stepOn = stepOn + pat_dir;
    else if (pattern == PAT_EXCROT)
      {
	if (stepOn == seq_length)
	  pat_dir = -1;
	else if (stepOn == 1)
	  pat_dir = 1;
	stepOn = stepOn + pat_dir;
      }
    else if (pattern == PAT_DOUBLE)
      {
	if (not pat_first)
	  if (stepForward)
	    {
	      stepOn = stepOn + 1;
	      if (stepOn > seq_length) stepOn = 1;
	    }
	  else
	    {
	      stepOn = stepOn - 1;
	      if (stepOn < 1) stepOn = seq_length;
	    }
	pat_first = not pat_first;
      }  

  // zero (mute)   
  if ((valZero == HIGH) && (old_valZero == LOW))
    {
      doNewGate = true;
      stepOn = 0;
#if DBG==1
	  Serial.println("zero ");
#endif
    }
  old_valZero = valZero;
  
  // reset (go back to step 1)
  if ((valReset == HIGH) && (old_valReset == LOW))   
    {
      doNewGate = true;
      stepOn = 1;
#if DBG==1
	  Serial.println("reset ");
#endif
    }
  old_valReset = valReset;
  
  // check the buttons, and actually we can start with button 2
  // because button 1 and reset produce the same signal and we already
  // checked that
  
  for (int ib = 2; ib <= 6; ++ib)
    {
      if (valButton[ib-1] == HIGH && old_valButton[ib-1] == LOW)
	{
	  doNewGate = true;
	  stepOn = ib;
#if DBG==1
	  Serial.print("pin ");
	  Serial.print(buttons[ib-1]);
	  Serial.print(" button ");
	  Serial.println(ib);
#endif
	}
      old_valButton[ib-1] = valButton[ib-1];
    }

  // Pins A6 and A7 cannot be read as digital on the Nano so read
  // as analog and expect > 1000 when button pressed
  
  if (valButton[6] >= 1000 && old_valButton[6] < 1000)
    {
      doNewGate = true;
      stepOn = 7;
#if DBG==1
	  Serial.print("pin ");
	  Serial.print(buttons[6]);
	  Serial.println(" button 7");
#endif
    }
  old_valButton[6] = valButton[6];
  
  if (valButton[7] >= 1000 && old_valButton[7] < 1000)
    {
      doNewGate = true;
      stepOn = 8;
#if DBG==1
	  Serial.print("pin ");
	  Serial.print(buttons[7]);
	  Serial.println(" button 8");
#endif
    }
  old_valButton[7] = valButton[7];
  
  // Now we've determined what to do, so if we need a new gate, do it
  
  if (doNewGate)
    {
      // Turn off the on step, then turn on the new on step

      if (old_stepOn != 0)
	{
#if DBG==1
	  Serial.print("digitalWrite ");
	  Serial.print(old_stepOn);
	  Serial.print(" ");
	  Serial.print(steps[old_stepOn-1]);
	  Serial.println(" LOW");
#endif
	digitalWrite (steps[old_stepOn-1], LOW);
	}
      delay (GATEOFFTIME);
   
      if (stepOn != 0)
	{
#if DBG==1
	  Serial.print("digitalWrite ");
	  Serial.print(stepOn);
	  Serial.print(" ");
	  Serial.print(steps[stepOn-1]);
	  Serial.println(" HIGH");
#endif
	  digitalWrite (steps[stepOn-1], HIGH);
	}
      old_stepOn = stepOn;
    }
}
