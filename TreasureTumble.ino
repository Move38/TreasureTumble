//TREASURE TUMBLE
//Game code by Jacob Surovsky
//Gravity code by Josh Levine

//Website: jacobsurovsky.com
//Instagram: @coolthingsbyjacob

const byte NO_PARENT_FACE = FACE_COUNT ;   // Signals that we do not currently have a parent

byte parent_face = NO_PARENT_FACE;

Timer lockout_timer;    // This prevents us from forming packet loops by giving changes time to die out.
// Remember that timers init to expired state

byte bottomFace;    // Which one of our faces is pointing north? Only valid when parent_face != NO_PARENT_FACE

const byte IR_IDLE_VALUE = 7;   // A value we send when we do not know which way is north

const int LOCKOUT_TIMER_MS = 250;               // This should be long enough for very large loops, but short enough to be unnoticable

//------------------------------------------------------

#define NICE_BLUE makeColorHSB(110, 255, 255) //more like cyan <-- SUCH A NICE COLOR TOO
#define PURPLE makeColorHSB(200, 255, 230)
#define FEATURE_COLOR makeColorHSB(25, 255, 240) //very red orange
#define TREASURE_COLOR makeColorHSB(90, 225, 255) //an emerald green
#define BG_COLOR makeColorHSB(50, 200, 255) //a temple tan ideally
//#define CRUMBLE_COLOR makeColorHSB(30, 255, 100)

bool amGod = false;
byte gravitySignal[6] = {IR_IDLE_VALUE, IR_IDLE_VALUE, IR_IDLE_VALUE, IR_IDLE_VALUE, IR_IDLE_VALUE, IR_IDLE_VALUE};

enum spawnerSignals {IM_SPAWNER, NOT_SPAWNER};
byte spawnerSignal[6] = {NOT_SPAWNER, NOT_SPAWNER, NOT_SPAWNER, NOT_SPAWNER, NOT_SPAWNER, NOT_SPAWNER};

enum treasureSignals {SENDING, RECEIVING, TREASURE, BLANK};
byte treasureSignal[6] = {BLANK, BLANK, BLANK, BLANK, BLANK, BLANK,};

enum blinkRoles {WALL, BUCKET, SPAWNER};
byte blinkRole = WALL;

bool bLongPress;
bool bChangeRole;

bool didIRandomize = true;

byte randomWallRole;

byte startFace;
byte goFace;
byte leftFace;
byte rightFace;
byte topLeftFace;
byte topRightFace;
byte topFace;

//wall role stuff
byte neighborCount;
byte firstFace;
byte secondFace;

//SPAWNER
bool treasurePrimed = false;
bool dropTreasure;
//autodrop stuff
bool autoDrop;
Timer autoDropTimer;

Timer crumbleTimer;
#define CRUMBLE_TIME 6000

byte treasureFace;
bool isTreasureOnMyBlink;
byte stepCounter;
byte treasureCounter; //counts how many times a piece hears "TREASURE" per time cycle

bool treasureFaces[6] = {false, false, false, false, false, false};

Timer treasureTimer;
#define TREASURE_TIME 150

void setup() {
  randomize();
}

void loop() {

  setRole();

  leftFace = (bottomFace + 1) % 6;
  topLeftFace = (bottomFace + 2) % 6;
  topFace = (bottomFace + 3) % 6;
  topRightFace = (bottomFace + 4) % 6;
  rightFace = (bottomFace + 5) % 6;

  //send data
  FOREACH_FACE(f) {
    byte sendData = (treasureSignal[f] << 4) | (spawnerSignal[f] << 3) | (gravitySignal[f]);
    setValueSentOnFace(sendData, f);
  }
}

