#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
int main(void) {
 setvbuf(stdout,0,_IONBF,0);
 puts("IP-REPLAY: started");
 sleep(90);
 for (int n=0;n<12;n++) {
  const char *paths[]={"/var/tmp/x01a-ip.log","/var/tmp/x01a-ip.err"};
  for(int j=0;j<2;j++) {
   int f=open(paths[j],O_RDONLY); char buf[2048]; ssize_t k;
   printf("IP-REPLAY: file %d open %s errno %d\n",j,f<0?"failed":"ok",f<0?errno:0);
   if(f>=0) { lseek(f,-16384,SEEK_END); while((k=read(f,buf,sizeof buf))>0) write(1,buf,k);close(f); }
  }
  puts("IP-REPLAY: end snapshot"); sleep(20);
 }
 return 0;
}
