#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <fstream>
#include <algorithm>
#include <thread>
#include <stdint.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

using namespace std;

#include <ostream>

namespace log_color {
   const string endn = "\r\n";
   
   enum color {
      /* foreground color */
      FG_RED = 31,FG_GREEN = 32, FG_BLUE = 34, FG_DEFAULT = 39, FG_BLACK = 30, FG_YELLOW = 33, FG_MAGENTA = 35, FG_CYAN = 36, FG_LIGHT_GRAY = 37, FG_DARK_GRAY = 90, FG_LIGHT_RED = 91, FG_LIGHT_GREEN = 92, FG_LIGHT_YELLOW = 93, FG_LIGHT_BLUE = 94, FG_LIGHT_MAGENTA = 95, FG_LIGHT_CYAN = 96, FG_WHITE = 97,
      /* background color */
      BG_RED = 41, BG_GREEN = 42, BG_BLUE = 44, BG_DEFAULT = 49, BG_BLACK = 40, BG_YELLOW = 43, BG_MAGENTA = 45, BG_CYAN = 46, BG_WHITE = 47,
   };
   
   class colored {
      color col;
   public:
      colored(color c) : col(c) {}
      friend std::ostream&
      operator<<(std::ostream& os, const colored& mod) {
         return os << "\033[" << mod.col<< "m";
      }
   };
   
   struct log {
      template <typename T>
      log(T str, log_color::color f = FG_GREEN, log_color::color b = BG_DEFAULT) {
         log_color::colored fg(f);
         log_color::colored bg(b);
         log_color::colored fgdef(log_color::FG_DEFAULT);
         log_color::colored bgdef(log_color::BG_DEFAULT);
         std::cout << bg << fg << str << fgdef << bgdef;
      }
      
      template <typename T>
      log operator << (T n) {
         return n;
      };
   };
};

using namespace log_color;

class serial_mon {
  public:
   int baudrate;
   char type;
   char destination[2];
   char *message;
   char *localUsb;
   int fd;
   
   int iosetup(string serialport, int baud) {
      struct termios toptions;
      fd = open(serialport.c_str(), O_RDWR | O_NOCTTY | O_NDELAY); //O_NOCTTY; O_NONBLOCK
      //fcntl(fd, F_SETFL, 0);
      
      if (fd == -1) {
         log( "iosetup[ unable to open port ]: ",FG_WHITE, BG_RED) << serialport << endn;
         return -1;
         }
      
      if (tcgetattr(fd, &toptions) < 0) {
         log( "iosetup[ couldn't get term attributes ]",FG_WHITE, BG_RED) << endn;
         return -1;
         }
      speed_t brate = baud;
      switch (baud) {
       case 4800:
          brate = B4800;
          break;
       case 9600:
          brate = B9600;
          break;
       case 19200:
          brate = B19200;
          break;
       case 38400:
          brate = B38400;
          break;
       case 57600:
          brate = B57600;
          break;
       case 115200:
          brate = B115200;
          break;
       case 460800:
          brate = B460800;
          break;
         }
      
      cfsetispeed(&toptions, brate);
      cfsetospeed(&toptions, brate);
      
      toptions.c_cflag &= ~PARENB;
      toptions.c_cflag &= ~CSTOPB;
      toptions.c_cflag &= ~CSIZE;
      toptions.c_cflag |= CS8;
      
      toptions.c_cflag &= ~CRTSCTS;
      toptions.c_cflag |= CREAD | CLOCAL;
      toptions.c_iflag &= ~(IXON | IXOFF | IXANY);
      toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
      toptions.c_oflag &= ~OPOST;
      
      toptions.c_cc[VMIN]  = 1;
      toptions.c_cc[VTIME] = 10;
      if (tcsetattr(fd, TCSANOW, &toptions) < 0) {
         log( "iosetup[ couldn't set term attributes ]",FG_WHITE, BG_RED) << endn;
         return -1;
         }
      return fd;
   }
   
   int iowrite(int fd, const char* str) {
      int len = strlen(str);
      int n = write(fd, str, len);
      if (n != len)
         return -1;
      return n;
   }
   
   int iowritebyte(int fd, uint8_t b) {
      int n = write(fd, &b, 1);
      if (n != 1)
         return -1;
      return 0;
   }
   
   int ioread_until(int fd, char buf[10][20], char until) {
      char b[1];
      int i = 0, j = 0;
      do {
         do {
            int n = read(fd, b, 1);
            if (n == -1)
               return -1;
            if (n == 0) {
               usleep(10 * 1000);
               continue;
               }
            buf[j][i] = b[0];
            i++;
            if(b[0] == until)
               break;
            } while (b[0] != ',');
         buf[j][i] = 0;
         i = 0;
         j++;
         } while (b[0] != until);
      return i;
   }
   
   char ioread(int fd) {
      char b[1];
      int n = read(fd, b, 1);
      if (n == -1){
         log( "ioread[ error read ]",FG_WHITE, BG_RED) << endn;
         }
      return b[0];
   }
   
   int ioavailable(int fd) {
      int nbytes = 0;
      ioctl(fd, FIONREAD, &nbytes);
      return nbytes;
   }
   
   void stop() {
      close(fd);
   }   
};

/* main */
serial_mon io;
string send = "";

void close_app(int s) {
   log("exit serial_mon", FG_WHITE, BG_RED) << endn;
   io.stop();
   exit(0);
   }


void sender() {
   while( 1 ) {
      char input[1024];
      cin.getline(input, sizeof(input));
      send = string(input);
      }
   }

void receive(int fd) {
   string result;
   while( 1 ) {

      if(send != "") {
         string cc = send+"\n";
         if( io.iowrite(fd, cc.c_str()) == -1 ) {
            log("error send: ", FG_WHITE, BG_RED);
            log(send, FG_WHITE, BG_BLUE) << endn;
            }
         else {
            log("*****************************************************************", FG_WHITE, BG_YELLOW) << endn;
            log("send:       ",FG_WHITE, BG_RED);
            log(send, FG_WHITE, BG_BLUE) << endn;
            }

         send = "";
         }
      
      int a = io.ioavailable(fd);
      if(a) {
         char c = io.ioread(fd);
         if(c != '\n') {
            result+=c;
            }
         else {
            log("receive:    ",FG_WHITE, BG_RED);
            log(result,  FG_WHITE, BG_BLUE) << endn;
            result = "";
            }
         }
      
      usleep(50*1000L);
      }
   }

int main(int argc, char ** argv) {
   if(argc!=3) {
      log("./serial_mon <device> <baudrate>",FG_WHITE, BG_BLUE) << endn;
      return 1;
      }

   signal(SIGINT, close_app);
   
   string file = argv[1];
   int baud    = atoi( argv[2] );   
   int fd      = io.iosetup(file, baud);
   log("device:   ", FG_WHITE, BG_BLUE);
   log(file, FG_BLACK, BG_YELLOW) << endn;
   log("baudrate: ", FG_WHITE, BG_BLUE);
   log(baud, FG_BLACK, BG_YELLOW) << endn;
   if(fd != -1) {
      thread s(sender);
      thread r(receive, fd);
      s.join();
      r.join();
      }
   else
      log("error opened device: ", FG_WHITE, BG_RED) << file << endn;
   return 0;
}


