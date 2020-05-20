/*********************************************************************
  Arduino state machine code for Optogenetics Controller

  Harvard School of Mouse Github Version from Hamilos et al., 2020
  
  Optogenetics Control System           - Allison Hamilos (ahamilos@g.harvard.edu)
  Original State System Backbone        - Lingfeng Hou
  Matlab Serial Communication Interface - Ofer Mazor
  
  Created       3/9/18 - ahamilos
  Last Modified 8/21/19 - ahamilos  VERSION CODE "AUG 21 2019 10am" */

  static String versionCode        = "AUG 21 2019 10am";
  
  /*
  Update Hx:
  Jul 13 2018
    -7/13/18: added a 3.3V line for another power source...
    -6/21/18: added cancellation trigger - turns off stimulation. Right now using first lick signal to stop ongoing stim, but could use others (good)
    -5/4/18:  added up and down time features
    -5/2/18:  added optotag function

  ------------------------------------------------------------------
  COMPATIBILITY REPORT:
    Matlab HOST: Matlab 2016a - FileName = OptogeneticsController.m (depends on ArduinoConnection.m)
    Arduino:
      Default: TEENSY
      Others:  UNO, TEENSY, DUE, MEGA
  ------------------------------------------------------------------
  Reserved:

    Event Markers: 0-16
    States:        0-8
    Result Codes:  0-3
    Parameters:    0-24
  ------------------------------------------------------------------
  Task Architecture: 

  --------------------------------------------------------------------
  States:
    0: _INIT                (private) 1st state in init loop, sets up communication to Matlab HOST
    1: IDLE_STATE           Awaiting command from Matlab HOST to begin experiment
  ---------------------------------------------------------------------
  Result Codes:
    0: CODE_CORRECT         First lick within response window               
    1: CODE_EARLY_LICK      Early lick -> Abort (Enforced No-Lick Only)
    2: CODE_LATE_LICK       Late Lick  -> Abort (Operant Only)
    3: CODE_NO_LICK         No Response -> Time Out
  ---------------------------------------------------------------------
  Parameters:
    0:  _DEBUG              (private) 1 to enable debug messages to HOST
    1:  HYBRID              1 to overrule pav/op - is op if before target, pav if target reached
    2:  PAVLOVIAN           1 to enable Pavlovian Mode
    3:  OPERANT             1 to enable Operant Mode
    4:  ENFORCE_NO_LICK     1 to enforce no lick in the pre-window interval
    5:  INTERVAL_MIN        Time to start of reward window (ms)

  ---------------------------------------------------------------------
    Incoming Message Syntax: (received from Matlab HOST)
      "(character)#"        -- a command
      "(character int1 int2)# -- update parameter (int1) to new value (int2)"
      Command characters:
        P  -- parameter
        O# -- HOST has received updated paramters, may resume trial
        Q# -- quit and go to IDLE_STATE
        G# -- begin trial (from IDLE_STATE)
        C  -- ChR2-1 request from HOST
        T  -- Optotag ladder
  ---------------------------------------------------------------------
    Outgoing Message Syntax: (delivered to Matlab HOST)
      ONLINE:  
        "~"                           Tells Matlab HOST arduino is running
      STATES:
        "@ (enum num) stateName"      Defines state names for Matlab HOST
        "$(enum num) num num"         State-number, parameter, value
 -                                          param = 0: current time
 -                                          param = 1: result code (enum num)
      EVENT MARKERS:
        "+(enum num) eventMarker"     Defines event markers with string
        "&(enum num) timestamp"       Event Marker with timestamp

      RESULT CODES:
      "* (enum num) result_code_name" Defines result code names with str 
      "` (enum num of result_code)"   Send result code for trial to Matlab HOST

      MESSAGE:
        "string"                      String to Matlab HOST serial monitor (debugging) 

      RECEIPT OF STIM:
        "R"             Arduino has received C from HOST
*********************************************************************/



/*****************************************************
  Global Variables
*****************************************************/

/*****************************************************
Arduino Pin Outs (Mode: TEENSY)
*****************************************************/

// Digital IN
#define PIN_CHR2_TRIGGER_1   0  // ChR2 Trigger Pin    (DUE = 34)  (MEGA = 34)  (UNO = 5?)  (TEENSY = 6?)
#define PIN_ARCHT_TRIGGER_1  3  // ArchT Trigger Pin   (DUE = 34)  (MEGA = 34)  (UNO = 5?)  (TEENSY = 6?)
#define PIN_TRIAL_INIT       6  // Aligns Trial Start  (DUE = 34)  (MEGA = 34)  (UNO = 5?)  (TEENSY = 6?)
#define PIN_CUE_ON           7  // Aligns Cue On       (DUE = 34)  (MEGA = 34)  (UNO = 5?)  (TEENSY = 6?)
#define PIN_CANCEL           8  // Cancels ongoing stim if high                             (TEENSY = 6?)

// Digital OUT
#define PIN_CHR2_LAMP_1      1   // ChR2 Lamp Pin       (DUE = 34)  (MEGA = 34)  (UNO = 5?)  (TEENSY = 6?)
#define PIN_CHR2_RECEIPT_1   2   // ChR2 Receipt Pin    (DUE = 35)  (MEGA = 28)  (UNO =  4)  (TEENSY = 4)
#define PIN_ARCHT_LAMP_1     4   // ArchT Lamp Pin      (DUE = 37)  (MEGA = 52)  (UNO =  7)  (TEENSY = 7)
#define PIN_ARCHT_RECEIPT_1  5   // ArchT Receipt Pin   (MEGA = 22)              (TEENSY = 9)
#define PIN_3_3             11   // A 3.3V source


/*****************************************************
Enums - DEFINE States
*****************************************************/
// All the states
enum State
{
  _INIT,                  // (Private) Initial state used on first loop. 
  IDLE_STATE,             // Idle state. Wait for go signal from host.
  INIT_EXP,               // House lamp OFF, random delay before cue presentation
  WAIT_REQUEST_STATE,     // 
  CHR2_STATE,             // 
  ARCHT_STATE,            // 
  UPDATE_PARAMS_STATE,    // 
  _NUM_STATES             // (Private) Used to count number of states
};

// State names stored as strings, will be sent to host
// Names cannot contain spaces!!!
static const char *_stateNames[] = 
{
  "_INIT",
  "IDLE",
  "INIT_EXP",
  "WAIT_REQUEST", 
  "CHR2",     
  "ARCHT",    
  "UPDATE_PARAMS"
};

// Define which states allow param update
static const int _stateCanUpdateParams[] = {1,1,1,1,1,1,1}; 
// Defined to allow Parameter upload from host at ANY time


/*****************************************************
Event Markers
*****************************************************/
enum EventMarkers
/* You may define as many event markers as you like.
    Assign event markers to any IN/OUT event
    Times and trials will be defined by global time, 
    which can be parsed later to validate time measurements */
{
  EVENT_EXP_INIT,       // Begin exp
  EVENT_TRIAL_INIT,         // Begin trial
  EVENT_CUE_ON,             // Begin cue presentation
  
  EVENT_BOSS_CHR2_REQUEST,  // Boss request ChR2
  EVENT_BOSS_CH2R_STIM_BEGIN,
  EVENT_BOSS_CH2R_STIM_END,

  EVENT_UI_CHR2_REQUEST,    // UI request ChR2
  EVENT_UI_CH2R_STIM_BEGIN, 
  EVENT_UI_CH2R_STIM_END,
  
  EVENT_ARCHT_STIM_BEGIN,   // Begin inhib
  EVENT_ARCHT_STIM_END,   // End inhib
  EVENT_CHR2_ERROR,
  EVENT_ARCHT_ERROR,

  EVENT_UI_OPTOTAG,     // Begin optotag ladder

  _NUM_OF_EVENT_MARKERS
};

static const char *_eventMarkerNames[] =    // * to define array of strings
{
  "EXP_INIT",
  "TRIAL_INIT",
  "CUE_ON",
  
  "BOSS_CHR2_REQUEST",
  "BOSS_CH2R_STIM_BEGIN",
  "BOSS_CH2R_STIM_END",
  
  "UI_CHR2_REQUEST",
  "UI_CH2R_STIM_BEGIN",
  "UI_CH2R_STIM_END",
  
  "ARCHT_STIM_BEGIN",
  "ARCHT_STIM_END",
  "CHR2_ERROR",
  "ARCHT_ERROR",

  "UI_OPTOTAG"
};

/*****************************************************
Result codes
*****************************************************/
enum ResultCode
{
  CODE_CHR2,                // ChR2 Stimulated
  CODE_CHR2_CANCEL,         // ChR2 in behavior cancelled by event (e.g., first lick)
  CODE_UI_CHR2,             // UI-commanded ChR2 Stimulated
  CODE_ARCHT,               // ArchT Stimulated
  CODE_OPTOTAG,             // Completed optotagging ladder
  _NUM_RESULT_CODES         // (Private) Used to count how many codes there are.
};