void wallLoop() {
  if (bChangeRole) {
    blinkRole = BUCKET;
    bChangeRole = false;
  }

  amGod = false;

  setColor(dim(BG_COLOR, 190));
  setColorOnFace(dim(BG_COLOR, 140), topLeftFace);
  setColorOnFace(dim(BG_COLOR, 140), topRightFace);


  FOREACH_FACE(f) {
    spawnerSignal[f] = NOT_SPAWNER;

    if (isBucket(f)) { //do I have a neighbor and are they shouting IM_BUCKET?
      byte bucketNeighbor = (f + 2) % 6; //and what about their neighbor?
      if (isBucket(bucketNeighbor)) {
        bottomFace = (f + 1) % 6;
        amGod = true; //I get to decide what direction gravity is for the entire game! Yippee!
      }
    }
  }

  FOREACH_FACE(f) {
    treasureFaces[f] = false;
  }

  countNeighbors(); //counts how many neighbors each blink has and stores that information

//  bool amDeathtrap;

//  if (buttonDoubleClicked()) {
//    amDeathtrap = !amDeathtrap;
//  }
//
//  if (amDeathtrap == true) {
//    deathtrapLoop();
//  } else if (amDeathtrap == false) {
    setWallRole(); //figures out what role to assign the blink based off of the neighbors counted
//  }
  gravityLoop(); //sets gravity and stuff

  FOREACH_FACE(f) {

    if (treasureSignal[f] == BLANK) {
      //listen for sending, go to receiving

      if (!isValueReceivedOnFaceExpired(f)) {
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == SENDING) {
          treasureSignal[f] = RECEIVING;
        }
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == TREASURE) {
          treasureSignal[f] = BLANK;
        }
      }
    } else if (treasureSignal[f] == SENDING) {
      //listen for receiving, go to treasure
      //if no neighbor, go back to blank

      if (!isValueReceivedOnFaceExpired(f)) {
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == RECEIVING) {
          treasureSignal[f] = TREASURE;
        }
      } else if (isValueReceivedOnFaceExpired(f)) {
        treasureSignal[f] = BLANK;
      }
    } else if (treasureSignal[f] == RECEIVING) {
      //listen for treasure, go to blank
      //if no neighbor, go back to blank

      if (!isValueReceivedOnFaceExpired(f)) {
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == TREASURE) {
          treasureSignal[f] = BLANK;
          startFace = f;
          stepCounter = 0;
          isTreasureOnMyBlink = true;
        }
      } else if (isValueReceivedOnFaceExpired(f)) {
        treasureSignal[f] = BLANK;
      }
    } else if (treasureSignal[f] == TREASURE) {
      //listen for blank, go to blank
      //if no neighbor, go back to blank

      if (!isValueReceivedOnFaceExpired(f)) {
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == BLANK) {
          treasureSignal[f] = BLANK;
        }
      } else if (isValueReceivedOnFaceExpired(f)) {
        treasureSignal[f] = BLANK;
      }
    }
  }

  treasureTumble(); //listens for if treasure is on the face and runs the animation cycle
}

bool isBucket (byte face) {
  if (!isValueReceivedOnFaceExpired(face)) { //I have a neighbor
    if (getGravitySignal(getLastValueReceivedOnFace(face)) == 6) { //if the neighbor is a bucket, return true
      return true;
    } else if (getGravitySignal(getLastValueReceivedOnFace(face)) != 6) { //if the neighbor isn't a bucket, return false
      return false;
    }
  } else {
    return false; //or if I don't have a neighbor, that's also a false
  }
}

byte treasureCount = 0;
bool bCountTreasure;

