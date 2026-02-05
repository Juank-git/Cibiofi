#ifndef WAVEPLATE_STEPPER_H
#define WAVEPLATE_STEPPER_H

#include <Arduino.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>
#include <WebSocketsServer.h>

// ==============================================
// Definición de pines para los motores
// ==============================================

// Pines HSPI (Motor 1)
#define HSPI_MOSI   13    // GPIO13
#define HSPI_MISO   12    // GPIO12
#define HSPI_SCLK   14    // GPIO14
#define HSPI_CS     15    // GPIO15
#define ENABLE_PIN1 26
#define DIR_PIN1    32
#define STEP_PIN1   33

// Pines VSPI (Motor 2)
#define VSPI_MOSI    23    // GPIO23
#define VSPI_MISO    19    // GPIO19
#define VSPI_SCLK    18    // GPIO18
#define VSPI_CS      5     // GPIO5
#define ENABLE_PIN2  2
#define DIR_PIN2     22
#define STEP_PIN2    21

// Pin del sensor Hall
#define HALL_SENSOR_PIN1 35
#define HALL_SENSOR_PIN2 34

// Resolución del motor en pasos (full step) y constante de relación
#define SM_RESOLUTION 200
#define MICROSTEPS 4
#define GEAR_RATIO 3

class WavePlateStepper {
private:
    TMC2130Stepper driver1;
    TMC2130Stepper driver2;
    AccelStepper stepper1;
    AccelStepper stepper2;
    
    // Parámetros de configuración
    int stepperCurrent1;
    int stepperCurrent2;
    int stepperSpeed1;
    int stepperSpeed2;
    int stepperAcc1;
    int stepperAcc2;
    int microsteps1;
    int microsteps2;
    
    // Variables para detección Hall
    volatile bool hallTriggered1;
    volatile bool hallTriggered2;

    // Referencia al WebSocket
    WebSocketsServer* webSocket;

public:
    // Constructor
    WavePlateStepper();
    
    // Inicialización
    void begin(WebSocketsServer* webSocketPtr);
    
    // Funciones ISR para los sensores Hall
    static void IRAM_ATTR hallISR1();
    static void IRAM_ATTR hallISR2();
    
    // Operaciones básicas
    void run();
    float getCurrentAngle1();
    float getCurrentAngle2();
    
    // Funciones para control de motores
    void homeMotor1();
    void homeMotor2();
    void homeAllMotors();
    void moveMotor1ToAngle(float targetAngle);
    void moveMotor2ToAngle(float targetAngle);
    void moveMotor1ByStep(float stepAngle);
    void moveMotor2ByStep(float stepAngle);
    
    // Setters para los parámetros
    void setCurrents(int current1, int current2);
    void setSpeeds(int speed1, int speed2);
    void setAccelerations(int acc1, int acc2);
    void setMicrosteps(int micro1, int micro2);
    
    // Funciones para obtener referencias directas a los steppers
    AccelStepper& getStepper1() { return stepper1; }
    AccelStepper& getStepper2() { return stepper2; }
};

// Instancia global accesible externamente
extern WavePlateStepper wavePlateStepper;

#endif // WAVEPLATE_STEPPER_H
