# esp-idf-selfie-trigger
Using a selfie device with ESP32-CAM.   

![Image](https://github.com/user-attachments/assets/cc97da4e-6c06-4604-8362-f81c6fb6eb58)

These are selfie devices that use a Bluetooth remote control to trigger the shutter on your phone.   
These act as GATT servers.   
This project is a GATT client that communicates with these devices.   
You can control your ESP32-CAM using these devices.

__Note__   
There are many cloned products.   
There is no guarantee that it will work with all products.   


# About AB Shutter3
These devices have the remote device name ```AB Shutter3```.   
Click [here](https://github.com/nopnop2002/esp-idf-bluetooth-remote) for details.   

# Software requirements
ESP-IDF V5.0 or later.   
ESP-IDF V4.4 release branch reached EOL in July 2024.   

# Hardware requirements
AB Shutter3.   
The number of buttons does not matter.   

# Installation   
```
git clone https://github.com/nopnop2002/esp-idf-selfie-trigger
cd esp-idf-selfie-trigger
idf.py menuconfig
idf.py flash
```

# Configuration   
![Image](https://github.com/user-attachments/assets/787b47ef-ceb5-48a9-8cb9-490838ba5a40)
![Image](https://github.com/user-attachments/assets/42b09ba2-4f17-4146-a94e-64d7923b2dee)


## WiFi Setting
Set the information of your access point.   
![Image](https://github.com/user-attachments/assets/31e57f57-437b-4e2c-8592-363bebbd915d)

## Device Setting
Set the GPIO information.   
It can be turned on when the connection to the device is successful and when the trigger is fired.   
It is difficult to tell when the connection with the device is successful, so we recommend adding an LED to this port.   
![Image](https://github.com/user-attachments/assets/4ae4bfb3-c1c2-4405-add2-aedbc2f782b2)


# How to use
Press any button and release the button on your selfie device to act as a shutter for these projects.   
These projects must be configured as UDP servers.   
This project works as a UDP client.   
![Image](https://github.com/user-attachments/assets/55ca83ac-cf31-4c15-9e8e-c89c7dc5e07c)

- https://github.com/nopnop2002/esp-idf-ftp-camera



# Reasons why it is not possible to integrate into camera project
This project requires a large amount of RAM and a large amount of SPI Flash area.   
For this reason it cannot be integrated with the camera project.   
