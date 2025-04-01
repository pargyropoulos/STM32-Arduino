#define vRed    0x01 //vehicle red
#define vYellow 0x40 //vehicle yellow
#define vGreen  0x08 //vehicle green
#define pRed    0x02 //pedestrian red
#define pGreen  0x04 //pedestrian green
#define LED_PIN    PB1 
#define BUTTON_PIN PB0
#define IS_BUTTON_PRESSED   (digitalRead(BUTTON_PIN)==LOW)
#define IS_BUTTON_RELEASED  (digitalRead(BUTTON_PIN)==HIGH)
#define Takt_Time 1000000 //T=1 sec
#define DebounceTimer       Timer3
#define Traffic_Light_Timer Timer2
#define State_Green   0x00
#define State_Yellow  0x01
#define State_Red     0x02
#define Button_Event  0x01
#define Timer_Event   0x02
const char text_A[]="ΦΑΝΑΡΙ ΟΧΗΜΑΤΩΝ:ΠΡΑΣΙΝΟ     -     ΦΑΝΑΡΙ ΠΕΖΩΝ: ΚΟΚΚΙΝΟ";
const char text_B[]="ΦΑΝΑΡΙ ΟΧΗΜΑΤΩΝ:ΚΙΤΡΙΝΟ     -     ΦΑΝΑΡΙ ΠΕΖΩΝ: ΚΟΚΚΙΝΟ";
const char text_C[]="ΦΑΝΑΡΙ ΟΧΗΜΑΤΩΝ:KOKKINΟ     -     ΦΑΝΑΡΙ ΠΕΖΩΝ: ΠΡΑΣΙΝΟ";
volatile unsigned int Event=Timer_Event;
volatile unsigned int Time_Accumulated=0x00;
unsigned toggle=0;

typedef struct{
  unsigned int traffic_light;
  const char *ptrText;
} Output_s;
Output_s Output[]={{vGreen  | pRed  ,text_A},
                   {vYellow | pRed  ,text_B},
                   {vRed    | pGreen,text_C}};
             
typedef struct {
  unsigned int state;
  unsigned int transition_delay;
  unsigned int state_next;
  Output_s *ptrOutput;
} statem_s;

statem_s StateMachine[]={/* Current_State  Delay  Next_State      Output/
                    /*S0*/  {State_Green  ,10   ,State_Yellow   ,&Output[0]},
                    /*S1*/  {State_Yellow ,3    ,State_Red      ,&Output[1]},
                    /*S2*/  {State_Red    ,10   ,State_Green    ,&Output[2]}};
statem_s *ptrStateMachine=StateMachine;

void Start_Timer (HardwareTimer tmr, unsigned int period,voidFuncPtr handler) {
   tmr.setMode(TIMER_CH1, TIMER_OUTPUTCOMPARE);
   tmr.setCount(0);
   tmr.setPeriod(period);
   tmr.attachInterrupt(TIMER_CH1, handler);
}

void Stop_Timer(HardwareTimer tmr) {
   tmr.setMode(TIMER_CH1, TIMER_DISABLED);
}

void Button_ISR() {  
  //Start Debounce Timer
  Start_Timer(DebounceTimer,Takt_Time/20,Button_Debounce);
}

void Button_Debounce(){
   if (IS_BUTTON_PRESSED) {
    toggle ^= 1;
    digitalWrite(LED_PIN, toggle);
    Event|=Button_Event;
   }
   //Stop Debounce Timer
   Stop_Timer(DebounceTimer);
}

void TrafficLightTimeKeeper_ISR(){
  Event|=Timer_Event;
  Time_Accumulated++;
}

void setup(){
  Serial.begin(9600);
  delay(1500); //add a small delay for RS232 to sync
  pinMode(LED_PIN,OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  //Set PA0 to PA6 as output and PA7 as is input
  GPIOA->regs->CRL = (0b1000<<32)|(0b0011<<28)|(0b0011<<24)|(0b0011<<20)|(0b0011<<16)|(0b0011<<12)|(0b0011<<8)|(0b0011<<4)|(0b0011);
  //clear all pins but PA7
  GPIOA->regs->ODR =(1<<8); 

  //Comment out next line to only work in auto mode without reading button state
  attachInterrupt(digitalPinToInterrupt(PB0), Button_ISR, FALLING);
  
  Start_Timer(Traffic_Light_Timer,Takt_Time,TrafficLightTimeKeeper_ISR);
  print_SevenSegment(ptrStateMachine->ptrOutput->traffic_light); 
  serialReport();
  
}

void loop() {
unsigned int tmp;
statem_s *ptrtmp;

  switch (Event){
    
    case Button_Event | Timer_Event:
    case Button_Event:
      Event^=Button_Event;
      if (ptrStateMachine->state==State_Green && (Time_Accumulated <= 5-1) ) {
        ptrtmp=ptrStateMachine;
        tmp=ptrStateMachine->state_next;
        ptrStateMachine=&StateMachine[tmp];
        Time_Accumulated^=Time_Accumulated;        
        if (ptrtmp->ptrOutput->traffic_light != ptrStateMachine->ptrOutput->traffic_light){
            Serial.println("Πατήθηκε ο διακόπτης από πεζό! Άμεση αλλαγή κατάστασης στο FSM");
            serialReport();
          print_SevenSegment(ptrStateMachine->ptrOutput->traffic_light); 
        }
        break;
      }
      
    case Timer_Event:
      Event^=Timer_Event;
      if (ptrStateMachine->transition_delay==Time_Accumulated) {
        ptrtmp=ptrStateMachine;
        tmp=ptrStateMachine->state_next;
        ptrStateMachine=&StateMachine[tmp];
        Time_Accumulated^=Time_Accumulated;
        if (ptrtmp->ptrOutput->traffic_light != ptrStateMachine->ptrOutput->traffic_light){
          serialReport();
          print_SevenSegment(ptrStateMachine->ptrOutput->traffic_light); 
        }  
      }
      break;
    
    default:
      break;
  }
    
}

void serialReport(){
  Serial.println(ptrStateMachine->ptrOutput->ptrText);
  Serial.println("--------------------------------------------------------");
}

void print_SevenSegment(unsigned int digit){
  //Write to Output Data Register
  GPIOA->regs->ODR=digit;
}
