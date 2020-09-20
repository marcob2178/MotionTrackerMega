#ifndef ACCEL_BNO_080
#define ACCEL_BNO_080

#include <accels/Accelerometer.h>
#include <accels/SparkFun_BNO080_Arduino_Library.h>

class AccelBNO080 : public Accelerometer
{

private:
    BNO080 *myIMU;

    float offset_roll, offset_pitch;

    void setOffsets(double _offset_y, double _offset_z)
    {
        offset_roll = _offset_y;
        offset_pitch = _offset_z;
    }

public:
    AccelBNO080()
    {
        myIMU = new BNO080();
    }

    void calibrate()
    {

        if (myIMU->dataAvailable())
            setOffsets(((myIMU->getRoll()) * 180.0 / PI), ((myIMU->getPitch()) * 180.0 / PI));
    }

    void begin()
    {
        if (!myIMU->begin())
        {
            Serial.println("BNO080 not detected at default I2C address. Check your jumpers and the hookup guide. Trying again...");
            delay(1000);

            if (!myIMU->begin())
            {
                Serial.println("Something wrong, freezing....");
                return;
            }
        }
        myIMU->enableRotationVector(10);
        myIMU->enableAccelerometer(10);
        myIMU->enableLinearAccelerometer(10); //Send data update every 50ms
        calibrate();
    }

    double getLinX()
    {
        return myIMU->getLinAccelX();
    }
    double getLinY()
    {
        return myIMU->getLinAccelY();
    }
    double getLinZ()
    {
        return myIMU->getLinAccelZ();
    }
    double getYaw()
    {
        return ((myIMU->getYaw()) * 180.0 / PI);
    }
    double getPitch()
    {
        return ((myIMU->getPitch()) * 180.0 / PI) - offset_pitch;
        //return myIMU->getPitch();
    }
    double getRoll()
    {
        return ((myIMU->getRoll()) * 180.0 / PI) - offset_roll;
        // return myIMU->getRoll();
    }

    void update()
    {
        myIMU->dataAvailable();
    }
};

#endif