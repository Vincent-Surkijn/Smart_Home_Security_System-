let mic;
let domElement;
let micLevel=1;
let bg;
let myText='tap to start';
let recording=false;
let minValue=0.05;

function preload(){
  bg = loadImage("./mic")
}

function setup(){
  let cnv = createCanvas(69, 69);
  cnv.mousePressed(canvasPressed);
  cnv.parent("microphone");
  textAlign(CENTER);
  textSize(14);
  mic = new p5.AudioIn();
  domElement = document.getElementById("micLevel");
}

function draw(){
  background(bg);
  fill(255,0,0); // 0, 76, 137
  text(myText, width/2, 20);
  
  micLevel = mic.getLevel();

//  if (micLevel > minValue && domElement.value < minValue ) {
	domElement.value=micLevel;
  /*} else if (micLevel < minValue && domElement.value > minValue) {
	domElement.value=micLevel;
  }*/
}

function canvasPressed() {
  userStartAudio();
  if (recording) {
    mic.stop();
    myText='tap to start';
    recording = false;
  } else {
    mic.start();
    myText='recording';
    recording = true;
  }
}