void bucketLoop() {
  if (bChangeRole) {
    blinkRole = WALL;
    bChangeRole = false;
  }

  gravityLoop();

  setColor(dim(FEATURE_COLOR, 150));
  setColorOnFace(BG_COLOR, bottomFace);


  if (buttonDoubleClicked()) {
    treasureCount = 0;
  }

  FOREACH_FACE(f) {
    gravitySignal[f] = 6; //6 means bucket, I don't make the rules
    spawnerSignal[f] = NOT_SPAWNER;
    //    treasureSignal[f] = BLANK;

    if (treasureSignal[f] == BLANK) {
      //listen for sending, go to receiving

      if (!isValueReceivedOnFaceExpired(f)) {
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == SENDING) {
          treasureSignal[f] = RECEIVING;
          bCountTreasure = true;
        }
      }
    }
  }

  if (treasureCount > 0) {
    setColorOnFace(TREASURE_COLOR, bottomFace);
    if (treasureCount > 5) {
      setColorOnFace(TREASURE_COLOR, leftFace);
      if (treasureCount > 10) {
        setColorOnFace(TREASURE_COLOR, rightFace);
        if (treasureCount > 15) {
          setColorOnFace(TREASURE_COLOR, topLeftFace);
          if (treasureCount > 20) {
            setColorOnFace(TREASURE_COLOR, topRightFace);
            if (treasureCount > 25) {
              setColorOnFace(TREASURE_COLOR, topFace);
              if (treasureCount > 30) {
                setColor(PURPLE);
              }
            }
          }
        }
      }
    }
  }


  FOREACH_FACE(f) {
    if (treasureSignal[f] == RECEIVING) {
      //listen for treasure, go to blank
      //if no neighbor, go back to blank

      if (!isValueReceivedOnFaceExpired(f)) {
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == TREASURE) {
          setColor(WHITE);
          treasureSignal[f] = BLANK;
          countTreasure();
        }
      }
    }
  }
}

void countTreasure() {
  if (bCountTreasure == true) {
    treasureCount = treasureCount + 1;
    bCountTreasure = false;
  }
}

void spawnerLoop() {
  if (bChangeRole) {
    blinkRole = WALL;
    bChangeRole = false;
  }


  FOREACH_FACE(f) {
    gravitySignal[f] = 7; //7 means ignore me I don't speak about gravity
    spawnerSignal[f] = IM_SPAWNER;
    //    treasureSignal[f] = BLANK;
  }

  gravityLoop();

  setColor(dim(TREASURE_COLOR, 120));

  if (buttonDoubleClicked()) { //double click to turn auto drop on and off
    autoDrop = !autoDrop;
  }

  if (autoDrop == true) { //if we're autodropping...
    setColorOnFace(WHITE, topFace);

    if (autoDropTimer.isExpired()) {
      setColor(WHITE);
      treasureSignal[bottomFace] = SENDING; //send out a treasure every 2 seconds
      autoDropTimer.set(2000);
    }
  } else if (autoDrop == false) { //if we're not auto dropping, all of this complicated listening/not listening logic
    treasureSignal[bottomFace] = BLANK;
    //setting treasure animation
    if (treasurePrimed == true) {
      treasurePrimedAnimation();
    } else {
      setColor(dim(TREASURE_COLOR, 120));
    }

    if (dropTreasure == true) {
      if (treasurePrimed == true) {
        treasureSignal[bottomFace] = SENDING;
      }
    }
  }

  FOREACH_FACE(f) {

    if (treasureSignal[f] == SENDING) {
      //listen for receiving, go to treasure
      //if no neighbor, go back to blank

      if (!isValueReceivedOnFaceExpired(f)) {
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == RECEIVING) {
          treasureSignal[f] = TREASURE;
          dropTreasure = false;
          treasurePrimed = false;
        }
      } else if (isValueReceivedOnFaceExpired(f)) {
        treasureSignal[f] = BLANK;
      }
    } else if (treasureSignal[f] == TREASURE) {
      //listen for blank, go to blank
      //if no neighbor, go back to blank

      if (!isValueReceivedOnFaceExpired(f)) {
        if (getTreasureSignal(getLastValueReceivedOnFace(f)) == BLANK) {
          treasureSignal[f] = BLANK;
        }
      } else if (isValueReceivedOnFaceExpired(f)) {
        treasureSignal[f] = BLANK;
      }
    }
  }
}

#define SHIMMER_TIME 100
byte shimmerFace;
Timer shimmerTimer;

void treasurePrimedAnimation() {

  if (shimmerTimer.isExpired()) {
    shimmerFace = (shimmerFace + 1) % 6;
    shimmerTimer.set(SHIMMER_TIME);
  }

  setColorOnFace(TREASURE_COLOR, shimmerFace);
}

