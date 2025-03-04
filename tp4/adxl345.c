#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kfifo.h>
#define I2C_DEVICE_BASEADDR 0x00
#define  BW_RATE   0x2C
#define  INT_ENABLE  0x2E
#define  DATA_FORMAT 0x31
#define  FIFO_CTL 0x38
#define  POWER_CTL_ON 0x39  //1
#define  POWER_CTL_OFF 0x39  //0




#define ADXL345_MAGIC 'a'
#define ADXL345_SET_AXIS _IOW(ADXL345_MAGIC, 1, int)
#define ADXL345_GET_AXIS _IOR(ADXL345_MAGIC, 2, int)

#define ADXL345_REG_DATAX0 0x32
#define ADXL345_REG_DATAY0 0x34
#define ADXL345_REG_DATAZ0 0x36

typedef struct adxl345_sample {
    int16_t x;
    int16_t y;
    int16_t z;
} adxl345_sample;

typedef struct adxl345_device {
    struct miscdevice misc;
    struct i2c_client *client;
    int axe;
    DECLARE_KFIFO(fifo, adxl345_sample, 16);
} adxl345_device;

static DECLARE_WAIT_QUEUE_HEAD(adxl345_wq);

static irqreturn_t adxl345_handler(int irq, void *dev_id) {
    return IRQ_WAKE_THREAD;
}

static int adxl345_read_sample(struct adxl345_device *dev, adxl345_sample *sample) {
    struct i2c_client *client = dev->client;
    uint8_t reg_addr = ADXL345_REG_DATAX0;
    uint8_t data[6];
    int ret;

    ret = i2c_master_send(client, &reg_addr, 1);
    if (ret < 0) return ret;

    ret = i2c_master_recv(client, data, 6);
    if (ret < 0) return ret;

    sample->x = (int16_t)((data[1] << 8) | data[0]);
    sample->y = (int16_t)((data[3] << 8) | data[2]);
    sample->z = (int16_t)((data[5] << 8) | data[4]);

    return 0;
}

static irqreturn_t adxl345_threaded_irq(int irq, void *dev_id) {
    adxl345_device *dev = dev_id;
    adxl345_sample sample;

    if (adxl345_read_sample(dev, &sample) == 0) {
        kfifo_put(&dev->fifo, sample);
        wake_up_interruptible(&adxl345_wq);
    }

    return IRQ_HANDLED;
}
/*
static ssize_t adxl345_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset) {
    struct adxl345_device *dev = container_of(file->private_data, adxl345_device, misc);
    adxl345_sample sample;
    int ret;

    if (kfifo_is_empty(&dev->fifo)) {
        pr_info("no data available in the fifo >> put to sleep  !!!\n");

        if (file->f_flags & O_NONBLOCK) return -EAGAIN;
        ret = wait_event_interruptible(adxl345_wq, !kfifo_is_empty(&dev->fifo));
        if (ret < 0) return ret;
    }

    if (!kfifo_get(&dev->fifo, &sample)) return -EIO;
    if (copy_to_user(user_buffer, &sample, sizeof(sample))) return -EFAULT;

    return sizeof(sample);
}
    */



    static ssize_t adxl345_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset) {
        adxl345_device *dev = container_of(file->private_data, adxl345_device, misc);
        struct i2c_client *client = dev->client;
        uint8_t reg_addr = ADXL345_REG_DATAX0;
        uint8_t data[6];  // Buffer for X, Y, Z registers
        int16_t accel_values[3];
        int ret;
    
        // Request all 6 bytes (X0, X1, Y0, Y1, Z0, Z1)
        ret = i2c_master_send(client, &reg_addr, 1);
        if (ret < 0) {
            pr_err("ADXL345: Failed to write reg address\n");
            return -EIO;
        }
    
        ret = i2c_master_recv(client, data, 6);
        if (ret < 0) {
            pr_err("ADXL345: Failed to read acceleration data\n");
            return -EIO;
        }
    
        // Convert raw bytes to int16 values
        accel_values[0] = (int16_t)((data[1] << 8) | data[0]); // X-axis
        accel_values[1] = (int16_t)((data[3] << 8) | data[2]); // Y-axis
        accel_values[2] = (int16_t)((data[5] << 8) | data[4]); // Z-axis
    
        // Copy data to user space
        if (copy_to_user(user_buffer, accel_values, sizeof(accel_values))) {
            return -EFAULT;
        }
    
        return sizeof(accel_values);
    }
    

