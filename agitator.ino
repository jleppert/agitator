#include <AFMotor.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>
#define AGITATORS 4
#define REVERSE_TIME 0.5
#define STATE_UPDATE_INTERVAL 20

AF_DCMotor agitators[AGITATORS] = { AF_DCMotor(1), AF_DCMotor(2), AF_DCMotor(3), AF_DCMotor(4) };

String inputString = "";
boolean stringComplete = false;

SimpleTimer stateUpdater;
StaticJsonBuffer<300> jsonWriteBuffer;
StaticJsonBuffer<200> jsonReadBuffer;

char strBuffer[200];

struct AgitatorState {
  int direction;
  int speed;
  unsigned long currentInterval;
  unsigned long totalInterval;
  unsigned long duration;
};
AgitatorState steady = { RELEASE, 0, 0, 0, 0 };
AgitatorState home = { BACKWARD, 255, 0, 0, 1500 };
AgitatorState agitatorsState[AGITATORS];
AgitatorState pendingState[AGITATORS];

void setup() {
  Serial.begin(9600);
  inputString.reserve(200);
  stateUpdater.setInterval(STATE_UPDATE_INTERVAL, updateState);
  for(int i = 0; i < AGITATORS; i++) {
    agitatorsState[i] = steady;
    pendingState[i] = steady;
  }
  sendState();
}

void resetSerialState() {
  inputString = "";
  stringComplete = false;
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

void loop() {
  if (stringComplete) {
    inputString.toCharArray(strBuffer, 200);
    JsonObject& root = jsonReadBuffer.parseObject(strBuffer);
    if(root.success()) {
      const char* command = root["command"];
      const int agitator  = root["agitator"];
      
      String cmd(command);
      if(cmd == "start") {
        const unsigned long interval = root["interval"];
        const int speed = root["speed"];
        const unsigned long duration = root["duration"];
        agitatorsState[agitator] = home;
        pendingState[agitator] = { FORWARD, speed, 0, interval, duration };
        sendState();
      } else if(cmd == "stop") {
        agitatorsState[agitator] = steady;
        sendState();
      } else if(cmd == "home") {
        agitatorsState[agitator] = home;
        sendState();
      } else if(cmd == "state") {
        sendState();
      } else {
        invalidCommand();
      }
    } else {
      invalidCommand();
    }
    
    resetSerialState();
  }
  
  stateUpdater.run();
}

void successfulCommand() {
  JsonObject& root = jsonWriteBuffer.createObject();
  root["success"] = true;
  root.printTo(Serial);
  Serial.println();
}

void invalidCommand() {
  JsonObject& root = jsonWriteBuffer.createObject();
  root["success"] = false;
  root["error"] = "Invalid command or JSON";
  root["command"] = inputString;
  root.printTo(Serial);
  Serial.println();
}

void sendState() {
  JsonObject& root = jsonWriteBuffer.createObject();
  JsonArray& stateArray = jsonWriteBuffer.createArray();
  for(int i = 0; i < AGITATORS; i++) {
    JsonObject& jsonState = jsonWriteBuffer.createObject();
    jsonState["agitator"] = i;
    if(agitatorsState[i].direction == RELEASE) {
      jsonState["direction"] = "stopped";
    } else if(agitatorsState[i].direction == FORWARD) {
      jsonState["direction"] = "forward";
    } else if(agitatorsState[i].direction == BACKWARD) {
      jsonState["direction"] = "backward";
    } else {
      jsonState["direction"] = "unknown";
    }
    jsonState["speed"] = agitatorsState[i].speed;
    jsonState["currentInterval"] = agitatorsState[i].currentInterval;
    jsonState["totalInterval"] = agitatorsState[i].totalInterval;
    jsonState["duration"] = agitatorsState[i].duration;
    stateArray.add(jsonState);
  }
  root["success"] = true;
  root.set("state", stateArray);
  root.printTo(Serial);
  Serial.println();
}

void updateState() {
  for(int i = 0; i < AGITATORS; i++) {
    AgitatorState state = agitatorsState[i];
    
    if(state.duration > 0) {
       if(state.totalInterval > 0) {
         if(state.currentInterval < state.totalInterval) {
           if(state.direction == BACKWARD) {
             state.currentInterval = state.currentInterval + (STATE_UPDATE_INTERVAL * REVERSE_TIME);
           } else {
             state.currentInterval = state.currentInterval + STATE_UPDATE_INTERVAL;
           }
         } else {
           if(state.direction == FORWARD) {
             state.direction = BACKWARD;
             state.currentInterval = 0;
           } else {
             state.direction = FORWARD;
             state.currentInterval = 0;
           }
         }
       }
       if(state.duration < STATE_UPDATE_INTERVAL) {
         state.duration = 0;
       } else {
         state.duration = state.duration - STATE_UPDATE_INTERVAL;
       }
    } else {
      AgitatorState pState = pendingState[i];
      if(pState.duration > 0) {
        state = pState;
        pendingState[i] = steady;
      } else {
        state = steady;
      }
    }
    
    agitatorsState[i] = state;
    agitators[i].setSpeed(state.speed);
    agitators[i].run(state.direction);
  }
}
