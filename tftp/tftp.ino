#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SD.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:

byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 135);

const int TFTP_DATA_SIZE    = 512;
const int TFTP_OPCODE_SIZE  = 2;
const int TFTP_BLOCKNO_SIZE = 2;
const int TFTP_MAX_PAYLOAD  = 512;
const int TFTP_ACK_SIZE ( TFTP_OPCODE_SIZE + TFTP_BLOCKNO_SIZE );
const int TFTP_PACKET_MAX_SIZE (  TFTP_OPCODE_SIZE + TFTP_BLOCKNO_SIZE + TFTP_MAX_PAYLOAD );

File archive;

unsigned int localPort = 69;      // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

char packetBuffer[TFTP_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
char ReplyBuffer[4];
char ErrorBuffer[37];
String nomArchivo;
int flagR = 0;
int flagW = 0;
bool itsOpen;
int no_block = 0;
int sendit = 0;
int timeout = 0;
void setup() {
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);
  Udp.begin(localPort);
  Serial.begin(9600);
  SD.begin(4);
}

void loop() {

  int packetSize = Udp.parsePacket();
  IPAddress remote = Udp.remoteIP();
  if(packetSize)
  {
    // read the packet into packetBufffer
    Udp.read(packetBuffer,TFTP_PACKET_MAX_SIZE);
    if ((byte)packetBuffer[1] == byte(1) ){
        if (flagR == 0) {
          nomArchivo = "";
          timeout = 0;
        }
        Serial.println("RRQ");
        for(int i = 2; byte(packetBuffer[i]) != byte(0); i++){
          nomArchivo += String(packetBuffer[i]);
        }
        if (SD.exists(nomArchivo)) {
          archive = SD.open(nomArchivo);
          no_block = 1;
          packetBuffer[0] = byte(0);
          packetBuffer[1] = byte(3);
          packetBuffer[2] = byte(0);
          packetBuffer[3] = byte(no_block);
          int i = 4;
          while(archive.available() && i < TFTP_PACKET_MAX_SIZE) {
            packetBuffer[i] = archive.read();
            i++;
          }
          if (i == TFTP_PACKET_MAX_SIZE) {
            no_block++;
            itsOpen = true;
          } else if (i < TFTP_PACKET_MAX_SIZE) {
            no_block = 0;
            archive.close();
            flagR = 0;
            itsOpen = false;
          }
          Udp.beginPacket(remote, Udp.remotePort());
          Udp.write(packetBuffer, TFTP_PACKET_MAX_SIZE);
          Udp.endPacket();
        } else {
          error(1);
          Udp.beginPacket(remote, Udp.remotePort());
          Udp.write(ErrorBuffer, TFTP_PACKET_MAX_SIZE);
          Udp.endPacket();
          timeout++;
        }

    } else if ((byte)packetBuffer[1] == byte(2) ) {
        Serial.println("WRQ");
        if (flagW == 0) {
          nomArchivo = "";
          timeout = 0;
        }
        for(int i = 2; byte(packetBuffer[i]) != byte(0); i++){
          nomArchivo += String(packetBuffer[i]);
        }

        if (SD.exists(nomArchivo)) {
          error(6);
          Udp.beginPacket(remote, Udp.remotePort());
          Udp.write(ErrorBuffer, TFTP_PACKET_MAX_SIZE);
          Udp.endPacket();
          Serial.println("existe");
        } else {
          ack(byte(0),byte(0));
          Serial.println(nomArchivo);
          Udp.beginPacket(remote, Udp.remotePort());
          Udp.write(ReplyBuffer, TFTP_ACK_SIZE);
          Udp.endPacket();
          flagW = 1;
          no_block = 1;
        }

    } else if ((byte)packetBuffer[1] == byte(3) ) {
        timeout = 0;
        Udp.read(packetBuffer,TFTP_PACKET_MAX_SIZE);
        if (block(no_block, int(packetBuffer[2]), int(packetBuffer[3]))) {
          //if (byte(packetBuffer[3]) == byte(1)) {
            archive = SD.open(nomArchivo, FILE_WRITE);
          //}
 
          ack((byte)packetBuffer[2],(byte)packetBuffer[3]);
          for (int i = 4;i < packetSize;i++) {
            archive.print((char)packetBuffer[i]);
          }
          archive.close();
          if (packetSize < 516){
            flagW = 0;
          }
          Udp.beginPacket(remote, Udp.remotePort());
          Udp.write(ReplyBuffer, TFTP_ACK_SIZE);
          Udp.endPacket();
          no_block++;
        } else {
          Udp.beginPacket(remote, Udp.remotePort());
          Udp.write(ReplyBuffer, TFTP_ACK_SIZE);
          Udp.endPacket();
        }
        
    } else if ((byte)packetBuffer[1] == byte(4) ) {
      //memset(packetBuffer, 0, TFTP_PACKET_MAX_SIZE);
      if(itsOpen && block(no_block, int(packetBuffer[2]), int(packetBuffer[3]))) {
        timeout = 0;
        packetBuffer[0] = byte(0);
        packetBuffer[1] = byte(3);
        if(no_block<256){
          packetBuffer[2] = byte(0);
          packetBuffer[3] = byte(no_block);
        } else {
          int b2 = no_block/256;
          packetBuffer[2] = byte(b2);
          packetBuffer[3] = byte(255);
        }
        int i = 4;
        while(archive.available() && i < TFTP_PACKET_MAX_SIZE) {
          packetBuffer[i] = archive.read();
          i++;
        }
        if (i == TFTP_PACKET_MAX_SIZE) {
          no_block++;
        } else if (i < TFTP_PACKET_MAX_SIZE) {
          archive.close();
          flagR = 0;
          itsOpen = false;
          no_block = 0;
        }
        Udp.beginPacket(remote, Udp.remotePort());
        Udp.write(packetBuffer, i);
        Udp.endPacket();
      } else if ( !block(no_block, int(packetBuffer[2]), int(packetBuffer[3])) ) {
        Udp.beginPacket(remote, Udp.remotePort());
        Udp.write(packetBuffer, TFTP_PACKET_MAX_SIZE);
        Udp.endPacket();
      } else if (timeout == 8000) {
        error(0);
        Udp.beginPacket(remote, Udp.remotePort());
        Udp.write(ErrorBuffer, TFTP_PACKET_MAX_SIZE);
        Udp.endPacket();
      } else {
        error(4);
        Udp.beginPacket(remote, Udp.remotePort());
        Udp.write(ErrorBuffer, TFTP_PACKET_MAX_SIZE);
        Udp.endPacket();
      }
      
      
    } else {
        error(2);
        Udp.beginPacket(remote, Udp.remotePort());
        Udp.write(ErrorBuffer, TFTP_PACKET_MAX_SIZE);
        Udp.endPacket();
    }
    timeout++;
  }
}