void gravityLoop() {

  if (amGod) {

    // I am the center blink!

    FOREACH_FACE(f) {

      gravitySignal[f] = (f + 6 - bottomFace) % 6; //eveything is being broadcast relative to the bottomFace

    }

    parent_face = NO_PARENT_FACE;
    lockout_timer.set(LOCKOUT_TIMER_MS);

    // That's all for the center blink!

  } else {

    // I am not the center blink!

    if (parent_face == NO_PARENT_FACE ) {   // If we have no parent, then look for one

      if (lockout_timer.isExpired()) {      // ...but only if we are not on lockout

        FOREACH_FACE(f) {

          if (!isValueReceivedOnFaceExpired(f)) {

            if (getGravitySignal(getLastValueReceivedOnFace(f)) < FACE_COUNT) {

              // Found a parent!

              parent_face = f;

              // Compute the oposite face of the one we are reading from

              byte our_oposite_face = (f + (FACE_COUNT / 2) ) % FACE_COUNT;

              // Grab the compass heading from the parent face we are facing
              // (this is also compass heading of our oposite face)

              byte parent_face_heading = getGravitySignal(getLastValueReceivedOnFace(f));

              // Ok, so now we know that `our_oposite_face` has a heading of `parent_face_heading`.

              // Compute which face is our north face from that

              bottomFace = ( (our_oposite_face + FACE_COUNT) - parent_face_heading) % FACE_COUNT;     // This +FACE_COUNT is to keep it positive

              // I guess we could break here, but breaks are ugly so instead we will keep looking
              // and use whatever the highest face with a parent that we find.
            }
          }
        }
      }

    } else {

      // Make sure our parent is still there and good

      if (isValueReceivedOnFaceExpired(parent_face) || ( getGravitySignal(getLastValueReceivedOnFace(parent_face )) == IR_IDLE_VALUE) ) {

        // We had a parent, but our parent is now gone

        parent_face = NO_PARENT_FACE;

        //setValueSentOnAllFaces( IR_IDLE_VALUE );  // Propigate the no-parentness to everyone resets viraly
        FOREACH_FACE(ff) {
          gravitySignal[ff] = IR_IDLE_VALUE;
        }
        lockout_timer.set(LOCKOUT_TIMER_MS);    // Wait this long before accepting a new parent to prevent a loop
      }
    }


    if (parent_face != NO_PARENT_FACE) {
      //I am connected to the tower
      didIRandomize = false;
      dropTreasure = true;
    } else {
      //I'm not connected to the tower
      treasurePrimed = true;
    }

    // Set our output values relative to our north

    FOREACH_FACE(f) {

      if (parent_face == NO_PARENT_FACE ||  f == parent_face ) {

        //setValueSentOnFace( IR_IDLE_VALUE , f );
        gravitySignal[f] = IR_IDLE_VALUE;
      } else {

        // Map directions onto our face indexes.
        // (It was surpsringly hard for my brain to wrap around this simple formula!)

        //setValueSentOnFace( ((f + FACE_COUNT) - bottomFace) % FACE_COUNT , f  );
        gravitySignal[f] = ((f + FACE_COUNT) - bottomFace) % FACE_COUNT;
      }
    }
  }
}

bool neighborFaces[6] = {false, false, false, false, false, false};

void countNeighbors() { //count how many neighbors are around me and keep it in an array

  neighborCount = 0;

  FOREACH_FACE(f) {
    if (getSpawnerSignal(getLastValueReceivedOnFace(f)) == NOT_SPAWNER) {
      if (!isValueReceivedOnFaceExpired(f)) {

        neighborCount++;
        neighborFaces[f] = true;
      } else {
        neighborFaces[f] = false;
      }
    }
  }
}