static long adxl345_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    adxl345_device *dev = container_of(file->private_data, adxl345_device, misc);
    int new_axis;

    switch (cmd) {
        case ADXL345_SET_AXIS:
            if (copy_from_user(&new_axis, (int __user *)arg, sizeof(int))) return -EFAULT;
            if (new_axis < 0 || new_axis > 2) return -EINVAL;
            dev->axe = new_axis;
            break;
        case ADXL345_GET_AXIS:
            if (copy_to_user((int __user *)arg, &dev->axe, sizeof(int))) return -EFAULT;
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

static struct file_operations adxl345_fops = {
    .owner = THIS_MODULE,
    .read = adxl345_read,
    .unlocked_ioctl = adxl345_ioctl,
};

static int adxl345_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    adxl345_device *dev;
    int irq = client->irq, ret;

    dev = devm_kzalloc(&client->dev, sizeof(adxl345_device), GFP_KERNEL);
    if (!dev) return -ENOMEM;

    dev->client = client;
    dev->axe = 0;
    INIT_KFIFO(dev->fifo);

    dev->misc.name = "adxl345";
    dev->misc.minor = MISC_DYNAMIC_MINOR;
    dev->misc.fops = &adxl345_fops;
 
   /*---------------------------------------------------------------------------------------------*/

   char sendbuff[2] ; 
   char FIFO_MOD[1] ; 


   //mode Stream
   sendbuff[0] = 0x38;
   sendbuff[1] = 0b10010100 ;    //stream mode   + INT1 bit 5   + 20 samples

   int sentDATA = i2c_master_send(client,sendbuff, 2);

   int sentADD = i2c_master_send(client,&sendbuff[0], 1);
   int fifoMod = i2c_master_recv(client,FIFO_MOD, 1);

   printk(KERN_INFO"SEND bytes : %d \n", sentADD);
   printk(KERN_INFO"FIFOMOD : 0x%02x\n", FIFO_MOD[0]);

   //read status
   sendbuff[0] = 0x39;
   char statusbuff[1];
     
   sentADD = i2c_master_send(client,&sendbuff[0], 1);
   int samples = i2c_master_recv(client,statusbuff, 1);

   
   printk(KERN_INFO"STATUS : 0x%02x\n", statusbuff[0]);
   


   // INTERRUPT 


   char INTBUFFER[1];
   sendbuff[0] = 0x2E;
   sendbuff[1] = 0x02;
   sentADD = i2c_master_send(client,sendbuff, 2);
   i2c_master_send(client,&sendbuff[0], 1);

   int intEnable = i2c_master_recv(client,INTBUFFER, 1);

   printk(KERN_INFO"INTERRUPT ENABLE : 0x%02x\n", INTBUFFER[0]);


   char INTMAP[1];
   sendbuff[0] = 0x2F;
   sendbuff[1] = 0b11111101;   //0 means that is sent to INT1
   sentADD = i2c_master_send(client,sendbuff, 2);


   
   i2c_master_send(client,&sendbuff[0], 1);

   int map = i2c_master_recv(client,INTMAP, 1);

   printk(KERN_INFO"INTERRUPT PIN MAP  : 0x%02x\n",INTMAP[0] )  ;
     

   char INTSRC[1];  //READ ONLY 
   sendbuff[0] = 0x30;
   
   sentADD = i2c_master_send(client,&sendbuff[0], 1);
   int intSrc = i2c_master_recv(client,INTSRC, 1);

  
   printk(KERN_INFO"INTERRUPT SOURCE  : 0x%02x\n", INTSRC[0]);

   //write some data on the registers X Y Z

   char DATAX0[2];  
   DATAX0[0] = 0x32;  
   DATAX0[1] = 0x00;  
   
   i2c_master_send(client,DATAX0, 2);
   /*---------------------------*/
   char DATAX1[2];  
   DATAX1[0] = 0x33;  
   DATAX1[1] = 0x11;  
   
   i2c_master_send(client,DATAX1, 2);
   /*---------------------------*/
   char DATAY0[2];  
   DATAY0[0] = 0x34;  
   DATAY0[1] = 0x22;  
   
   i2c_master_send(client,DATAY0, 2);
   /*---------------------------*/
   char DATAY1[2];  
   DATAY1[0] = 0x35;  
   DATAY1[1] = 0x33;  
   
   i2c_master_send(client,DATAY1, 2);
   /*---------------------------*/
   char DATAZ0[2];  
   DATAZ0[0] = 0x36;  
   DATAZ0[1] = 0x44;  
   
   i2c_master_send(client,DATAZ0, 2);
   /*---------------------------*/

   char DATAZ1[2];  
   DATAZ1[0] = 0x37;  
   DATAZ1[1] = 0x55;  
   
   i2c_master_send(client,DATAZ1, 2);
   /*---------------------------*/
   char RATE[2] ={BW_RATE, 0x0A};
   char FORMAT[2] ={DATA_FORMAT,0x08};  //FULL RES
   char POWER[2]   ={POWER_CTL_ON, 0x08}; // MEASURE MODE

   i2c_master_send(client,RATE, 2);
   i2c_master_send(client,FORMAT, 2);
   i2c_master_send(client,POWER, 2);



   


   /*---------------------------------------------------------------------------------------------*/



    

    if (irq <= 0) return -EINVAL;
    ret = devm_request_threaded_irq(&client->dev, irq, adxl345_handler, adxl345_threaded_irq, IRQF_TRIGGER_HIGH, "adxl345_irq", dev);
    if (ret) return ret;

    i2c_set_clientdata(client, dev);
    misc_register(&(dev->misc));
    return 0;
}

static int adxl345_remove(struct i2c_client *client) {
    adxl345_device *dev = i2c_get_clientdata(client);
    misc_deregister(&dev->misc);
    return 0;
}

static struct i2c_device_id adxl345_idtable[] = {
    { "adxl345", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, adxl345_idtable);

#ifdef CONFIG_OF
static const struct of_device_id adxl345_of_match[] = {
    { .compatible = "vendor,adxl345", },
    {}
};
MODULE_DEVICE_TABLE(of, adxl345_of_match);
#endif

static struct i2c_driver adxl345_driver = {
    .driver = {
        .name = "adxl345",
        .of_match_table = of_match_ptr(adxl345_of_match),
    },
    .id_table = adxl345_idtable,
    .probe = adxl345_probe,
    .remove = adxl345_remove,
};
module_i2c_driver(adxl345_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ADXL345 accelerometer driver");
MODULE_AUTHOR("Me");
