#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h> 

int main(int argc, char** argv){

    /*struct dirent *de;
    DIR *dr = opendir("/dev"); 
    if (dr == NULL)  // opendir returns NULL if couldn't open directory 
    { 
        printf("Could not open current directory" ); 
        return 0; 
    } 
    while ((de = readdir(dr)) != NULL){ 
            printf("%s\n", de->d_name); }
    closedir(dr); */

    
    int fd= open("/dev/adxl-1",O_RDONLY);

    if(fd<0){
        printf("value od error : %d", fd);
        perror("openfailed");
        return -1;
    }

    //read
    char data[2]={0,0};

    int ret=read(fd,data,2);
    if(ret<0){
        perror("reaffailed");
        close(fd);
        return -2;
    }
    printf("WE READ : %d %d", data[0],data[1]);
    return 0;
}