// We'll send result code translations to MATLAB at startup
static const char *_resultCodeNames[] =
{
  "CHR2",
  "CHR2_CANCELLED",
  "UI_CHR2",
  "ARCHT",
  "OPTOTAG"
};


/*****************************************************
Audio cue frequencies
*****************************************************/
enum SoundEventFrequencyEnum
{
  TONE_ERROR   = 440,              // Experimental Error
};

/*****************************************************
Parameters that can be updated by HOST
*****************************************************/
// Storing everything in array _params[]. Using enum ParamID as array indices so it's easier to add/remove parameters. 
enum ParamID
{
  _DEBUG,                         // (Private) 1 to enable debug messages from HOST. Default 0.
  CANCEL_ENABLED,                 // Use cancellation pin to halt ongoing stim
  CHR2_FREQUENCY,                 // Frequency of stim (Hz) -- if ZERO, puts out constant HI
  CHR2_PERIOD,
  CHR2_DURATION,                  // Duration of stim (ms)
  CHR2_DUTY_CYCLE,                // Fraction of time in HIGH position (0-1)
  CHR2_UP_TIME,
  CHR2_DOWN_TIME,
  ARCHT_FREQUENCY,                // Frequency of stim (Hz) -- if ZERO, puts out constant HI 
  ARCHT_PERIOD,
  ARCHT_DURATION,                 // Duration of stim (ms)
  ARCHT_DUTY_CYCLE,               // Fraction of time in HIGH position (0-1)
  ARCHT_UP_TIME,
  ARCHT_DOWN_TIME,
  UI_CHR2_FREQUENCY,        // Frequency of custom pulse (Hz) -- if ZERO, puts out constant HI 
  UI_CHR2_PERIOD,
  UI_CHR2_DURATION,       // Duration of a custom pulse (ms) (unleased with UI click)
  UI_CHR2_DUTY_CYCLE,       // Fraction of time in HIGH position (0-1)
  UI_CHR2_UP_TIME,
  UI_CHR2_DOWN_TIME,
  UI_ARCHT_FREQUENCY,       // Frequency of custom pulse (Hz) -- if ZERO, puts out constant HI 
  UI_ARCHT_PERIOD,
  UI_ARCHT_DURATION,        // Duration of a custom pulse (ms) (unleased with UI click)
  UI_ARCHT_DUTY_CYCLE,      // Fraction of time in HIGH position (0-1)
  UI_ARCHT_UP_TIME,
  UI_ARCHT_DOWN_TIME,
  _NUM_PARAMS                     // (Private) Used to count how many parameters there are so we can initialize the param array with the correct size. Insert additional parameters before this.
}; //**** BE SURE TO ADD NEW PARAMS TO THE NAMES LIST BELOW!*****//

// Store parameter names as strings, will be sent to host
// Names cannot contain spaces!!!
static const char *_paramNames[] = 
{
  "_DEBUG",
  "CANCEL_ENABLED",
  "ChR2_FREQUENCY",
  "ChR2_PERIOD",
  "ChR2_DURATION",
  "ChR2_DUTY_CYCLE",
  "ChR2_UP_TIME",
  "ChR2_DOWN_TIME",
  "ArchT_FREQUENCY",
  "ArchT_PERIOD",
  "ArchT_DURATION",
  "ArchT_DUTY_CYCLE",
  "ArchT_UP_TIME",
  "ArchT_DOWN_TIME",
  "UI_ChR2_FREQUENCY",
  "UI_ChR2_PERIOD",
  "UI_ChR2_DURATION",
  "UI_ChR2_DUTY_CYCLE",
  "UI_ChR2_UP_TIME",
  "UI_ChR2_DOWN_TIME",
  "UI_ArchT_FREQUENCY",
  "UI_ArchT_PERIOD",
  "UI_ArchT_DURATION",
  "UI_ArchT_DUTY_CYCLE",
  "UI_ArchT_UP_TIME",
  "UI_ArchT_DOWN_TIME"
}; //**** BE SURE TO INIT NEW PARAM VALUES BELOW!*****//

// Initialize parameters in ms
float _params[_NUM_PARAMS] = 
{
  1,                              // _DEBUG
  1,                              // CANCEL_ENABLED
  1,                              // CHR2_FREQUENCY
  1000,                           // CHR2_PERIOD
  1000,                           // CHR2_DURATION
  50,                             // CHR2_DUTY_CYCLE
  500, 							              // CHR2_UP_TIME
  500,							              // CHR2_DOWN_TIME
  1,                              // ARCHT_FREQUENCY
  1000,                           // ARCHT_PERIOD
  7000,                           // ARCHT_DURATION
  100, 	                          // ARCHT_DUTY_CYCLE
  7000,							              // ARCHT_UP_TIME
  0,	  						              // ARCHT_DOWN_TIME
  1,                              // UI_CHR2_FREQUENCY
  1000,	                          // UI_CHR2_PERIOD
  1000,                           // UI_CHR2_DURATION
  50,                             // UI_CHR2_DUTY_CYCLE
  500, 							              // UI_CHR2_UP_TIME
  500,							              // UI_CHR2_DOWN_TIME
  1,                              // UI_ARCHT_FREQUENCY
  1000,                           // UI_ARCHT_PERIOD
  500,                            // UI_ARCHT_DURATION
  1, 	                            // UI_ARCHT_DUTY_CYCLE
  500,							              // UI_ARCHT_UP_TIME
  0								                // UI_ARCHT_DOWN_TIME
};

/*****************************************************
Other Global Variables 
*****************************************************/
// Variables declared here can be carried to the next loop, AND read/written in function scope as well as main scope
// (previously defined):
static State _state                   = _INIT;    // This variable (current _state) get passed into a _state function, which determines what the next _state should be, and updates it to the next _state.
static State _prevState               = _INIT;    // Remembers the previous _state from the last loop (actions should only be executed when you enter a _state for the first time, comparing currentState vs _prevState helps us keep track of that).
static char _command                  = ' ';      // Command char received from host, resets on each loop
static int _arguments[2]              = {0};      // Two integers received from host , resets on each loop
static int _argcopy[2]				        = {0};
static int _resultCode                = -1;   // Reset result code

// Define additional global variables
static signed long _exp_timer           = 0;
static signed long _time_ChR2_stimulated= 0;
static signed long _ChR2_refractory_clock= 0;
static signed long _duty_timer_ChR2     = 0;

// static long _ChR2_CYCLE_TIME       = 1/_params[CHR2_FREQUENCY];
// static signed long _ChR2_HIGH_TIME    = long(double(_params[CHR2_DUTY_CYCLE])/100*1/double(_params[CHR2_FREQUENCY])*1000);
// static signed long _ChR2_LOW_TIME     = long((1 - double(_params[CHR2_DUTY_CYCLE])/100)*1/double(_params[CHR2_FREQUENCY])*1000);
// static signed long _UI_ChR2_HIGH_TIME = long(double(_params[UI_CHR2_DUTY_CYCLE])/100*1/double(_params[UI_CHR2_FREQUENCY])*1000);
// static signed long _UI_ChR2_LOW_TIME  = long((1 - double(_params[UI_CHR2_DUTY_CYCLE])/100)*1/double(_params[UI_CHR2_FREQUENCY])*1000);
static bool _ChR2_trigger_on          = false;
static bool _trial_is_stimulated      = false;
static bool _stimulation_requested    = false;
static bool _ChR2_receipt_received    = false;
static bool _BOSS_ChR2_stim_in_progress = false;
static bool _UI_ChR2_stim_in_progress = false;
static bool _duty_state_ChR2          = false;

static bool _hold                     = false;
static bool _isParamsUpdateStarted    = false;
static bool _isParamsUpdateDone       = true;
static bool _ready_for_next_trial     = true;
static bool _ready_for_cue            = true;



/*****************************************************
  INITIALIZATION LOOP
*****************************************************/
void setup()
{
  //--------------------I/O initialization------------------//
  // OUTPUTS
  pinMode(PIN_CHR2_LAMP_1, OUTPUT);            
  pinMode(PIN_CHR2_RECEIPT_1, OUTPUT);         
  pinMode(PIN_ARCHT_LAMP_1, OUTPUT);           
  pinMode(PIN_ARCHT_RECEIPT_1, OUTPUT);
  pinMode(PIN_3_3, OUTPUT);        
  // INPUTS
  pinMode(PIN_CHR2_TRIGGER_1, INPUT);          
  pinMode(PIN_ARCHT_TRIGGER_1, INPUT);    
  pinMode(PIN_TRIAL_INIT, INPUT);             
  pinMode(PIN_CUE_ON, INPUT);   
  pinMode(PIN_CANCEL, INPUT);     
  //--------------------------------------------------------//

  //------------------------Serial Comms--------------------//
  Serial.begin(115200);                       // Set up USB communication at 115200 baud 

} // End Initialization Loop -----------------------------------------------------------------------------------------------------