void setWallRole() { //this is where we auto-assign what wall role to be

  if (isAlone()) {
    setColor(dim(BG_COLOR, 190));
    setColorOnFace(dim(BG_COLOR, 140), topLeftFace);
    setColorOnFace(dim(BG_COLOR, 140), topRightFace);

  }

  else if (neighborFaces[bottomFace] == false) { //no one directly beneath me
    if (neighborFaces[leftFace] == true && neighborFaces[rightFace] == false) { //but one neighbor to the left
      goFace = leftFace;
      goSideLoop();
    }
    else if (neighborFaces[leftFace] == false && neighborFaces[rightFace] == true) { //but one neighbor to the right
      goFace = rightFace;
      goSideLoop();
    }
    else if (neighborFaces[leftFace] == true && neighborFaces[rightFace] == true) { //but two neighbors to the left and right
      switcherLoop();
    }
    else if (neighborFaces[leftFace] == false && neighborFaces[rightFace] == false) { //no one underneath me anywhere
      deathtrapLoop();
    }

  } else if (neighborFaces[bottomFace] == true) { //someone directly beneath me
    if (neighborFaces[leftFace] == false && neighborFaces[rightFace] == false) { //but no one to either side
      goFace = bottomFace;
      goSideLoop();
    }
    else if (neighborFaces[leftFace] == true && neighborFaces[rightFace] == false) { //and someone to the left
      if (getGravitySignal(getLastValueReceivedOnFace(bottomFace)) == 6) {
        goFace = bottomFace;
      } else {
        goFace = leftFace;
      }

      goSideLoop();
    }
    else if (neighborFaces[leftFace] == false && neighborFaces[rightFace] == true) { //and someone to the right
      if (getGravitySignal(getLastValueReceivedOnFace(bottomFace)) == 6) {
        goFace = bottomFace;
      } else {
        goFace = rightFace;
      }
      goSideLoop();
    }
    else if (neighborFaces[leftFace] == true && neighborFaces[rightFace] == true) { //there's three people beneath me

      if (neighborCount == 3) { //and somehow that's it
        splitterLoop();
      }
      else if (neighborCount == 4) {
        if (neighborFaces[topFace] == true) { //one blink is directly above me, total of 4 neighbors
          splitterLoop();
        } else { //one blink is above me on either side, for total of 4 neighbors
          switcherLoop();
        }
      } else if (neighborCount == 5) { //two blinks are above me, total of 5 neighbors
        switcherLoop();
      } else if (neighborCount == 6) { //I'm fully surrounded, total of six neighbors
        splitterLoop();
      }
    }
  }
}

void goSideLoop() {

  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      setColorOnFace(OFF, f);
    }
  }

  setColorOnFace(FEATURE_COLOR, leftFace);
  setColorOnFace(FEATURE_COLOR, bottomFace);
  setColorOnFace(FEATURE_COLOR, rightFace);
  setColorOnFace(OFF, goFace);

  if (stepCounter == 0) {
    treasureFaces[startFace] = true;
  }
  else if (stepCounter == 1) {
    treasureFaces[startFace] = true;
  }
  else if (stepCounter == 2) {
    treasureFaces[startFace] = true;
    treasureFaces[goFace] = true;
  }
  else if (stepCounter == 3) {
    treasureFaces[goFace] = true;
    treasureSignal[goFace] = SENDING;
  }
  else if (stepCounter == 4) {

    isTreasureOnMyBlink = false;
  }
}

void splitterLoop() {
  setColorOnFace(PURPLE, (bottomFace + 1) % 6);
  setColorOnFace(PURPLE, (bottomFace + 5) % 6);
  setColorOnFace(NICE_BLUE, bottomFace);

  if (stepCounter == 0) {
    treasureFaces[startFace] = true;
  }
  else if (stepCounter == 1) {
    treasureFaces[startFace] = true;
  }
  else if (stepCounter == 2) {
    treasureFaces[startFace] = true;
    //    treasureFaces[bottomFace] = true;
    //    treasureFaces[leftFace] = true;
    //    treasureFaces[rightFace] = true;
    setColorOnFace(WHITE, bottomFace);
    setColorOnFace(WHITE, leftFace);
    setColorOnFace(WHITE, rightFace);
  }
  else if (stepCounter == 3) {
    //    treasureFaces[bottomFace] = true;
    //    treasureFaces[leftFace] = true;
    //    treasureFaces[rightFace] = true;
    setColorOnFace(WHITE, bottomFace);
    setColorOnFace(WHITE, leftFace);
    setColorOnFace(WHITE, rightFace);
    treasureSignal[bottomFace] = SENDING;
    treasureSignal[leftFace] = SENDING;
    treasureSignal[rightFace] = SENDING;
  }
  else if (stepCounter == 4) {
    isTreasureOnMyBlink = false;
  }
}

