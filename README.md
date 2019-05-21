# MultitechConduitIoTHub
A simple C client implementing integration between the Multitech local LoRaWAN network server and IoT Hub.

To Build:

Follow the instructions for 'Getting Started' on the Multitech Conduit Azure certified device catalogue page. (https://catalog.azureiotsolutions.com/details?title=MultiConnect-Conduit&source=all-devices-page )

You should be able to setup the cross compilation tool chain and successfully compile the Azure IoT Hub C SDK samples. Then make this sample.

To run:

Place your IoT Hub connection string in the ConnectionString.json file.

Connect to the Conduit via USB cable or via SSH, log in as admin/admin
In the admin user's default directory (the one you are in when logged in), is a sub directory called iothub
cd iothub
Run the iot hub client in the foreground by:
./IoTHubClient
If you want to run the client in the background, but still see the output use:
./IoTHubClient &
You can use the top command to watch the process.