/*****************************************************
  MAIN LOOP
*****************************************************/
void loop()
{
  // Initialization
  mySetup();

  // SET 3.3V line to high
  // digitalWrite(PIN_3_3, HIGH);

  // Main loop (R# resets it)
  while (true)
  {
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      Step 1: Read USB MESSAGE from HOST (if available)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    // 1) Check USB for MESSAGE from HOST, if available. String is read byte by byte. (Each character is a byte, so reads e/a character)

    static String usbMessage  = "";             // Initialize usbMessage to empty string, only happens once on first loop (thanks to static!)
    _command = ' ';                              // Initialize _command to a SPACE
    _arguments[0] = 0;                           // Initialize 1st integer argument
    _arguments[1] = 0;                           // Initialize 2nd integer argument

    if (Serial.available() > 0)  {              // If there's something in the SERIAL INPUT BUFFER (i.e., if another character from host is waiting in the queue to be read)
      char inByte = Serial.read();                  // Read next character
      
      // The pound sign ('#') indicates a complete message!------------------------
      if (inByte == '#')  {                         // If # received, terminate the message
        // Parse the string, and updates `_command`, and `_arguments`
        _command = getCommand(usbMessage);               // getCommand pulls out the character from the message for the _command         
        getArguments(usbMessage, _arguments);            // getArguments pulls out the integer values from the usbMessage
        usbMessage = "";                                // Clear message buffer (resets to prepare for next message)
        if (_command == 'R') {
          break;
        }
      }
      else {
        // append character to message buffer
        usbMessage = usbMessage + inByte;       // Appends the next character from the queue to the usbMessage string
      }
    }

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      Step 2: Update the State Machine
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    // Depending on what _state we're in , call the appropriate _state function, which will evaluate the transition conditions, and update `_state` to what the next _state should be
    switch (_state) {
      case _INIT:
        idle_state();
        break;

      case IDLE_STATE:
        idle_state();
        break;
      
      case INIT_EXP:
        init_exp();
        break;

      case WAIT_REQUEST_STATE:
        wait_request_state();
        break;

      case CHR2_STATE:
        chr2_state();
        break;

      case ARCHT_STATE:
        archt_state();
        break;

      case UPDATE_PARAMS_STATE:
        update_params_state();
        break;
    } // End switch statement--------------------------
  }
} // End main loop-------------------------------------------------------------------------------------------------------------



void mySetup()
{

  //--------------Set ititial OUTPUTS----------------//
  setChR2Lamp(false);     
  setChR2Receipt(false);  
  // setARCHTLamp(false); 
    // setARCHTReceipt(false);

  //---------------------------Reset a bunch of variables---------------------------//
  _state                    = _INIT;    // This variable (current _state) get passed into a _state function, which determines what the next _state should be, and updates it to the next _state.
  _prevState                = _INIT;    // Remembers the previous _state from the last loop (actions should only be executed when you enter a _state for the first time, comparing currentState vs _prevState helps us keep track of that).
  _command                  = ' ';      // Command char received from host, resets on each loop
  _arguments[0]             = 0;        // Two integers received from host , resets on each loop
  _arguments[1]             = 0;        // Two integers received from host , resets on each loop
  _resultCode               = -1;     // Reset result code

  // Reset global variables
  _exp_timer        = 0;
  _time_ChR2_stimulated   = 0;
  _ChR2_refractory_clock  = 0;
  _duty_timer_ChR2        = 0;

  // _ChR2_HIGH_TIME     = long((double(_params[CHR2_DUTY_CYCLE])/100)*(1/double(_params[CHR2_FREQUENCY]))*1000);
  // _ChR2_LOW_TIME      = long((1 - (double(_params[CHR2_DUTY_CYCLE])/100))*(1/double(_params[CHR2_FREQUENCY]))*1000);
  if (_params[_DEBUG]) {sendMessage("Initial Duty Cycle high time = " + String(_params[CHR2_UP_TIME]) + " low time = " +  String(_params[CHR2_DOWN_TIME]) + ".");}
  // _UI_ChR2_HIGH_TIME    = long((double(_params[UI_CHR2_DUTY_CYCLE])/100*1/double(_params[UI_CHR2_FREQUENCY])*1000));
  // _UI_ChR2_LOW_TIME     = long((1 - double(_params[UI_CHR2_DUTY_CYCLE])/100)*1/double(_params[UI_CHR2_FREQUENCY])*1000);
  _ChR2_trigger_on        = false;
  _trial_is_stimulated    = false;
  _stimulation_requested  = false;
  _ChR2_receipt_received  = false;
  _BOSS_ChR2_stim_in_progress= false;
  _UI_ChR2_stim_in_progress= false;
  _duty_state_ChR2        = false;

  _hold                   = false;
  _isParamsUpdateStarted  = false;
  _isParamsUpdateDone     = true;
  _ready_for_next_trial   = true;
  _ready_for_cue          = true;
  

  // Tell PC that we're running by sending '~' message:
  hostInit();                         // Sends all parameters, states and error codes to Matlab (LF Function)    
}



/*****************************************************
  States for the State Machine
*****************************************************/

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  IDLE STATE - awaiting start cue from Matlab HOST
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void idle_state() {
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ACTION LIST -- initialize the new state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (_state != _prevState) {                      // If ENTERTING IDLE_STATE:
    sendMessage("-");
    sendMessage("Optogenetics_Controller4");
    sendMessage("Version Code: " versionCode);
    _prevState = _state;                             // Assign _prevState to idle _state
    sendMessage("$" + String(_state));               // Send a message to host upon _state entry -- $1 (Idle State)
    // Reset Outputs
    setChR2Lamp(false);     
    setChR2Receipt(false);  
    // setARCHTLamp(false); 
      // setARCHTReceipt(false);
    // Reset state variables
    _resultCode = -1;                              // Clear previously registered result code
      

    //------------------------DEBUG MODE--------------------------//
    if (_params[_DEBUG]) {
      sendMessage("Idle.");
    }  
    //----------------------end DEBUG MODE------------------------//
  }

  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    TRANSITION LIST -- checks conditions, moves to next state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  
  if (_command == 'G') {                           // If Received GO signal from HOST ---transition to---> READY
    _state = INIT_EXP;                              // State set to INIT_EXP
    return;                                           // Exit function
  }

  if (checkParamUpdate()){return;};                // Sends us to the update param state until finished updating.   

  _state = IDLE_STATE;                             // Return to IDLE_STATE
} // End IDLE_STATE ------------------------------------------------------------------------------------------------------------------------------------------