void ack(byte block0, byte block1) {
  ReplyBuffer[0] = byte(0);
  ReplyBuffer[1] = byte(4);
  ReplyBuffer[2] = block0;
  ReplyBuffer[3] = block1;
}

void error(int e) {
  ErrorBuffer[0] = byte(0);
  ErrorBuffer[1] = byte(5);
  ErrorBuffer[2] = byte(0);
  ErrorBuffer[3] = byte(e);
  String message = "";
  switch (e) {
    case 0:
      message = "Not defined error";
      for (int i = 4; i < sizeof(message)+4; i++){
        ErrorBuffer[i] = message[i-4];
      }
    case 1:
      message = "File not found";
      for (int i = 4; i < sizeof(message)+4; i++){
        ErrorBuffer[i] = message[i-4];
      }
    case 2:
      message = "Access violation";
      for (int i = 4; i < sizeof(message)+4; i++){
        ErrorBuffer[i] = message[i-4];
      }
    case 3:
      message = "Disk full or allocation exceeded";
      for (int i = 4; i < sizeof(message)+4; i++){
        ErrorBuffer[i] = message[i-4];
      }
    case 4:
      message = "Illegal TFTP operation";
      for (int i = 4; i < sizeof(message)+4; i++){
        ErrorBuffer[i] = message[i-4];
      }
    case 5:
      message = "Unknown transfer ID";
      for (int i = 4; i < sizeof(message)+4; i++){
        ErrorBuffer[i] = message[i-4];
      }
    case 6:
      message = "File already exists";
      for (int i = 4; i < sizeof(message)+4; i++){
        ErrorBuffer[i] = message[i-4];
      }
    case 7:
      message = "No such user";
      for (int i = 4; i < sizeof(message)+4; i++){
        ErrorBuffer[i] = message[i-4];
      }
  }
  ErrorBuffer[36] = byte(0);
}

bool block(int b, int b1, int b2) {
  if (b1 > 0) {
    if (b == b1 * b2) return true;
    else return false;
  } else {
    if (b == b2) return true;
    else return false;
  }
}

