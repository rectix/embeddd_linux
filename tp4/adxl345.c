#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>


#define ADXL345_MAGIC 'a'
#define ADXL345_SET_AXIS _IOW(ADXL345_MAGIC, 1, int)
#define ADXL345_GET_AXIS _IOR(ADXL345_MAGIC, 2, int)


#define I2C_DEVICE_BASEADDR 0x00
#define  BW_RATE   0x2C
#define  INT_ENABLE  0x2E
#define  DATA_FORMAT 0x31
#define  FIFO_CTL 0x38
#define  POWER_CTL_ON 0x39  //1
#define  POWER_CTL_OFF 0x39  //0

#define ADXL345_REG_DATAX0 0x32
#define ADXL345_REG_DATAX1 0x33


#define X_AXE _IO('a', 1)

int numDev = 0;



//typedef struct adxl345_device0  {



//struct miscdevice misc ;
//}adxl345_device0;

typedef struct adxl345_device {
    struct miscdevice misc;
    struct i2c_client *client;
    int axe;
}adxl345_device;
static int write_register_i2c(struct i2c_client *client, char reg_address, char value)
{
    char buffer[2];
    int error;
    buffer[0] = reg_address;
    buffer[1] = value;
    error = i2c_master_send(client, buffer, 2);

    if (error < 0) {
        pr_info("[ERROR] writing register %x\n", reg_address);
        return error;
    }

    return 0;
}


static int read_register_i2c(struct i2c_client *client, char reg_address, char *value)
{
    char buffer[1];
    int error = 0;
    buffer[0] = reg_address;
    error = i2c_master_send(client, buffer, 1);

    if (error < 0) {
        pr_info("[ERROR] reading register (send) %x\n", reg_address);
        return error;
    }

    error = i2c_master_recv(client, value, 1);

    if (error < 0) {
        pr_info("[ERROR] reading register (receive) %x\n", reg_address);
        return error;
    }
    return 0;
}


static int adxl345_open(struct inode *inode, struct file *file)
{
    struct adxl345_device *dev = container_of(file->private_data,  adxl345_device, misc);
    pr_info(" open   function success \n");

    return 0;
}


static ssize_t adxl345_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset) {
    adxl345_device *dev = container_of(file->private_data, adxl345_device, misc);
    struct i2c_client *client = dev->client;
    uint8_t reg_addr[2];
    uint8_t data[2];  
    int16_t accel_value;
    int ret;

    
    switch (dev->axe) {
        case 0: reg_addr[0] = ADXL345_REG_DATAX0;reg_addr[1] = ADXL345_REG_DATAX1; break;
        default: return -EINVAL;  
    }

     ret = i2c_master_send(client,&reg_addr[0], 1);
     ret = i2c_master_recv(client, &data[0], 1);
     ret = i2c_master_send(client,&reg_addr[1], 1);
     ret = i2c_master_recv(client, &data[1], 1);
  

     accel_value = (int16_t)((data[1] << 8) | data[0]);

     if (copy_to_user(user_buffer, &accel_value, sizeof(accel_value)))
         return -EFAULT;
 
     return sizeof(accel_value);
 }




// IOCTL function
static long adxl345_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    adxl345_device *dev = container_of(file->private_data, adxl345_device, misc);
    int new_axis;

    switch (cmd) {
        case ADXL345_SET_AXIS:
            if (copy_from_user(&new_axis, (int __user *)arg, sizeof(int))) {
                return -EFAULT;
            }
            if (new_axis < 0 || new_axis > 2) {
                return -EINVAL;  
            }
            dev->axe = new_axis;
            printk(KERN_INFO "ADXL345: Axis set to %d\n", new_axis);
            break;

        case ADXL345_GET_AXIS:
            if (copy_to_user((int __user *)arg, &dev->axe, sizeof(int))) {
                return -EFAULT;
            }
            break;

        default:
            return -ENOTTY;  // Invalid command
    }
    return 0;
}


/******************************************************************************************************************/



static const struct file_operations adxl345_fops = {
    .owner = THIS_MODULE,
    .read = adxl345_read,
    .open = adxl345_open,
    .unlocked_ioctl = adxl345_ioctl,

   
};
static int adxl345_probe(struct i2c_client *client,const struct i2c_device_id *id)
{


            



            adxl345_device *dev;

            dev =  kmalloc(sizeof(adxl345_device), GFP_KERNEL);
            dev ->client = client;
            dev->axe = 0; 
            
            
            numDev++ ;
            dev ->misc.name =  "adxl345"; // kasprintf(GFP_KERNEL, "adxl345_%d",numDev);
            dev->misc.minor = MISC_DYNAMIC_MINOR;
            dev->misc.fops = &adxl345_fops;

            



       
            char sendbuff[2] ;
            char dev_id[1];
            sendbuff[0] = I2C_DEVICE_BASEADDR;

            printk(KERN_INFO":number of bytes  Ox%02x   \n",i2c_master_send(client, &sendbuff[0] , 1));
            i2c_master_recv(client,dev_id,1);
            printk(KERN_INFO"device ID : Ox%02x \n", dev_id[0]);



           
            write_register_i2c(client, BW_RATE, 0x0A);
            write_register_i2c(client, INT_ENABLE, 0x02);
            write_register_i2c(client, DATA_FORMAT, 0x00);
            write_register_i2c(client, FIFO_CTL, 0x00);
            write_register_i2c(client, POWER_CTL_ON, 0x08);
            write_register_i2c(client, FIFO_CTL, 0b10010100);



            
           
            /* 
            sendbuff[0] = 0x2C;
            sendbuff[1] = 0x0A;

            i2c_master_send(client,sendbuff, 2);
            


            sendbuff[0] = 0x2E;
            sendbuff[1] = 0x00;
            i2c_master_send(client,sendbuff, sizeof(sendbuff));


            sendbuff[0] = 0x31;
            sendbuff[1] = 0x00;
            i2c_master_send(client,sendbuff, sizeof(sendbuff));


            sendbuff[0] = 0x38;
            sendbuff[1] = 0x00;
            i2c_master_send(client,sendbuff, sizeof(sendbuff));


            
            sendbuff[0] = 0x2E;
            sendbuff[1] = 0b00001000;
            i2c_master_send(client,sendbuff, sizeof(sendbuff));


            
            
            */
            

            /****************************************************************/



            i2c_set_clientdata(client, dev);
            misc_register(&(dev->misc));

   return 0;
}

static int adxl345_remove(struct i2c_client *client)
{

/********************************************************************************************************************************/
    printk(KERN_INFO"bye .... \n");
    char sendbuff[2] ;

 adxl345_device *dev;
 dev =  kmalloc(sizeof(adxl345_device), GFP_KERNEL);

 dev  = (adxl345_device *)i2c_get_clientdata(client);
 
 misc_deregister((&(dev->misc)));
 //kfree(dev->misc.name);
 numDev --;


/********************************************************************************************************************************/
    return 0;
}
/***********************************************************************************************/











static struct i2c_device_id adxl345_idtable[] = {
    { "adxl345", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, adxl345_idtable);

#ifdef CONFIG_OF

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