/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  UPDATE_PARAMS_STATE - Hold the controller until update complete
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void update_params_state() {
  /* Only exit once parameters completed update. Give a warning if stim in progress */

  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ACTION LIST -- initialize the new state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (_hold != true) { /* if previous state not updating params... */
    sendMessage("$" + String(_state));                  // Send  HOST _state entry
    checkWarnings();
    _hold = true;
    float cycle_time = 0;

    if (_params[_DEBUG]) {sendMessage("The current _argcopy[0] = " + String(_argcopy[0]) + " _argcopy[1] = " + String(_argcopy[1]) + ". CHR2_DUTY_CYCLE = " + String(CHR2_DUTY_CYCLE));}

    // Check if params are the freq or duration etc and then update dependents accordingly
    if (_argcopy[0] == CHR2_DUTY_CYCLE) {
      cycle_time = _params[CHR2_PERIOD];
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_argcopy[1]) = " + float(_argcopy[1]));}
      _params[CHR2_UP_TIME] = float(_argcopy[1])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[CHR2_UP_TIME] = " + String(_params[CHR2_UP_TIME]));}
      sendMessage("^ " + String(CHR2_UP_TIME) + " ChR2_UP_TIME " + String(_params[CHR2_UP_TIME]));
      _params[CHR2_DOWN_TIME] = cycle_time - float(_params[CHR2_UP_TIME]);
      sendMessage("^ " + String(CHR2_DOWN_TIME) + " ChR2_DOWN_TIME " + String(_params[CHR2_DOWN_TIME]));
    }
    if (_argcopy[0] == CHR2_UP_TIME) {
      cycle_time = _params[CHR2_PERIOD];
      _params[CHR2_DUTY_CYCLE] = 100*float(_argcopy[1]) / cycle_time;
      sendMessage("^ " + String(CHR2_DUTY_CYCLE) + " ChR2_DUTY_CYCLE " + String(_params[CHR2_DUTY_CYCLE]));
      _params[CHR2_DOWN_TIME] = cycle_time - float(_argcopy[1]);
      sendMessage("^ " + String(CHR2_DOWN_TIME) + " ChR2_DOWN_TIME " + String(_params[CHR2_DOWN_TIME]));
    }
    if (_argcopy[0] == CHR2_DOWN_TIME) {
      cycle_time = _params[CHR2_PERIOD];
      _params[CHR2_UP_TIME] = cycle_time - float(_argcopy[1]);
      sendMessage("^ " + String(CHR2_UP_TIME) + " ChR2_UP_TIME " + String(_params[CHR2_UP_TIME]));
      _params[CHR2_DUTY_CYCLE] = 100*float(_params[CHR2_UP_TIME]) / cycle_time;
      sendMessage("^ " + String(CHR2_DUTY_CYCLE) + " ChR2_DUTY_CYCLE " + String(_params[CHR2_DUTY_CYCLE]));
    }
    if (_argcopy[0] == CHR2_FREQUENCY) {
	  _params[CHR2_PERIOD] = 1000/float(_argcopy[1]);
      sendMessage("^ " + String(CHR2_PERIOD) + " ChR2_PERIOD " + String(_params[CHR2_PERIOD]));
      cycle_time = _params[CHR2_PERIOD];
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_params[CHR2_DUTY_CYCLE] = " + float(_params[CHR2_DUTY_CYCLE]));}
      _params[CHR2_UP_TIME] = float(_params[CHR2_DUTY_CYCLE])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[CHR2_UP_TIME] = " + String(_params[CHR2_UP_TIME]));}
      sendMessage("^ " + String(CHR2_UP_TIME) + " ChR2_UP_TIME " + String(_params[CHR2_UP_TIME]));
      _params[CHR2_DOWN_TIME] = cycle_time - float(_params[CHR2_UP_TIME]);
      sendMessage("^ " + String(CHR2_DOWN_TIME) + " ChR2_DOWN_TIME " + String(_params[CHR2_DOWN_TIME]));
    }
    if (_argcopy[0] == CHR2_PERIOD) {
	  _params[CHR2_FREQUENCY] = 1000/float(_argcopy[1]);
      sendMessage("^ " + String(CHR2_FREQUENCY) + " ChR2_FREQUENCY " + String(_params[CHR2_FREQUENCY]));
      cycle_time = float(_argcopy[1]);
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_params[CHR2_DUTY_CYCLE] = " + float(_params[CHR2_DUTY_CYCLE]));}
      _params[CHR2_UP_TIME] = float(_params[CHR2_DUTY_CYCLE])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[CHR2_UP_TIME] = " + String(_params[CHR2_UP_TIME]));}
      sendMessage("^ " + String(CHR2_UP_TIME) + " ChR2_UP_TIME " + String(_params[CHR2_UP_TIME]));
      _params[CHR2_DOWN_TIME] = cycle_time - float(_params[CHR2_UP_TIME]);
      sendMessage("^ " + String(CHR2_DOWN_TIME) + " ChR2_DOWN_TIME " + String(_params[CHR2_DOWN_TIME]));
    }


  if (_argcopy[0] == ARCHT_DUTY_CYCLE) {
      cycle_time = _params[ARCHT_PERIOD];
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_argcopy[1]) = " + float(_argcopy[1]));}
      _params[ARCHT_UP_TIME] = float(_argcopy[1])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[ARCHT_UP_TIME] = " + String(_params[ARCHT_UP_TIME]));}
      sendMessage("^ " + String(ARCHT_UP_TIME) + " ArchT_UP_TIME " + String(_params[ARCHT_UP_TIME]));
      _params[ARCHT_DOWN_TIME] = cycle_time - float(_params[ARCHT_UP_TIME]);
      sendMessage("^ " + String(ARCHT_DOWN_TIME) + " ArchT_DOWN_TIME " + String(_params[ARCHT_DOWN_TIME]));
    }
    if (_argcopy[0] == ARCHT_UP_TIME) {
      cycle_time = _params[ARCHT_PERIOD];
      _params[ARCHT_DUTY_CYCLE] = 100*float(_argcopy[1]) / cycle_time;
      sendMessage("^ " + String(ARCHT_DUTY_CYCLE) + " ArchT_DUTY_CYCLE " + String(_params[ARCHT_DUTY_CYCLE]));
      _params[ARCHT_DOWN_TIME] = cycle_time - float(_argcopy[1]);
      sendMessage("^ " + String(ARCHT_DOWN_TIME) + " ArchT_DOWN_TIME " + String(_params[ARCHT_DOWN_TIME]));
    }
    if (_argcopy[0] == ARCHT_DOWN_TIME) {
      cycle_time = _params[ARCHT_PERIOD];
      _params[ARCHT_UP_TIME] = cycle_time - float(_argcopy[1]);
      sendMessage("^ " + String(ARCHT_UP_TIME) + " ArchT_UP_TIME " + String(_params[ARCHT_UP_TIME]));
      _params[ARCHT_DUTY_CYCLE] = 100*float(_params[ARCHT_UP_TIME]) / cycle_time;
      sendMessage("^ " + String(ARCHT_DUTY_CYCLE) + " ArchT_DUTY_CYCLE " + String(_params[ARCHT_DUTY_CYCLE]));
    }
    if (_argcopy[0] == ARCHT_FREQUENCY) {
	  _params[ARCHT_PERIOD] = 1000/float(_argcopy[1]);
      sendMessage("^ " + String(ARCHT_PERIOD) + " ArchT_PERIOD " + String(_params[ARCHT_PERIOD]));
      cycle_time = _params[ARCHT_PERIOD];
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_params[ARCHT_DUTY_CYCLE] = " + float(_params[ARCHT_DUTY_CYCLE]));}
      _params[ARCHT_UP_TIME] = float(_params[ARCHT_DUTY_CYCLE])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[ARCHT_UP_TIME] = " + String(_params[ARCHT_UP_TIME]));}
      sendMessage("^ " + String(ARCHT_UP_TIME) + " ArchT_UP_TIME " + String(_params[ARCHT_UP_TIME]));
      _params[ARCHT_DOWN_TIME] = cycle_time - float(_params[ARCHT_UP_TIME]);
      sendMessage("^ " + String(ARCHT_DOWN_TIME) + " ArchT_DOWN_TIME " + String(_params[ARCHT_DOWN_TIME]));
    }
    if (_argcopy[0] == ARCHT_PERIOD) {
	  _params[ARCHT_FREQUENCY] = 1000/float(_argcopy[1]);
      sendMessage("^ " + String(ARCHT_FREQUENCY) + " ArchT_FREQUENCY " + String(_params[ARCHT_FREQUENCY]));
      cycle_time = float(_argcopy[1]);
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_params[ARCHT_DUTY_CYCLE] = " + float(_params[ARCHT_DUTY_CYCLE]));}
      _params[ARCHT_UP_TIME] = float(_params[ARCHT_DUTY_CYCLE])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[ARCHT_UP_TIME] = " + String(_params[ARCHT_UP_TIME]));}
      sendMessage("^ " + String(ARCHT_UP_TIME) + " ArchT_UP_TIME " + String(_params[ARCHT_UP_TIME]));
      _params[ARCHT_DOWN_TIME] = cycle_time - float(_params[ARCHT_UP_TIME]);
      sendMessage("^ " + String(ARCHT_DOWN_TIME) + " ArchT_DOWN_TIME " + String(_params[ARCHT_DOWN_TIME]));
    }



    if (_argcopy[0] == UI_CHR2_DUTY_CYCLE) {
      cycle_time = _params[UI_CHR2_PERIOD];
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_argcopy[1]) = " + float(_argcopy[1]));}
      _params[UI_CHR2_UP_TIME] = float(_argcopy[1])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[UI_CHR2_UP_TIME] = " + String(_params[UI_CHR2_UP_TIME]));}
      sendMessage("^ " + String(UI_CHR2_UP_TIME) + " UI_ChR2_UP_TIME " + String(_params[UI_CHR2_UP_TIME]));
      _params[UI_CHR2_DOWN_TIME] = cycle_time - float(_params[UI_CHR2_UP_TIME]);
      sendMessage("^ " + String(UI_CHR2_DOWN_TIME) + " UI_ChR2_DOWN_TIME " + String(_params[UI_CHR2_DOWN_TIME]));
    }
    if (_argcopy[0] == UI_CHR2_UP_TIME) {
      cycle_time = _params[UI_CHR2_PERIOD];
      _params[UI_CHR2_DUTY_CYCLE] = 100*float(_argcopy[1]) / cycle_time;
      sendMessage("^ " + String(UI_CHR2_DUTY_CYCLE) + " UI_ChR2_DUTY_CYCLE " + String(_params[UI_CHR2_DUTY_CYCLE]));
      _params[UI_CHR2_DOWN_TIME] = cycle_time - float(_argcopy[1]);
      sendMessage("^ " + String(UI_CHR2_DOWN_TIME) + " UI_ChR2_DOWN_TIME " + String(_params[UI_CHR2_DOWN_TIME]));
    }
    if (_argcopy[0] == UI_CHR2_DOWN_TIME) {
      cycle_time = _params[UI_CHR2_PERIOD];
      _params[UI_CHR2_UP_TIME] = cycle_time - float(_argcopy[1]);
      sendMessage("^ " + String(UI_CHR2_UP_TIME) + " UI_ChR2_UP_TIME " + String(_params[UI_CHR2_UP_TIME]));
      _params[UI_CHR2_DUTY_CYCLE] = 100*float(_params[UI_CHR2_UP_TIME]) / cycle_time;
      sendMessage("^ " + String(UI_CHR2_DUTY_CYCLE) + " UI_ChR2_DUTY_CYCLE " + String(_params[UI_CHR2_DUTY_CYCLE]));
    }
    if (_argcopy[0] == UI_CHR2_FREQUENCY) {
	  _params[UI_CHR2_PERIOD] = 1000/float(_argcopy[1]);
      sendMessage("^ " + String(UI_CHR2_PERIOD) + " UI_ChR2_PERIOD " + String(_params[UI_CHR2_PERIOD]));
      cycle_time = _params[UI_CHR2_PERIOD];
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_params[UI_CHR2_DUTY_CYCLE] = " + float(_params[UI_CHR2_DUTY_CYCLE]));}
      _params[UI_CHR2_UP_TIME] = float(_params[UI_CHR2_DUTY_CYCLE])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[UI_CHR2_UP_TIME] = " + String(_params[UI_CHR2_UP_TIME]));}
      sendMessage("^ " + String(UI_CHR2_UP_TIME) + " UI_ChR2_UP_TIME " + String(_params[UI_CHR2_UP_TIME]));
      _params[UI_CHR2_DOWN_TIME] = cycle_time - float(_params[UI_CHR2_UP_TIME]);
      sendMessage("^ " + String(UI_CHR2_DOWN_TIME) + " UI_ChR2_DOWN_TIME " + String(_params[UI_CHR2_DOWN_TIME]));
    }
    if (_argcopy[0] == UI_CHR2_PERIOD) {
	  _params[UI_CHR2_FREQUENCY] = 1000/float(_argcopy[1]);
      sendMessage("^ " + String(UI_CHR2_FREQUENCY) + " UI_ChR2_FREQUENCY " + String(_params[UI_CHR2_FREQUENCY]));
      cycle_time = float(_argcopy[1]);
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_params[UI_CHR2_DUTY_CYCLE] = " + float(_params[UI_CHR2_DUTY_CYCLE]));}
      _params[UI_CHR2_UP_TIME] = float(_params[UI_CHR2_DUTY_CYCLE])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[UI_CHR2_UP_TIME] = " + String(_params[UI_CHR2_UP_TIME]));}
      sendMessage("^ " + String(UI_CHR2_UP_TIME) + " UI_ChR2_UP_TIME " + String(_params[UI_CHR2_UP_TIME]));
      _params[UI_CHR2_DOWN_TIME] = cycle_time - float(_params[UI_CHR2_UP_TIME]);
      sendMessage("^ " + String(UI_CHR2_DOWN_TIME) + " UI_ChR2_DOWN_TIME " + String(_params[UI_CHR2_DOWN_TIME]));
    }


  	if (_argcopy[0] == UI_ARCHT_DUTY_CYCLE) {
      cycle_time = _params[UI_ARCHT_PERIOD];
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_argcopy[1]) = " + float(_argcopy[1]));}
      _params[UI_ARCHT_UP_TIME] = float(_argcopy[1])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[UI_ARCHT_UP_TIME] = " + String(_params[UI_ARCHT_UP_TIME]));}
      sendMessage("^ " + String(UI_ARCHT_UP_TIME) + " UI_ArchT_UP_TIME " + String(_params[UI_ARCHT_UP_TIME]));
      _params[UI_ARCHT_DOWN_TIME] = cycle_time - float(_params[UI_ARCHT_UP_TIME]);
      sendMessage("^ " + String(UI_ARCHT_DOWN_TIME) + " UI_ArchT_DOWN_TIME " + String(_params[UI_ARCHT_DOWN_TIME]));
    }
    if (_argcopy[0] == UI_ARCHT_UP_TIME) {
      cycle_time = _params[UI_ARCHT_PERIOD];
      _params[UI_ARCHT_DUTY_CYCLE] = 100*float(_argcopy[1]) / cycle_time;
      sendMessage("^ " + String(UI_ARCHT_DUTY_CYCLE) + " UI_ArchT_DUTY_CYCLE " + String(_params[UI_ARCHT_DUTY_CYCLE]));
      _params[UI_ARCHT_DOWN_TIME] = cycle_time - float(_argcopy[1]);
      sendMessage("^ " + String(UI_ARCHT_DOWN_TIME) + " UI_ArchT_DOWN_TIME " + String(_params[UI_ARCHT_DOWN_TIME]));
    }
    if (_argcopy[0] == UI_ARCHT_DOWN_TIME) {
      cycle_time = _params[UI_ARCHT_PERIOD];
      _params[UI_ARCHT_UP_TIME] = cycle_time - float(_argcopy[1]);
      sendMessage("^ " + String(UI_ARCHT_UP_TIME) + " UI_ArchT_UP_TIME " + String(_params[UI_ARCHT_UP_TIME]));
      _params[UI_ARCHT_DUTY_CYCLE] = 100*float(_params[UI_ARCHT_UP_TIME]) / cycle_time;
      sendMessage("^ " + String(UI_ARCHT_DUTY_CYCLE) + " UI_ArchT_DUTY_CYCLE " + String(_params[UI_ARCHT_DUTY_CYCLE]));
    }
    if (_argcopy[0] == UI_ARCHT_FREQUENCY) {
	  _params[UI_ARCHT_PERIOD] = 1000/float(_argcopy[1]);
      sendMessage("^ " + String(UI_ARCHT_PERIOD) + " UI_ArchT_PERIOD " + String(_params[UI_ARCHT_PERIOD]));
      cycle_time = _params[UI_ARCHT_PERIOD];
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_params[UI_ARCHT_DUTY_CYCLE] = " + float(_params[UI_ARCHT_DUTY_CYCLE]));}
      _params[UI_ARCHT_UP_TIME] = float(_params[UI_ARCHT_DUTY_CYCLE])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[UI_ARCHT_UP_TIME] = " + String(_params[UI_ARCHT_UP_TIME]));}
      sendMessage("^ " + String(UI_ARCHT_UP_TIME) + " UI_ArchT_UP_TIME " + String(_params[UI_ARCHT_UP_TIME]));
      _params[UI_ARCHT_DOWN_TIME] = cycle_time - float(_params[UI_ARCHT_UP_TIME]);
      sendMessage("^ " + String(UI_ARCHT_DOWN_TIME) + " UI_ArchT_DOWN_TIME " + String(_params[UI_ARCHT_DOWN_TIME]));
    }
    if (_argcopy[0] == UI_ARCHT_PERIOD) {
	  _params[UI_ARCHT_FREQUENCY] = 1000/float(_argcopy[1]);
      sendMessage("^ " + String(UI_ARCHT_FREQUENCY) + " UI_ArchT_FREQUENCY " + String(_params[UI_ARCHT_FREQUENCY]));
      cycle_time = float(_argcopy[1]);
	  if (_params[_DEBUG]) {sendMessage("cycle_time = " + String(cycle_time) + " float(_params[UI_ARCHT_DUTY_CYCLE] = " + float(_params[UI_ARCHT_DUTY_CYCLE]));}
      _params[UI_ARCHT_UP_TIME] = float(_params[UI_ARCHT_DUTY_CYCLE])/100 * cycle_time;
      if (_params[_DEBUG]) {sendMessage("_params[UI_ARCHT_UP_TIME] = " + String(_params[UI_ARCHT_UP_TIME]));}
      sendMessage("^ " + String(UI_ARCHT_UP_TIME) + " UI_ArchT_UP_TIME " + String(_params[UI_ARCHT_UP_TIME]));
      _params[UI_ARCHT_DOWN_TIME] = cycle_time - float(_params[UI_ARCHT_UP_TIME]);
      sendMessage("^ " + String(UI_ARCHT_DOWN_TIME) + " UI_ArchT_DOWN_TIME " + String(_params[UI_ARCHT_DOWN_TIME]));
    }


    if (_params[_DEBUG]) {sendMessage("Collecting parameter update from Matlab HOST...holding in update_params_state()");}
  }

  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    TRANSITION LIST -- checks conditions, moves to next state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (checkQuit()) {return;}
  if (checkUpdateComplete()) {return;}
  _state = UPDATE_PARAMS_STATE;           // If not done, return to the waiting state
} // End INIT_EXP STATE ------------------------------------------------------------------------------------------------------------------------------------------









