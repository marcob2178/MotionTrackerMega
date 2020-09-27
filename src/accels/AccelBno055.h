#ifndef ACCEL_BNO_055
#define ACCEL_BNO_055

#include <accels/Accelerometer.h>
#include <accels/Adafruit_BNO055.h>

class AccelBNO055 : public Accelerometer
{

private:
    Adafruit_BNO055 *bno;
    imu::Vector<3> euler;
    imu::Vector<3> linaccel;

    double offset_y, offset_z;
    
    void setOffsets(double _offset_y, double _offset_z)
    {
        offset_y = _offset_y;
        offset_z = _offset_z;
    }
    //  x       y       z
    //  yaw     pitch   roll

public:
    AccelBNO055(int id, int address)
    {
        bno = new Adafruit_BNO055(id, address);
    };

    void calibrate()
    {
        euler = bno->getVector(Adafruit_BNO055::VECTOR_EULER);
        setOffsets(euler.y(), euler.z());
    }

    void begin()
    {
        if (!bno->begin())
        {
            Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
            return;
        }
        delay(500);
        calibrate();
    }

    double getLinX()
    {
        return linaccel.x();
    }
    double getLinY()
    {
        return linaccel.y();
    }
    double getLinZ()
    {
        return linaccel.z();
    }

    double getYaw()
    {
        return euler.x();
    }

    double getPitch()
    {
        double pitch = euler.y() - offset_y;
         if (pitch < -180)
            return pitch + 360;
        else
            return pitch;
    }

    double getRoll()
    { 
        double roll = euler.z() - offset_z;
        if (roll < -180)
            return roll + 360;
        else
            return roll;
    }

    void update()
    {
        euler = bno->getVector(Adafruit_BNO055::VECTOR_EULER);
        //delayMicroseconds(1000);
        linaccel = bno->getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
        //delayMicroseconds(1000);
    }
};

#endif