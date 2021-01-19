/*
Originally from:
// Daniel Shiffman
// http://youtube.com/thecodingtrain

 Tweaked from Dan's above sketch to make this Processing3 version of my Arduino implementation

 Copyright 2021 Ｌｏｒａｘｉｐａｍ

 MIT licensed. See LICENSE.txt for details.

 Soli Deo Gloria

*/

int dayPart, index = 0;
int horas, minutos, segundos;
long daysecs;
  
// Set the window size and frame rate and show the first instance
void setup() {
  size(300, 400);
  background(0);
  
  // at 2 per sec, we need 675 ticks to make 1/256th of a day
  frameRate(2);
  
  // Get the time and recalc the day progress
  dayPart = recalc();
  
  // Log to the console with the header line
  console(true);

  // Show the initial display
  sevenSegmentClock(dayPart);
  
  // Align the index with the time
  index = int((daysecs*2) % 675);
}

void draw() {
  background(0);

  if (index % 675 == 0) {
    dayPart = recalc();
    console(false);
    index = int((daysecs*2) % 675);
  }

  sevenSegmentClock(dayPart);
  
  index++;
}


// Recalculate the day progress
int recalc() {
  int dayFrac;
  
  // grab the time and calculate the seconds of the day
  horas = hour();
  minutos = minute();
  segundos = second();
  daysecs = horas * 60 * 60 + minutos * 60 + segundos;

  // what 8-bit fraction of the day is it?
  dayFrac = int(map(daysecs, 0, 86400, 0, 256));

  return dayFrac;
}

// Send status to the console. With or without headers
void console(boolean headers) {
  if (headers) println("daypart\tdaysec\ttime");

  print(dayPart);
  print("\t");
  print(daysecs);
  print("\t");
  print((horas < 10 ? "0" : "")+horas+":"+(minutos < 10 ? "0" : "")+minutos+":"+(segundos < 10 ? "0" : "")+segundos);
  println();

}

// Returns the RGBA color fill for a given segment, with pulse (depends on index shared var)
color getColor(int val, int shift) {
  final int contrast = 0x30;
  final int pulse = 0x0C;
  final int pulsetick = 6;
  final int r = 0xFC;
  final int g = 0x00;
  final int b = 0x00;
  int a = contrast + (0xFF-(pulse*(index%pulsetick))-contrast) * ((val >> shift) & 1);
  return color(r, g, b, a);
}

// light up the LED using DP,C,B,E,F,D,G,A LSB bit order
void sevenSegmentClock(int val) {
  pushMatrix();
  noStroke();
  noFill();
  
  // Turn this LED on its side
  translate(0,height/2);
  rotate(-PI/2.0);
  
  // A
  fill(getColor(val, 0));
  rect(60, 20, 78, 18, 10, 10, 0, 0);
  // G
  fill(getColor(val, 1));
  rect(60, 140, 78, 18, 10);
  // D
  fill(getColor(val, 2));
  rect(60, 260, 78, 18, 0, 0, 10, 10);
  // F
  fill(getColor(val, 3));
  rect(40, 40, 18, 98, 10, 0, 0, 10);
  // E
  fill(getColor(val, 4));
  rect(40, 160, 18, 98, 10, 0, 0, 10);
  // B
  fill(getColor(val, 5));
  rect(140, 40, 18, 98, 0, 10, 10, 0);
  // C
  fill(getColor(val, 6));
  rect(140, 160, 18, 98, 0, 10, 10, 0);
  // DP
  fill(getColor(val, 7));
  rect(164, 260, 18, 18, 8);

  popMatrix();
  
}

// light up the LED using standard DP,G,F,E,D,C,B,A LSB bit order
void sevenSegmentRaw(int val) {
  pushMatrix();
  noStroke();
  noFill();
  
  // A
  fill(getColor(val, 0));
  rect(60, 20, 78, 18, 10, 10, 0, 0);
  // B
  fill(getColor(val, 1));
  rect(140, 40, 18, 98, 0, 10, 10, 0);
  // C
  fill(getColor(val, 2));
  rect(140, 160, 18, 98, 0, 10, 10, 0);
  // D
  fill(getColor(val, 3));
  rect(60, 260, 78, 18, 0, 0, 10, 10);
  // E
  fill(getColor(val, 4));
  rect(40, 160, 18, 98, 10, 0, 0, 10);
  // F
  fill(getColor(val, 5));
  rect(40, 40, 18, 98, 10, 0, 0, 10);
  // G
  fill(getColor(val, 6));
  rect(60, 140, 78, 18, 10);
  // DP
  fill(getColor(val, 7));
  rect(164, 260, 18, 18, 8);

  popMatrix();
}