/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  INIT_EXP - Initialized exp, awaiting instruction from Boss arduino

  The only purpose of this state is to (re-)initialize the experiment's timer for plotting and other ref events
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void init_exp() {
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ACTION LIST -- initialize the new state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (_state != _prevState) {                       // If ENTERTING READY STATE:
    //-----------------INIT CLOCKS--------------//
    _exp_timer = signedMillis();

    // Send event marker (trial_init) to HOST with timestamp
    sendMessage("&" + String(EVENT_EXP_INIT) + " " + String(signedMillis() - _exp_timer));
    _prevState = _state;                                // Assign _prevState to READY _state
    sendMessage("$" + String(_state));                  // Send  HOST _state entry -- $2 (Ready State)
    
    if (_params[_DEBUG]) {sendMessage("Experiment Started.");}

    _state = WAIT_REQUEST_STATE;            // Go to waiting state
  }
} // End INIT_EXP STATE ------------------------------------------------------------------------------------------------------------------------------------------



/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  WAIT_REQUEST_STATE - Awaiting instruction from Boss arduino

  We mostly sit in this state, waiting for new instructions from UI or from Boss
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void wait_request_state() {
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ACTION LIST -- initialize the new state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (_state != _prevState) {             // If ENTERTING WAITING STATE:
    _prevState = _state;                    // Assign _prevState to READY _state
    sendMessage("$" + String(_state));      // Send  HOST _state entry
    if (_params[_DEBUG]) {sendMessage("Awaiting command from BOSS arduino.");}
  }
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    TRANSITION LIST -- checks conditions, moves to next state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (checkQuit()) {return;}        // If true, quit
  checkTrialStart();
  checkReferenceEvent() ;
  checkStimChR2();            // Turns off ongoing stim if time's up
  // checkStimARCHT();            // "
  if (checkParamUpdate()) {return;}   // If true, we jump to param update state and hold there till done, at expense of some error in timing of stimulation events, which we will record and warn USER thereof
  if (checkChR2request()) {return;}   // If true, jump to ChR2 stimulator state. Check for a new stimulation request. If ongoing stim in progress, this overwrites previous**
  // if (checkARCHTrequest()) {return;}   // If true, jump to ArchT stimulator state. ""
  _state = WAIT_REQUEST_STATE;      // Continue waiting state
} // End WAIT_REQUEST_STATE ------------------------------------------------------------------------------------------------------------------------------------------