void deathtrapLoop() {

  setColorOnFace(WHITE, bottomFace);
  setColorOnFace(RED, leftFace);
  setColorOnFace(RED, rightFace);

  if (stepCounter == 0) {
    treasureFaces[startFace] = true;
  }
  else if (stepCounter == 1) {
    treasureFaces[startFace] = true;
  }
  else if (stepCounter == 2) {
    treasureFaces[startFace] = false;
    setColor(RED);
    setColorOnFace(OFF, bottomFace);
  }
  else if (stepCounter == 3) {
    isTreasureOnMyBlink = false;
  }
  else if (stepCounter == 4) {

  }

  treasureSignal[bottomFace] = BLANK;
}

bool goLeft;
bool bSwitchDirection = false;

void switcherLoop() {
  if (goLeft == true) {
    setColorOnFace(PURPLE, topRightFace);
    setColorOnFace(FEATURE_COLOR, leftFace);

    if (stepCounter == 0) {
      treasureFaces[startFace] = true;
    }
    else if (stepCounter == 1) {
      treasureFaces[startFace] = true;
    }
    else if (stepCounter == 2) {
      treasureFaces[startFace] = true;
      treasureFaces[leftFace] = true;
    }
    else if (stepCounter == 3) {
      treasureFaces[leftFace] = true;
      treasureSignal[leftFace] = SENDING;
      bSwitchDirection = true;
    }
    else if (stepCounter == 4) {
      isTreasureOnMyBlink = false;

    }
  } else if (goLeft == false) {
    setColorOnFace(PURPLE, topLeftFace);
    setColorOnFace(FEATURE_COLOR, rightFace);

    if (stepCounter == 0) {
      treasureFaces[startFace] = true;
    }
    else if (stepCounter == 1) {
      treasureFaces[startFace] = true;
    }
    else if (stepCounter == 2) {
      treasureFaces[startFace] = true;
      treasureFaces[rightFace] = true;
    }
    else if (stepCounter == 3) {
      treasureFaces[rightFace] = true;
      treasureSignal[rightFace] = SENDING;
      bSwitchDirection = true;
    }
    else if (stepCounter == 4) {
      isTreasureOnMyBlink = false;

    }
  }
}


void treasureTumble() {

  if (isTreasureOnMyBlink == true) {
    if (treasureTimer.isExpired()) {
      treasureTimer.set(TREASURE_TIME);
      stepCounter = stepCounter + 1;
    } else if (!treasureTimer.isExpired()) {

    }

    FOREACH_FACE(f) {
      if (treasureFaces[f] == true) {
        setColorOnFace(TREASURE_COLOR, f);
      }
    }

  } else if (isTreasureOnMyBlink == false) {
    switchDirection();
    stepCounter = 0;
  }
}

void switchDirection() { //look, it works even if it's annoyingly complicated
  if (bSwitchDirection == true) {
    goLeft = !goLeft;
    bSwitchDirection = false;
  }
}

void setRole() {
  if (hasWoken()) {
    bLongPress = false;
  }

  if (buttonLongPressed()) {
    bLongPress = true;
  }

  if (buttonReleased()) {
    if (bLongPress) {
      //now change the role
      bChangeRole = true;
      bLongPress = false;
    }
  }

  if (buttonMultiClicked()) {
    if (buttonClickCount() == 3) {
      if (blinkRole == WALL) {
        blinkRole = SPAWNER;
      } else if ( blinkRole == SPAWNER) {
        blinkRole = WALL;
      }
    }
  }

  switch (blinkRole) {
    case WALL:
      wallLoop();
      break;
    case BUCKET:
      bucketLoop();
      break;
    case SPAWNER:
      spawnerLoop();
      break;
  }

  if (bLongPress) {
    //transition color
    setColor(WHITE);
  }
}

byte getGravitySignal(byte data) { //all the gravity communication happens here
  return (data & 7); //returns bits D, E, and F
}

byte getSpawnerSignal(byte data) {
  return ((data >> 3) & 1); //returns bit C
}

byte getTreasureSignal(byte data) {
  return ((data >> 4) & 3); //returns bits A and B?
}
