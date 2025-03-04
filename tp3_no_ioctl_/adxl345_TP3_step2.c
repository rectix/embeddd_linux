#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>

//      TP3
#include <linux/fs.h>
#include <linux/slab.h>
//#include <linux/uacess.h>//?????
//#include <unistd.h>// for ssize_t for _read()
typedef struct adxl345_device{
    struct miscdevice misc;
}adxl_device;

//coding the adxl345_read function
#include <linux/cdev.h>
typedef struct mydata_on_open{
    struct cdev cdev;
    struct i2c_client* myclient;
    //adxl_device* mydevice;///pas nécessaire on a misc??
}mydata;



static int adxl345_open(struct inode *inode, struct file *file){
    //define the srtuct WHERE???
    //mydata* open_data;
    //open_data = container_of(inode->i_cdev, mydata, cdev);

    //file->private_data = open_data;

    printk(KERN_INFO"open OPENDED" );
    return 0;
}

static int adxl345_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos){
adxl_device* my_dev;
my_dev=(adxl_device*) file->private_data;
struct i2c_client* myclient;

myclient=to_i2c_client(my_dev->misc.parent);
//reading 1 sample
//0x32 50 DATAX0 R X-Axis Data 0
//0x33 51 DATAX1 R X-Axis Data 1

u8 data_send0[1]={0x32};
u8 data_recv0[1]={};
u8 data_send1[1]={0x33};
u8 data_recv1[1]={};
printk(KERN_INFO"read!! read:!!");
i2c_master_send(myclient, data_send0,1);
printk(KERN_INFO"read!!   2");
i2c_master_recv(myclient,data_recv0,1);
printk(KERN_INFO"read!!   3");

i2c_master_send(myclient, data_send1,1);
i2c_master_recv(myclient,data_recv1,1);

u8 out[2] = {data_recv0[0],data_recv1[0]};

copy_to_user(buf, out,2*sizeof(char));

return 0;
}





int nb_adxl=0;

static int adxl345_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
//  2nd Step
int Bsent,Brcv;

u8 send_buff[1]={0x00};
u8 recv_buff[1];

printk(KERN_INFO" %d   %d 1.\n",id[0],id[1]);

printk(KERN_INFO "the write gave us %d",i2c_master_send(client,send_buff,1));
Brcv= i2c_master_recv(client,recv_buff, sizeof(recv_buff));
printk(KERN_INFO "received DEVID : %02x with number of bytes %d", recv_buff[0],Brcv);

//  3rd step

//0x2C BW_RATE value 100
u8 rate_buff[2]={};
rate_buff[0]=0x2C;
rate_buff[1]=0x0A;
i2c_master_send(client, rate_buff,1);

//check 
u8 rate_recv[1]={0x2C};
i2c_master_send(client, rate_recv,sizeof(rate_recv));
Brcv= i2c_master_recv(client,rate_recv, sizeof(rate_recv));
printk(KERN_INFO "[checking]received Data rate : %d, in %d bytes",rate_recv[0],Brcv);


//0x2E INT_ENABLE value 
u8 int_buff[2]={0x2E,0x00};
i2c_master_send(client, int_buff,sizeof(int_buff));

//0x31 DATA_FORMAT 
//what does "default" mean?
u8 format_buff[2]={0x31,0x00};
i2c_master_send(client, format_buff,sizeof(format_buff));

//0x38 FIFO_CTL 
//set all to 0
u8 fifo_buff[2]={0x38,0x00};
i2c_master_send(client, fifo_buff,sizeof(fifo_buff));

//0x2D 45 POWER_CTL 
//to activate set D3 Measure to 1
u8 power_buff[2]={0x2D,0b00001000};//initialement à 00000000
i2c_master_send(client, power_buff,sizeof(power_buff));



//      TP3

/*typedef struct adxl345_device{
    struct miscdevice misc;
}adxl_device;*/
static const struct file_operations adxl_fops={
    .owner=THIS_MODULE,
    .open=adxl345_open,
    .read=adxl345_read
};

adxl_device* devi;
devi=kmalloc(sizeof(adxl_device),GFP_KERNEL);
i2c_set_clientdata(client, devi);


devi->misc.minor=MISC_DYNAMIC_MINOR;

nb_adxl++;
devi->misc.name=kasprintf(GFP_KERNEL, "adxl-1");//-%d",nb_adxl);

//affecting the read
//read and OPEN
devi->misc.fops=&adxl_fops;
//affecting parent
devi->misc.parent=&client->dev;

misc_register(&(devi->misc));

return 0;
}


static int adxl345_remove(struct i2c_client *client){
//      TP3
adxl_device* devo;
devo=kmalloc(sizeof(adxl_device),GFP_KERNEL);

devo=(adxl_device*)i2c_get_clientdata(client);
misc_deregister(&(devo->misc));



//0x2D 45 POWER_CTL 
//turn off->standby
u8 power_buff[2]={0x2D,0b00000000};//initialement à 00000000
i2c_master_send(client, power_buff,sizeof(power_buff));



printk(KERN_INFO "Goodbye I2C 1. \n");
return 0;
}

/* La liste suivante permet l'association entre un périphérique et son
   pilote dans le cas d'une initialisation statique sans utilisation de
   device tree.
i2c_client
   Chaque entrée contient une chaîne de caractère utilisée pour
   faire l'association et un entier qui peut être utilisé par le
   pilote pour effectuer des traitements différents en fonction
   du périphérique physique détecté (cas d'un pilote pouvant gérer
   différents modèles de périphérique).
*/
static struct i2c_device_id adxl345_idtable[] = {
    { "adxl345", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, adxl345_idtable);

#ifdef CONFIG_OF
/* Si le support des device trees est disponible, la liste suivante
   permet de faire l'association à l'aide du device tree.

   Chaque entrée contient une structure de type of_device_id. Le champ
   compatible est
 une chaîne qui est utilisée pour faire l'association
   avec les champs compatible dans le device tree. Le champ data est
   un pointeur void* qui peut être utilisé par le pilote pour
   effectuer des traitements différents en fonction du périphérique
   physique détecté.
*/
static const struct of_device_id adxl345_of_match[] = {
    { .compatible = "vendor,adxl345",
      .data = NULL },
    {}
};

MODULE_DEVICE_TABLE(of, adxl345_of_match);
#endif

static struct i2c_driver adxl345_driver = {
    .driver = {
        /* Le champ name doit correspondre au nom du module
           et ne doit pas contenir d'espace */
        .name   = "adxl345",
        .of_match_table = of_match_ptr(adxl345_of_match),
    },

    .id_table       = adxl345_idtable,
    .probe          = adxl345_probe,
    .remove         = adxl345_remove,
};

module_i2c_driver(adxl345_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("adxl345 driver");
MODULE_AUTHOR("Me");