/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  CHR2_STATE - Begin stim, send receipt
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void chr2_state() {
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ACTION LIST -- initialize the new state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (_state != _prevState) {                       // If ENTERTING WAITING STATE:
    _prevState = _state;                                // Assign _prevState to READY _state
    sendMessage("$" + String(_state));                  // Send  HOST _state entry
    if (_params[_DEBUG]) {sendMessage("Stimulating ChR2 and sending receipt to BOSS arduino.");}
  }
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    TRANSITION LIST -- checks conditions, moves to next state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (checkQuit()) {return;}        // If true, exit
  checkStimChR2();            // Turns off ongoing stim if time's up
  // checkStimARCHT();            // "
  if (checkChR2request()) {return;}   // If true, means start new stimulation, overwriting previous
  // if (checkARCHTrequest()) {return;}   // If true, initiates new ARCHT request
  _state = WAIT_REQUEST_STATE;      // Return to waiting state
} // End CHR2_STATE ------------------------------------------------------------------------------------------------------------------------------------------

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  ARCHT_STATE - Begin stim, send receipt
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void archt_state() {
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ACTION LIST -- initialize the new state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (_state != _prevState) {                       // If ENTERTING WAITING STATE:
    _prevState = _state;                                // Assign _prevState to READY _state
    sendMessage("$" + String(_state));                  // Send  HOST _state entry
    if (_params[_DEBUG]) {sendMessage("Stimulating ChR2 and sending receipt to BOSS arduino.");}
    // stimulateARCHT(PIN_ARCHT_LAMP_1);
  }
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    TRANSITION LIST -- checks conditions, moves to next state
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  if (checkQuit()) {return;}        // If true, exit
  if (checkChR2request()) {return;}   // If true, means start new stimulation, overwriting previous
  // if (checkARCHTrequest()) {return;}   // If true, initiates new ARCHT request
  checkStimChR2();            // Turns off ongoing stim if time's up
  // checkStimARCHT();            // "
  _state = WAIT_REQUEST_STATE;      // Return to waiting state
} // End ARCHT_STATE ------------------------------------------------------------------------------------------------------------------------------------------









/*****************************************************
  DEFINE TRANSITION-STATE REDUNDANCY FXS
*****************************************************/

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check for Quit Command
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
bool checkQuit() {
  if (_command == 'Q')  {                          // HOST: "QUIT" -> IDLE_STATE
    _state = IDLE_STATE;                             // Set IDLE_STATE
    return true;                                 // Exit Fx
  }
  else {return false;}
} // end checkQuit---------------------------------------------------------------------------------------------------------------------

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check for Trial Start (lamp off)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void checkTrialStart() {
  if (_ready_for_next_trial && !digitalRead(PIN_TRIAL_INIT))  {
    _ready_for_next_trial = false;
    _ready_for_cue = true;
    sendMessage("&" + String(EVENT_TRIAL_INIT) + " " + String(signedMillis() - _exp_timer));
  }
  else if (!_ready_for_next_trial && digitalRead(PIN_TRIAL_INIT))  {
    _ready_for_next_trial = true;
  }
} // end checkTrialStart---------------------------------------------------------------------------------------------------------------------

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check for Reference Event (cue)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void checkReferenceEvent() {
  if (_ready_for_cue && digitalRead(PIN_CUE_ON))  {
    sendMessage("&" + String(EVENT_CUE_ON) + " " + String(signedMillis() - _exp_timer));
    _ready_for_cue = false;
  }
} // end checkTrialStart---------------------------------------------------------------------------------------------------------------------

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check Warnings (is stim ongoing during param update?)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void checkWarnings() {
  if (_BOSS_ChR2_stim_in_progress || _UI_ChR2_stim_in_progress) {
    sendMessage("WARNING: Updated params during ongoing ChR2 stimulation");
    // Send event marker (abort) to HOST with timestamp
    sendMessage("&" + String(EVENT_CHR2_ERROR) + " " + String(signedMillis() - _exp_timer));
  }
  // if (checkARCHTStim()) {
  //  sendMessage("WARNING: Updated params during ongoing ARCHT stimulation");
  //  // Send event marker (abort) to HOST with timestamp
  //  sendMessage("&" + String(EVENT_ARCHT_ERROR) + " " + String(signedMillis() - _exp_timer));
  // }
} // end checkQuit---------------------------------------------------------------------------------------------------------------------


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check for Param Update
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
bool checkParamUpdate() {
  if (_command == 'P') {                          // Received new param from HOST: format "P _paramID _newValue" ('P' for Parameters)
  	// Make a copy of the arguments:
  	_argcopy[0] = _arguments[0];
  	_argcopy[1] = _arguments[1];

    _isParamsUpdateStarted = true;                  // Mark transmission start. Don't start next trial until we've finished.
    _isParamsUpdateDone = false;
    _params[_arguments[0]] = _arguments[1];         // Update parameter. Serial input "P 0 1000" changes the 1st parameter to 1000.
    _state = UPDATE_PARAMS_STATE;                            // Return -> ITI
    if (_params[_DEBUG]) {sendMessage("Parameter " + String(_arguments[0]) + " changed to " + String(_arguments[1]));} 
    return true;
  }
  else {return false;}
} // end checkParamUpdate---------------------------------------------------------------------------------------------------------------------


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check for Param Update Complete
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
bool checkUpdateComplete() {
  if (_command == 'O') {                          // HOST transmission complete: HOST sends 'O' for Over.
    _isParamsUpdateStarted = false;
    _isParamsUpdateDone = true;                     // Mark transmission complete.
    _state = _prevState;                            // Return -> ITI
    _hold = false;
    if (_params[_DEBUG]) {sendMessage("Parameter update complete.");}
    return true;                                         // Exit Fx
  }
  else {return false;}
} // end checkParamUpdate---------------------------------------------------------------------------------------------------------------------


