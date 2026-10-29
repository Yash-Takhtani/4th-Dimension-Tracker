const int buzzer = 20;

const int melody[] = {
  1600, 2000, 2400, 2000,
  1600, 2000, 2400, 2000,
  1800, 2200, 2600, 2200,
  1800, 2200, 2600, 2200
};

const int noteDuration[] = {
  180, 180, 260, 180,
  180, 180, 260, 300,
  180, 180, 260, 180,
  180, 180, 260, 300
};

const int melodyLength = sizeof(melody) / sizeof(melody[0]);

int currentNote = 0;
unsigned long noteStart = 0;
unsigned long nextNoteTime = 0;

bool musicPlaying = false;
bool noteOn = false;

void startMusic() {
  musicPlaying = true;
  currentNote = 0;
  noteOn = false;
  nextNoteTime = millis();
}

void stopMusic() {
  musicPlaying = false;
  noteOn = false;
  ledcWrite(buzzer, 0);
}

void updateMusic() {
  if (!musicPlaying) return;

  unsigned long now = millis();

  if (noteOn) {
    if (now - noteStart >= (unsigned long)noteDuration[currentNote - 1]) {
      ledcWrite(buzzer, 0);
      noteOn = false;
      nextNoteTime = now + 35;
    }
    return;
  }

  if ((long)(now - nextNoteTime) >= 0) {
    if (currentNote >= melodyLength) {
      currentNote = 0;
    }

    ledcWriteTone(buzzer, melody[currentNote]);
    ledcWrite(buzzer, 128);

    noteStart = now;
    noteOn = true;
    currentNote++;
  }
}

void setup() {
  ledcAttach(buzzer, 2000, 8);
  startMusic();
}

void loop() {
  updateMusic();
}