/*****************************************************
  STIMULATION CHECKS
*****************************************************/
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check for ChR2 request

  Change to the CHR2_STATE, send receipt to Boss if Boss called
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
bool checkChR2request() {
  if (digitalRead(PIN_CHR2_TRIGGER_1) && ~(_ChR2_refractory_clock - _time_ChR2_stimulated < 100)) {
    if (_params[_DEBUG]) {sendMessage("Received ChR2 request from BOSS arduino. Sending receipt.");}
    sendMessage("&" + String(EVENT_BOSS_CHR2_REQUEST) + " " + String(signedMillis() - _exp_timer));
    _UI_ChR2_stim_in_progress = false;
    _BOSS_ChR2_stim_in_progress = true;
    setChR2Receipt(true);
    stimulateChR2(PIN_CHR2_LAMP_1);
    // _ChR2_LOW_TIME  = long((1 - (double(_params[CHR2_DUTY_CYCLE])/100))*(1/double(_params[CHR2_FREQUENCY]))*1000);    // make sure matches updated params
    // _ChR2_HIGH_TIME = long((double(_params[CHR2_DUTY_CYCLE])/100)*(1/double(_params[CHR2_FREQUENCY]))*1000);      // "
    sendMessage("&" + String(EVENT_BOSS_CH2R_STIM_BEGIN) + " " + String(_time_ChR2_stimulated - _exp_timer));
    if (_params[_DEBUG]) {sendMessage("Current Duty Cycle high time = " + String(_params[CHR2_UP_TIME]) + " low time = " +  String(_params[CHR2_DOWN_TIME]) + ".");}
    
    _state = CHR2_STATE;
    return true;
  }
  else if (_command == 'C'  && ~(_ChR2_refractory_clock - _time_ChR2_stimulated < 100)) {
    if (_params[_DEBUG]) {sendMessage("Received ChR2 request from HOST. Sending receipt to BOSS.");}
    sendMessage("&" + String(EVENT_UI_CHR2_REQUEST) + " " + String(signedMillis() - _exp_timer));
    _UI_ChR2_stim_in_progress = true;
    _BOSS_ChR2_stim_in_progress = false;
    setChR2Receipt(true);
    stimulateChR2(PIN_CHR2_LAMP_1);
    // _UI_ChR2_HIGH_TIME  = long(double(_params[UI_CHR2_DUTY_CYCLE])/100*1/double(_params[UI_CHR2_FREQUENCY])*1000);   // make sure matches updated params
    // _UI_ChR2_LOW_TIME   = long((1 - double(_params[UI_CHR2_DUTY_CYCLE])/100)*1/double(_params[UI_CHR2_FREQUENCY])*1000); // ""
    sendMessage("&" + String(EVENT_UI_CH2R_STIM_BEGIN) + " " + String(_time_ChR2_stimulated - _exp_timer));
    sendMessage("R"); // Kill UI request
    if (_params[_DEBUG]) {sendMessage("Killed HOST request with R message.");}
    _state = CHR2_STATE;
    return true;
  }
  else if (_command == 'T') {
    if (_params[_DEBUG]) {sendMessage("Initiaing optotagging ladder.");}
    sendMessage("&" + String(EVENT_UI_OPTOTAG) + " " + String(signedMillis() - _exp_timer));
    _UI_ChR2_stim_in_progress = true;
    setChR2Receipt(true);
    sendMessage("&" + String(EVENT_UI_CH2R_STIM_BEGIN) + " " + String(_time_ChR2_stimulated - _exp_timer));
    sendMessage("R"); // Kill UI request
    optotagChR2(PIN_CHR2_LAMP_1);
    if (_params[_DEBUG]) {sendMessage("Killed HOST request with R message, optotagging complete.");}
    _state = CHR2_STATE;
    return true;
  }
  else {return false;}
} // end checkParamUpdate---------------------------------------------------------------------------------------------------------------------


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Begin ChR2 stimulation - resets any ongoing stim!
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void stimulateChR2(int PIN) {
  _time_ChR2_stimulated = signedMillis();
  setChR2Lamp(true);
  _duty_timer_ChR2 = _time_ChR2_stimulated;
  _duty_state_ChR2 = true;
  if (_params[_DEBUG]) {sendMessage("Begin ChR2 Lamp ON.");}
} // end stimulateChR2


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Optotagging Ladder - Overwrites any ongoing stim and can't be interrupted!
  Using Clara Starkweather's paramters (Nat Neuro 2017):
  "we delivered trains of ten 473-nm light pulses, each 5 ms long, 
  at 1, 5, 10, 20 and 50 Hz, with an intensity of 5–20 mW/mm2 at the tip of the fiber."
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void optotagChR2(int PIN) {
  if (_params[_DEBUG]) {sendMessage("Optotagging ladder in progress...");}
  float Hz = 1;
  float duration = 1/Hz * 1000;
  if (_params[_DEBUG]) {sendMessage("10x 1 Hz (5ms up, " + String(duration) + ")...");}
  for ( int stims = 1; stims<=10; stims = stims + 1 ) {
      setChR2Lamp(true);
      if (_params[_DEBUG]) {sendMessage("Delay 5ms...");}
      delay(5);
      setChR2Lamp(false);
      if (_params[_DEBUG]) {sendMessage("Delay " + String(duration-5) + "ms...");}
      delay(duration - 5);
  }
  if (_params[_DEBUG]) {sendMessage("Delay 10 sec...");}
  delay(10000);


  
  Hz = 5;
  duration = 1/Hz * 1000;
  if (_params[_DEBUG]) {sendMessage("10x 5 Hz (5ms up " + String(duration) + ")...");}
  for ( int stims = 1; stims<=10; stims = stims + 1 ) {
      setChR2Lamp(true);
      if (_params[_DEBUG]) {sendMessage("Delay 5ms...");}
      delay(5);
      setChR2Lamp(false);
      if (_params[_DEBUG]) {sendMessage("Delay " + String(duration-5) + "ms...");}
      delay(duration - 5);
  }
  if (_params[_DEBUG]) {sendMessage("Delay 10 sec...");}
  delay(10000);


  
  Hz = 10;
  duration = 1/Hz * 1000;
  if (_params[_DEBUG]) {sendMessage("10x 10 Hz (5ms up " + String(duration) + ")...");}
  for ( int stims = 1; stims<=10; stims = stims + 1 ) {
      setChR2Lamp(true);
      if (_params[_DEBUG]) {sendMessage("Delay 5ms...");}
      delay(5);
      setChR2Lamp(false);
      if (_params[_DEBUG]) {sendMessage("Delay " + String(duration-5) + "ms...");}
      delay(duration - 5);
  }
  if (_params[_DEBUG]) {sendMessage("Delay 10 sec...");}
  delay(10000);


  
  Hz = 20;
  duration = 1/Hz * 1000;
  if (_params[_DEBUG]) {sendMessage("10x 20 Hz (5ms up " + String(duration) + ")...");}
  for ( int stims = 1; stims<=10; stims = stims + 1 ) {
      setChR2Lamp(true);
      if (_params[_DEBUG]) {sendMessage("Delay 5ms...");}
      delay(5);
      setChR2Lamp(false);
      if (_params[_DEBUG]) {sendMessage("Delay " + String(duration-5) + "ms...");}
      delay(duration - 5);
  }
  if (_params[_DEBUG]) {sendMessage("Delay 10 sec...");}
  delay(10000);

  
  Hz = 50;
  duration = 1/Hz * 1000;
  if (_params[_DEBUG]) {sendMessage("10x 50 Hz (5ms up " + String(duration) + ")...");}
  for ( int stims = 1; stims<=10; stims = stims + 1 ) {
      setChR2Lamp(true);
      if (_params[_DEBUG]) {sendMessage("Delay 5ms...");}
      delay(5);
      setChR2Lamp(false);
      if (_params[_DEBUG]) {sendMessage("Delay " + String(duration-5) + "ms...");}
      delay(duration - 5);
  }
  if (_params[_DEBUG]) {sendMessage("Delay 10 sec...");}
  delay(10000);

} // end optotagChR2

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check if ChR2 stimulation is on
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void checkStimChR2() {
  if (_BOSS_ChR2_stim_in_progress) {
    if (_params[CANCEL_ENABLED] && digitalRead(PIN_CANCEL)) {
      setChR2Lamp(false);
      setChR2Receipt(false);
      sendMessage("&" + String(EVENT_BOSS_CH2R_STIM_END) + " " + String(signedMillis() - _exp_timer));
      _BOSS_ChR2_stim_in_progress = false;
      _UI_ChR2_stim_in_progress = false;
      _duty_state_ChR2 = false;
      _time_ChR2_stimulated = 0;
      _resultCode = CODE_CHR2_CANCEL;                   // Send Result Code to Arduino
      sendMessage("`" + String(_resultCode));           // Send result to HOST
      _resultCode = -1;                                 // Reset result code to null state
      if (_params[_DEBUG]) {sendMessage("CANCELED BY EVENT INPUT - End Boss ChR2 stim (lamp OFF).");}
    }                         
    else if (signedMillis() - _time_ChR2_stimulated > _params[CHR2_DURATION]) {
      setChR2Lamp(false);
      setChR2Receipt(false);
      sendMessage("&" + String(EVENT_BOSS_CH2R_STIM_END) + " " + String(signedMillis() - _exp_timer));
      _BOSS_ChR2_stim_in_progress = false;
      _UI_ChR2_stim_in_progress = false;
      _duty_state_ChR2 = false;
      _time_ChR2_stimulated = 0;
      _resultCode = CODE_CHR2;                      // Send Result Code to Arduino
      sendMessage("`" + String(_resultCode));           // Send result to HOST
      _resultCode = -1;                                 // Reset result code to null state
      if (_params[_DEBUG]) {sendMessage("End Boss ChR2 stim (lamp OFF).");}
    }
    else if (signedMillis() - _duty_timer_ChR2 > _params[CHR2_UP_TIME] && _duty_state_ChR2) {
      if (_params[_DEBUG]) {sendMessage("ChR2 Duty UP. HIGH time (ms) = " + String(_params[CHR2_UP_TIME]) + "signedMillis()-dutytimer = " + String(signedMillis() - _duty_timer_ChR2));}
      setChR2Lamp(false);
      _duty_state_ChR2 = false;
      _duty_timer_ChR2 = signedMillis();
    }
    else if (signedMillis() - _duty_timer_ChR2 > _params[CHR2_DOWN_TIME] && !_duty_state_ChR2) {
      if (_params[_DEBUG]) {sendMessage("ChR2 Duty LO. LOW time (ms) = " + String(_params[CHR2_DOWN_TIME]) + "signedMillis()-dutytimer = " + String(signedMillis() - _duty_timer_ChR2));}
      _duty_state_ChR2 = true;
      setChR2Lamp(true);
      _duty_timer_ChR2 = signedMillis();
    }
  }
  else if (_UI_ChR2_stim_in_progress) {
    if (signedMillis() - _time_ChR2_stimulated > _params[UI_CHR2_DURATION]) {
      setChR2Lamp(false);
      setChR2Receipt(false);
      sendMessage("&" + String(EVENT_UI_CH2R_STIM_END) + " " + String(signedMillis() - _exp_timer));
      _BOSS_ChR2_stim_in_progress = false;
      _UI_ChR2_stim_in_progress = false;
      _duty_state_ChR2 = false;
      _time_ChR2_stimulated = 0;
      _resultCode = CODE_UI_CHR2;                   // Send Result Code to Arduino
      sendMessage("`" + String(_resultCode));           // Send result to HOST
      _resultCode = -1;                                 // Reset result code to null state
      if (_params[_DEBUG]) {sendMessage("End UI ChR2 stim (lamp OFF).");}
    }
    else if (signedMillis() - _duty_timer_ChR2 > _params[UI_CHR2_UP_TIME] && _duty_state_ChR2) {
      setChR2Lamp(false);
      _duty_timer_ChR2 = signedMillis();
      _duty_state_ChR2 = false;
      if (_params[_DEBUG]) {sendMessage("ChR2 Duty UP.");}
    }
    else if (signedMillis() - _duty_timer_ChR2 > _params[UI_CHR2_DOWN_TIME] && !_duty_state_ChR2) {
      setChR2Lamp(true);
      _duty_state_ChR2 = true;
      _duty_timer_ChR2 = signedMillis();
      if (_params[_DEBUG]) {sendMessage("ChR2 Duty LOW.");}
    }
  }
} // end checkStimChR2---------------------------------------------------------------------------------------------------------------------








/*****************************************************
  HARDWARE CONTROLS
*****************************************************/
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Set ChR2 Lamp (ON/OFF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void setChR2Lamp(bool turnOn) {
  if (turnOn) {
    digitalWrite(PIN_CHR2_LAMP_1, HIGH);
  }
  else {
    digitalWrite(PIN_CHR2_LAMP_1, LOW);
  }
} // end Set ChR2 Lamp---------------------------------------------------------------------------------------------------------------------

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Check Lamp On (ON/OFF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
bool checkLampON(int PIN) {
  if (digitalRead(PIN)) {
    return true;
  }
  else {
    return false;
  }
} // end Set ChR2 Lamp---------------------------------------------------------------------------------------------------------------------

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Set ChR2 Receipt (ON/OFF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void setChR2Receipt(bool turnOn) {
  if (turnOn) {
    digitalWrite(PIN_CHR2_RECEIPT_1, HIGH);
  }
  else {
    digitalWrite(PIN_CHR2_RECEIPT_1, LOW);
  }
} // end Set ChR2 Lamp---------------------------------------------------------------------------------------------------------------------



/*****************************************************
  SERIAL COMMUNICATION TO HOST
*****************************************************/

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  SEND MESSAGE to HOST
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void sendMessage(String message)   // Capital (String) because is defining message as an object of type String from arduino library
{
  Serial.println(message);
} // end Send Message---------------------------------------------------------------------------------------------------------------------

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  GET COMMAND FROM HOST (single character)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
char getCommand(String message)
{
  message.trim();                 // Remove leading and trailing white space
  return message[0];              // Parse message string for 1st character (the command)
} // end Get Command---------------------------------------------------------------------------------------------------------------------

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  GET ARGUMENTS (of the command) from HOST (2 int array)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void getArguments(String message, int *_arguments)  // * to initialize array of strings(?)
{
  _arguments[0] = 0;              // Init Arg 0 to 0 (reset)
  _arguments[1] = 0;              // Init Arg 1 to 0 (reset)

  message.trim();                 // Remove leading and trailing white space from MESSAGE

  //----Remove command (first character) from string:-----//
  String parameters = message;    // The remaining part of message is now "parameters"
  parameters.remove(0,1);         // Remove the command character and # (e.g., "P#")
  parameters.trim();              // Remove any spaces before next char

  //----Parse first (optional) integer argument-----------//
  String intString = "";          // init intString as a String object. intString concatenates the arguments as a string
  while ((parameters.length() > 0) && (isDigit(parameters[0]))) 
  {                               // while the first argument in parameters has digits left in it unparced...
    intString += parameters[0];       // concatenate the next digit to intString
    parameters.remove(0,1);           // delete the added digit from "parameters"
  }
  _arguments[0] = intString.toInt();  // transform the intString into integer and assign to first argument (Arg 0)


  //----Parse second (optional) integer argument----------//
  parameters.trim();              // trim the space off of parameters
  intString = "";                 // reinitialize intString
  while ((parameters.length() > 0) && (isDigit(parameters[0]))) 
  {                               // while the second argument in parameters has digits left in it unparced...
    intString += parameters[0];       // concatenate the next digit to intString
    parameters.remove(0,1);           // delete the added digit from "parameters"
  }
  _arguments[1] = intString.toInt();  // transform the intString into integer and assign to second argument (Arg 1)
} // end Get Arguments---------------------------------------------------------------------------------------------------------------------


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  INIT HOST (send States, Names/Value of Parameters to HOST)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void hostInit()
{
  //------Send state names and which states allow parameter update-------//
  for (int iState = 0; iState < _NUM_STATES; iState++)
  {// For each state, send "@ (number of state) (state name) (0/1 can update params)"
      sendMessage("@ " + String(iState) + " " + _stateNames[iState] + " " + String(_stateCanUpdateParams[iState]));
  }

  //-------Send event marker codes---------------------------------------//
  /* Note: "&" reserved for uploading new event marker and timestamp. "+" is reserved for initially sending event marker names */
  for (int iCode = 0; iCode < _NUM_OF_EVENT_MARKERS; iCode++)
  {// For each state, send "+ (number of event marker) (event marker name)"
      sendMessage("+ " + String(iCode) + " " + _eventMarkerNames[iCode]); // Matlab adds 1 to each code # to index from 1-n rather than 0-n
  }

  //-------Send param names and default values---------------------------//
  for (int iParam = 0; iParam < _NUM_PARAMS; iParam++)
  {// For each param, send "# (number of param) (param names) (param init value)"
      sendMessage("# " + String(iParam) + " " + _paramNames[iParam] + " " + String(_params[iParam]));
  }
  //--------Send result code interpretations.-----------------------------//
  for (int iCode = 0; iCode < _NUM_RESULT_CODES; iCode++)
  {// For each result code, send "* (number of result code) (result code name)"
      sendMessage("* " + String(iCode) + " " + _resultCodeNames[iCode]);
  }
  sendMessage("~");                           // Tells PC that Arduino is on (Send Message is a LF Function)
}

long signedMillis()
{
  double time = (double)(millis());
  return time;
}














